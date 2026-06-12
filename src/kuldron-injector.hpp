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

#include <string>
#include <vector>

// Upgrades OBS's *own* streaming output to multitrack audio when the streamer
// has Kuldron configured as their normal OBS stream target.
//
// Mechanism (verified against the OBS 31.1.1 frontend): OBS configures the
// stream output in SetupStreaming(), then fires OBS_FRONTEND_EVENT_
// STREAMING_STARTING synchronously, and only then calls obs_output_start().
// In that window the output is fully built but inactive, so we may attach our
// per-track audio encoders before liftoff — flv-mux emits enhanced-RTMP
// multitrack whenever >=2 audio encoders are attached, with no service gating.
// Video (encoder, GOP) is left exactly as the streamer configured it.
//
// inject() must therefore be called synchronously from the STREAMING_STARTING
// frontend callback; once the output is active it declines (the stream then
// goes out single-track exactly as without the plugin).
class KuldronInjector {
public:
	KuldronInjector() = default;
	~KuldronInjector() { cleanup(); }

	KuldronInjector(const KuldronInjector &) = delete;
	KuldronInjector &operator=(const KuldronInjector &) = delete;

	// Attach one audio encoder per enabled track to OBS's pending streaming
	// output (wire tracks 0..N-1, replacing OBS's single mix at 0). Returns
	// true on success; on failure fills `reason` and leaves the output
	// untouched.
	bool inject(const KuldronConfig &cfg, std::string &reason);

	// Detach (output permitting) and release our encoders. Call from the
	// STREAMING_STOPPED frontend callback, or on teardown.
	void cleanup();

	bool active() const { return !encs.empty(); }
	size_t trackCount() const { return encs.size(); }

private:
	std::vector<obs_encoder_t *> encs;
};
