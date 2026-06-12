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

#include <cstddef>

// OBS exposes 6 audio mixer tracks (MAX_AUDIO_MIXES). That is also our hard
// ceiling on how many audio tracks we can send — one encoder per mixer.
static constexpr size_t KS_MAX_TRACKS = 6;

static constexpr const char *KS_DOCK_ID = "kuldron-dock";
static constexpr const char *KS_DOCK_TITLE = "Kuldron";

// Stable OBS object names for the libobs objects we own. Reused (not leaked)
// across Go Live / Stop cycles.
static constexpr const char *KS_OUTPUT_NAME = "kuldron_output";
static constexpr const char *KS_SERVICE_NAME = "kuldron_service";
static constexpr const char *KS_VIDEO_ENCODER_NAME = "kuldron_video";
