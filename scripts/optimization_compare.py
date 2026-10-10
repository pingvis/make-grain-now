"""Interleaved real-CUDA kernel comparison; not Adobe playback measurements."""
import argparse
import ctypes as C
from pathlib import Path
import json
import hashlib
from itertools import product
import numpy as np
from core_tests import Params

parser = argparse.ArgumentParser()
parser.add_argument('candidate', type=Path)
parser.add_argument('--baseline', type=Path, default=Path('build/Release/texture_core.dll'))
parser.add_argument('--report', type=Path, default=Path('tests/outputs/optimization-comparison.json'))
args = parser.parse_args()

def load(path):
    dll = C.CDLL(str(path.resolve()))
    dll.ft_render.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.POINTER(Params), C.POINTER(C.c_float)]
    dll.ft_error.restype = C.c_char_p
    return dll

old, new = load(args.baseline), load(args.candidate)

def render(dll, image, p):
    output = np.empty_like(image)
    ms = C.c_float()
    if dll.ft_render(image.ctypes.data, output.ctypes.data, image.shape[1], image.shape[0], C.byref(p), C.byref(ms)):
        raise RuntimeError(dll.ft_error().decode())
    return output, ms.value

report = {'scope': 'Interleaved standalone CUDA event timings; no CPU transfers in timings, not Adobe playback.',
          'baseline_sha256': hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
          'candidate_sha256': hashlib.sha256(args.candidate.read_bytes()).hexdigest(),
          'parity': [], 'measurements': []}
rng = np.random.default_rng(91)
for size in (.25, .8, 1.2, 2.6, 8):
    for model in (0, 1):
        image = rng.uniform(-.2, 2, (93, 131, 4)).astype(np.float32)
        image[:, :, 3] = rng.uniform(0, 1, image.shape[:2])
        image[:3, :, 3] = 0
        for linear, glow, origin in product((0, 1), (False, True), (-27, 4096)):
            p = Params(size=size, particle=model, linear=linear, originX=origin, originY=13, scale=.75, frame=71,
                       halation=.15 if glow else 0, bloom=.08 if glow else 0)
            a, _ = render(old, image, p)
            b, _ = render(new, image, p)
            error = float(np.max(np.abs(a-b)))
            report['parity'].append({'size': size, 'model': model, 'linear': linear, 'glow':glow, 'origin':origin, 'maximum_error': error})
            assert np.isfinite(b).all() and np.array_equal(a[:, :, 3], b[:, :, 3])
            assert error <= 1e-6, report['parity'][-1]
for w, h in ((1920,1080), (2560,1440), (3840,2160)):
    image = np.full((h,w,4), .5, np.float32)
    image[:, :, 3] = 1
    image[h//3:h//2, w//3:w//2, :3] = 1.2
    for label, kw in [('fast grain', {'halation':0,'bloom':0}), ('particle grain', {'halation':0,'bloom':0,'particle':1}),
                      ('glow only', {'grain':0}), ('fast combined', {}), ('particle combined', {'particle':1})]:
        p = Params(scale=h/1080, **kw)
        for dll in (old, new):
            for _ in range(2): render(dll, image, p)
        times = [[], []]
        for repeat in range(10):
            for index in ((0,1) if repeat%2==0 else (1,0)):
                _, ms = render((old,new)[index], image, p)
                times[index].append(ms)
        medians = [float(np.median(t)) for t in times]
        row = {'width':w, 'height':h, 'case':label, 'baseline_median_ms':medians[0], 'candidate_median_ms':medians[1],
               'speedup':medians[0]/medians[1], 'baseline_samples_ms':times[0], 'candidate_samples_ms':times[1]}
        report['measurements'].append(row)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2))
        print(f'{w}x{h} {label}: {medians[0]:.3f} -> {medians[1]:.3f} ms ({row["speedup"]:.2f}x)', flush=True)
report['passed'] = True
args.report.write_text(json.dumps(report, indent=2))
