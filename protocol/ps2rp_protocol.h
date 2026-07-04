/*
    Copyright (C) 2026 brkzlr <brksys@icloud.com>

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

#ifndef PS2RP_PROTOCOL_H
#define PS2RP_PROTOCOL_H

#include <stdint.h>

#define PS2RP_MAGIC "PS2R" // 4 bytes on the wire, no NUL.
#define PS2RP_PROTOCOL_VERSION 1
#define PS2RP_DEFAULT_PORT 50003

#define PS2RP_TITLE_ID_SIZE 12
#define PS2RP_MAX_NAME_LENGTH 255

enum Ps2rpEvent {
	PS2RP_EVENT_LAUNCH = 0, // A game is starting. Requires titleId and/or name.
	PS2RP_EVENT_CLEAR = 1, // Stop displaying the current game.
	PS2RP_EVENT_HEARTBEAT = 2, // Reserved for possible future resident announcers.
};

enum Ps2rpSource {
	PS2RP_SOURCE_UNKNOWN = 0,
	PS2RP_SOURCE_DISC_LAUNCHER = 1,
	PS2RP_SOURCE_OPL = 2,
};

typedef struct {
	char magic[4]; // "PS2R", not NUL terminated.
	uint8_t version; // PS2RP_PROTOCOL_VERSION of the announcer.
	uint8_t event; // One of Ps2rpEvent.
	uint8_t source; // One of Ps2rpSource, informational only.
	uint8_t nameLength; // Byte count of the UTF-8 name that follows the header.
	char titleId[PS2RP_TITLE_ID_SIZE]; // e.g. "SLES_514.34", NUL padded. All zeros if unknown.
} Ps2rpHeader;

#endif // PS2RP_PROTOCOL_H
