"""Standalone CUDA stress and metamorphic checks; no Adobe host claims."""
from concurrent.futures import ThreadPoolExecutor
import json
import numpy as np
from core_tests import Params, render, ROOT, OUTPUTS
from host_buffer_tests import native

def scene(w, h, seed):
    rng = np.random.default_rng(seed)
    a = rng.uniform(-.15, 2, (h, w, 4)).astype(np.float32)
    a[:, :, 3] = rng.uniform(.05, 1, (h, w))
    a[:min(3, h), :, 3] = 0
    return a

def worker(seed):
    # Repeatedly grow and shrink the per-thread buffers, interleaving bypass and active frames.
    shapes = [(1, 1), (3, 7), (131, 93), (513, 287), (127, 191), (2, 2)]
    for cycle in range(3):
        for w, h in shapes:
            a = scene(w, h, seed + cycle)
            p = Params(seed=seed, frame=cycle, scale=max(h / 1080, .01))
            b, _ = render(a, p)
            repeat, _ = render(a, p)
            assert np.array_equal(b, repeat), 'Concurrent/repeated result changed'
            assert np.isfinite(b).all() and np.array_equal(b[:, :, 3], a[:, :, 3])
            assert np.array_equal(b[a[:, :, 3] == 0], a[a[:, :, 3] == 0])
            bypass, _ = render(a, Params(grain=0, halation=0, bloom=0))
            assert np.array_equal(a, bypass), 'Resize/bypass contaminated input'
    return {'thread_seed': seed, 'renders': len(shapes) * 3 * 3}

def main():
    with ThreadPoolExecutor(max_workers=4) as pool:
        threads = list(pool.map(worker, [71, 137, 919, 2461]))
    a = scene(131, 93, 47)
    full, _ = render(a, Params())
    _, cropped = native(a, Params(), crop=(11, 7), shape=(55, 75))
    assert np.allclose(cropped[:, :, [2, 1, 0, 3]], full[7:62, 11:86], atol=1e-7), 'Active ROI differs from full-source render'
    _, outside = native(a, Params(), crop=(-3, -2), shape=(101, 139))
    assert np.count_nonzero(outside[:2]) == 0 and np.count_nonzero(outside[:, :3]) == 0, 'Out-of-source crop must be transparent black'
    assert np.count_nonzero(outside[95:]) == 0 and np.count_nonzero(outside[:, 134:]) == 0
    # Light scattering adds linearly when input and output are explicitly linear and grain is off.
    a = np.zeros((193, 257, 4), np.float32); a[:, :, 3] = 1; a[96, 128, :3] = [3, 2, 1]
    hal, _ = render(a, Params(grain=0, bloom=0, linear=1))
    bloom, _ = render(a, Params(grain=0, halation=0, linear=1))
    both, _ = render(a, Params(grain=0, linear=1))
    error = float(np.max(np.abs(both - (hal + bloom - a))))
    assert error < 1e-6, 'Glow modules are not independently additive'
    # Hidden RGB in fully transparent pixels must never seed visible glow in neighboring pixels.
    a[96, 128, :3] = 100; a[96, 128, 3] = 0
    transparent, _ = render(a, Params(grain=0, linear=1))
    visible = a[:, :, 3] > 0
    assert np.count_nonzero(transparent[:, :, :3][visible]) == 0, 'Hidden RGB leaked into glow'
    report = {'tests': 'passed', 'scope': 'standalone real-CUDA concurrency and native-buffer tests, not Adobe MFR',
              'threads': threads, 'total_threaded_renders': sum(t['renders'] for t in threads),
              'active_roi': 'passed', 'out_of_source_roi': 'passed',
              'transparent_highlight_exclusion': 'passed', 'linear_glow_additivity_max_error': error}
    (OUTPUTS / 'stress-report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
