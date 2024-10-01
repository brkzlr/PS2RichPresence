/*
    Copyright (C) 2024 brkzlr <brksys@icloud.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "SambaReader.h"

#include <json-c/json.h>
#include <json-c/json_visit.h>
#include <stdio.h>
#include <string.h>

static struct {
	const char* sharePath;
	string_t gameName;
	time_t timestamp;
	bool isValid;
} g_smbGameInfo;

static int ProcessSmbKeys(json_object* jso, int flags, json_object* parent_jso, const char* jso_key, size_t* jso_index, void* userarg)
{
	static bool processedName = false;

	if (processedName && g_smbGameInfo.timestamp != 0) {
		processedName = false;
		g_smbGameInfo.isValid = true;
		return JSON_C_VISIT_RETURN_STOP;
	}
	if (!parent_jso || flags == JSON_C_VISIT_SECOND) {
		return JSON_C_VISIT_RETURN_CONTINUE;
	}

	if (!strcmp(jso_key, "service_path")) {
		if (strcmp(g_smbGameInfo.sharePath, json_object_get_string(jso))) {
			return JSON_C_VISIT_RETURN_POP;
		}
		const char* jsonFilename = json_object_get_string(json_object_object_get(parent_jso, "filename"));
		if (!jsonFilename) {
			// This shouldn't happen!
			return JSON_C_VISIT_RETURN_ERROR;
		}

		// Get rid of the "CD/" or "DVD/" prefix as OPL opens the folder too and samba reads it as part of the filename.
		const char* properName = strchr(jsonFilename, '/');
		if (!properName) {
			// Something has the OPL folder open but it's not the PS2.
			// Check if the PS2 opened a file in the other entries.
			return JSON_C_VISIT_RETURN_POP;
		}

		size_t nameLength = strlen(++properName);
		if (strstr(properName, ".iso") || strstr(properName, ".ISO") || strstr(properName, ".zso") || strstr(properName, ".ZSO")) {
			// Find the .iso/.zso suffix if there's any to get rid of it.
			nameLength -= 4;
		}

		size_t newBufferSize = nameLength + 1;
		if (g_smbGameInfo.gameName.string) {
			if (g_smbGameInfo.gameName.bufferSize < newBufferSize) {
				// We'll add old size to new size to potentially avoid a realloc on next game.
				newBufferSize += g_smbGameInfo.gameName.bufferSize;
				g_smbGameInfo.gameName.string = realloc(g_smbGameInfo.gameName.string, newBufferSize);
				g_smbGameInfo.gameName.bufferSize = newBufferSize;
			}
		}
		else {
			g_smbGameInfo.gameName.string = malloc(newBufferSize);
			g_smbGameInfo.gameName.bufferSize = newBufferSize;
		}
		strncpy(g_smbGameInfo.gameName.string, properName, nameLength);
		g_smbGameInfo.gameName.string[nameLength] = '\0';

		processedName = true;
	}
	else if (!strcmp(jso_key, "opened_at")) {
		// There can be multiple timestamps during game launch, so we'll just grab the first one we find.
		// They're all seconds from each other anyway so there's no need for precision.
		if (g_smbGameInfo.timestamp == 0) {
			const char* timeStr = json_object_get_string(jso);
			if (!timeStr) {
				// This shouldn't happen!
				return JSON_C_VISIT_RETURN_ERROR;
			}

			// Grab date and time.
			struct tm tm = { 0 };
			char* leftover = strptime(timeStr, "%Y-%m-%dT%T", &tm);

			// UNIX strptime does not support milliseconds so we'll skip over them.
			if (*leftover == '.') {
				if (!strcmp(jso_key, "opened_at")) {
					// There can be multiple timestamps during game launch, so we'll just grab the first one we find.
					// They're all seconds from each other anyway so there's no need for precision.
					if (g_smbGameInfo.timestamp == 0) {
						const char* timeStr = json_object_get_string(jso);
						if (!timeStr) {
							// This shouldn't happen!
							return JSON_C_VISIT_RETURN_ERROR;
						}

						// Grab date and time.
						struct tm tm = { 0 };
						char* leftover = strptime(timeStr, "%Y-%m-%dT%T", &tm);

						// UNIX strptime does not support milliseconds so we'll skip over them.
						if (*leftover == '.') {
							while (*leftover != '-' && *leftover != '+')
								++leftover;
						}

						// Timezone.
						strptime(leftover, "%z", &tm);

						// Use localtime to figure out if daylight savings should apply or not.
						time_t currentTime = time(NULL);
						tm.tm_isdst = localtime(&currentTime)->tm_isdst;

						g_smbGameInfo.timestamp = mktime(&tm);
					}
					return JSON_C_VISIT_RETURN_POP;
				}
				while (*leftover != '-' && *leftover != '+')
					++leftover;
			}

			// Timezone.
			strptime(leftover, "%z", &tm);

			// Use localtime to figure out if daylight savings should apply or not.
			time_t currentTime = time(NULL);
			tm.tm_isdst = localtime(&currentTime)->tm_isdst;

			g_smbGameInfo.timestamp = mktime(&tm);
		}
		return JSON_C_VISIT_RETURN_POP;
	}
	return JSON_C_VISIT_RETURN_CONTINUE;
}

void SmbReader_Cleanup(void)
{
	if (g_smbGameInfo.gameName.string) {
		free(g_smbGameInfo.gameName.string);
	}
	g_smbGameInfo.gameName.string = NULL;
	g_smbGameInfo.gameName.bufferSize = 0;
	g_smbGameInfo.timestamp = 0;
	g_smbGameInfo.isValid = false;
}

void SmbReader_Init(const char* sharePath)
{
	SmbReader_Cleanup();
	g_smbGameInfo.sharePath = sharePath;
}

void SmbReader_ReadStatus(void)
{
	if (!g_smbGameInfo.sharePath) {
		return;
	}
	g_smbGameInfo.timestamp = 0;
	g_smbGameInfo.isValid = false;

	FILE* cmdFd = popen("smbstatus -L -j", "r");
	json_object* rootObj = json_object_from_fd(fileno(cmdFd));
	json_c_visit(json_object_object_get(rootObj, "open_files"), 0, ProcessSmbKeys, NULL);

	json_object_put(rootObj);
	pclose(cmdFd);
}

const string_t* SmbReader_GetGameName(void)
{
	return &g_smbGameInfo.gameName;
}

time_t SmbReader_GetGameTimestamp(void)
{
	return g_smbGameInfo.timestamp;
}

bool SmbReader_IsInfoValid(void)
{
	return g_smbGameInfo.isValid;
}
