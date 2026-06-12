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

#include "kuldron-output.hpp"
#include "kuldron-plugin.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

// ffmpeg_aac is bundled with every OBS build on every platform, so the plugin
// works without depending on the platform AAC encoder (mf_aac / CoreAudio_AAC).
static constexpr const char *AUDIO_ENCODER_ID = "ffmpeg_aac";

// Get the video encoder. When OBS is already streaming (the multistream
// case), SHARE its running encoder outright — libobs encoders feed any
// number of outputs, so the Kuldron leg costs zero extra GPU encode and the
// video is bit-identical to the primary stream. We hold our own reference
// and release it like any encoder we created; the encoder stays alive for
// whichever output still uses it. The streamer's GOP rides as-is: the
// ingest's connect-time rejection owns the <=2s rule.
//
// When OBS isn't streaming there is nothing to share, so build a default
// x264 encoder.
static obs_encoder_t *createVideoEncoder(const KuldronConfig &cfg)
{
	obs_output_t *streamOut = obs_frontend_get_streaming_output(); // incref'd
	if (streamOut) {
		obs_encoder_t *shared = nullptr;
		if (obs_output_active(streamOut)) {
			obs_encoder_t *live = obs_output_get_video_encoder(streamOut); // borrowed
			if (live)
				shared = obs_encoder_get_ref(live);
		}
		obs_output_release(streamOut);
		if (shared)
			return shared; // already bound to the video pipeline
	}

	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "bitrate", cfg.videoBitrate > 0 ? cfg.videoBitrate : 2500);
	obs_data_set_string(s, "rate_control", "CBR");
	// Our own encoder, so we pick its defaults; x264's auto keyint (250
	// frames) would always fail the ingest's <=2s GOP check.
	obs_data_set_int(s, "keyint_sec", 2);
	obs_encoder_t *ve = obs_video_encoder_create("obs_x264", KS_VIDEO_ENCODER_NAME, s, nullptr);
	obs_data_release(s);
	if (ve)
		obs_encoder_set_video(ve, obs_get_video());
	return ve;
}

static std::string messageForCode(int code, const char *lastError)
{
	std::string detail = (lastError && *lastError) ? std::string(" (") + lastError + ")" : "";
	switch (code) {
	case OBS_OUTPUT_SUCCESS:
		return "Stream ended.";
	case OBS_OUTPUT_BAD_PATH:
		return "Invalid server URL or stream key." + detail;
	case OBS_OUTPUT_CONNECT_FAILED:
		return "Could not connect to the Kuldron ingest server." + detail;
	case OBS_OUTPUT_INVALID_STREAM:
		return "The ingest server rejected the stream (key, GOP, or codec)." + detail;
	case OBS_OUTPUT_DISCONNECTED:
		return "Disconnected from the ingest server." + detail;
	case OBS_OUTPUT_UNSUPPORTED:
		return "The encoder configuration is unsupported." + detail;
	case OBS_OUTPUT_NO_SPACE:
		return "Out of disk space." + detail;
	case OBS_OUTPUT_ENCODE_ERROR:
		return "Encoder failure." + detail;
	default:
		return "Stream stopped unexpectedly." + detail;
	}
}

KuldronOutput::~KuldronOutput()
{
	shutdown();
}

bool KuldronOutput::active() const
{
	std::lock_guard<std::mutex> lock(mtx);
	return output && obs_output_active(output);
}

bool KuldronOutput::start(const KuldronConfig &cfg, std::string &error)
{
	std::string reason = cfg.validate();
	if (!reason.empty()) {
		error = reason;
		return false;
	}

	std::lock_guard<std::mutex> lock(mtx);

	if (output && obs_output_active(output)) {
		error = "Already live.";
		return false;
	}

	// Free any objects lingering from a previous (stopped) session. Doing this
	// here — outside any signal dispatch — is the only place we release the
	// output, which avoids freeing it inside its own "stop" callback.
	releaseObjects();

	// --- custom RTMP service (ingest URL + stream key) ---
	obs_data_t *ss = obs_data_create();
	obs_data_set_string(ss, "server", cfg.server.c_str());
	obs_data_set_string(ss, "key", cfg.key.c_str());
	obs_service_t *svc = obs_service_create("rtmp_custom", KS_SERVICE_NAME, ss, nullptr);
	obs_data_release(ss);
	if (!svc) {
		error = "Failed to create the RTMP service.";
		return false;
	}

	// --- video encoder (shared with OBS's live stream when possible) ---
	obs_encoder_t *ve = createVideoEncoder(cfg);
	if (!ve) {
		obs_service_release(svc);
		error = "Failed to create the video encoder.";
		return false;
	}

	// --- one audio encoder per enabled OBS track, on that track's mixer ---
	std::vector<int> tracks = cfg.enabledTrackIndices();
	std::vector<obs_encoder_t *> encs;
	for (size_t i = 0; i < tracks.size(); i++) {
		obs_data_t *as = obs_data_create();
		obs_data_set_int(as, "bitrate", cfg.audioBitrate);
		std::string name = std::string("kuldron_audio_") + std::to_string(i);
		obs_encoder_t *ae =
			obs_audio_encoder_create(AUDIO_ENCODER_ID, name.c_str(), as, (size_t)tracks[i], nullptr);
		obs_data_release(as);
		if (!ae) {
			for (auto e : encs)
				obs_encoder_release(e);
			obs_encoder_release(ve);
			obs_service_release(svc);
			error = "Failed to create the audio encoder for Track " + std::to_string(tracks[i] + 1) + ".";
			return false;
		}
		obs_encoder_set_audio(ae, obs_get_audio());
		encs.push_back(ae);
	}

	// --- output: attach video + every audio encoder (wire track 0..N-1) ---
	obs_output_t *out = obs_output_create("rtmp_output", KS_OUTPUT_NAME, nullptr, nullptr);
	if (!out) {
		for (auto e : encs)
			obs_encoder_release(e);
		obs_encoder_release(ve);
		obs_service_release(svc);
		error = "Failed to create the RTMP output.";
		return false;
	}
	obs_output_set_video_encoder(out, ve);
	for (size_t i = 0; i < encs.size(); i++)
		obs_output_set_audio_encoder(out, encs[i], i);
	obs_output_set_service(out, svc);

	signal_handler_t *sh = obs_output_get_signal_handler(out);
	signal_handler_connect(sh, "start", onStart, this);
	signal_handler_connect(sh, "stop", onStop, this);

	if (!obs_output_start(out)) {
		const char *le = obs_output_get_last_error(out);
		error = (le && *le) ? std::string(le) : "OBS refused to start the stream (see the OBS log).";
		signal_handler_disconnect(sh, "start", onStart, this);
		signal_handler_disconnect(sh, "stop", onStop, this);
		obs_output_release(out);
		for (auto e : encs)
			obs_encoder_release(e);
		obs_encoder_release(ve);
		obs_service_release(svc);
		return false;
	}

	output = out;
	service = svc;
	venc = ve;
	aencs = std::move(encs);
	signalsConnected = true;

	obs_log(LOG_INFO, "streaming %zu audio track(s) to %s", tracks.size(), cfg.server.c_str());
	return true;
}

void KuldronOutput::stop()
{
	std::lock_guard<std::mutex> lock(mtx);
	if (output && obs_output_active(output)) {
		obs_output_stop(output); // async; onStop fires when finished
	} else {
		releaseObjects();
	}
}

void KuldronOutput::shutdown()
{
	std::lock_guard<std::mutex> lock(mtx);
	if (output && signalsConnected) {
		// Disconnect before force-stop so our handler can't re-enter while we
		// tear down on the unload path.
		signal_handler_t *sh = obs_output_get_signal_handler(output);
		signal_handler_disconnect(sh, "start", onStart, this);
		signal_handler_disconnect(sh, "stop", onStop, this);
		signalsConnected = false;
	}
	if (output && obs_output_active(output))
		obs_output_force_stop(output);
	releaseObjects();
}

void KuldronOutput::releaseObjects()
{
	if (output && signalsConnected) {
		signal_handler_t *sh = obs_output_get_signal_handler(output);
		signal_handler_disconnect(sh, "start", onStart, this);
		signal_handler_disconnect(sh, "stop", onStop, this);
		signalsConnected = false;
	}
	for (auto e : aencs)
		obs_encoder_release(e);
	aencs.clear();
	if (venc) {
		obs_encoder_release(venc);
		venc = nullptr;
	}
	if (service) {
		obs_service_release(service);
		service = nullptr;
	}
	if (output) {
		obs_output_release(output);
		output = nullptr;
	}
}

void KuldronOutput::onStart(void *data, calldata_t *)
{
	auto *self = static_cast<KuldronOutput *>(data);
	if (self->onStarted)
		self->onStarted();
}

void KuldronOutput::onStop(void *data, calldata_t *cd)
{
	auto *self = static_cast<KuldronOutput *>(data);
	long long code = OBS_OUTPUT_SUCCESS;
	calldata_get_int(cd, "code", &code);
	const char *le = self->output ? obs_output_get_last_error(self->output) : nullptr;
	std::string msg = messageForCode((int)code, le);
	// Note: we intentionally do NOT release the output here — releasing an
	// output from inside its own "stop" signal is a use-after-free risk.
	// The objects are freed on the next start() or on shutdown().
	if (self->onStopped)
		self->onStopped((int)code, msg);
}
