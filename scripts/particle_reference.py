"""Independent NumPy reference for our bounded equal-radius Boolean-disc prototype.

This verifies the implemented model, not calibration to real film or equivalence to
the complete variable-radius Newson algorithm. No third-party renderer code used.
"""
import numpy as np,json
from core_tests import Params,render,ROOT,OUTPUTS
def hash32(x):
    x=np.asarray(x,dtype=np.uint64)&0xffffffff
    x=(x^(x>>16))*0x7feb352d&0xffffffff
    x=(x^(x>>15))*0x846ca68b&0xffffffff
    return (x^(x>>16)).astype(np.uint32)
def spatial(x,y,s):return hash32((np.asarray(x).astype(np.uint64)*0x9e3779b9)^(np.asarray(y).astype(np.uint64)*0x85ebca6b)^np.uint64(s))
def rnd(s):return (hash32(s)>>8).astype(np.float32)*np.float32(1/16777216)
def normal(s):return (np.sqrt(np.float32(-2)*np.log(np.maximum(rnd(s),np.float32(1.e-7))))*np.cos(np.float32(6.2831853)*rnd(np.asarray(s,dtype=np.uint32)^0xa51cd321))).astype(np.float32)
def reference(w,h,density,radius,seed,frame):
    seed=int(hash32(np.uint64(seed)^hash32(frame)));x,y=np.meshgrid(np.arange(w,dtype=np.float32)+.5,np.arange(h,dtype=np.float32)+.5)
    radius=np.float32(radius);cell=np.float32(2*radius);lam=np.float32(-np.log(np.float32(1-density))*4/np.float32(np.pi));result=np.zeros((h,w),np.float32)
    for sample in range(8):
        j=spatial(np.floor(x).astype(np.int32),np.floor(y).astype(np.int32),seed^((sample+1)*0x618da7))
        px=x+np.float32(.35)*normal(j);py=y+np.float32(.35)*normal(j^0x98123)
        cx=np.floor(px/cell).astype(np.int32);cy=np.floor(py/cell).astype(np.int32);covered=np.zeros((h,w),bool)
        for oy in [-1,0,1]:
            for ox in [-1,0,1]:
                s=spatial(cx+ox,cy+oy,seed);u=rnd(s);prob=np.full((h,w),np.exp(-lam),np.float32);cdf=prob.copy();count=np.zeros((h,w),np.int32)
                for k in range(1,25):
                    active=u>cdf;count+=active;prob=prob*lam/np.float32(k);cdf+=prob
                    if not active.any():break
                for k in range(int(count.max())):
                    ps=hash32(s.astype(np.uint64)^np.uint64(((k+1)*0xd168aae1)&0xffffffff))
                    dx=px-((cx+ox).astype(np.float32)+rnd(ps))*cell;dy=py-((cy+oy).astype(np.float32)+rnd(ps^0x6591b))*cell
                    covered|=(k<count)&(dx*dx+dy*dy<radius*radius)
        result+=covered
    return (result/8-density)/np.sqrt(np.float32(density*(1-density)))
def main():
    reports=[]
    for density in [.15,.5,.85]:
        a=np.full((64,96,4),density,np.float32);a[:,:,3]=1;p=Params(grain=.025,size=2,color=0,halation=0,bloom=0,particle=1,shadows=1,highlights=1)
        b,ms=render(a,p);gpu=(b[:,:,0]-density)/p.grain;cpu=reference(96,64,density,p.size*.45,p.seed,p.frame)
        diff=np.abs(gpu-cpu);mismatched=int(np.count_nonzero(diff>1e-4));assert mismatched<=6,f'Particle reference mismatch at {density}: {mismatched}'
        reports.append(dict(density=density,mismatched_samples=mismatched,total_pixels=64*96,max_abs_error=float(diff.max()),kernel_ms=ms,mean=float(gpu.mean()),stddev=float(gpu.std())))
    a=np.full((540,960,4),.5,np.float32);a[:,:,3]=1
    timings=[]
    for model in [0,1]:
        p=Params(particle=model,color=0,scale=.5);render(a,p);timings.append(dict(model='particle' if model else 'fast',width=960,height=540,kernel_median_ms=float(np.median([render(a,p)[1] for _ in range(5)]))))
    report={'comparison':'Independent NumPy equal-radius Boolean coverage reference; fixed intensities only','results':reports,'timings':timings,'limits':'Does not validate full Newson variable radii, optical convergence, calibration, or fast approximation matching. Eight optical samples; fixed-density test images.'}
    (OUTPUTS/'particle-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()
