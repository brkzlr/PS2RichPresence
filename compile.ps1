# Copyright (C) 2026 brkzlr <brksys@icloud.com>
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

param([switch]$Clean)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
$DiscordSourceDir = Join-Path $RootDir "extern\discord-rpc"
$DiscordBuildDir = Join-Path $DiscordSourceDir "build"
$BuildDir = Join-Path $RootDir "build"
$Generator = "Visual Studio 17 2022"

function Run([string]$Command, [string[]]$Arguments) {
	& $Command @Arguments
	if ($LASTEXITCODE) { throw "$Command failed with exit code $LASTEXITCODE." }
}

Set-Location $RootDir

if ($Clean) {
	Write-Host "`n==> Cleaning generated outputs"
	foreach ($GeneratedPath in @(
		(Join-Path $RootDir "bin"),
		$BuildDir,
		(Join-Path $RootDir "include"),
		(Join-Path $RootDir "lib"),
		$DiscordBuildDir
	)) {
		if (Test-Path $GeneratedPath) {
			Remove-Item -Recurse -Force $GeneratedPath
		}
	}
}

$DiscordRuntime = Join-Path $RootDir "bin\discord-rpc.dll"
if (-not (Test-Path (Join-Path $RootDir "include\discord_rpc.h")) -or
	-not (Test-Path (Join-Path $RootDir "lib\discord-rpc.lib")) -or
	-not (Test-Path $DiscordRuntime)) {
	Run "git" @("submodule", "update", "--init", "--recursive", "extern/discord-rpc")

	# Upstream uses Unix mkdir and unzip to fetch RapidJSON.
	$RapidJsonHeader = Join-Path $DiscordSourceDir "thirdparty\rapidjson-master\include\rapidjson\document.h"
	if (-not (Test-Path $RapidJsonHeader)) {
		Write-Host "`n==> Downloading RapidJSON"
		$ThirdPartyDir = Join-Path $DiscordSourceDir "thirdparty"
		$RapidJsonArchive = Join-Path $ThirdPartyDir "rapidjson-master.zip"
		New-Item -ItemType Directory -Force -Path $ThirdPartyDir | Out-Null
		Invoke-WebRequest -Uri "https://github.com/Tencent/rapidjson/archive/refs/heads/master.zip" -OutFile $RapidJsonArchive -UseBasicParsing
		Expand-Archive -LiteralPath $RapidJsonArchive -DestinationPath $ThirdPartyDir -Force
		Remove-Item -Force $RapidJsonArchive
		if (-not (Test-Path $RapidJsonHeader)) {
			throw "RapidJSON extraction failed."
		}
	}

	Write-Host "`n==> Building discord-rpc"
	Run "cmake" @(
		"-S", $DiscordSourceDir,
		"-B", $DiscordBuildDir,
		"-G", $Generator,
		"-A", "x64",
		"-DCMAKE_INSTALL_PREFIX=$RootDir",
		"-DBUILD_SHARED_LIBS=ON",
		"-DBUILD_EXAMPLES=OFF",
		"-DUSE_STATIC_CRT=ON",
		"-DCLANG_FORMAT_CMD="
	)
	Run "cmake" @(
		"--build", $DiscordBuildDir,
		"--config", "Release",
		"--target", "install"
	)
}

Write-Host "`n==> Building Windows PC receiver"
Run "cmake" @(
	"-S", $RootDir,
	"-B", $BuildDir,
	"-G", $Generator,
	"-A", "x64"
)
Run "cmake" @(
	"--build", $BuildDir,
	"--config", "Release"
)

Write-Host "`n==> Build complete"
Write-Host "PC receiver: $(Join-Path $RootDir 'bin\ps2rpc.exe')"
Write-Host "Discord DLL: $DiscordRuntime"
Write-Host "Title DB:    $(Join-Path $RootDir 'bin\ps2_titles.tsv')"
