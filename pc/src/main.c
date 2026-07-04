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

#include "ps2rp_protocol.h"

#include "TitleDatabase.h"

#include <ctype.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TITLE_DB_FILENAME "ps2_titles.tsv"
#define RICHPRESENCE_TEXT_MAX 128
#define RICHPRESENCE_ASSET_KEY_MAX 32

typedef struct {
	uint8_t event;
	uint8_t source;
	char titleId[PS2RP_TITLE_ID_SIZE + 1];
	char name[PS2RP_MAX_NAME_LENGTH + 1];
} AnnouncePacket;

static struct {
	char details[RICHPRESENCE_TEXT_MAX + 1];
	char state[RICHPRESENCE_TEXT_MAX + 1];
	char largeImageKey[RICHPRESENCE_ASSET_KEY_MAX + 1];
	char largeImageText[RICHPRESENCE_TEXT_MAX + 1];
	time_t timestamp;
	bool active;
} s_currentGame;

static struct DiscordRichPresence s_richPresence;
static TitleDatabase s_titleDatabase;
static const char* s_largeImageFallbackKey = NULL;
static bool s_useTitleImageKeys = false;
static bool s_presenceCleared = true;

static void PublishCurrentGame(void);

static bool s_discordActive = false;
static void HandleDiscordReady(const DiscordUser* request)
{
	puts("Detected Discord! Listening for announcements...");
	s_discordActive = true;
	if (s_currentGame.active) {
		PublishCurrentGame();
	}
}

static void HandleDiscordDisconnected(int errorCode, const char* message)
{
	if (s_discordActive) {
		puts("Discord disconnected! Waiting for it to come back...");
		s_discordActive = false;
	}
}

static void HandleDiscordErrored(int errorCode, const char* message)
{
	if (s_discordActive) {
		printf("Discord error: %s! Waiting for it to come back...\n", message);
		s_discordActive = false;
	}
}

static volatile sig_atomic_t s_stop = false;
static void sigintHandler(int unused)
{
	s_stop = true;
}

static struct termios s_originalTermios;
static bool s_termiosChanged = false;
static void SetupTerminal(void)
{
	if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &s_originalTermios)) {
		return;
	}

	// Disable line buffering and echo so keybinds react to a single keypress.
	struct termios rawTermios = s_originalTermios;
	rawTermios.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
	if (!tcsetattr(STDIN_FILENO, TCSANOW, &rawTermios)) {
		s_termiosChanged = true;
	}
}

static void RestoreTerminal(void)
{
	if (s_termiosChanged) {
		tcsetattr(STDIN_FILENO, TCSANOW, &s_originalTermios);
		s_termiosChanged = false;
	}
}

static void ClearCurrentGame(void)
{
	s_currentGame.active = false;
	if (!s_presenceCleared) {
		s_richPresence.startTimestamp = 0;
		Discord_ClearPresence();
		s_presenceCleared = true;
	}
}

static void PublishCurrentGame(void)
{
	s_richPresence.details = s_currentGame.details;
	s_richPresence.state = s_currentGame.state[0] ? s_currentGame.state : NULL;
	s_richPresence.largeImageKey = s_currentGame.largeImageKey[0] ? s_currentGame.largeImageKey : NULL;
	s_richPresence.largeImageText = s_currentGame.largeImageText[0] ? s_currentGame.largeImageText : NULL;
	s_richPresence.startTimestamp = s_currentGame.timestamp;
	Discord_UpdatePresence(&s_richPresence);
	s_presenceCleared = false;
}

static const char* SourceName(unsigned int source)
{
	return source == PS2RP_SOURCE_OPL ? "Open PS2 Loader" : source == PS2RP_SOURCE_DISC_LAUNCHER ? "Disc"
	                                                                                             : "Unknown Source";
}

static void HandleAnnounce(const AnnouncePacket* packet)
{
	switch (packet->event) {
	case PS2RP_EVENT_LAUNCH: {
		const char* databaseName = TitleDatabase_Find(&s_titleDatabase, packet->titleId);
		const char* displayName = packet->name[0] ? packet->name : databaseName;
		if (!displayName || !displayName[0]) {
			displayName = packet->titleId;
		}

		snprintf(s_currentGame.details, sizeof(s_currentGame.details), "%s", displayName);
		if (packet->titleId[0]) {
			snprintf(s_currentGame.state, sizeof(s_currentGame.state), "%s - %s", SourceName(packet->source), packet->titleId);
		}
		else {
			snprintf(s_currentGame.state, sizeof(s_currentGame.state), "%s", SourceName(packet->source));
		}

		s_currentGame.largeImageKey[0] = '\0';
		if (s_useTitleImageKeys && TitleDatabase_CompactKey(packet->titleId, s_currentGame.largeImageKey, sizeof(s_currentGame.largeImageKey))) {
			for (char* cursor = s_currentGame.largeImageKey; *cursor; cursor++) {
				*cursor = (char)tolower((unsigned char)*cursor);
			}
		}
		if (!s_currentGame.largeImageKey[0] && s_largeImageFallbackKey) {
			snprintf(s_currentGame.largeImageKey, sizeof(s_currentGame.largeImageKey), "%s", s_largeImageFallbackKey);
		}
		if (s_currentGame.largeImageKey[0]) {
			snprintf(s_currentGame.largeImageText, sizeof(s_currentGame.largeImageText), "%s", databaseName ? databaseName : displayName);
		}
		else {
			s_currentGame.largeImageText[0] = '\0';
		}
		s_currentGame.timestamp = time(NULL);
		s_currentGame.active = true;

		printf("Game launch announced from %s: %s", SourceName(packet->source), s_currentGame.details);
		if (packet->titleId[0]) {
			printf(" (%s)", packet->titleId);
		}
		puts(". Setting presence now...");
		if (s_discordActive) {
			PublishCurrentGame();
		}
		break;
	}
	case PS2RP_EVENT_CLEAR:
		puts("Clear announced! Clearing presence now...");
		ClearCurrentGame();
		break;
	default:
		// Unknown or reserved events (like HEARTBEAT) are ignored on purpose.
		break;
	}
}

static bool ReadAnnounce(int socketFd, AnnouncePacket* outPacket)
{
	uint8_t buffer[sizeof(Ps2rpHeader) + PS2RP_MAX_NAME_LENGTH];
	ssize_t received = recvfrom(socketFd, buffer, sizeof(buffer), 0, NULL, NULL);
	if (received < (ssize_t)sizeof(Ps2rpHeader)) {
		return false;
	}

	Ps2rpHeader header;
	memcpy(&header, buffer, sizeof(header));
	if (memcmp(header.magic, PS2RP_MAGIC, sizeof(header.magic))) {
		return false;
	}
	if (header.version != PS2RP_PROTOCOL_VERSION) {
		printf("Ignoring announce with unknown protocol version %u!\n", header.version);
		return false;
	}
	if ((size_t)received - sizeof(Ps2rpHeader) < header.nameLength) {
		return false;
	}

	outPacket->event = header.event;
	outPacket->source = header.source;
	memcpy(outPacket->titleId, header.titleId, PS2RP_TITLE_ID_SIZE);
	outPacket->titleId[PS2RP_TITLE_ID_SIZE] = '\0';
	memcpy(outPacket->name, buffer + sizeof(Ps2rpHeader), header.nameLength);
	outPacket->name[header.nameLength] = '\0';
	return header.event != PS2RP_EVENT_LAUNCH || outPacket->titleId[0] || outPacket->name[0];
}

static void HandleKeypress(void)
{
	char key;
	if (read(STDIN_FILENO, &key, 1) != 1) {
		return;
	}

	switch (key) {
	case 'c':
		puts("Clearing presence!");
		ClearCurrentGame();
		break;
	case 'q':
		s_stop = true;
		break;
	}
}

int main(int argc, char** argv)
{
	const char* appID = NULL;
	const char* titleDbPath = NULL;
	const char* titleOverridePath = NULL;
	bool titleDbPathExplicit = false;
	long int port = PS2RP_DEFAULT_PORT;

	int opt;
	while ((opt = getopt(argc, argv, ":a:p:d:u:A:C")) != -1) {
		switch (opt) {
		case 'a':
			appID = optarg;
			break;
		case 'p':
			port = strtol(optarg, NULL, 10);
			if (port <= 0 || port > 65535) {
				puts("Port must be between 1 and 65535!");
				return -1;
			}
			break;
		case 'd':
			titleDbPath = optarg;
			titleDbPathExplicit = true;
			break;
		case 'u':
			titleOverridePath = optarg;
			break;
		case 'A':
			if (strlen(optarg) > RICHPRESENCE_ASSET_KEY_MAX) {
				printf("Discord image asset keys must be %d bytes or less!\n", RICHPRESENCE_ASSET_KEY_MAX);
				return -1;
			}
			s_largeImageFallbackKey = optarg;
			break;
		case 'C':
			s_useTitleImageKeys = true;
			break;
		case ':':
			printf("Missing argument for option -%c!\n", optopt);
			return -1;
		default:
			printf("Unknown option -%c\n", optopt);
			return -1;
		}
	}

	if (!appID) {
		printf("Usage: %s -a (Discord app id) [-p Announce port, default %d] [-d title_db.tsv] [-u title_overrides.tsv] [-A image_asset_key] [-C]\n", argv[0], PS2RP_DEFAULT_PORT);
		return -1;
	}

	char defaultTitleDbPath[4096];
	if (!titleDbPath) {
		const char* slash = strrchr(argv[0], '/');
		if (slash) {
			snprintf(defaultTitleDbPath, sizeof(defaultTitleDbPath), "%.*s/%s", (int)(slash - argv[0]), argv[0], TITLE_DB_FILENAME);
		}
		else {
			snprintf(defaultTitleDbPath, sizeof(defaultTitleDbPath), "%s", TITLE_DB_FILENAME);
		}
		titleDbPath = defaultTitleDbPath;
	}

	if (!TitleDatabase_LoadFile(&s_titleDatabase, titleDbPath)) {
		if (titleDbPathExplicit) {
			printf("Failed to load title database: %s\n", titleDbPath);
			TitleDatabase_Destroy(&s_titleDatabase);
			return -1;
		}
		printf("Title database not found at %s; bare title IDs won't be resolved.\n", titleDbPath);
	}
	if (titleOverridePath && !TitleDatabase_LoadFile(&s_titleDatabase, titleOverridePath)) {
		printf("Failed to load title override database: %s\n", titleOverridePath);
		TitleDatabase_Destroy(&s_titleDatabase);
		return -1;
	}
	if (!TitleDatabase_Finalize(&s_titleDatabase)) {
		puts("Failed to prepare the title database.");
		TitleDatabase_Destroy(&s_titleDatabase);
		return -1;
	}
	if (s_titleDatabase.count > 0) {
		printf("Loaded %zu title database entries.\n", s_titleDatabase.count);
	}

	struct sigaction sighandler = { 0 };
	sighandler.sa_handler = sigintHandler;
	sigaction(SIGINT, &sighandler, NULL);
	sigaction(SIGTERM, &sighandler, NULL);

	int listenerFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (listenerFd < 0) {
		perror("Failed to create the announce socket");
		TitleDatabase_Destroy(&s_titleDatabase);
		return -1;
	}

	int reuseAddr = 1;
	setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons((uint16_t)port);
	if (bind(listenerFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("Failed to bind the announce socket");
		close(listenerFd);
		TitleDatabase_Destroy(&s_titleDatabase);
		return -1;
	}

	DiscordEventHandlers handlers = { 0 };
	handlers.ready = HandleDiscordReady;
	handlers.disconnected = HandleDiscordDisconnected;
	handlers.errored = HandleDiscordErrored;
	Discord_Initialize(appID, &handlers, 1, NULL);

	SetupTerminal();
	printf("Listening for PS2 announcements on port %ld.\n", port);
	if (s_termiosChanged) {
		puts("Press 'c' to clear the presence or 'q' to quit.");
	}

	struct pollfd pollFds[2] = { 0 };
	pollFds[0].fd = listenerFd;
	pollFds[0].events = POLLIN;
	pollFds[1].fd = STDIN_FILENO;
	pollFds[1].events = POLLIN;
	const nfds_t pollCount = s_termiosChanged ? 2 : 1;

	while (!s_stop) {
		int readyCount = poll(pollFds, pollCount, 1000);
		Discord_RunCallbacks();
		if (readyCount <= 0) {
			continue;
		}

		if (pollFds[0].revents & POLLIN) {
			AnnouncePacket packet;
			if (ReadAnnounce(listenerFd, &packet)) {
				HandleAnnounce(&packet);
			}
		}
		if (pollCount > 1 && (pollFds[1].revents & POLLIN)) {
			HandleKeypress();
		}
	}

	puts("Stopping and cleaning up! Please wait...");
	RestoreTerminal();
	ClearCurrentGame();
	close(listenerFd);
	TitleDatabase_Destroy(&s_titleDatabase);
	Discord_Shutdown();
	return 0;
}
