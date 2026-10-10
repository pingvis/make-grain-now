"""Compare actual AE float PSD or EXR exports without an 8-bit conversion.

Header layout: https://openexr.com/en/latest/OpenEXRFileLayout.html
Pixel decoding is delegated to the installed FFmpeg EXR decoder.
"""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / '.deps/psd-python'))
import json
import struct
import subprocess
import hashlib
from datetime import datetime, timezone
import numpy as np

outputs = Path(__file__).resolve().parents[1] / 'tests/outputs'
metadata = {}

def psd_planes(path):
    from psd_tools import PSDImage
    psd = PSDImage.open(path)
    assert psd.depth == 32 and psd.color_mode.name == 'RGB'
    # AE's straight-alpha exports have no white matte. PSDImage.numpy() removes
    # a presumed white background, and numpy('shape') synthesizes opaque alpha
    # when AE omits the transparency tag. Decode the stored float planes instead.
    raw = psd._record.image_data.get_data(psd._record.header, False)
    result = np.frombuffer(raw, dtype='>f4').reshape(psd.channels, psd.height, psd.width)
    return psd, np.moveaxis(result.astype(np.float32), 0, -1)

def cstring(data, offset):
    end = data.index(b'\0', offset)
    return data[offset:end].decode('ascii'), end + 1

def header(path):
    data = path.read_bytes()
    assert struct.unpack_from('<I', data)[0] == 20000630, 'Invalid EXR magic'
    position, attributes = 8, {}
    while data[position]:
        name, position = cstring(data, position)
        kind, position = cstring(data, position)
        size = struct.unpack_from('<I', data, position)[0]
        position += 4
        attributes[name] = (kind, data[position:position + size])
        position += size
    channels, position = {}, 0
    channel_data = attributes['channels'][1]
    while channel_data[position]:
        name, position = cstring(channel_data, position)
        pixel_type = struct.unpack_from('<i', channel_data, position)[0]
        channels[name] = {0: 'uint32', 1: 'float16', 2: 'float32'}[pixel_type]
        position += 16
    x0, y0, x1, y1 = struct.unpack('<4i', attributes['dataWindow'][1])
    return {'channels': channels, 'width': x1 - x0 + 1, 'height': y1 - y0 + 1,
            'compression_code': attributes['compression'][1][0]}

def pixels(name):
    candidates = [p for p in outputs.glob(f'ae-float-{name}-[0-9][0-9][0-9][0-9][0-9].*') if p.suffix in ('.exr', '.psd')]
    assert len(candidates) == 1, f'Expected one actual float image for {name}: {candidates}'
    if candidates[0].suffix == '.psd':
        psd, planes = psd_planes(candidates[0])
        assert psd.depth == 32, f'PSD is not float32: {psd.depth}'
        assert psd.color_mode.name == 'RGB', psd.color_mode
        metadata[name] = {'format': 'PSD', 'width': psd.width, 'height': psd.height, 'depth': psd.depth}
        result = planes[:, :, :3]
        assert result.dtype == np.float32 and np.isfinite(result).all()
        return result
    info = header(candidates[0])
    metadata[name] = info
    assert all(info['channels'].get(c) == 'float32' for c in 'RGB'), info
    # NO/RLE/ZIPS/ZIP/PIZ preserve values; lossy compression invalidates this check.
    assert info['compression_code'] in (0, 1, 2, 3, 4), info
    raw = subprocess.check_output([
        'C:/ffmpeg/bin/ffmpeg.exe', '-v', 'error', '-i', str(candidates[0]),
        '-frames:v', '1', '-pix_fmt', 'gbrpf32le', '-f', 'rawvideo', '-'])
    planar = np.frombuffer(raw, dtype='<f4').reshape(3, info['height'], info['width'])
    result = np.moveaxis(planar[[2, 0, 1]], 0, -1)
    assert np.isfinite(result).all(), f'Nonfinite pixels in {name}'
    return result

def difference(a, b):
    assert a.shape == b.shape
    delta = np.abs(a - b)
    return {'mean': float(delta.mean()), 'maximum': float(delta.max()),
            'changed_pixels': int(np.any(delta != 0, axis=-1).sum())}

def main():
    log = (outputs / 'ae-float.txt').read_text(encoding='utf-8-sig')
    assert 'ERROR:' not in log and 'Saved own float project' in log, log
    clip, adjustment = pixels('clip'), pixels('adjustment')
    def alpha(name):
        paths = list(outputs.glob(f'ae-float-{name}-[0-9][0-9][0-9][0-9][0-9].psd'))
        assert len(paths) == 1, paths
        psd, planes = psd_planes(paths[0])
        assert psd.channels == 4
        return planes[:, :, 3:4]

    alpha_source, alpha_effect = alpha('alpha-source'), alpha('alpha-effect')
    assert alpha_source.min() == 0 and 0 < alpha_source.max() < 1
    report = {
        'checked_at_utc': datetime.now(timezone.utc).isoformat(),
        'binary_sha256': hashlib.sha256((outputs.parents[1] / 'build/Release/MakeGrainNow.aex').read_bytes()).hexdigest(),
        'scope': 'Actual AE 32-bit-float PSD exports, 1920x1080 at 50 fps; working space logged in ae-float.txt. CUDA versus CPU-visible route, alpha and preliminary Exposure stacking; not Premiere, Lumetri, ROI, MFR or playback.',
        'passed': False,
        'bypass_vs_source': difference(pixels('bypass'), pixels('source')),
        'nested_vs_upstream': difference(pixels('nested'), pixels('upstream')),
        'adjustment_bypass_vs_upstream': difference(pixels('adjustment-bypass'), pixels('upstream')),
        'adjustment_vs_clip': difference(adjustment, clip),
        'repeat_same_frame': difference(pixels('repeat'), clip),
        'temporal_grain': difference(pixels('next-frame'), clip),
        'upstream_change': difference(pixels('changed'), clip),
        'float_prerender_vs_adjustment': difference(pixels('prerender'), adjustment),
        'cuda_vs_cpu_visible': difference(pixels('cpu-visible'), clip),
        'alpha_preservation': difference(alpha_effect, alpha_source),
        'partial_alpha_rgb_effect': difference(pixels('alpha-effect'), pixels('alpha-source')),
        'exports': metadata,
        'comparison_tolerance': 5e-6,
    }
    report_path = outputs / 'ae-float-report.json'
    report_path.write_text(json.dumps(report, indent=2))
    for name in ('bypass_vs_source', 'nested_vs_upstream', 'adjustment_bypass_vs_upstream', 'adjustment_vs_clip', 'repeat_same_frame', 'float_prerender_vs_adjustment', 'cuda_vs_cpu_visible', 'alpha_preservation'):
        assert report[name]['maximum'] <= report['comparison_tolerance'], (name, report[name])
    assert report['temporal_grain']['changed_pixels'] > 0
    assert report['upstream_change']['changed_pixels'] > 0
    assert report['partial_alpha_rgb_effect']['changed_pixels'] > 0
    report['passed'] = True
    report_path.write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
