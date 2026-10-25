#!/usr/bin/env bash
# Preserve the exact APK confirmed working on the tablet before rolling replacement.
set -euo pipefail
tag=vulkan-sync-8003146
apk=FilamentSDL3-test-arm64.apk
sha=80031464b6e224b4bfc89c4eb4933ee639e40059
digest=82a50bd1d49d053c24582defa289a2d379dd969c0761778bae493cf5f48f86f6
directory="$(mktemp -d)"
trap 'rm -rf -- "$directory"' EXIT
if gh release view "$tag" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then
    gh release download "$tag" --pattern "$apk" --dir "$directory" --repo "$GITHUB_REPOSITORY"
    printf '%s  %s\n' "$digest" "$directory/$apk" | sha256sum --check
    exit 0
fi
gh release download test-build --pattern "$apk" --dir "$directory" --repo "$GITHUB_REPOSITORY"
# If another build replaced the rolling APK, stop rather than mislabel a new binary.
printf '%s  %s\n' "$digest" "$directory/$apk" | sha256sum --check
gh release create "$tag" "$directory/$apk" --repo "$GITHUB_REPOSITORY" \
    --target "$sha" --prerelease --latest=false \
    --title 'Tablet Vulkan synchronization baseline (8003146)' \
    --notes "Exact APK confirmed by the user to render on the tablet. FOREST_VULKAN_SYNC=ON. Not suitable for FPS comparisons. SHA-256: $digest"
