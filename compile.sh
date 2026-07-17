#!/usr/bin/env bash
# Copyright (C) 2024 brkzlr <brksys@icloud.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
OS_NAME=$(uname -s)
SKIP_PS2=0
CLEAN=0

for arg in "$@"; do
	case "$arg" in
	--clean)
		CLEAN=1
		;;
	--skip-ps2)
		SKIP_PS2=1
		;;
	-h|--help)
		cat <<EOF
Usage: ./compile.sh [--clean] [--skip-ps2]

Builds PS2RichPresence on Linux and macOS.

Options:
  --clean     Remove generated build outputs before building.
  --skip-ps2  Build only the PC listener and runtime data.
EOF
		exit 0
		;;
	*)
		printf 'Unknown option: %s\n' "$arg" >&2
		exit 1
		;;
	esac
done

case "$OS_NAME" in
Linux|Darwin)
	;;
*)
	printf 'Unsupported OS: %s. Linux and macOS are supported.\n' "$OS_NAME" >&2
	exit 1
	;;
esac

cd "$ROOT_DIR"

if [[ ! -d .git && -z "${CI_RUNNING:-}" ]]; then
	printf 'Could not find ".git". Clone this repo with git instead of downloading a ZIP.\n' >&2
	exit 1
fi

for command_name in cmake git make; do
	if ! command -v "$command_name" >/dev/null 2>&1; then
		printf 'Missing required command: %s\n' "$command_name" >&2
		exit 1
	fi
done

if [[ "$CLEAN" -eq 1 ]]; then
	printf '\n==> Cleaning generated outputs\n'
	rm -rf \
		"$ROOT_DIR/bin" \
		"$ROOT_DIR/build" \
		"$ROOT_DIR/include" \
		"$ROOT_DIR/lib" \
		"$ROOT_DIR/extern/discord-rpc/build" \
		"$ROOT_DIR/ps2/disc-launcher/build" \
		"$ROOT_DIR/ps2/disc-launcher/ps2rp.elf"
fi

case "$OS_NAME" in
Darwin)
	DISCORD_LIBRARY="$ROOT_DIR/lib/libdiscord-rpc.dylib"
	;;
*)
	DISCORD_LIBRARY="$ROOT_DIR/lib/libdiscord-rpc.so"
	;;
esac
DISCORD_HEADER="$ROOT_DIR/include/discord_rpc.h"

if [[ ! -f "$DISCORD_HEADER" || ! -f "$DISCORD_LIBRARY" ]]; then
	printf '\n==> Building discord-rpc\n'
	git submodule update --init --recursive extern/discord-rpc
	cmake \
		-S "$ROOT_DIR/extern/discord-rpc" \
		-B "$ROOT_DIR/extern/discord-rpc/build" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$ROOT_DIR" \
		-DBUILD_SHARED_LIBS=ON \
		-DBUILD_EXAMPLES=OFF \
		-DCLANG_FORMAT_CMD=
	cmake --build "$ROOT_DIR/extern/discord-rpc/build" --config Release --target install

	if [[ ! -f "$DISCORD_HEADER" || ! -f "$DISCORD_LIBRARY" ]]; then
		printf 'discord-rpc did not install the expected header/library.\n' >&2
		exit 1
	fi
else
	printf '\n==> Using existing discord-rpc build\n'
fi

printf '\n==> Building PC listener\n'
cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/build" --config Release

if [[ "$SKIP_PS2" -eq 1 ]]; then
	printf '\n==> Skipping PS2 disc launcher\n'
elif [[ -n "${PS2SDK:-}" ]] &&
	command -v mips64r5900el-ps2-elf-gcc >/dev/null 2>&1 &&
	command -v mips64r5900el-ps2-elf-strip >/dev/null 2>&1 &&
	[[ -x "$PS2SDK/bin/bin2c" ]]; then
	printf '\n==> Building PS2 disc launcher with local ps2dev\n'
	make -C "$ROOT_DIR/ps2/disc-launcher" clean all
else
	docker_command=()
	if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
		docker_command=(docker)
	elif command -v sudo >/dev/null 2>&1 && sudo -n docker info >/dev/null 2>&1; then
		docker_command=(sudo docker)
	elif [[ -t 0 ]] && command -v sudo >/dev/null 2>&1; then
		printf 'Docker needs elevated access. You may be prompted for your password.\n'
		sudo docker info >/dev/null
		docker_command=(sudo docker)
	else
		printf 'Could not build PS2 disc launcher. Install Docker, start Docker or install ps2dev and export PS2SDK. Use --skip-ps2 to build only the PC listener.\n' >&2
		exit 1
	fi

	printf '\n==> Building PS2 disc launcher with Docker\n'
	"${docker_command[@]}" run --rm \
		-e HOST_UID="$(id -u)" \
		-e HOST_GID="$(id -g)" \
		-v "$ROOT_DIR":/src \
		-w /src/ps2/disc-launcher \
		ps2dev/ps2dev:latest \
		sh -lc 'apk add --no-cache make >/dev/null &&
			export PATH=/usr/local/ps2dev/bin:/usr/local/ps2dev/ee/bin:/usr/local/ps2dev/iop/bin:$PATH &&
			make clean all &&
			{ chown -R "$HOST_UID:$HOST_GID" build ps2rp.elf ../../bin/ps2rp.elf 2>/dev/null || true; }'
fi

printf '\n==> Build complete\n'
printf 'PC listener: %s\n' "$ROOT_DIR/bin/ps2rpc"
printf 'Title DB:     %s\n' "$ROOT_DIR/bin/ps2_titles.tsv"
if [[ -f "$ROOT_DIR/bin/ps2rp.elf" ]]; then
	printf 'PS2 ELF:      %s\n' "$ROOT_DIR/bin/ps2rp.elf"
fi
