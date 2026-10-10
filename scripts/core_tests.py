from pathlib import Path
import ctypes as C
import numpy as np
import json,time,os
ROOT=Path(__file__).resolve().parents[1]
BUILD=Path(os.environ.get('MGN_BUILD_DIR',str(ROOT/'build'))).resolve()
OUTPUTS=Path(os.environ.get('MGN_OUTPUT_DIR',str(ROOT/'tests/outputs'))).resolve()
OUTPUTS.mkdir(parents=True,exist_ok=True)
class Params(C.Structure):
    _fields_=[(n,C.c_float) for n in ['grain','size','color','halation','halRadius','bloom','bloomRadius','threshold','softness','shadows','highlights','scale','originX','originY']]+[(n,C.c_uint32) for n in ['seed','frame']]+[(n,C.c_int) for n in ['linear','particle']]
    def __init__(self,**kw):
        values=dict(grain=.025,size=1.2,color=.15,halation=.15,halRadius=8,bloom=.08,bloomRadius=32,threshold=.7,softness=.2,shadows=.7,highlights=.35,scale=1,seed=731,frame=0,linear=0,particle=0)
        values.update(kw);super().__init__(**values)
dll=C.CDLL(str(BUILD/'Release/texture_core.dll'))
dll.ft_render.argtypes=[C.c_void_p,C.c_void_p,C.c_int,C.c_int,C.POINTER(Params),C.POINTER(C.c_float)]
dll.ft_error.restype=C.c_char_p
def render(a,p):
    a=np.ascontiguousarray(a,dtype=np.float32);b=np.empty_like(a);ms=C.c_float()
    if dll.ft_render(a.ctypes.data,b.ctypes.data,a.shape[1],a.shape[0],C.byref(p),C.byref(ms)):raise RuntimeError(dll.ft_error().decode())
    return b,ms.value
def main():
    rng=np.random.default_rng(10)
    a=rng.uniform(-.2,2,(127,191,4)).astype('float32');a[:,:,3]=rng.uniform(0,1,a.shape[:2]);a[:9,:,3]=0
    b,_=render(a,Params(grain=0,halation=0,bloom=0));assert np.array_equal(a,b),'bypass changes pixels'
    b,_=render(a,Params());c,_=render(a,Params());assert np.array_equal(b,c),'repeat mismatch'
    assert np.array_equal(a[:,:,3],b[:,:,3]),'alpha changed'
    assert np.array_equal(a[:9],b[:9]),'hidden RGB changed'
    assert np.isfinite(b).all(),'nonfinite output';assert b[:,:,0].max()>1,'HDR clipped'
    c,_=render(a,Params(frame=1));assert not np.array_equal(b,c),'grain frozen'
    dark=np.zeros((128,128,4),np.float32);dark[:,:,3]=1;light=dark.copy();light[64,64,:3]=8
    d,_=render(dark,Params(grain=0,linear=1));l,_=render(light,Params(grain=0,linear=1))
    assert np.count_nonzero(d[:,:,:3])==0,'black glow';assert l[64,68,0]>0,'halo missing'
    assert l[64,68,0]>l[64,68,2],'halation is not warm'
    changed=light.copy();changed[64,64,:3]=.1;z,_=render(changed,Params(grain=0,linear=1));assert z[64,68,0]==0,'upstream input stale'
    mono=np.full((256,256,4),.5,np.float32);mono[:,:,3]=1
    m,_=render(mono,Params(color=0,halation=0,bloom=0));delta=m[:,:,:3]-.5
    assert np.array_equal(delta[:,:,0],delta[:,:,1]),'mono grain differs by channel'
    assert abs(delta.mean())<.001,'grain mean drift'
    refs,_=render(mono,Params(particle=1,color=0,halation=0,bloom=0));assert np.isfinite(refs).all()
    report={'tests':'passed','hardware':'RTX 3060 Ti','measurements':[],'note':'CUDA events time kernels only; wall time includes pageable CPU transfer and adapter overhead. Standalone, not Adobe host results.'}
    for w,h in [(1920,1080),(2560,1440),(3840,2160)]:
        a=np.full((h,w,4),.5,np.float32);a[:,:,3]=1;a[h//3:h//2,w//3:w//2,:3]=1.2
        p=Params(scale=h/1080);render(a,p)
        kernels=[];walls=[]
        for _ in range(8):
            t=time.perf_counter();_,ms=render(a,p);walls.append((time.perf_counter()-t)*1000);kernels.append(ms)
        report['measurements'].append(dict(width=w,height=h,kernel_median_ms=float(np.median(kernels)),kernel_p95_ms=float(np.percentile(kernels,95)),cpu_roundtrip_median_ms=float(np.median(walls))))
    (OUTPUTS/'core-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
