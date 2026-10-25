# Filament static SDK

`filament.json` records the fork repository, full source commit, engine version and NDK.
The engine fix starts at upstream v1.78.1. After device verification, its PR is intended
for the fork's `release/1.78.1` maintenance branch. The fork's `main` is a different upstream
snapshot; merging the fix does not require changing the application's engine version.
A commit remains a valid source identity after its branch has been merged.

The checkout action uses exactly `filament.json.revision` as its `ref`. The current pin
includes the pipeline fix and frame-history storage reuse on `fix/vulkan-lifetime`.
It does not select a recently updated branch or follow branch-head changes. Updating
the engine requires changing this pin to the chosen tested commit.

`build_filament.yml` is called by the manual Android test workflow and can also be run
manually. It checks the single `filament-static` dependency release in filament-test.
Matching source metadata and all five final assets must exist to reuse it. A different
source pin rebuilds the SDKs. Source/NDK metadata errors and incomplete published releases
fail immediately. There is no fallback to an official SDK or a replacement source file.

| Archive | Libraries and tools |
|---|---|
| `filament-linux.tgz` | Linux x64 `.a`; matching `matc` and `cmgen` |
| `filament-android-arm64.tgz` | Android arm64-v8a `.a`, NDK 30.0.16248370 |
| `filament-macos-universal.tgz` | macOS x86_64 + arm64 `.a` and host tools; deployment target 11.0 |
| `filament-windows-x64-msvc.tgz` | Windows x64 MSVC `.lib` and host tools, dynamic CRT `/MD` |
| `SHA256SUMS` | SHA-256 checksums for all four archives |

All SDK libraries are built in Release mode. Windows uses MSVC, matching static SDL3's
CRT selection; macOS verifies both architectures with `lipo`. Vulkan is enabled on both
new desktop targets. Each native build runs the pipeline lifetime, utils and frame-history tests first.
Linux also builds Android with its matching host tools. Samples and SDL2 are excluded.
`filament.json` sets `lto` to `ON`. Release builds enable both Filament LTO and CMake IPO;
the native APK and software Vulkan readback also link with LTO. Linux uses matching LLVM
18 archive tools and LLD. A required-LTO preflight fails on unsupported toolchains.
`SDK_LTO` in release notes and `lto` in `SDK-INFO.json` are checked before SDK reuse.
The APK CMake configuration also rejects a non-LTO SDK.

The release stays a draft while matrix jobs upload SDKs and individual checksums.
The final job checks every hash, source stamp, library layout and host-tool set, publishes
the combined checksum manifest, removes temporary per-file checksums, then publishes the
release. Consumers reject a draft. A failed build can be retried on the same branch.
No Actions Artifacts or additional repository secrets are used.

The release name is fixed; `SOURCE_COMMIT` in its notes and `SDK-INFO.json` in each SDK
record the full engine commit. Updates replace the assets under `filament-static`, like
`sdl3-static`. This name itself does not retain old binaries. To reproduce an old SDK,
check out the old application commit (including its pin and recipe) and build that source
with the recorded toolchain. Exact binary rollback requires a separately preserved archive.
The earlier `filament-sdk-d74ff02b482e` release remains available for run #25.

Local asset preparation selects host tools for Linux x64, macOS universal or Windows x64,
and also downloads the Android SDK. Unsupported hosts fail. The archives include BlueVK
and platform-specific utils headers. CMake rejects an SDK from a different source commit.
To update the engine, push its change, set the new full SHA in `filament.json`, then run
this workflow on the application branch. No Actions setup is required in the fork.

The pipeline fix uses existing command-buffer resource ownership. Cache age controls eviction;
GPU fences control release of command references. No idle wait is added to cache collection.
Vulkan material uniform batching is explicitly enabled through `RendererEngine.h`.
Frame-history updates retain their backing allocation, while public history snapshots copy
only the requested entries. A native NOOP-backend regression verifies storage reuse across
40 frames, newest-first order, bounded copies and snapshot independence. This changes CPU
bookkeeping; no GPU completion or presentation synchronization is removed.

Windows builds run the native Python builder through PowerShell. The fork captures
Windows call stacks, retains all job-pool slots despite heap-base alignment, and
preserves drive-root separators in path splitting. All Utils tests remain enabled.
The Windows wakeup stress test keeps 5,000 rounds and its 200 ms per-round completion
check; its outer budget is 180 seconds to accommodate Windows timer/thread overhead.

Android run #25 succeeded using the fork SDK with validation OFF and queue waiting OFF.
It includes the pipeline ownership fix and a Release-mode SDK. This does not establish
that Vulkan is faster. Run #25 predates the batching/LTO changes; the next successful
workflow build will produce the optimized configuration. Device validation and native
macOS/Windows CI execution are still pending.

