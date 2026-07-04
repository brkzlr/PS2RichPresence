# PS2RichPresence
This is a program that will set your Discord rich presence to whatever game your PS2 is playing, using a small packet sent by the PS2 over your local network when a game starts.

## How does it work?
When the PS2 starts a game, the PS2 side sends a UDP broadcast packet with the game's title ID and/or display name. This program listens for that packet and uses the [Discord-RPC](https://github.com/brkzlr/discord-rpc) library to communicate with your locally open Discord client and set rich presence details using your own Discord application ID. (See [Prerequisites](#prerequisites) below)

The packet format is defined in [protocol/ps2rp_protocol.h](protocol/ps2rp_protocol.h). The PS2 side broadcasts it, so there is no PC IP to enter anywhere. Just run the listener on a computer in the same network as the PS2.

## Supported OS
- Linux
  - If you download the release `.zip` instead of compiling the program yourself, you need to have glibc version equal to or higher than `2.35` as the zip is created using Ubuntu 22.04 as base.
- MacOS

## Prerequisites
You will need to create your own Discord application (just like you would do creating a bot if you know how) so you can receive an application ID which is used to display the rich presence.
1) Go to [Discord Developer Portal](https://discord.com/developers/applications).
2) Click on the "New Application" button on top right of the page.
3) Name the application as you wish but know that this name will be part of your rich presence.
   - So for example, if you name it (Your name)'s PS2, then your rich presence will show up as "Playing (Your name)'s PS2" with the game's name under it.
4) Click on the newly created application. (If the page didn't redirect you already)
   - Optionally add an "App Icon" in the "General Information" page, like the PlayStation logo if you wish.
5) Grab the "Application ID" below your description and tags. **You will need this to run the program**.

## Requirements
### Compilation:
- CMake 3.22 or higher.
- C compiler with C11 support, plus a C++ compiler for discord-rpc.
  - Any modern GCC or Clang toolkit will do.
- `make`.
- Docker, unless you have a local ps2dev toolchain installed and `PS2SDK` exported.
- Discord-RPC library.
  - A compilation fixed fork is already included in this repo as a submodule, so you don't need to do anything about this.
- A PC that won't crash and burn when compiling code.
  - Not burning is optional, just make sure it won't crash during compilation and have your fire extinguisher ready :)

## Building
Just run `compile.sh` inside the folder after cloning the repo using git and it will handle everything for you, if you fulfill the [compilation requirements](#compilation) that is. It builds discord-rpc, the PC listener, the title database copy and the PS2 disc launcher. Use `./compile.sh --skip-ps2` if you only want the PC listener.
```
git clone --recursive https://github.com/brkzlr/PS2RichPresence
cd PS2RichPresence
./compile.sh
```
Obviously you must have `git` installed for this. Downloading the ZIP file won't work as you need the discord-rpc submodule from this repo and submodules are not included in ZIP downloads.

You can also run `sudo cmake --install build` in the same folder after running `./compile.sh` if you want to have PS2RichPresence installed to `/opt/PS2RichPresence`.
You can delete the cloned folder afterwards and remove all compilation dependencies. The program will run just fine from there as the installation process makes it portable.

The PS2 disc launcher needs the ps2dev toolchain. `compile.sh` uses a local ps2dev install when available. Otherwise it builds the ELF through Docker. This writes `bin/ps2rp.elf`.

## Usage
Run the command outlined below in any of the following locations:
- Resulting `bin` folder if you locally compiled.
- Folder where you extracted the release `.zip` to.
- `/opt/PS2RichPresence` if you ran the installation command in the previous section.
```
./ps2rpc -a (Discord application ID) [-p Announce port] [-d title_db.tsv] [-u title_overrides.tsv] [-A image_asset_key] [-C]
```
- Discord application ID: This is the App ID that you copied in the **Prerequisites** steps.
- Announce port: Optional parameter to change the UDP port the program listens on. Defaults to `50003`, only change it if you also change it on the PS2 side.
- Title database: Optional path to a TSV title database. By default, the program loads `ps2_titles.tsv` from the same folder as `ps2rpc`.
- Title overrides: Optional TSV file loaded after the default title database. Use one `title_id<TAB>display name` entry per line. Title IDs may be written as `SLUS-20273` or `SLUS_202.73`.
- Image asset key: Optional Discord application asset key to use as the large image, such as `ps2`.
- `-C`: Use per-title Discord image asset keys based on the title ID, such as `slus20273`. If the title has no ID or if you also provide `-A`, the fallback image asset key is used.

While the program is running you can use the following keys:
- `c`: Clear the current rich presence. Useful after turning off the PS2, as the PS2 side does not send shutdown packets.
- `q`: Quit the program. Quitting will also cleanup so there's no need to press C beforehand if you want to quit.

## Notes
- Discord client needs to be open and your account logged in for this to work.
- There's no required order for running the program. You can run this before or after you open Discord as it will automatically detect Discord as needed.
- If Discord closes while this is running, Discord drops the status. If Discord comes back, the listener sets the last announced game again unless you cleared it.
- Every launch announcement replaces the current presence and starts a new timer. Rebooting the PS2 into a new game just updates your presence, no interaction needed on the PC side.
- The playing time shown in the presence starts when the PC receives the launch announcement. For OPL this is when OPL starts the game. For the disc launcher this is just before the game boots, after the disc and network checks finish.
- Cover art is opt-in because Discord-RPC only accepts image keys uploaded to your Discord application. `-C` does not download covers. It uses title IDs as image asset keys so you can upload the covers yourself.
- The disc launcher uses DHCP and subnet broadcast. Run it with a PS2 disc inserted. It reads `SYSTEM.CNF`, announces the disc title ID if the network is ready and then boots the disc. The PC listener resolves the title ID through `ps2_titles.tsv`.

## Licence
- GPLv3 for PS2RichPresence.
- The bundled PS2 title database is generated from PCSX2's GPLv3-compatible `GameIndex.yaml`.
- MIT for discord-rpc.
