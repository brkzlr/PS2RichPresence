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
#include "Discord/discord_rpc.h"

#include "RichPresence.h"
#include "SambaReader.h"

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void Cleanup(void)
{
	RichPresence_Cleanup();
	SmbReader_Cleanup();
}

static bool s_discordActive = false;
void HandleDiscordReady(const DiscordUser* request)
{
	puts("Detected Discord! Starting main loop...");
	s_discordActive = true;
}

void HandleDiscordDisconnected(int errorCode, const char* message)
{
	if (s_discordActive) {
		puts("Discord disconnected! Stopping main loop...");
		Cleanup();
		s_discordActive = false;
	}
}

void HandleDiscordErrored(int errorCode, const char* message)
{
	if (s_discordActive) {
		printf("Discord error: %s! Stopping main loop", message);
		Cleanup();
		s_discordActive = false;
	}
}

static bool s_stop = false;
static void sigintHandler(int unused)
{
	puts("Stopping and cleaning up! Please wait...");
	s_stop = true;
}

static void* DiscordThread(void* args)
{
	while (!s_stop) {
		Discord_RunCallbacks();
		sleep(1);
	}

	return NULL;
}

int main(int argc, char** argv)
{
	if (getuid() != 0) {
		puts("Please run this program as root!");
		return -1;
	}

	const char* appID = NULL;
	const char* sharePath = NULL;
	long int refreshPeriod = 10;

	int opt;
	while ((opt = getopt(argc, argv, ":a:s:t:")) != -1) {
		switch (opt) {
		case 'a':
			appID = optarg;
			break;
		case 's':
			sharePath = optarg;
			break;
		case 't':
			refreshPeriod = strtol(optarg, NULL, 10);
			if (refreshPeriod <= 0) {
				puts("Refresh period must be higher than 0!");
				return -1;
			}
			break;
		case ':':
			printf("Missing argument for option -%c!\n", optopt);
			return -1;
		default:
			printf("Unknown option -%c\n", optopt);
			return -1;
		}
	}

	if (!appID || !sharePath) {
		printf("Usage: %s -a (Discord app id) -s \"(PS2 samba share path)\" [-t Refresh period in seconds]\n", argv[0]);
		return -1;
	}

	struct sigaction sighandler = { 0 };
	sighandler.sa_handler = sigintHandler;
	sigaction(SIGINT, &sighandler, NULL);

	DiscordEventHandlers handlers;
	handlers.ready = HandleDiscordReady;
	handlers.disconnected = HandleDiscordDisconnected;
	handlers.errored = HandleDiscordErrored;

	Discord_Initialize(appID, &handlers, 1, NULL);
	SmbReader_Init(sharePath);

	pthread_t discordThreadID;
	if (pthread_create(&discordThreadID, NULL, DiscordThread, NULL)) {
		puts("Error creating Discord thread, aborting!");
		return -1;
	}

	while (!s_stop) {
		if (s_discordActive) {
			SmbReader_ReadStatus();
			RichPresence_Update();
		}
		sleep(refreshPeriod);
	}

	pthread_join(discordThreadID, NULL);
	Cleanup();
	Discord_Shutdown();
	return 0;
}
