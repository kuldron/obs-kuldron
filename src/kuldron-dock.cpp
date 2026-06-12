/*
Kuldron OBS plugin — multitrack audio + multistreaming
Copyright (C) 2026 Kuldron

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "kuldron-api.hpp"
#include "kuldron-dock.hpp"
#include "kuldron-plugin.hpp"

#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>

#include <cstring>

namespace {
// The name a streamer gave OBS audio Track idx+1 in Settings > Output > Audio
// (Advanced output mode), stored per-profile as [AdvOut] Track{N}Name. Empty if
// unnamed or not in advanced mode.
QString obsTrackName(int idx)
{
	config_t *pc = obs_frontend_get_profile_config();
	if (!pc)
		return QString();
	QByteArray key = QStringLiteral("Track%1Name").arg(idx + 1).toUtf8();
	const char *name = config_get_string(pc, "AdvOut", key.constData());
	return (name && *name) ? QString::fromUtf8(name) : QString();
}

// True if OBS's own configured streaming target points at Kuldron — in which
// case OBS would already be streaming there (single-track), so auto-starting a
// second connection to the same key would collide.
bool obsServiceIsKuldron()
{
	obs_service_t *svc = obs_frontend_get_streaming_service(); // borrowed, do not release
	if (!svc)
		return false;
	const char *url = obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
	return url && strstr(url, "kuldron.com") != nullptr;
}
} // namespace

// --- construction ---------------------------------------------------------

KuldronDock::KuldronDock(QWidget *parent) : QWidget(parent)
{
	cfg = KuldronConfig::load();
	buildUi();
	loadIntoUi();
	autoFillKeyFromObs();

	// Output callbacks fire on libobs threads; bounce to the UI thread.
	output.onStarted = [this]() {
		QMetaObject::invokeMethod(
			this,
			[this]() {
				setLive(true);
				setStatus(QStringLiteral("Live to Kuldron."), false);
			},
			Qt::QueuedConnection);
	};
	output.onStopped = [this](int code, std::string message) {
		QString msg = QString::fromStdString(message);
		QMetaObject::invokeMethod(
			this,
			[this, code, msg]() {
				autoStarted = false;
				setLive(false);
				setStatus(msg, code != OBS_OUTPUT_SUCCESS);
			},
			Qt::QueuedConnection);
	};

	// Keep cfg mirrored from the widgets so the synchronous STREAMING_STARTING
	// path (the injector) never has to read widget state itself.
	for (auto *c : trackChecks)
		connect(c, &QCheckBox::toggled, this, &KuldronDock::syncCfg);
	connect(autoStartCheck, &QCheckBox::toggled, this, &KuldronDock::syncCfg);
	connect(serverEdit, &QLineEdit::editingFinished, this, &KuldronDock::syncCfg);
	connect(keyEdit, &QLineEdit::editingFinished, this, &KuldronDock::syncCfg);

	obs_frontend_add_event_callback(handleFrontendEvent, this);
}

KuldronDock::~KuldronDock()
{
	obs_frontend_remove_event_callback(handleFrontendEvent, this);
	injector.cleanup();
	output.shutdown();
	readFromUi();
	cfg.save();
}

// --- UI construction ------------------------------------------------------

void KuldronDock::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	// Connection
	auto *connBox = new QGroupBox(QStringLiteral("Connection"), this);
	auto *connForm = new QFormLayout(connBox);
	connForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	connForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
	serverEdit = new QLineEdit(connBox);
	serverEdit->setMinimumWidth(240);
	serverEdit->setPlaceholderText(QStringLiteral("rtmp://ingest.kuldron.com:1935/live"));
	keyEdit = new QLineEdit(connBox);
	keyEdit->setMinimumWidth(200);
	keyEdit->setEchoMode(QLineEdit::Password);
	keyEdit->setPlaceholderText(QStringLiteral("Paste your Kuldron stream key"));
	auto *showKey = new QCheckBox(QStringLiteral("Show"), connBox);
	connect(showKey, &QCheckBox::toggled, this,
		[this](bool on) { keyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
	auto *keyRow = new QHBoxLayout();
	keyRow->addWidget(keyEdit, 1);
	keyRow->addWidget(showKey);
	connForm->addRow(QStringLiteral("Server"), serverEdit);
	connForm->addRow(QStringLiteral("Stream key"), keyRow);
	root->addWidget(connBox);

	// Audio tracks
	auto *trackBox = new QGroupBox(QStringLiteral("Audio tracks"), this);
	auto *trackLayout = new QVBoxLayout(trackBox);
	auto *trackHint =
		new QLabel(QStringLiteral("Tick the OBS tracks to send — each becomes a separate audio track viewers "
					  "can fade. Assign sources to tracks and name them in OBS (Edit > Advanced "
					  "Audio Properties, Settings > Output > Audio)."),
			   trackBox);
	trackHint->setWordWrap(true);
	trackLayout->addWidget(trackHint);

	for (int i = 0; i < (int)KS_MAX_TRACKS; i++) {
		trackChecks[i] = new QCheckBox(trackBox);
		trackLayout->addWidget(trackChecks[i]);
	}

	auto *refreshBtn = new QPushButton(QStringLiteral("Refresh track names"), trackBox);
	connect(refreshBtn, &QPushButton::clicked, this, &KuldronDock::refreshTrackNames);
	trackLayout->addWidget(refreshBtn);
	root->addWidget(trackBox);

	// Multistream (companion output for non-Kuldron OBS targets)
	auto *multiBox = new QGroupBox(QStringLiteral("Multistream"), this);
	auto *multiLayout = new QVBoxLayout(multiBox);
	autoStartCheck = new QCheckBox(QStringLiteral("Enabled"), multiBox);
	multiLayout->addWidget(autoStartCheck);
	auto *multiHint = new QLabel(
		QStringLiteral("When your OBS stream target is not Kuldron, a stream will still be sent to Kuldron."),
		multiBox);
	multiHint->setWordWrap(true);
	multiLayout->addWidget(multiHint);
	root->addWidget(multiBox);

	// Go Live
	goLiveBtn = new QPushButton(QStringLiteral("Go Live to Kuldron"), this);
	goLiveBtn->setMinimumHeight(36);
	connect(goLiveBtn, &QPushButton::clicked, this, &KuldronDock::onGoLiveClicked);
	root->addWidget(goLiveBtn);

	statusLabel = new QLabel(QString(), this);
	statusLabel->setWordWrap(true);
	root->addWidget(statusLabel);

	root->addStretch(1);
}

// --- config <-> UI --------------------------------------------------------

void KuldronDock::refreshTrackNames()
{
	for (int i = 0; i < (int)KS_MAX_TRACKS; i++) {
		QString label = QStringLiteral("Track %1").arg(i + 1);
		QString name = obsTrackName(i);
		if (!name.isEmpty())
			label += QStringLiteral(" — %1").arg(name);
		trackChecks[i]->setText(label);
	}
}

void KuldronDock::loadIntoUi()
{
	serverEdit->setText(QString::fromStdString(cfg.server));
	keyEdit->setText(QString::fromStdString(cfg.key));
	autoStartCheck->setChecked(cfg.autoStart);

	refreshTrackNames();
	for (int i = 0; i < (int)KS_MAX_TRACKS; i++)
		trackChecks[i]->setChecked(cfg.enabledTracks & (1u << i));
}

void KuldronDock::readFromUi()
{
	cfg.server = serverEdit->text().trimmed().toStdString();
	cfg.key = keyEdit->text().trimmed().toStdString();
	cfg.autoStart = autoStartCheck->isChecked();

	cfg.enabledTracks = 0;
	for (int i = 0; i < (int)KS_MAX_TRACKS; i++) {
		if (trackChecks[i]->isChecked())
			cfg.enabledTracks |= (1u << i);
	}
}

void KuldronDock::syncCfg()
{
	readFromUi();
	cfg.save();
}

void KuldronDock::autoFillKeyFromObs()
{
	if (!cfg.key.empty())
		return;
	obs_service_t *svc = obs_frontend_get_streaming_service(); // borrowed
	if (!svc)
		return;
	const char *url = obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
	if (!url || !strstr(url, "kuldron.com"))
		return;
	const char *k = obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_STREAM_KEY);
	if (k && *k) {
		cfg.key = k;
		keyEdit->setText(QString::fromUtf8(k));
	}
}

// --- start / stop ---------------------------------------------------------

// Send the enabled tracks' OBS names (wire order) to the API so viewer
// faders show real labels (KUL-206). Unnamed tracks send "" and keep the
// backend's per-slot "Audio N" fallback. Fire-and-forget.
void KuldronDock::sendTrackNames(const std::string &server, const std::string &key)
{
	std::vector<std::string> names;
	for (int idx : cfg.enabledTrackIndices())
		names.push_back(obsTrackName(idx).trimmed().toStdString());
	postTrackNames(server, key, names);
}

void KuldronDock::startOutput()
{
	readFromUi();
	cfg.save();

	std::string error;
	if (!output.start(cfg, error)) {
		autoStarted = false;
		setStatus(QString::fromStdString(error), true);
		return;
	}
	sendTrackNames(cfg.server, cfg.key);
	goLiveBtn->setEnabled(false); // re-enabled by the start/stop callback
	setStatus(QStringLiteral("Connecting..."), false);
}

void KuldronDock::onGoLiveClicked()
{
	if (live) {
		autoStarted = false;
		setStatus(QStringLiteral("Stopping..."), false);
		output.stop();
		return;
	}
	startOutput();
}

// --- OBS streaming lifecycle (injection + auto-start) ----------------------

void KuldronDock::handleFrontendEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<KuldronDock *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		// Synchronous on purpose: this is the window after OBS configured
		// its stream output but before obs_output_start(). Queueing would
		// run after the output started, when injection is impossible.
		self->onObsStreamingStarting();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		QMetaObject::invokeMethod(self, [self]() { self->onObsStreamingStarted(); }, Qt::QueuedConnection);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		QMetaObject::invokeMethod(self, [self]() { self->onObsStreamingStopping(); }, Qt::QueuedConnection);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		QMetaObject::invokeMethod(self, [self]() { self->onObsStreamingStopped(); }, Qt::QueuedConnection);
		break;
	default:
		break;
	}
}

void KuldronDock::onObsStreamingStarting()
{
	if (!obsServiceIsKuldron())
		return;
	// OBS itself is streaming to Kuldron: upgrade its output in place. cfg is
	// kept current by syncCfg(), so no widget access here.
	lastInjectFailReason.clear();
	if (injector.inject(cfg, lastInjectFailReason)) {
		// The stream rides OBS's own service; name the tracks under ITS
		// server + key, not the dock's.
		obs_service_t *svc = obs_frontend_get_streaming_service(); // borrowed
		const char *url = svc ? obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_SERVER_URL)
				      : nullptr;
		const char *key = svc ? obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_STREAM_KEY)
				      : nullptr;
		if (url && key)
			sendTrackNames(url, key);
	}
}

void KuldronDock::onObsStreamingStarted()
{
	if (obsServiceIsKuldron()) {
		// Passive mode: report how the injection went; never start a second
		// connection to the same key.
		if (injector.active()) {
			setStatus(QStringLiteral("Live to Kuldron — OBS's stream upgraded to %1 audio tracks.")
					  .arg(injector.trackCount()),
				  false);
		} else {
			setStatus(QStringLiteral("OBS is streaming to Kuldron with a single audio track (%1).")
					  .arg(QString::fromStdString(lastInjectFailReason)),
				  true);
		}
		return;
	}

	if (!autoStartCheck->isChecked() || output.active())
		return;
	autoStarted = true;
	startOutput();
}

void KuldronDock::onObsStreamingStopping()
{
	// Only follow OBS down if we came up with it; never kill a manual Go Live.
	if (autoStarted && output.active())
		output.stop();
	autoStarted = false;
}

void KuldronDock::onObsStreamingStopped()
{
	if (injector.active()) {
		injector.cleanup();
		setStatus(QStringLiteral("Stream ended."), false);
	}
}

// --- view state -----------------------------------------------------------

void KuldronDock::setLive(bool isLive)
{
	live = isLive;
	goLiveBtn->setEnabled(true);
	goLiveBtn->setText(isLive ? QStringLiteral("Stop") : QStringLiteral("Go Live to Kuldron"));

	serverEdit->setEnabled(!isLive);
	keyEdit->setEnabled(!isLive);
	for (auto *c : trackChecks)
		c->setEnabled(!isLive);
}

void KuldronDock::setStatus(const QString &text, bool error)
{
	statusLabel->setText(text);
	statusLabel->setStyleSheet(error ? QStringLiteral("color: #d9534f;") : QString());
	if (error)
		obs_log(LOG_WARNING, "%s", text.toUtf8().constData());
}
