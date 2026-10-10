"""Compile our original CUDA kernels with the project-local official NVIDIA NVRTC runtime."""
from pathlib import Path
import ctypes as C
import os
import argparse
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output-dir', default=str(root / 'build'))
output_dir = Path(parser.parse_args().output_dir).resolve()
nvdir = root / '.deps/nvidia/cuda_nvrtc/bin'
cookie = os.add_dll_directory(str(nvdir))
os.environ['PATH'] = str(nvdir) + os.pathsep + os.environ['PATH']
builtins = C.WinDLL(str(nvdir/'nvrtc-builtins64_128.dll'))
nv = C.WinDLL(str(nvdir/'nvrtc64_120_0.dll'))
nv.nvrtcCreateProgram.argtypes = [C.POINTER(C.c_void_p),C.c_char_p,C.c_char_p,C.c_int,C.c_void_p,C.c_void_p]
nv.nvrtcCompileProgram.argtypes = [C.c_void_p,C.c_int,C.POINTER(C.c_char_p)]
nv.nvrtcGetProgramLogSize.argtypes = [C.c_void_p,C.POINTER(C.c_size_t)]
nv.nvrtcGetProgramLog.argtypes = [C.c_void_p,C.c_char_p]
nv.nvrtcGetPTXSize.argtypes = [C.c_void_p,C.POINTER(C.c_size_t)]
nv.nvrtcGetPTX.argtypes = [C.c_void_p,C.c_char_p]
nv.nvrtcDestroyProgram.argtypes = [C.POINTER(C.c_void_p)]
prog=C.c_void_p()
source=(root/'src/core/kernels.cu').read_bytes()
assert nv.nvrtcCreateProgram(C.byref(prog),source,b'kernels.cu',0,None,None)==0
options=(C.c_char_p*3)(b'--gpu-architecture=compute_75',b'--std=c++17',b'--use_fast_math')
status=nv.nvrtcCompileProgram(prog,len(options),options)
n=C.c_size_t();nv.nvrtcGetProgramLogSize(prog,C.byref(n)); log=C.create_string_buffer(n.value);nv.nvrtcGetProgramLog(prog,log)
if log.value: print(log.value.decode())
if status: raise RuntimeError(f'NVRTC failed: {status}')
nv.nvrtcGetPTXSize(prog,C.byref(n));ptx=C.create_string_buffer(n.value);assert nv.nvrtcGetPTX(prog,ptx)==0
output_dir.mkdir(parents=True, exist_ok=True)
(output_dir/'kernels.ptx').write_bytes(ptx.value)
(output_dir/'kernel_source.h').write_text('static const unsigned char texture_ptx[] = {\n'+','.join(str(b) for b in ptx.value+b'\0')+'\n};')
nv.nvrtcDestroyProgram(C.byref(prog))
print(f'Compiled CUDA PTX: {len(ptx.value)} bytes')
