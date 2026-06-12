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

#include "kuldron-config.hpp"
#include "kuldron-plugin.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <plugin-support.h>

static constexpr const char *CONFIG_FILE = "config.json";

KuldronConfig KuldronConfig::defaults()
{
	return KuldronConfig{};
}

std::vector<int> KuldronConfig::enabledTrackIndices() const
{
	std::vector<int> v;
	for (int i = 0; i < (int)KS_MAX_TRACKS; i++) {
		if (enabledTracks & (1u << i))
			v.push_back(i);
	}
	return v;
}

std::string KuldronConfig::validate() const
{
	if (server.empty())
		return "Server URL is empty.";
	if (key.empty())
		return "Stream key is empty.";
	if (enabledTrackIndices().empty())
		return "Select at least one audio track to send.";
	return "";
}

KuldronConfig KuldronConfig::load()
{
	char *path = obs_module_config_path(CONFIG_FILE);
	if (!path)
		return KuldronConfig::defaults();

	obs_data_t *data = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!data)
		return KuldronConfig::defaults();

	KuldronConfig c;
	c.server = obs_data_get_string(data, "server");
	c.key = obs_data_get_string(data, "key");
	c.autoStart = obs_data_get_bool(data, "auto_start");
	c.videoBitrate = (int)obs_data_get_int(data, "video_bitrate");
	c.audioBitrate = (int)obs_data_get_int(data, "audio_bitrate");

	// has_user_value distinguishes "saved 0 tracks" from "key absent", so an
	// older config or a corrupt file falls back to the defaults rather than to
	// an unusable no-tracks state.
	if (obs_data_has_user_value(data, "enabled_tracks"))
		c.enabledTracks = (uint32_t)obs_data_get_int(data, "enabled_tracks");

	obs_data_release(data);

	if (c.server.empty())
		c.server = KuldronConfig::defaults().server;
	if (c.audioBitrate <= 0)
		c.audioBitrate = KuldronConfig::defaults().audioBitrate;
	return c;
}

void KuldronConfig::save() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "server", server.c_str());
	obs_data_set_string(data, "key", key.c_str());
	obs_data_set_bool(data, "auto_start", autoStart);
	obs_data_set_int(data, "video_bitrate", videoBitrate);
	obs_data_set_int(data, "audio_bitrate", audioBitrate);
	obs_data_set_int(data, "enabled_tracks", enabledTracks);

	// Ensure the module config dir exists (obs_module_config_path(nullptr)
	// returns the directory itself).
	char *dir = obs_module_config_path(nullptr);
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path(CONFIG_FILE);
	if (path) {
		if (!obs_data_save_json_safe(data, path, "tmp", "bak"))
			obs_log(LOG_WARNING, "failed to save config to %s", path);
		bfree(path);
	}
	obs_data_release(data);
}
