# RTX 3060 Ti measurements

The optimized alpha build preserves the original image-processing model and
controls. It skips particle cells that cannot affect an optical sample, reads
integer blur taps directly, and avoids unused glow decoding for grain-only rendering.
Two comparison runs passed 80 image cases each with exact pixel and alpha equality.

Paired particle grain + halation + bloom **CUDA kernel time**:

| Resolution | First-run before → after | Time reduction across two runs |
| --- | --- | --- |
| 1920×1080 | 4.94 → 4.51 ms | 9–17% |
| 2560×1440 | 9.16 → 7.32 ms | about 20% |
| 3840×2160 | 18.51 → 17.38 ms | 6–9% |

Each run contains ten interleaved measurements per implementation after warmup.
Timings exclude CPU transfers and native packing/unpacking. Fast combined
kernel gains are approximately 1–5%; some grain-only differences are timing noise.

Separate current **native mock adapter** medians include packing/unpacking and
synchronization: fast 0.82/1.40/3.07 ms, particle 4.42/7.64/17.61 ms at
1080/1440/2160. These are not paired before/after measurements.

All these timings are standalone. They exclude Adobe decode, grading, scheduling
and display, depend on load/content, and do not establish real-time playback.
Particle mode leaves limited headroom at 4K50 and is not advertised as real-time
to Premiere. Actual AE float exports validate specific correctness cases rather
than playback performance. See the raw JSON reports in verification/.
