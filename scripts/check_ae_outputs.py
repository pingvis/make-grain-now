"""Inspect authored Adobe smoke/stacking renders, never substitute standalone results."""
from pathlib import Path
import json
import io
import subprocess
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[1]
outputs = root / 'tests/outputs'

def pixels(name):
    path = outputs / f'{name}.png'
    if not path.exists():
        candidates = [p for p in outputs.glob(f'{name}*') if p.suffix.lower() in ('.png', '.tif', '.tiff', '.avi', '.mov')]
        if len(candidates) != 1:
            raise FileNotFoundError(f'Expected one actual host render for {name}; found {candidates}')
        path = candidates[0]
    if path.suffix.lower() in ('.avi', '.mov'):
        encoded = subprocess.check_output(['C:/ffmpeg/bin/ffmpeg.exe', '-v', 'error', '-i', str(path), '-frames:v', '1', '-f', 'image2pipe', '-vcodec', 'png', '-'])
        frame = Image.open(io.BytesIO(encoded)).convert('RGBA')
    else:
        frame = Image.open(path).convert('RGBA')
    return np.asarray(frame, dtype=np.float32) / 255

def difference(a, b):
    assert a.shape == b.shape, 'Host image dimensions differ'
    delta = np.abs(a - b)
    return {'mean': float(delta.mean()), 'maximum': float(delta.max()),
            'changed_pixels': int(np.any(delta > 0, axis=-1).sum())}

smoke_log = (outputs / 'ae-smoke.txt').read_text(encoding='utf-8-sig')
assert 'Native render completed' in smoke_log and 'Frame 1 completed' in smoke_log, smoke_log
report = {'scope': 'AE-authored lossless RGB AVI render checks, 8-bit comparison; not Premiere, alpha, float equivalence or playback',
          'passed': False,
          'smoke_temporal': difference(pixels('ae-smoke-frame-0'), pixels('ae-smoke-frame-1'))}
assert report['smoke_temporal']['changed_pixels'] > 0, 'Held image grain did not change between frames'

stack_log = (outputs / 'ae-stacking.txt').read_text(encoding='utf-8-sig')
assert 'ERROR:' not in stack_log and 'Saved own project' in stack_log, stack_log
ordered = pixels('ae-grade-then-texture')
report['effect_order'] = difference(ordered, pixels('ae-texture-then-grade'))
report['upstream_change'] = difference(ordered, pixels('ae-upstream-changed'))
report['adjustment_vs_clip'] = difference(ordered, pixels('ae-adjustment-texture'))
report['quantized_prerender_vs_adjustment'] = difference(pixels('ae-prerender-texture'), pixels('ae-adjustment-texture'))
report['bypass_vs_host_baseline'] = difference(pixels('ae-bypass'), pixels('ae-source-baseline'))
report['nested_vs_direct_baseline'] = difference(pixels('ae-nested-baseline'), pixels('ae-upstream-composite'))
report['adjustment_bypass_vs_upstream'] = difference(pixels('ae-adjustment-bypass'), pixels('ae-upstream-composite'))
assert report['effect_order']['changed_pixels'] > 0, 'Effect order produced identical output'
assert report['upstream_change']['changed_pixels'] > 0, 'Changing upstream exposure produced identical output'
# Matching clip and nested adjustment routes should agree to one 8-bit output code.
(outputs / 'ae-host-report.json').write_text(json.dumps(report, indent=2))
assert report['adjustment_vs_clip']['maximum'] <= 1.01 / 255, report['adjustment_vs_clip']
assert report['bypass_vs_host_baseline']['maximum'] == 0, report['bypass_vs_host_baseline']
report['passed'] = True
(outputs / 'ae-host-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
