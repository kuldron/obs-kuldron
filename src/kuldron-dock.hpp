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

#pragma once

#include "kuldron-config.hpp"
#include "kuldron-injector.hpp"
#include "kuldron-output.hpp"
#include "kuldron-plugin.hpp"

#include <obs-frontend-api.h>

#include <QWidget>

class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

// The Kuldron dock. Two modes, picked by where OBS itself streams:
//   - OBS's stream target IS Kuldron  -> passive: on stream start we upgrade
//     OBS's own output to multitrack (KuldronInjector); nothing to click.
//   - OBS streams elsewhere (Twitch/multistream) -> companion: our own
//     KuldronOutput, started manually (Go Live) or automatically with OBS's
//     streaming lifecycle (auto-start toggle).
// Routing sources to tracks, naming them, and the encoder all come from OBS;
// this dock just picks tracks.
class KuldronDock : public QWidget {
	Q_OBJECT

public:
	explicit KuldronDock(QWidget *parent = nullptr);
	~KuldronDock() override;

private slots:
	void onGoLiveClicked();
	void refreshTrackNames();
	void syncCfg(); // mirror widget state into cfg (so non-UI readers never touch widgets)

private:
	void buildUi();
	void loadIntoUi();
	void readFromUi();
	void setLive(bool isLive);
	void setStatus(const QString &text, bool error);

	void startOutput();        // shared by manual Go Live + auto-start
	void autoFillKeyFromObs(); // prefill the key from OBS's service if it's Kuldron
	// POST the enabled tracks' OBS names to the API (viewer fader labels).
	void sendTrackNames(const std::string &server, const std::string &key);

	// React to OBS's own streaming lifecycle. STREAMING_STARTING is handled
	// synchronously — that's the pre-obs_output_start window the injector
	// needs; queueing it would land after the output started.
	static void handleFrontendEvent(enum obs_frontend_event event, void *data);
	void onObsStreamingStarting();
	void onObsStreamingStarted();
	void onObsStreamingStopping();
	void onObsStreamingStopped();

	KuldronConfig cfg;
	KuldronOutput output;
	KuldronInjector injector;
	bool live = false;
	bool autoStarted = false;         // our output started because OBS started streaming
	std::string lastInjectFailReason; // why the last injection attempt declined

	QLineEdit *serverEdit = nullptr;
	QLineEdit *keyEdit = nullptr;
	QCheckBox *autoStartCheck = nullptr;
	QCheckBox *trackChecks[KS_MAX_TRACKS] = {};
	QPushButton *goLiveBtn = nullptr;
	QLabel *statusLabel = nullptr;
};
