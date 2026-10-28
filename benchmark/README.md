# Unified forest benchmark — implementation in progress

Schema version `1.0.0`. This branch is a work in progress; no Galaxy Tab S8 measurements or completed APK validation are claimed.

Each run directory contains `metadata.json`, `frames.csv`, `metrics.csv`, and `summary.json`.
The metadata and summary carry the same `schema_version`, `run_id`, `preset_id`, `valid`, and `invalid_reason`.
`test_compare.py::fixture()` is an executable **synthetic** schema example. Do not publish its numbers as device results.

## Statistics and clocks

Use a monotonic clock, collect application callback intervals only after a 30 second warmup, then measure at least 60 seconds. Reset camera and animation clock before warmup. Repeat each preset three times independently, restoring baseline settings between runs.

- Mean FPS = 1000 × sample count / sum of intervals in milliseconds.
- Mean frame time = sum / count.
- P50/P95/P99 use nearest rank: sorted intervals at `ceil(p × count) - 1`.
- 1% low FPS = 1000 / mean of the **slowest `ceil(count × .01)` intervals**.
- The first measured timestamp anchors the interval sequence and is not itself a sample.
- Application callback FPS is not presentation FPS. Unmeasured presentation FPS is `null`.
- Store actual CPU/GPU scopes, source API, and units. Never infer whole-frame GPU time from a slowest pass or sum potentially overlapping passes.

`frames.csv` columns:

```csv
frame_index,elapsed_ms,interval_ms,animation_seconds,cpu_scene_ms,cpu_effects_ms,cpu_submit_ms,cpu_wait_ms
```

Unsupported CPU values are empty. Nonempty CPU columns require a matching metric definition in metadata. `metrics.csv` is for asynchronous native observations:

```csv
sample_id,observed_frame,metric,value_ms
```

`observed_frame` identifies the application callback receiving the result, not the GPU execution frame. Deduplicate native samples before writing. Never emit zero as a substitute for unavailable timestamps.

## Comparison CLI

```sh
python3 -m unittest discover -s benchmark -p 'test_*.py'
python3 benchmark/compare.py exported-directory engine2.zip engine3.zip --validate-only
python3 benchmark/compare.py exported-directory engine2.zip engine3.zip --output comparison
```

The tool uses only the Python standard library. It recomputes statistics from raw intervals, checks dimensions and preset settings, rejects malformed data, groups backend / presentation policy / profiling / hardware / scene conditions, and writes `comparison.md` and `comparison.csv`.
Different builds or settings of the same engine refuse aggregation. Unknown actual presentation modes are isolated per engine, preventing unsupported cross-engine rankings. Three engines × three repetitions are needed for a complete comparison. CPU scopes are shown separately. GPU passes keep their names.

## Required physical-device verification

Pending: complete Android integration, release APK build/package checks, start/cancel/export and restore behavior, background/rotation invalidation, shader/reflection readiness, actual GPU timestamp support, repeated-run memory use, showcase vs Standard overhead, thermal behavior and matched screenshots at 38.52 seconds.

For Bevy's 8–9 FPS, independent resolution/shadow/AA/Bloom/effects tests can identify sensitivity to those workloads. Native render-world timings can separate preparation and submission scopes. These observations alone cannot prove shader ALU, texture bandwidth, occupancy, overdraw, driver stalls or CPU/GPU queue dependencies; AGI/Perfetto or supported vendor counters are still needed. Adreno GPU preprocessing support must be recorded from the actual Bevy adapter, not assumed from model name.

## Filament Android integration

Tap **Benchmark** at the upper right: presets 1–8 run three repetitions; tap the progress button to cancel. Results and ZIP export use Android UI/SAF. Reference capture is separate and freezes animation at 38.52 s, using asynchronous Android PixelCopy. The screenshot has a matching settings JSON. Backend selection remains the existing FPS overlay while idle; benchmark input locks it.

Standard disables adaptive resolution and preserves the existing native pipeline: TAA + FXAA, MSAA disabled, SSAO, Bloom, fog, 3 × 2048 shadow maps at 65 m. This differs from Godot/Bevy's 48 m/four cascades and must remain visible in comparisons. The 65% preset fixes both native resolution bounds to .65; metadata records the renderer's last actual scale. No-effects removes six batches and three local lights from the scene and skips only their update; foliage remains animated. Ending or cancelling restores the saved view options and camera.

`steady_clock` spans measure `Forest.animate`, `Effects.animate`, successful `Renderer.beginFrame` calls and `render(main view)` through HUD/endFrame. The beginFrame span includes CPU work and waiting; it is not pure wait time. Submitted application intervals include rejected beginFrame attempts. GPU Diagnostic additionally reads `Renderer.getFrameInfoHistory(16)` and deduplicates each native frame/metric independently, so a pending GPU timer can arrive after its CPU result. Values are native nanoseconds converted to ms. The public pinned API exposes main-thread, backend-thread and whole native renderer GPU spans, including HUD; no per-pass timestamps are collected or inferred.

No per-frame GPU wait was added. The existing lifecycle initialization/destruction waits remain. Standard avoids detailed history collection, but Filament may retain internal frame timers. Vulkan FIFO is fixed in the pinned fork; GLES retains its original EGL default interval. Neither is claimed uncapped. Missing GPU/driver strings are null.

Local checks: C++ protocol/statistics tests; native Android arm64 Release `libmain.so` built with the pinned 1.78.1 fork and NDK 30; Java SDL activity/bridge compiled against API 35. Full signed APK assembly, SAF/PixelCopy, actual 65% target dimensions, GPU timer support, overhead and tablet lifecycle/visual checks remain pending.

Sources: [pinned Renderer API](https://github.com/edward9s/filament/blob/48dc0de08cba360502ad62cbc29b9a0a6e75abc2/filament/include/filament/Renderer.h), [Vulkan presentation implementation](https://github.com/edward9s/filament/blob/48dc0de08cba360502ad62cbc29b9a0a6e75abc2/filament/backend/src/vulkan/platform/VulkanPlatformSwapChainImpl.cpp).
