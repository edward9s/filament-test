#!/usr/bin/env bash
set -euo pipefail

# Official, immutable upstream release assets: no library source builds.
FILAMENT_VERSION=1.78.1
SDL_VERSION=3.4.18
FILAMENT_SHA256=4346d06e0312b64a11fb26cbc387cd0b4687165c9cabaeeaa6f855fc6d44160f
SDL_SHA256=e09e4d6593335d089ba124375e8b8abd0fd75173dffcd2c17012b45af121d4e9

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/.deps" "$ROOT/android/app/libs"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -s "$ROOT/.deps/filament/lib/arm64-v8a/libfilament.a" ||
      ! -f "$ROOT/.deps/filament/include/filament/Engine.h" ]]; then
    curl -fL --retry 3 \
      "https://github.com/google/filament/releases/download/v${FILAMENT_VERSION}/filament-v${FILAMENT_VERSION}-android-native.tgz" \
      -o "$TMP/filament.tgz"
    echo "${FILAMENT_SHA256}  $TMP/filament.tgz" | sha256sum -c -
    mkdir -p "$TMP/filament-extract"
    tar --no-same-owner -xzf "$TMP/filament.tgz" -C "$TMP/filament-extract"
    engine_h="$(find "$TMP/filament-extract" -type f -path '*/include/filament/Engine.h' -print -quit)"
    if [[ -z "$engine_h" ]]; then
        echo "Cannot find Filament headers in official archive" >&2
        exit 1
    fi
    sdk_root="$(dirname "$(dirname "$(dirname "$engine_h")")")"
    mkdir -p "$ROOT/.deps/filament"
    cp -a "$sdk_root/." "$ROOT/.deps/filament/"
fi

test -s "$ROOT/.deps/filament/lib/arm64-v8a/libfilament.a"

if [[ ! -s "$ROOT/android/app/libs/SDL3.aar" ]]; then
    curl -fL --retry 3 \
      "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-devel-${SDL_VERSION}-android.zip" \
      -o "$TMP/sdl.zip"
    echo "${SDL_SHA256}  $TMP/sdl.zip" | sha256sum -c -
    mkdir -p "$TMP/sdl-extract"
    unzip -q "$TMP/sdl.zip" -d "$TMP/sdl-extract"
    aar="$(find "$TMP/sdl-extract" -type f -name 'SDL3*.aar' -print -quit)"
    if [[ -z "$aar" ]]; then
        echo "Cannot find SDL3 Android AAR in official archive" >&2
        exit 1
    fi
    cp "$aar" "$ROOT/android/app/libs/SDL3.aar"
fi
unzip -tq "$ROOT/android/app/libs/SDL3.aar" >/dev/null
echo "Ready: Filament ${FILAMENT_VERSION} native arm64, SDL3 ${SDL_VERSION} Android AAR"
