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
#include "RichPresence.h"

#include "Discord/discord_rpc.h"
#include "SambaReader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct DiscordRichPresence s_richPresence;
static bool s_isPresenceCleared = true;

static void ClearPresence(void)
{
	if (!s_isPresenceCleared) {
		s_richPresence.startTimestamp = 0;
		Discord_ClearPresence();
		s_isPresenceCleared = true;
	}
}

void RichPresence_Cleanup(void)
{
	ClearPresence();
	if (s_richPresence.details) {
		free((char*)s_richPresence.details);
		s_richPresence.details = NULL;
	}
}

void RichPresence_Update(void)
{
	if (SmbReader_IsInfoValid()) {
		const string_t* gameName = SmbReader_GetGameName(); // If info is valid, gameName->string is not NULL.
		if (s_richPresence.details && !strcmp(s_richPresence.details, gameName->string)) {
			// We might've cleared and restarted the same game.
			if (s_isPresenceCleared)
				goto setpresence;
			return;
		}

		if (s_richPresence.details) {
			size_t oldBufferSize = strlen(s_richPresence.details) + 1;
			if (oldBufferSize < gameName->bufferSize) {
				// We'll add old size to new size to potentially avoid a realloc on next game.
				s_richPresence.details = realloc((char*)s_richPresence.details, oldBufferSize + gameName->bufferSize);
			}
		}
		else {
			s_richPresence.details = malloc(gameName->bufferSize);
		}
		strcpy((char*)s_richPresence.details, gameName->string);

	setpresence:
		printf("Detected new game: %s. Setting presence now...\n", s_richPresence.details);
		s_richPresence.startTimestamp = SmbReader_GetGameTimestamp();
		Discord_UpdatePresence(&s_richPresence);
		s_isPresenceCleared = false;
	}
	else if (!s_isPresenceCleared) {
		puts("No game detected! Clearing presence now...");
		ClearPresence();
	}
}
