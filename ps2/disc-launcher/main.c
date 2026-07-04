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

#include "ps2rp_protocol.h"

#include <delaythread.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <kernel.h>
#include <libcdvd.h>
#include <loadfile.h>
#include <netman.h>
#include <ps2ips.h>
#include <sbv_patches.h>
#include <sifrpc.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SYSTEM_CNF_PATH "cdrom0:\\SYSTEM.CNF;1"
#define SYSTEM_CNF_MAX_LENGTH 1024
#define BOOT_PATH_MAX_LENGTH 256
#define WAIT_TICK_US 100000
#define DISC_DETECT_WAIT_TICKS 150
#define NET_WAIT_TICKS 150

extern unsigned char ps2dev9_irx[];
extern unsigned int size_ps2dev9_irx;
extern unsigned char netman_irx[];
extern unsigned int size_netman_irx;
extern unsigned char smap_irx[];
extern unsigned int size_smap_irx;
extern unsigned char ps2ip_irx[];
extern unsigned int size_ps2ip_irx;
extern unsigned char ps2ips_irx[];
extern unsigned int size_ps2ips_irx;

static int LoadModuleBuffer(void* buffer, unsigned int size)
{
	int result = 0;
	int moduleId = SifExecModuleBuffer(buffer, size, 0, NULL, &result);
	return moduleId < 0 || result != 0 ? -1 : 0;
}

static const char* SkipLineSpaces(const char* cursor, const char* end)
{
	while (cursor < end && (*cursor == ' ' || *cursor == '\t')) {
		cursor++;
	}
	return cursor;
}

int main(void)
{
	SifInitRpc(0);
	while (!SifIopReset("", 0)) {
	}
	while (!SifIopSync()) {
	}
	SifInitRpc(0);
	SifLoadFileInit();
	SifInitIopHeap();
	sbv_patch_enable_lmb();

	char bootPath[BOOT_PATH_MAX_LENGTH];
	int launchReady = 0;
	if (sceCdInit(SCECdINoD) && sceCdStatus() != SCECdStatShellOpen) {
		int discReady = 0;
		for (int i = 0; i < DISC_DETECT_WAIT_TICKS; i++) {
			if (sceCdStatus() == SCECdStatShellOpen) {
				break;
			}

			int error = sceCdGetError();
			if (error == SCECdErOPENS || error == SCECdErNODISC) {
				break;
			}

			if (sceCdDiskReady(0) == SCECdComplete) {
				discReady = 1;
				break;
			}
			DelayThread(WAIT_TICK_US);
		}

		if (discReady) {
			int diskType = sceCdGetDiskType();
			for (int i = 0; (diskType == SCECdDETCT || diskType == SCECdDETCTCD || diskType == SCECdDETCTDVDS || diskType == SCECdDETCTDVDD) && i < DISC_DETECT_WAIT_TICKS; i++) {
				if (sceCdStatus() == SCECdStatShellOpen || sceCdGetError() == SCECdErOPENS) {
					diskType = SCECdNODISC;
					break;
				}
				DelayThread(WAIT_TICK_US);
				diskType = sceCdGetDiskType();
			}

			if (diskType == SCECdPS2DVD || diskType == SCECdPS2CD || diskType == SCECdPS2CDDA) {
				char systemCnf[SYSTEM_CNF_MAX_LENGTH];
				FILE* file = fopen(SYSTEM_CNF_PATH, "r");
				if (file) {
					size_t length = fread(systemCnf, 1, sizeof(systemCnf) - 1, file);
					int readError = ferror(file);
					fclose(file);
					systemCnf[length] = '\0';

					const char* start = systemCnf;
					const char* end = systemCnf + length;
					while (!readError && start && start < end) {
						const char* cursor = SkipLineSpaces(start, end);
						if ((end - cursor) > 5 && !memcmp(cursor, "BOOT2", 5) && (cursor[5] == '=' || cursor[5] == ' ' || cursor[5] == '\t')) {
							cursor = SkipLineSpaces(cursor + 5, end);
							if (cursor < end && *cursor == '=') {
								cursor = SkipLineSpaces(cursor + 1, end);

								const char* valueStart = cursor;
								while (cursor < end && !isspace((unsigned char)*cursor)) {
									cursor++;
								}

								size_t valueLength = (size_t)(cursor - valueStart);
								if (valueLength > 0 && valueLength < sizeof(bootPath)) {
									memcpy(bootPath, valueStart, valueLength);
									bootPath[valueLength] = '\0';
									launchReady = 1;
								}
							}
							break;
						}

						start = memchr(start, '\n', (size_t)(end - start));
						if (start) {
							start++;
						}
					}
				}
			}
		}
	}

	if (launchReady) {
		t_ip_info ipInfo;
		int networkReady = 0;
		if (LoadModuleBuffer(ps2dev9_irx, size_ps2dev9_irx) == 0 && LoadModuleBuffer(netman_irx, size_netman_irx) == 0 && NetManInit() >= 0 && LoadModuleBuffer(smap_irx, size_smap_irx) == 0) {
			NetManSetLinkMode(NETMAN_NETIF_ETH_LINK_MODE_AUTO);
			if (LoadModuleBuffer(ps2ip_irx, size_ps2ip_irx) == 0 && LoadModuleBuffer(ps2ips_irx, size_ps2ips_irx) == 0 && ps2ip_init() >= 0) {
				memset(&ipInfo, 0, sizeof(ipInfo));
				memcpy(ipInfo.netif_name, "sm0", 4);
				ipInfo.dhcp_enabled = 1;
				ps2ip_setconfig(&ipInfo);

				for (int i = 0; i < NET_WAIT_TICKS; i++) {
					if (NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP && ps2ip_getconfig("sm0", &ipInfo) > 0 && ipInfo.dhcp_enabled && ipInfo.dhcp_status == DHCP_STATE_BOUND) {
						networkReady = 1;
						break;
					}
					DelayThread(WAIT_TICK_US);
				}
			}
		}

		if (networkReady) {
			Ps2rpHeader packet;
			memset(&packet, 0, sizeof(packet));
			memcpy(packet.magic, PS2RP_MAGIC, sizeof(packet.magic));
			packet.version = PS2RP_PROTOCOL_VERSION;
			packet.event = PS2RP_EVENT_LAUNCH;
			packet.source = PS2RP_SOURCE_DISC_LAUNCHER;

			const char* title = strchr(bootPath, ':');
			title = title ? title + 1 : bootPath;
			while (*title == '\\' || *title == '/') {
				title++;
			}

			const char* titleEnd = strchr(title, ';');
			for (const char* cursor = title; *cursor && cursor != titleEnd; cursor++) {
				if (*cursor == '\\' || *cursor == '/') {
					title = cursor + 1;
				}
			}

			size_t titleLength = titleEnd ? (size_t)(titleEnd - title) : strlen(title);
			if (titleLength > PS2RP_TITLE_ID_SIZE) {
				titleLength = PS2RP_TITLE_ID_SIZE;
			}
			memcpy(packet.titleId, title, titleLength);

			struct sockaddr_in address;
			memset(&address, 0, sizeof(address));
			address.sin_family = AF_INET;
			address.sin_port = htons(PS2RP_DEFAULT_PORT);
			address.sin_addr.s_addr = ipInfo.ipaddr.s_addr | ~ipInfo.netmask.s_addr;

			int sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock >= 0) {
				const int broadcast = 1;
				setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
				sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr*)&address, sizeof(address));
				close(sock);
			}
		}
		SifExitRpc();
		LoadExecPS2(bootPath, 0, NULL);
	}

	SifExitRpc();
	ExecOSD(0, NULL);
}
