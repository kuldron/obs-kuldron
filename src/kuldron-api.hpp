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

#include <string>
#include <vector>

// Fire-and-forget POST of the streamer's track names to the Kuldron API
// (KUL-206). Enhanced-RTMP multitrack carries no per-track name, so this
// side-channel is how viewers' faders get real labels instead of "Audio N".
//
// `names[i]` labels wire track i (the audio_i rendition); empty entries mean
// "no name" and the backend falls back per slot. The API base is derived
// from the RTMP server host (rtmp://ingest.X -> https://api.X); hosts that
// don't match (IPs, test boxes) skip silently. Runs on a detached thread
// with a short timeout — never blocks or fails a stream.
void postTrackNames(const std::string &rtmpServer, const std::string &streamKey, const std::vector<std::string> &names);
