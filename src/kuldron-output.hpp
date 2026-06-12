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

#include <obs.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Owns one rtmp_output plus its video encoder and N per-stem audio encoders,
// driving libobs directly so the multitrack-audio output is emitted regardless
// of OBS frontend gating (every native custom-server path is Twitch-gated; see
// KUL-196). Each stem is an audio encoder bound to a distinct mixer index and
// attached at output track 0..N-1 — the on-wire trackId the ingest fan-out
// splits into audio_0/audio_1/... HLS renditions.
class KuldronOutput {
public:
	KuldronOutput() = default;
	~KuldronOutput();

	KuldronOutput(const KuldronOutput &) = delete;
	KuldronOutput &operator=(const KuldronOutput &) = delete;

	// Build encoders + service + output from cfg and start streaming.
	// Returns false and fills `error` on any setup failure (config invalid,
	// encoder creation failed, output refused to start). On failure all
	// partially-created objects are released.
	bool start(const KuldronConfig &cfg, std::string &error);

	// Request a graceful stop. Objects are released once the output's "stop"
	// signal fires (or immediately if it was never active).
	void stop();

	// Force-stop and release everything synchronously. Safe to call from
	// obs_module_unload.
	void shutdown();

	bool active() const;

	// Callbacks are invoked from libobs threads; marshal to the UI thread in
	// the handler. `onStopped` receives the OBS_OUTPUT_* code and a
	// human-readable message (last_error when present).
	std::function<void()> onStarted;
	std::function<void(int code, std::string message)> onStopped;

private:
	void releaseObjects(); // caller holds mtx
	static void onStart(void *data, calldata_t *cd);
	static void onStop(void *data, calldata_t *cd);

	mutable std::mutex mtx;
	obs_output_t *output = nullptr;
	obs_service_t *service = nullptr;
	obs_encoder_t *venc = nullptr;
	std::vector<obs_encoder_t *> aencs;
	bool signalsConnected = false;
};
