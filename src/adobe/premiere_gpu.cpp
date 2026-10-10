#define NOMINMAX
#include "PrSDKGPUFilter.h"
#include "PrSDKGPUDeviceSuite.h"
#include "PrSDKPPixSuite.h"
#include "PrSDKVideoSegmentSuite.h"
#include "SPBasic.h"
#include "parameters.h"
#include "cuda_engine.h"
#include <memory>
#include <mutex>

namespace {
struct PremiereGPU {
 SPBasicSuite* basic=nullptr;const PrSDKGPUDeviceSuite* device=nullptr;const PrSDKPPixSuite* ppix=nullptr;const PrSDKVideoSegmentSuite* segment=nullptr;
 csSDK_uint32 index=0;csSDK_int32 node=0;PrGPUDeviceInfo info{};CudaApi api;std::unique_ptr<CudaEngine> engine;std::mutex lock;
 template<class T> void acquire(const char* name,int version,const T** suite){if(basic->AcquireSuite(name,version,(const void**)suite)||!*suite)throw std::runtime_error("Premiere GPU suite unavailable");}
 void initialize(PrGPUFilterInstance* inst){
  basic=inst->piSuites->utilFuncs->getSPBasicSuite();index=inst->inDeviceIndex;node=inst->inNodeID;
  acquire(kPrSDKGPUDeviceSuite,kPrSDKGPUDeviceSuiteVersion,&device);acquire(kPrSDKPPixSuite,kPrSDKPPixSuiteVersion,&ppix);acquire(kPrSDKVideoSegmentSuite,kPrSDKVideoSegmentSuiteVersion,&segment);
  if(device->GetDeviceInfo(kPrSDKGPUDeviceSuiteVersion,index,&info)||info.outDeviceFramework!=PrGPUDeviceFramework_CUDA)throw std::runtime_error("CUDA Mercury renderer required");
  api.check(api.cuCtxPushCurrent((CUcontext)info.outContextHandle),"Premiere context");
  try {engine.reset(new CudaEngine(api,[this](size_t n){void* p=nullptr;if(device->AllocateDeviceMemory(index,n,&p))throw std::runtime_error("Premiere GPU memory allocation");return (CUdeviceptr)p;},[this](CUdeviceptr p){device->FreeDeviceMemory(index,(void*)p);}));}
  catch(...){CUcontext old;api.cuCtxPopCurrent(&old);throw;}CUcontext old;api.cuCtxPopCurrent(&old);
 }
 ~PremiereGPU(){if(engine){api.cuCtxPushCurrent((CUcontext)info.outContextHandle);engine.reset();CUcontext old;api.cuCtxPopCurrent(&old);}if(basic){if(segment)basic->ReleaseSuite(kPrSDKVideoSegmentSuite,kPrSDKVideoSegmentSuiteVersion);if(ppix)basic->ReleaseSuite(kPrSDKPPixSuite,kPrSDKPPixSuiteVersion);if(device)basic->ReleaseSuite(kPrSDKGPUDeviceSuite,kPrSDKGPUDeviceSuiteVersion);}}
 PrParam get(int id,PrTime t){PrParam p{};if(segment->GetParam(node,id-1,t,&p))throw std::runtime_error("Premiere parameter checkout");return p;}
 float number(int id,PrTime t){auto p=get(id,t);switch(p.mType){case kPrParamType_Float64:return (float)p.mFloat64;case kPrParamType_Float32:return p.mFloat32;case kPrParamType_Int32:return (float)p.mInt32;case kPrParamType_Int16:return (float)p.mInt16;case kPrParamType_Int8:return (float)p.mInt8;default:throw std::runtime_error("Unsupported parameter type");}}
 prSuiteError render(const PrGPUFilterRenderParams* r,const PPixHand* frames,csSDK_size_t count,PPixHand* output){
  if(!count||!frames)return suiteError_InvalidParms;
  std::lock_guard<std::mutex> guard(lock);api.check(api.cuCtxPushCurrent((CUcontext)info.outContextHandle),"Premiere render context");
  struct Pop{CudaApi& a;~Pop(){CUcontext ctx;a.cuCtxPopCurrent(&ctx);}} pop{api};
  PrPixelFormat format;prRect bounds{};csSDK_int32 srcPitch=0,dstPitch=0;void *a=nullptr,*b=nullptr;
  if(ppix->GetPixelFormat(frames[0],&format)||ppix->GetBounds(frames[0],&bounds))return suiteError_Fail;
  if(format!=PrPixelFormat_GPU_BGRA_4444_32f&&format!=PrPixelFormat_GPU_BGRA_4444_16f)return suiteError_NotImplemented;
  int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
  bool created=false;
  if(!*output){if(device->CreateGPUPPix(index,format,w,h,r->inRenderPARNum,r->inRenderPARDen,r->inRenderFieldType,output))return suiteError_Fail;created=true;}
  try {
   if(ppix->GetRowBytes(frames[0],&srcPitch)||ppix->GetRowBytes(*output,&dstPitch)||device->GetGPUPPixData(frames[0],&a)||device->GetGPUPPixData(*output,&b))throw std::runtime_error("Premiere GPU frame access");
   auto p=readTextureParams([&](int id){return number(id,r->inClipTime);},[&](int id){return (int)number(id,r->inClipTime);});
   p.frame=r->inRenderTicksPerFrame?(uint32_t)(r->inClipTime/r->inRenderTicksPerFrame):0;p.scale=h/1080.f;p.originX=(float)bounds.left;p.originY=(float)bounds.top;
   engine->renderHost((CUdeviceptr)a,(CUdeviceptr)b,w,h,w,h,srcPitch,dstPitch,format==PrPixelFormat_GPU_BGRA_4444_16f,0,0,0,p,(CUstream)info.outCommandQueueHandle);
   api.check(api.cuStreamSynchronize((CUstream)info.outCommandQueueHandle),"Finish Premiere frame");return suiteError_NoError;
  }catch(...){if(created){ppix->Dispose(*output);*output=nullptr;}throw;}
 }
};
prSuiteError create(PrGPUFilterInstance* inst){try{std::unique_ptr<PremiereGPU> state(new PremiereGPU);state->initialize(inst);inst->outIsRealtime=state->number(MGN_MODEL,0)!=2;inst->ioPrivatePluginData=state.release();return suiteError_NoError;}catch(...){return suiteError_Fail;}}
prSuiteError dispose(PrGPUFilterInstance* inst){delete (PremiereGPU*)inst->ioPrivatePluginData;inst->ioPrivatePluginData=nullptr;return suiteError_NoError;}
prSuiteError dependencies(PrGPUFilterInstance*,const PrGPUFilterRenderParams*,csSDK_int32*,PrGPUFilterFrameDependency*){return suiteError_NotImplemented;}
prSuiteError precompute(PrGPUFilterInstance*,const PrGPUFilterRenderParams*,csSDK_int32,PPixHand){return suiteError_NotImplemented;}
prSuiteError render(PrGPUFilterInstance* inst,const PrGPUFilterRenderParams* r,const PPixHand* frames,csSDK_size_t n,PPixHand* output){try{return ((PremiereGPU*)inst->ioPrivatePluginData)->render(r,frames,n,output);}catch(...){return suiteError_Fail;}}
}
extern "C" __declspec(dllexport) prSuiteError xGPUFilterEntry(csSDK_uint32 version,csSDK_int32* index,prBool startup,piSuitesPtr,PrGPUFilter* filter,PrGPUFilterInfo* info){
 if(!startup)return suiteError_NoError;if(version<PrSDKGPUFilterInterfaceVersion1||*index!=0)return suiteError_InvalidParms;
 *info={};info->outInterfaceVersion=PrSDKGPUFilterInterfaceVersion; // Null match name uses this module's PiPL.
 filter->CreateInstance=create;filter->DisposeInstance=dispose;filter->GetFrameDependencies=dependencies;filter->Precompute=precompute;filter->Render=render;return suiteError_NoError;
}
