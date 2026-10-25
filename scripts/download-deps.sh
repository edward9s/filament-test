#!/usr/bin/env bash
set -euo pipefail

# Use official prebuilt Filament and the repository SDL3 static SDK.
FILAMENT_VERSION=1.78.1
FILAMENT_SHA256=4346d06e0312b64a11fb26cbc387cd0b4687165c9cabaeeaa6f855fc6d44160f

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/.deps"
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

# Public Vulkan platform headers must match the prebuilt Filament release.
python3 "$ROOT/scripts/download-vulkan.py"
python3 "$ROOT/scripts/prepare-pipeline.py"


# Consume only the SDK published by build_sdl.yml; never rebuild SDL or use its AAR.
SDL_REPOSITORY="${GITHUB_REPOSITORY:-edward9s/filament-test}"
SDL_RELEASE=sdl3-static
command -v gh >/dev/null || { echo "gh CLI is required for the SDL3 release." >&2; exit 1; }

gh api "repos/$SDL_REPOSITORY/releases/tags/$SDL_RELEASE" --jq '.body' > "$TMP/sdl-release.txt"
SDL_VERSION="$(sed -n 's/^SDL_VERSION=//p' "$TMP/sdl-release.txt")"
if [[ ! "$SDL_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "SDL3 release has no valid SDL_VERSION metadata." >&2
    exit 1
fi

SDL_DIR="$ROOT/.deps/sdl3"
sdl_ready() {
    [[ -s "$SDL_DIR/lib/libSDL3.a" ]] &&
    [[ -s "$SDL_DIR/include/SDL3/SDL.h" ]] &&
    [[ -s "$SDL_DIR/lib/cmake/SDL3/SDL3Config.cmake" ]] &&
    [[ -s "$SDL_DIR/android/java/org/libsdl/app/SDLActivity.java" ]] &&
    grep -Fxq "SDL_VERSION=$SDL_VERSION" "$SDL_DIR/BUILD-INFO.txt"
}

if ! sdl_ready; then
    archive="SDL3-$SDL_VERSION-android-arm64-v8a-static.tar.gz"
    gh release download "$SDL_RELEASE" --repo "$SDL_REPOSITORY" \
        --pattern "$archive" --dir "$TMP"
    gh release download "$SDL_RELEASE" --repo "$SDL_REPOSITORY" \
        --pattern SHA256SUMS --dir "$TMP"

    expected="$(awk -v name="$archive" '$2 == name {print $1}' "$TMP/SHA256SUMS")"
    if [[ ! "$expected" =~ ^[0-9a-f]{64}$ ]]; then
        echo "Missing or invalid SHA-256 for $archive." >&2
        exit 1
    fi
    actual="$(sha256sum "$TMP/$archive" | cut -d' ' -f1)"
    if [[ "$actual" != "$expected" ]]; then
        echo "SHA-256 mismatch for $archive." >&2
        exit 1
    fi

    mkdir -p "$TMP/sdl-sdk"
    tar --no-same-owner -xzf "$TMP/$archive" -C "$TMP/sdl-sdk"
    test -s "$TMP/sdl-sdk/lib/libSDL3.a"
    test -s "$TMP/sdl-sdk/include/SDL3/SDL.h"
    test -s "$TMP/sdl-sdk/lib/cmake/SDL3/SDL3Config.cmake"
    test -s "$TMP/sdl-sdk/android/java/org/libsdl/app/SDLActivity.java"
    grep -Fxq "SDL_VERSION=$SDL_VERSION" "$TMP/sdl-sdk/BUILD-INFO.txt"
    grep -Fxq "PLATFORM=android-arm64-v8a" "$TMP/sdl-sdk/BUILD-INFO.txt"
    rm -rf "$SDL_DIR"
    mv "$TMP/sdl-sdk" "$SDL_DIR"
fi
echo "Ready: Filament $FILAMENT_VERSION native arm64, SDL3 $SDL_VERSION static arm64 (repository Release)"
