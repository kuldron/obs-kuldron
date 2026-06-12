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

#include <curl/curl.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <thread>

namespace {

// rtmp://ingest.kuldron.com:1935/live -> "https://api.kuldron.com", or ""
// when the host doesn't follow the ingest.<domain> convention (raw IPs,
// test boxes) — in which case there is no API to talk to and we skip.
std::string apiBaseFromRtmpServer(const std::string &server)
{
	const std::string scheme = "rtmp://";
	if (server.rfind(scheme, 0) != 0)
		return "";
	std::string rest = server.substr(scheme.size());
	size_t hostEnd = rest.find_first_of(":/");
	std::string host = hostEnd == std::string::npos ? rest : rest.substr(0, hostEnd);
	const std::string prefix = "ingest.";
	if (host.rfind(prefix, 0) != 0)
		return "";
	return "https://api." + host.substr(prefix.size());
}

// Minimal JSON string escaping (quotes, backslashes, control chars).
std::string jsonEscape(const std::string &s)
{
	std::string out;
	out.reserve(s.size() + 8);
	for (char c : s) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			} else {
				out += c;
			}
		}
	}
	return out;
}

void postBlocking(const std::string &url, const std::string &body)
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		obs_log(LOG_WARNING, "track names: curl init failed");
		return;
	}
	curl_slist *headers = curl_slist_append(nullptr, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	// The API's WAF rejects requests without a User-Agent (HTTP 403) —
	// libcurl sends none by default. Found live on the first 1.0.0 run.
	std::string ua = std::string("obs-kuldron/") + PLUGIN_VERSION;
	curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	CURLcode rc = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	if (rc != CURLE_OK) {
		obs_log(LOG_WARNING, "track names POST failed: %s", curl_easy_strerror(rc));
	} else if (status >= 300) {
		obs_log(LOG_WARNING, "track names POST -> HTTP %ld", status);
	} else {
		obs_log(LOG_INFO, "track names sent (HTTP %ld)", status);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
}

} // namespace

void postTrackNames(const std::string &rtmpServer, const std::string &streamKey, const std::vector<std::string> &names)
{
	if (streamKey.empty() || names.empty())
		return;
	std::string base = apiBaseFromRtmpServer(rtmpServer);
	if (base.empty()) {
		obs_log(LOG_DEBUG, "track names: non-standard ingest host; skipping");
		return;
	}

	std::string body = "{\"stream_key\":\"" + jsonEscape(streamKey) + "\",\"names\":[";
	for (size_t i = 0; i < names.size(); i++) {
		if (i)
			body += ",";
		body += "\"" + jsonEscape(names[i]) + "\"";
	}
	body += "]}";

	std::string url = base + "/streams/track-names";
	std::thread([url, body]() { postBlocking(url, body); }).detach();
}
