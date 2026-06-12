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

#include <cstdint>
#include <string>
#include <vector>

struct KuldronConfig {
	// rtmp_custom "server" — the application URL, key appended by OBS.
	std::string server = "rtmp://ingest.kuldron.com:1935/live";
	std::string key;

	// Start our multitrack output automatically when OBS starts streaming
	// (e.g. to Twitch/YouTube via OBS or a multistream plugin), so a normal
	// "Start Streaming" also lights up Kuldron.
	bool autoStart = false;

	// Fallback encoder settings, used only when OBS isn't streaming and we
	// can't clone its live stream encoder. 0 bitrate => 2500.
	int videoBitrate = 0;
	int audioBitrate = 160; // kbps per sent track

	// Which OBS audio tracks to send, as a bitmask over mixer indices 0..5
	// (bit i == "Track i+1"). Each enabled track becomes its own enhanced-RTMP
	// audio track, which the ingest fan-out turns into an audio_N HLS rendition
	// and the player gives an independent fader. Routing sources to tracks and
	// naming them stays in OBS (Advanced Audio Properties / Output > Audio).
	uint32_t enabledTracks = 0b000111; // Tracks 1, 2, 3 by default

	static KuldronConfig defaults();

	// Persisted as JSON under the module config dir (obs_module_config_path).
	// The stream key lives here (per-machine), never in the scene collection.
	static KuldronConfig load();
	void save() const;

	// Returns "" if OK, otherwise a human-readable reason.
	std::string validate() const;

	// Enabled mixer indices in ascending (== on-wire) order.
	std::vector<int> enabledTrackIndices() const;
};
