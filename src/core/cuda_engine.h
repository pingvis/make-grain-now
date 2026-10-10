#pragma once
#define NOMINMAX
#include <Windows.h>
#include <cuda.h>
#include "texture.h"
#include <stdexcept>
#include <string>
#include <functional>

struct CudaApi {
 HMODULE dll;
#define API(name) decltype(&name) name = nullptr
 API(cuInit); API(cuDeviceGet); API(cuDevicePrimaryCtxRetain); API(cuDevicePrimaryCtxRelease);
 API(cuCtxPushCurrent); API(cuCtxPopCurrent); API(cuModuleLoadData); API(cuModuleUnload); API(cuModuleGetFunction);
 API(cuMemAlloc); API(cuMemFree); API(cuMemcpyHtoDAsync); API(cuMemcpyDtoHAsync);
 API(cuLaunchKernel); API(cuStreamSynchronize); API(cuStreamCreate); API(cuStreamDestroy);
 API(cuEventCreate); API(cuEventDestroy); API(cuEventRecord); API(cuEventElapsedTime);
#undef API
 CudaApi();
 CudaApi(const CudaApi&)=delete;CudaApi& operator=(const CudaApi&)=delete;
 ~CudaApi(){if(dll)FreeLibrary(dll);}
 void check(CUresult r,const char* action) {if(r!=CUDA_SUCCESS) throw std::runtime_error(std::string(action)+" CUDA error "+std::to_string((int)r));}
};
class CudaEngine {
 CudaApi& api;
 CUmodule module=nullptr; CUfunction extractFn,blurFn,composeFn,packFn,unpackFn,downFn;
 CUdeviceptr a=0,b=0,hal=0,bloom=0; size_t capacity=0;
 std::function<CUdeviceptr(size_t)> alloc;
 std::function<void(CUdeviceptr)> freeMem;
 CUdeviceptr nativeIn=0,nativeOut=0;size_t nativeCapacity=0;
 void launch(CUfunction fn,int w,int h,void** args,CUstream stream);
public:
 CudaEngine(CudaApi&,std::function<CUdeviceptr(size_t)> = {},std::function<void(CUdeviceptr)> = {});
 CudaEngine(const CudaEngine&)=delete;CudaEngine& operator=(const CudaEngine&)=delete;
 ~CudaEngine();
 void render(CUdeviceptr input,CUdeviceptr output,int w,int h,const TextureParams&,CUstream);
 void renderHost(CUdeviceptr input,CUdeviceptr output,int w,int h,int outW,int outH,int srcPitch,int dstPitch,int half,int premult,int cropX,int cropY,const TextureParams&,CUstream);
};
