#!/bin/bash
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

SCRIPTDIR=$(cd -- "$(dirname -- "$0")" && pwd -P)
cd $SCRIPTDIR
if [ ! -d ".git" ] && [ -z "$CI_RUNNING" ]; then
	echo "Error! Could not find \".git\" folder!"
	echo "This can happen if you downloaded the ZIP file instead of cloning through git."
	exit 1
fi

# Compile and install discord-rpc
if [ ! -d "$SCRIPTDIR/lib" ]; then
	git submodule update --init
	cd extern/discord-rpc/
	cmake -B build -DCMAKE_INSTALL_PREFIX=$SCRIPTDIR -DBUILD_SHARED_LIBS=ON || exit 1
	cmake --build build --config Release --target install || exit 1
	cd $SCRIPTDIR/include
	mkdir -p Discord
	mv *.h Discord/
fi

# Compile PS2RichPresence
cd $SCRIPTDIR
cmake -DCMAKE_BUILD_TYPE=Release -B build || exit 1
cmake --build build || exit 1
