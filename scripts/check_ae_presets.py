"""Compare the targeted real After Effects 32-bit float preset-update exports."""
from pathlib import Path
import hashlib
import json
from datetime import datetime, timezone
import numpy as np
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'.deps/psd-python'))
from psd_tools import PSDImage

def psd_planes(path):
    psd=PSDImage.open(path)
    assert psd.depth==32 and psd.color_mode.name=='RGB'
    raw=psd._record.image_data.get_data(psd._record.header,False)
    planes=np.frombuffer(raw,dtype='>f4').reshape(psd.channels,psd.height,psd.width)
    return psd,np.moveaxis(planes.astype(np.float32),0,-1)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'tests/outputs'
def read(name):
    paths = list(OUT.glob('ae-preset-'+name+'-[0-9][0-9][0-9][0-9][0-9].psd'))
    assert len(paths)==1, paths
    return psd_planes(paths[0])[1]

log = (OUT/'ae-preset-validation.txt').read_text(encoding='utf-8-sig')
assert 'ERROR:' not in log and 'project ready for UI interaction.' in log
report = dict(binary_sha256=hashlib.sha256((ROOT/'build/Release/MakeGrainNow.aex').read_bytes()).hexdigest().upper(),
              checked_at_utc=datetime.now(timezone.utc).isoformat(),scope='Actual AE 2026, synthetic 640x360/50fps, 32-bit float PSD. Not Premiere, playback, ROI or MFR qualification.',comparisons={})
for suffix in ('fast','fast-next','simulated'):
    old,new=read('old-'+suffix),read('new-'+suffix)
    error=float(np.max(np.abs(old-new)))
    assert error<=1e-6,(suffix,error)
    report['comparisons']['legacy '+suffix]={'maximum_error':error,'tolerance':1e-6}
source,zero,half,full=[read('strength-'+n) for n in ('source','0','50','100')]
assert np.array_equal(source,zero),'Strength zero altered source'
# AE's built-in float PSD template stores linearized RGB for this unlinearized
# project. Known synthetic source colors independently identify gamma 2.4.
known=np.array([.3,.4,.5],dtype=np.float32)
assert np.max(np.abs(source[100,100,:3]-known**np.float32(2.4)))<2e-6
def display(rgb): return np.maximum(rgb,0)**np.float32(1/2.4)
expected=((display(source[:,:,:3])+display(full[:,:,:3]))*np.float32(.5))**np.float32(2.4)
error=float(np.max(np.abs(half[:,:,:3]-expected)))
assert error<=5e-6,error
assert np.array_equal(source[:,:,3:],full[:,:,3:]) and np.array_equal(source[:,:,3:],half[:,:,3:]),'Alpha changed'
assert not np.array_equal(source,full),'Effect did not render'
report['comparisons']['strength zero']={'maximum_error':0,'exact':True}
report['comparisons']['strength half']={'maximum_error':error,'tolerance':5e-6,'export_conversion':'Gamma 2.4 linearized PSD, approximately implemented by Adobe color conversion; independently checked against known synthetic source RGB within 2e-6.'}
report['legacy_parameter_animation_migration']='passed actual script assertions'
report['new_instance_simulated_default']='passed actual script assertion'
report['ui_interaction']='Recorded separately in ae-preset-ui-history.txt; not asserted by this pixel checker.'
history_path=OUT/'ae-preset-ui-history.txt'
if history_path.exists():
    states=[json.loads(line.split(' ',1)[1]) for line in history_path.read_text().splitlines() if ' {' in line]
    variation=next((i for i,s in enumerate(states) if s['Look']==13 and s['Lock grain settings']==1 and s['Lock halation settings']==1 and s['Bloom amount']!=5),None)
    if variation is not None:
        before,changed,undo=states[variation-1:variation+2]
        assert all(changed[k]==before[k] for k in before if k not in ('Look','Bloom amount','Bloom radius','Explore seed'))
        assert undo==before,'Actual UI undo did not restore the full state'
        assert .9125*before['Bloom amount']<=changed['Bloom amount']<=1.0875*before['Bloom amount']
        pattern=states[variation+2]
        assert pattern['Grain seed']!=undo['Grain seed']
        assert all(pattern[k]==undo[k] for k in undo if k!='Grain seed')
        report['ui_interaction']='Passed recorded actual AE New look, locked variation, complete Undo and pattern-only change.'
reopen_path=OUT/'ae-preset-reopen.txt'
if reopen_path.exists():
    assert reopen_path.read_text().startswith('PASS:')
    report['actual_save_reopen']='passed: all numeric settings, model, locks and seeds'
report['result']='passed'
(OUT/'ae-preset-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
