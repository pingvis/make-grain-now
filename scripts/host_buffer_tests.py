"""Exercise GPU-native pitched buffers, half precision, alpha and ROI cropping without Adobe."""
import ctypes as C
import numpy as np
from core_tests import dll,Params,render,ROOT
fn=dll.ft_test_host
fn.argtypes=[C.c_void_p,C.c_void_p]+[C.c_int]*10+[C.POINTER(Params)]
def native(a,p,half=False,premult=False,crop=(0,0),shape=None):
    h,w,_=a.shape;oh,ow=shape or (h,w);dtype=np.float16 if half else np.float32
    buf=np.zeros((h,w+7,4),dtype=dtype);rgb=a[:,:,:3].copy()
    if premult:rgb*=a[:,:,3,None]
    buf[:,:w,0]=rgb[:,:,2];buf[:,:w,1]=rgb[:,:,1];buf[:,:w,2]=rgb[:,:,0];buf[:,:w,3]=a[:,:,3]
    dest=np.full((oh,ow+5,4),-99,dtype=dtype)
    ret=fn(buf.ctypes.data,dest.ctypes.data,w,h,ow,oh,buf.strides[0],dest.strides[0],int(half),int(premult),*crop,C.byref(p))
    if ret:raise RuntimeError(dll.ft_error().decode())
    return buf,dest[:,:ow]
def main():
    rng=np.random.default_rng(40);a=rng.uniform(-.1,2,(93,131,4)).astype(np.float32);a[:,:,3]=rng.uniform(.05,1,a.shape[:2]);a[:5,:,3]=0
    for half in [False,True]:
        for premult in [False,True]:
            src,b=native(a,Params(grain=0,halation=0,bloom=0),half,premult,(11,7),(55,75))
            assert np.array_equal(b,src[7:62,11:86]),'bypass/crop changes native bytes'
            src,b=native(a,Params(),half,premult)
            assert np.array_equal(b[:,:,3],src[:,:131,3]),'native alpha changes'
            assert np.array_equal(b[:5],src[:5,:131]),'native hidden RGB changes'
            assert np.isfinite(b).all(),'native nonfinite'
            assert np.max(b[:,:,:3])>1,'native HDR clipped'
    a=np.ones((53,77,4),np.float32)*.5;a[:,:,3]=1
    expected,_=render(a,Params());src,b=native(a,Params())
    assert np.allclose(b[:,:,2],expected[:,:,0],atol=1e-7),'native channel mismatch'
    print('Native CUDA buffer tests passed: padded float/half BGRA, premultiplication, alpha, bypass, crop and range. These are adapter tests outside Adobe.')
if __name__=='__main__':main()
