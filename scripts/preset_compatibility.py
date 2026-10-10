"""Legacy 100%-strength CUDA pixel compatibility, without performance experiments."""
import ctypes as C
import argparse
import hashlib
import json
from itertools import product
from pathlib import Path
import numpy as np
from core_tests import Params

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--baseline',type=Path,required=True,help='Preserved previous texture_core.dll')
parser.add_argument('--candidate',type=Path,default=ROOT/'build-presets/Release/texture_core.dll')
args=parser.parse_args()
paths = [args.baseline.resolve(),args.candidate.resolve()]
assert paths[0].read_bytes()!=paths[1].read_bytes(),'Baseline and candidate are the same binary.'
libraries = [C.CDLL(str(p)) for p in paths]
for lib in libraries:
    lib.ft_render.argtypes = [C.c_void_p,C.c_void_p,C.c_int,C.c_int,C.POINTER(Params),C.POINTER(C.c_float)]
    lib.ft_error.restype = C.c_char_p

report = {'scope':'Standalone CUDA legacy pixel comparison; no Adobe host or performance claim.',
          'library_sha256':[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths], 'cases':[]}
rng = np.random.default_rng(91)
for size, model, linear, glow, origin in product((.25,.8,1.2,2.6,8),(0,1),(0,1),(False,True),(-27,4096)):
    image = rng.uniform(-.2,2,(93,131,4)).astype(np.float32)
    image[:,:,3] = rng.uniform(0,1,image.shape[:2]); image[:3,:,3] = 0
    params = Params(size=size,particle=model,linear=linear,originX=origin,originY=13,scale=.75,frame=71,
                    halation=.15 if glow else 0,bloom=.08 if glow else 0,mix=1)
    outputs = []
    for lib in libraries:
        result = np.empty_like(image); ms = C.c_float()
        if lib.ft_render(image.ctypes.data,result.ctypes.data,image.shape[1],image.shape[0],C.byref(params),C.byref(ms)):
            raise RuntimeError(lib.ft_error().decode())
        outputs.append(result)
    error = float(np.max(np.abs(outputs[0]-outputs[1])))
    report['cases'].append(dict(size=size,model=model,linear=linear,glow=glow,origin=origin,maximum_error=error))
    assert np.array_equal(outputs[0][:,:,3],outputs[1][:,:,3]), 'Alpha changed'
    assert error <= 1e-6, report['cases'][-1]
report['maximum_error'] = max(c['maximum_error'] for c in report['cases'])
report['exact_cases'] = sum(c['maximum_error']==0 for c in report['cases'])
report['result'] = 'All 80 cases pass the 1e-6 float tolerance; alpha is exact.'
(ROOT/'tests/outputs/presets/legacy-pixel-report.json').write_text(json.dumps(report,indent=2))
print(report['result'])
