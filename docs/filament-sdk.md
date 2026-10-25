# Pinned Filament SDK

`filament.json` is the source of truth for repository, full commit, base version and NDK.
The fix branch starts at upstream v1.78.1, independently of the fork's rolling main.

`build_filament.yml` is a reusable workflow in this repository. The manual Android test
workflow calls it before building the APK. No Actions setup or secrets are required in
the public Filament fork. It checks out the pinned source, runs the pipeline lifetime and
utils tests, builds a complete Linux SDK with host tools, then builds the arm64 Android
SDK using those same tools and NDK 30.0.16248370. Samples and SDL2 are excluded.

Both SDKs include the BlueVK/platform headers required by our adapter. Each archive has
`SDK-INFO.json` containing its source revision and archive identity. The workflow saves
both archives and `SHA256SUMS` to the `filament-sdk-<first 12 SHA characters>` prerelease
in filament-test. First use takes longer because it builds from source. Later runs reuse
the existing release, checking its full source SHA and asset names. There is no fallback
to the official SDK and no per-file source replacement.

For local builds, authenticate `gh` for this private repository, then run
`bash scripts/download-deps.sh`. Downloads are hash checked, and extracted metadata must
match the pin and target architecture. CMake also rejects a different source revision.
To update the engine, push the engine change first, set the full new SHA in `filament.json`,
then run the Android workflow on the consumer branch. Existing SDK releases stay intact.

The source fix retains pipelines through the existing command-buffer resource list.
Cache age controls eviction only; GPU fences control release of command references.
The resource manager destroys the native pipeline after all references are gone.
Tests cover eviction with two pending users, completion while still cached, and replacing
a cached pipeline while its old submission is pending. Device-creation corrections from
the previously working version are carried into the fork unchanged.

This branch requires a tablet run with validation on and queue waiting off before merging.
The software readback and CPU tests cannot establish Android GPU correctness or speed.
Keep Vulkan material uniform batching disabled until a separate validation/performance run.
