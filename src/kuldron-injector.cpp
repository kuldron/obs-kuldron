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

#include "kuldron-injector.hpp"
#include "kuldron-plugin.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

// Same encoder choice as the companion output: ffmpeg_aac ships with every
// OBS build on every platform.
static constexpr const char *AUDIO_ENCODER_ID = "ffmpeg_aac";

bool KuldronInjector::inject(const KuldronConfig &cfg, std::string &reason)
{
	if (active()) {
		reason = "already injected";
		return false;
	}

	std::vector<int> tracks = cfg.enabledTrackIndices();
	if (tracks.size() < 2) {
		// With one (or zero) tracks ticked OBS's own single-track stream is
		// already equivalent; leave it alone.
		reason = "fewer than two audio tracks enabled";
		return false;
	}

	obs_output_t *out = obs_frontend_get_streaming_output(); // incref'd
	if (!out) {
		reason = "no streaming output";
		return false;
	}
	if (obs_output_active(out)) {
		// The pre-start window didn't materialize (unexpected frontend
		// ordering, or an enhanced-broadcasting output that starts
		// asynchronously). Decline; the stream goes out single-track.
		obs_output_release(out);
		reason = "the stream output is already running";
		return false;
	}

	// The video encoder is left exactly as the streamer configured it. The
	// ingest enforces its own GOP requirement (<=2s, closed) at connect with a
	// descriptive rejection, so a bad keyframe interval surfaces as an OBS
	// setting to fix rather than being silently overridden here.

	// One encoder per ticked track at wire index 0..N-1. Index 0 replaces the
	// single mix OBS attached in SetupStreaming(), so the on-wire layout is
	// exactly the ticked tracks (no duplicated full mix for viewers).
	for (size_t i = 0; i < tracks.size(); i++) {
		obs_data_t *as = obs_data_create();
		obs_data_set_int(as, "bitrate", cfg.audioBitrate);
		std::string name = std::string("kuldron_inject_") + std::to_string(i);
		obs_encoder_t *enc =
			obs_audio_encoder_create(AUDIO_ENCODER_ID, name.c_str(), as, (size_t)tracks[i], nullptr);
		obs_data_release(as);
		if (!enc) {
			for (size_t j = 0; j < encs.size(); j++) {
				// Leave index 0 attached if we never replaced it; only undo
				// what we set.
				obs_output_set_audio_encoder(out, nullptr, j);
				obs_encoder_release(encs[j]);
			}
			encs.clear();
			obs_output_release(out);
			reason = "failed to create an audio encoder";
			return false;
		}
		obs_encoder_set_audio(enc, obs_get_audio());
		obs_output_set_audio_encoder(out, enc, i);
		encs.push_back(enc);
	}

	obs_output_release(out);
	obs_log(LOG_INFO, "upgraded OBS's stream to %zu-track multitrack audio", encs.size());
	return true;
}

void KuldronInjector::cleanup()
{
	if (encs.empty())
		return;

	// Detach from the (now stopped) output so it never holds pointers to
	// encoders we are about to release — OBS reuses the output object across
	// stream sessions and only re-sets index 0 itself.
	obs_output_t *out = obs_frontend_get_streaming_output(); // incref'd
	if (out) {
		if (!obs_output_active(out)) {
			for (size_t i = 0; i < encs.size(); i++)
				obs_output_set_audio_encoder(out, nullptr, i);
		} else {
			// Teardown while the output still runs only happens during OBS
			// shutdown (outputs are stopped before modules unload).
			obs_log(LOG_WARNING, "releasing injected encoders while output active");
		}
		obs_output_release(out);
	}

	for (auto *e : encs)
		obs_encoder_release(e);
	encs.clear();
}
