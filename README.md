# PS2RichPresence
This is a program that will set your Discord rich presence to whatever game your PS2 is playing through OPL with an SMB connection using the open source [Samba](https://www.samba.org/) suite available on Unix platforms.

## How does it work?
It uses [Discord-RPC](https://github.com/brkzlr/discord-rpc) library to communicate with your locally open Discord client and set rich presence details using your own Discord application ID. (See [Prerequisites](#prerequisites) below)

The presence details will be the iso/zso filename (excluding the ".iso"/".zso" extension) of the currently opened game on the PS2, which is grabbed using `smbstatus` included in the Samba suite.

This program **must** be run from the PC that acts as your PS2 SMB server, as it relies on `smbstatus` checking your locally accessed SMB files.

## Supported OS
Compatible OS that this will compile and run on are as follows:
- Linux
  - If you download the release `.zip` instead of compiling the program yourself, you need to have glibc version equal to or higher than `2.35` as the zip is created using Ubuntu 22.04 as base.
- MacOS
  - Even though this program will compile and run fine on MacOS, you will need to install the open source Samba suite on modern MacOS versions as Apple removed it in favour of their own implementation.
  - If you don't install Samba, then the program won't do anything as `smbstatus` is missing by default on modern MacOS.
  - Check [Runtime](#runtime) below for more info about this.

Windows is currently **not supported** as the methods I use in this program are not compatible with the way Windows does shares.
Windows support is not off the table though, I just didn't bother with it so far as I mainly made the program for myself and I don't use the OS.

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
- C compiler suite that supports C11 standard or higher.
  - Any modern GCC or Clang toolkit will do.
- Discord-RPC library.
  - A compilation fixed fork is already included in this repo as a submodule, so you don't need to do anything about this.
- [JSON-C](https://github.com/json-c/json-c) library.
  - You can compile it yourself or just install it using your package manager. A few examples below:
    - Debian/Ubuntu based distros: `sudo apt install libjson-c-dev`
    - Fedora: `sudo dnf install json-c-devel`
    - Arch Linux: `sudo pacman -S json-c`
    - MacOS: `brew install json-c`
- A PC that won't crash and burn when compiling code.
  - Not burning is optional, just make sure it won't crash during compilation and have your fire extinguisher ready :)

### Runtime:
You need to have the Samba suite installed to access the required `smbstatus` command that is used by the program.

You **should already have this** as you should be compiling/downloading this on the same PC that is your PS2 SMB server where ideally Samba would already be used (looking at you MacOS).

You just need to have [Jansson](https://github.com/akheron/jansson) library support enabled in Samba, so double check if it comes enabled, otherwise you might need to compile Samba with this support yourself.
You can check this by trying to run `sudo smbstatus -L -j` and see if it gives an error telling you about libjansson.

The steps below are to install Samba if you don't already have it installed for whatever reason, but ideally you should be setting up Samba for PS2 beforehand and skipping these steps.
- Linux: It should come included with most Linux distributions but if not, you can just install it using your package manager. The package should be named `samba` on most of them.
- MacOS: Use [Brew](https://brew.sh/) to install Samba by running `brew install samba`.
  - You might need some additional steps to make it run. This [StackExchange](https://apple.stackexchange.com/a/459346) post might be helpful.
  - From my personal testing, Brew doesn't seem to include Jansson library supported version of Samba, so you might need to compile it yourself if it's the same case for you.

## Building
Just run `compile.sh` inside the folder after cloning the repo using git and it will handle everything for you, if you fulfill the [compilation requirements](#compilation) that is.
```
git clone --recursive https://github.com/brkzlr/PS2RichPresence
cd PS2RichPresence
./compile.sh
```
Obviously you must have `git` installed for this. Downloading the ZIP file won't work as you need the discord-rpc submodule from this repo and submodules are not included in ZIP downloads.

You can also run `sudo cmake --install build` in the same folder after running `./compile.sh` if you want to have PS2RichPresence installed to `/opt/PS2RichPresence`.
You can delete the cloned folder afterwards and remove all compilation dependencies. The program will run just fine from there as the installation process makes it portable.

## Usage
Run the command outlined below in any of the following locations:
- Resulting `bin` folder if you locally compiled.
- Folder where you extracted the release `.zip` to.
- `/opt/PS2RichPresence` if you ran the installation command in the previous section.
```
sudo -E ./ps2rpc -a (Discord application ID) -s "(PS2 samba share path)" [-t Refresh period in seconds]
```
- Discord application ID: This is the App ID that you copied in the **Prerequisites** steps.
- PS2 samba share path: This is the path of the OPL folder that contains "CD", "DVD" and such.
  - For example: If your OPL folder is in `/mnt/PS2OPL` then you'll type `-s "/mnt/PS2OPL"`.
  - Using double quotes is mandatory if your path has spaces in it.
- Refresh period in seconds: This is an optional parameter where you can choose how often the program will check Samba for an active game.
  - Omitting this parameter will use a default of 10 seconds, which is good enough for most cases.
  - Setting it too low can cause your rich presence to be cleared a couple of times during the game's boot process as OPL unloads and loads the file several times.

Running as sudo is necessary because `smbstatus` requires it.

## Notes
- Discord client needs to be open and your account logged in for this to work.
- There's no required order for running the program. You can run this before or after you open Discord as it will automatically detect Discord as needed.
- Closing Discord while this is running will automatically clear your rich presence status. Opening Discord back up while this is running will automatically set the presence back (if you're still playing a game of course).
- Same thing applies to OPL too. You can open this program before or after you launched your PS2 game.
- Opening this program after you launched your PS2 game won't affect the rich presence playing time because this reads the timestamp of when OPL opened the game file.
  - For example: If you forgot to run this program and you've been playing for an hour already, running it after will automatically set your rich presence and say you've been playing it for an hour.

## Licence
- GPLv3 for PS2RichPresence.
- MIT for discord-rpc.
