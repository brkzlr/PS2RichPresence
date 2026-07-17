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

#include "discord_rpc.h"

#include "ps2rp_protocol.h"

#include "TitleDatabase.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#include <conio.h>
#include <io.h>
#include <windows.h>

typedef SOCKET SocketHandle;
#define CloseSocket closesocket
#define PrintSocketError(message) fprintf(stderr, "%s (Winsock error %d).\n", message, WSAGetLastError())
#define ShutdownSockets WSACleanup
#else
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

typedef int SocketHandle;
#define CloseSocket close
#define PrintSocketError perror
#define ShutdownSockets() ((void)0)
#endif

#define RICHPRESENCE_TEXT_MAX 128
#define RICHPRESENCE_ASSET_KEY_MAX 32
#define USAGE_TEXT "Usage: %s -a (Discord app id) [-p Announce port, default %d] [-d title_db.tsv] " \
	               "[-u title_overrides.tsv] [-A image_asset_key] [-C]\n"

static struct {
	char details[RICHPRESENCE_TEXT_MAX + 1], state[RICHPRESENCE_TEXT_MAX + 1], largeImageText[RICHPRESENCE_TEXT_MAX + 1];
	char largeImageKey[RICHPRESENCE_ASSET_KEY_MAX + 1];
	time_t timestamp;
} s_currentGame;

static bool s_presenceSet, s_discordActive;

static void ClearCurrentGame(void)
{
	s_currentGame.details[0] = '\0';
	if (s_presenceSet) {
		Discord_ClearPresence();
		s_presenceSet = false;
	}
}

static void PublishCurrentGame(void)
{
	Discord_UpdatePresence(&(DiscordRichPresence) {
		.state = s_currentGame.state,
		.details = s_currentGame.details,
		.startTimestamp = s_currentGame.timestamp,
		.largeImageKey = s_currentGame.largeImageKey,
		.largeImageText = s_currentGame.largeImageText,
	});
	s_presenceSet = true;
}

static void HandleDiscordReady(const DiscordUser* request)
{
	(void)request;
	puts("Detected Discord! Listening for announcements...");
	s_discordActive = true;
	if (s_currentGame.details[0]) {
		PublishCurrentGame();
	}
}

static void HandleDiscordDisconnected(int errorCode, const char* message)
{
	(void)errorCode, (void)message;
	if (s_discordActive) {
		puts("Discord disconnected! Waiting for it to come back...");
		s_discordActive = false;
	}
}

static void HandleDiscordErrored(int errorCode, const char* message)
{
	(void)errorCode;
	if (s_discordActive) {
		printf("Discord error: %s! Waiting for it to come back...\n", message);
		s_discordActive = false;
	}
}

#ifdef _WIN32
static volatile LONG s_stop;
static BOOL WINAPI SignalHandler(DWORD signal)
{
	if (signal > CTRL_BREAK_EVENT) {
		return FALSE;
	}
	InterlockedExchange(&s_stop, 1);
	return TRUE;
}
#else
static volatile sig_atomic_t s_stop;
static void SignalHandler(int unused)
{
	(void)unused;
	s_stop = true;
}
#endif

int main(int argc, char** argv)
{
	const char* appID = NULL, *titleDbPath = NULL, *titleOverridePath = NULL, *largeImageFallbackKey = NULL;
	TitleDatabase titleDatabase = { 0 };
	bool useTitleImageKeys = false, keyboardEnabled = false, inputError = false;
	long port = PS2RP_DEFAULT_PORT;

	for (int index = 1; index < argc; index++) {
		const char* argument = argv[index];
		if (!strcmp(argument, "-h") || !strcmp(argument, "--help")) {
			printf(USAGE_TEXT, argv[0], PS2RP_DEFAULT_PORT);
			return EXIT_SUCCESS;
		}
		if (!strcmp(argument, "--")) {
			break;
		}
		if (!strcmp(argument, "-C")) {
			useTitleImageKeys = true;
			continue;
		}
		if (argument[0] != '-' || !argument[1] || !strchr("apduA", argument[1])) {
			fprintf(stderr, "Unknown option %s!\n", argument);
			return EXIT_FAILURE;
		}

		const char* value = argument[2] ? argument + 2 : ++index < argc ? argv[index] : NULL;
		if (!value) {
			fprintf(stderr, "Missing argument for option %s!\n", argument);
			return EXIT_FAILURE;
		}

		switch (argument[1]) {
		case 'a':
			appID = value;
			break;
		case 'p': {
			char* end;
			port = strtol(value, &end, 10);
			if (end == value || *end || port <= 0 || port > 65535) {
				fputs("Port must be a number between 1 and 65535!\n", stderr);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'd':
			titleDbPath = value;
			break;
		case 'u':
			titleOverridePath = value;
			break;
		case 'A':
			if (strlen(value) > RICHPRESENCE_ASSET_KEY_MAX) {
				fprintf(stderr, "Discord image asset keys must be %d bytes or less!\n", RICHPRESENCE_ASSET_KEY_MAX);
				return EXIT_FAILURE;
			}
			largeImageFallbackKey = value;
			break;
		}
	}

	if (!appID) {
		printf(USAGE_TEXT, argv[0], PS2RP_DEFAULT_PORT);
		return EXIT_FAILURE;
	}

	bool titleDbPathExplicit = titleDbPath != NULL;
	char defaultTitleDbPath[4096];
	if (!titleDbPath) {
		const char* executablePath = argv[0];
#ifdef _WIN32
		char modulePath[4096];
		DWORD modulePathLength = GetModuleFileNameA(NULL, modulePath, (DWORD)sizeof(modulePath));
		if (modulePathLength && modulePathLength < (DWORD)sizeof(modulePath)) {
			executablePath = modulePath;
		}
		const char* separator = strrchr(executablePath, '\\');
#else
		const char* separator = strrchr(executablePath, '/');
#endif
		int written = snprintf(defaultTitleDbPath, sizeof(defaultTitleDbPath), "%.*sps2_titles.tsv",
		                       separator ? (int)(separator - executablePath + 1) : 0, executablePath);
		if (written < 0 || (size_t)written >= sizeof(defaultTitleDbPath)) {
			fputs("The executable path is too long to locate the title database.\n", stderr);
			return EXIT_FAILURE;
		}
		titleDbPath = defaultTitleDbPath;
	}

	if (!TitleDatabase_LoadFile(&titleDatabase, titleDbPath)) {
		TitleDatabase_Destroy(&titleDatabase);
		if (titleDbPathExplicit) {
			fprintf(stderr, "Failed to load title database: %s\n", titleDbPath);
			return EXIT_FAILURE;
		}
		printf("Title database could not be loaded from %s. Bare title IDs won't be resolved.\n", titleDbPath);
	}
	if (titleOverridePath && !TitleDatabase_LoadFile(&titleDatabase, titleOverridePath)) {
		fprintf(stderr, "Failed to load title override database: %s\n", titleOverridePath);
		TitleDatabase_Destroy(&titleDatabase);
		return EXIT_FAILURE;
	}
	TitleDatabase_Finalize(&titleDatabase);
	if (titleDatabase.count) {
		printf("Loaded %zu title database entries.\n", titleDatabase.count);
	}

#ifdef _WIN32
	int startupError = WSAStartup(MAKEWORD(2, 2), &(WSADATA) { 0 });
	if (startupError) {
		fprintf(stderr, "Failed to initialize Winsock (error %d).\n", startupError);
		TitleDatabase_Destroy(&titleDatabase);
		return EXIT_FAILURE;
	}
#endif

	SocketHandle listenerFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (listenerFd == (SocketHandle)-1) {
		PrintSocketError("Failed to create the announce socket");
		ShutdownSockets();
		TitleDatabase_Destroy(&titleDatabase);
		return EXIT_FAILURE;
	}

#ifndef _WIN32
	setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int));
#endif

	if (bind(listenerFd, (struct sockaddr*)&(struct sockaddr_in) { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) }, sizeof(struct sockaddr_in))) {
		PrintSocketError("Failed to bind the announce socket");
		CloseSocket(listenerFd);
		ShutdownSockets();
		TitleDatabase_Destroy(&titleDatabase);
		return EXIT_FAILURE;
	}

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(SignalHandler, TRUE)) {
		fputs("Failed to install Ctrl+C handler.\n", stderr);
	}
#else
	struct sigaction sighandler = { .sa_handler = SignalHandler };
	sigaction(SIGINT, &sighandler, NULL);
	sigaction(SIGTERM, &sighandler, NULL);
#endif

	Discord_Initialize(appID, &(DiscordEventHandlers) { .ready = HandleDiscordReady, .disconnected = HandleDiscordDisconnected, .errored = HandleDiscordErrored }, 1, NULL);

#ifdef _WIN32
	UINT originalOutputCodePage = 0;
	if ((keyboardEnabled = _isatty(_fileno(stdin)))) {
		originalOutputCodePage = GetConsoleOutputCP();
		SetConsoleOutputCP(CP_UTF8);
	}
#else
	struct termios originalTermios;
	if (isatty(STDIN_FILENO) && !tcgetattr(STDIN_FILENO, &originalTermios)) {
		struct termios rawTermios = originalTermios;
		rawTermios.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
		keyboardEnabled = !tcsetattr(STDIN_FILENO, TCSANOW, &rawTermios);
	}
#endif

	struct pollfd pollFds[2] = { { listenerFd, POLLIN, 0 }, { 0, POLLIN, 0 } };

	printf("Listening for PS2 announcements on port %ld.\n", port);
	if (keyboardEnabled) {
		puts("Press 'c' to clear the presence or 'q' to quit.");
	}

#ifdef _WIN32
	while (!InterlockedCompareExchange(&s_stop, 0, 0)) {
#else
	while (!s_stop) {
#endif
#ifdef _WIN32
		int readyCount = WSAPoll(pollFds, 1, 250);
#else
		int readyCount = poll(pollFds, keyboardEnabled ? 2 : 1, 250);
		if (readyCount < 0 && errno == EINTR) {
			continue;
		}
#endif
		if (readyCount < 0) {
			PrintSocketError("Failed while waiting for announcements");
			inputError = true;
			break;
		}
		Discord_RunCallbacks();

		if (pollFds[0].revents & POLLIN) {
			struct { Ps2rpHeader header; char name[PS2RP_MAX_NAME_LENGTH + 1]; } packet;
			char titleId[PS2RP_TITLE_ID_SIZE + 1];
			int received = (int)recv(listenerFd, (char*)&packet, (int)sizeof(packet) - 1, 0);
			if (received >= (int)sizeof(packet.header) && !memcmp(packet.header.magic, PS2RP_MAGIC, sizeof(packet.header.magic))) {
				if (packet.header.version != PS2RP_PROTOCOL_VERSION) {
					printf("Ignoring announce with unknown protocol version %u!\n", (unsigned int)packet.header.version);
				}
				else if (received >= (int)(sizeof(packet.header) + packet.header.nameLength)) {
					memcpy(titleId, packet.header.titleId, PS2RP_TITLE_ID_SIZE);
					titleId[PS2RP_TITLE_ID_SIZE] = '\0';
					packet.name[packet.header.nameLength] = '\0';
					if (packet.header.event == PS2RP_EVENT_CLEAR) {
						puts("Clear announced! Clearing presence now...");
						ClearCurrentGame();
					}
					else if (packet.header.event == PS2RP_EVENT_LAUNCH && (titleId[0] || packet.name[0])) {
						const char* databaseName = TitleDatabase_Find(&titleDatabase, titleId);
						const char* displayName = packet.name[0] ? packet.name : databaseName ? databaseName : titleId;
						const char* sourceName = packet.header.source == PS2RP_SOURCE_OPL ? "Open PS2 Loader" : packet.header.source == PS2RP_SOURCE_DISC_LAUNCHER ? "Disc" : "Unknown Source";
						snprintf(s_currentGame.details, sizeof(s_currentGame.details), "%s", displayName);
						snprintf(s_currentGame.state, sizeof(s_currentGame.state), "%s%s%s", sourceName,
						         titleId[0] ? " - " : "", titleId);

						if (!useTitleImageKeys || !TitleDatabase_CompactKey(titleId, s_currentGame.largeImageKey, sizeof(s_currentGame.largeImageKey))) {
							snprintf(s_currentGame.largeImageKey, sizeof(s_currentGame.largeImageKey), "%s", largeImageFallbackKey ? largeImageFallbackKey : "");
						}
						else {
							for (char* cursor = s_currentGame.largeImageKey; *cursor; cursor++) {
								*cursor = (char)tolower((unsigned char)*cursor);
							}
						}
						snprintf(s_currentGame.largeImageText, sizeof(s_currentGame.largeImageText), "%s",
						         s_currentGame.largeImageKey[0] ? (databaseName ? databaseName : displayName) : "");
						s_currentGame.timestamp = time(NULL);

						printf("Game launch announced from %s: %s%s%s%s. Setting presence now...\n", sourceName, s_currentGame.details,
						       titleId[0] ? " (" : "", titleId, titleId[0] ? ")" : "");
						if (s_discordActive) {
							PublishCurrentGame();
						}
					}
				}
			}
		}
#ifdef _WIN32
		if (keyboardEnabled && _kbhit()) {
			int key = _getch();
#else
		if (keyboardEnabled && (pollFds[1].revents & POLLIN)) {
			char key = '\0';
			(void)read(STDIN_FILENO, &key, 1);
#endif
			if (key == 'c') {
				puts("Clearing presence!");
				ClearCurrentGame();
			}
			else if (key == 'q') {
				break;
			}
		}
	}

	puts("Stopping and cleaning up! Please wait...");
#ifdef _WIN32
	if (originalOutputCodePage) {
		SetConsoleOutputCP(originalOutputCodePage);
	}
#else
	if (keyboardEnabled) {
		tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
	}
#endif
	ClearCurrentGame();
	CloseSocket(listenerFd);
	ShutdownSockets();
	TitleDatabase_Destroy(&titleDatabase);
	Discord_Shutdown();
	return inputError ? EXIT_FAILURE : EXIT_SUCCESS;
}
