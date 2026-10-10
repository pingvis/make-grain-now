#include "cuda_engine.h"
#include "kernel_source.h"

CudaApi::CudaApi() {
 dll=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32); if(!dll)throw std::runtime_error("NVIDIA CUDA driver unavailable");
 try {
#define LOAD(name,entry) name=reinterpret_cast<decltype(name)>(GetProcAddress(dll,entry)); if(!name)throw std::runtime_error("Missing " entry)
 LOAD(cuInit,"cuInit");LOAD(cuDeviceGet,"cuDeviceGet");LOAD(cuDevicePrimaryCtxRetain,"cuDevicePrimaryCtxRetain");LOAD(cuDevicePrimaryCtxRelease,"cuDevicePrimaryCtxRelease_v2");
 LOAD(cuCtxPushCurrent,"cuCtxPushCurrent_v2");LOAD(cuCtxPopCurrent,"cuCtxPopCurrent_v2");LOAD(cuModuleLoadData,"cuModuleLoadData");LOAD(cuModuleUnload,"cuModuleUnload");LOAD(cuModuleGetFunction,"cuModuleGetFunction");
 LOAD(cuMemAlloc,"cuMemAlloc_v2");LOAD(cuMemFree,"cuMemFree_v2");LOAD(cuMemcpyHtoDAsync,"cuMemcpyHtoDAsync_v2");LOAD(cuMemcpyDtoHAsync,"cuMemcpyDtoHAsync_v2");
 LOAD(cuLaunchKernel,"cuLaunchKernel");LOAD(cuStreamSynchronize,"cuStreamSynchronize");LOAD(cuStreamCreate,"cuStreamCreate");LOAD(cuStreamDestroy,"cuStreamDestroy_v2");
 LOAD(cuEventCreate,"cuEventCreate");LOAD(cuEventDestroy,"cuEventDestroy_v2");LOAD(cuEventRecord,"cuEventRecord");LOAD(cuEventElapsedTime,"cuEventElapsedTime");
#undef LOAD
 check(cuInit(0),"Initialize");
 }catch(...){FreeLibrary(dll);dll=nullptr;throw;}
}
CudaEngine::CudaEngine(CudaApi& api_,std::function<CUdeviceptr(size_t)> al,std::function<void(CUdeviceptr)> fr):api(api_),alloc(al),freeMem(fr) {
 if(!alloc)alloc=[this](size_t n){CUdeviceptr p;api.check(api.cuMemAlloc(&p,n),"Allocate");return p;};
 if(!freeMem)freeMem=[this](CUdeviceptr p){api.cuMemFree(p);};
 api.check(api.cuModuleLoadData(&module,texture_ptx),"Load kernels");
 try {
 api.check(api.cuModuleGetFunction(&extractFn,module,"extract"),"Find extract");
 api.check(api.cuModuleGetFunction(&blurFn,module,"blur"),"Find blur");
 api.check(api.cuModuleGetFunction(&composeFn,module,"compose"),"Find compose");
 api.check(api.cuModuleGetFunction(&packFn,module,"pack"),"Find pack");
 api.check(api.cuModuleGetFunction(&unpackFn,module,"unpack"),"Find unpack");
 api.check(api.cuModuleGetFunction(&downFn,module,"down"),"Find downsample");
 }catch(...){api.cuModuleUnload(module);module=nullptr;throw;}
}
CudaEngine::~CudaEngine(){for(auto p:{a,b,hal,bloom,nativeIn,nativeOut})if(p)freeMem(p);if(module)api.cuModuleUnload(module);}
void CudaEngine::launch(CUfunction fn,int w,int h,void** args,CUstream stream){api.check(api.cuLaunchKernel(fn,(w+15)/16,(h+15)/16,1,16,16,1,0,stream,args,nullptr),"Launch");}
void CudaEngine::render(CUdeviceptr input,CUdeviceptr output,int w,int h,const TextureParams& param,CUstream stream) {
 int qw=(w+3)/4,qh=(h+3)/4; size_t n=(size_t)qw*qh*16;
 if(n>capacity) {for(auto p:{a,b,hal,bloom})if(p)freeMem(p);a=b=hal=bloom=0;capacity=0;a=alloc(n);b=alloc(n);hal=alloc(n);bloom=alloc(n);capacity=n;}
 TextureParams p=param;
 int gw[2]={qw,qw},gh[2]={qh,qh},factors[2]={1,1};
 if(p.mix!=0&&(p.halation!=0||p.bloom!=0)){
  void* e[]={&input,&a,&w,&h,&qw,&qh,&p};launch(extractFn,qw,qh,e,stream);
  for(int type=0;type<2;type++) {if((type?p.bloom:p.halation)==0)continue;float radius=(type?p.bloomRadius:p.halRadius)*p.scale/4.f;int vertical=0;CUdeviceptr dest=type?bloom:hal;
   while(radius/factors[type]>2.5f)factors[type]*=2;
   gw[type]=(qw+factors[type]-1)/factors[type];gh[type]=(qh+factors[type]-1)/factors[type];radius/=factors[type];
   CUdeviceptr source=a;
   if(factors[type]>1){void* d[]={&a,&dest,&qw,&qh,&gw[type],&gh[type],&factors[type]};launch(downFn,gw[type],gh[type],d,stream);source=dest;}
   void* bx[]={&source,&b,&gw[type],&gh[type],&radius,&vertical};launch(blurFn,gw[type],gh[type],bx,stream);
   vertical=1;void* by[]={&b,&dest,&gw[type],&gh[type],&radius,&vertical};launch(blurFn,gw[type],gh[type],by,stream);
  }
 }
 void* c[]={&input,&output,&hal,&bloom,&w,&h,&gw[0],&gh[0],&gw[1],&gh[1],&factors[0],&factors[1],&p};launch(composeFn,w,h,c,stream);
}
void CudaEngine::renderHost(CUdeviceptr input,CUdeviceptr output,int w,int h,int outW,int outH,int srcPitch,int dstPitch,int half,int premult,int cropX,int cropY,const TextureParams& p,CUstream stream){
 int bypass=p.mix==0||(p.grain==0&&p.halation==0&&p.bloom==0);
 if(bypass){void* args[]={&input,&input,&output,&w,&h,&outW,&outH,&srcPitch,&dstPitch,&half,&premult,&cropX,&cropY,&bypass};launch(unpackFn,outW,outH,args,stream);return;}
 size_t n=(size_t)w*h*16;
 if(n>nativeCapacity){for(auto q:{nativeIn,nativeOut})if(q)freeMem(q);nativeIn=nativeOut=0;nativeCapacity=0;nativeIn=alloc(n);nativeOut=alloc(n);nativeCapacity=n;}
 void* pk[]={&input,&nativeIn,&w,&h,&srcPitch,&half,&premult};launch(packFn,w,h,pk,stream);
 render(nativeIn,nativeOut,w,h,p,stream);
 void* up[]={&nativeOut,&input,&output,&w,&h,&outW,&outH,&srcPitch,&dstPitch,&half,&premult,&cropX,&cropY,&bypass};launch(unpackFn,outW,outH,up,stream);
}
static thread_local std::string error;
struct Standalone {
 CudaApi api;CUdevice device=0;CUcontext context=nullptr;CUstream stream=nullptr;CUevent start=nullptr,stop=nullptr;
 CudaEngine* engine=nullptr;CUdeviceptr input=0,output=0;size_t capacity=0;
 Standalone(){api.check(api.cuDeviceGet(&device,0),"Device");api.check(api.cuDevicePrimaryCtxRetain(&context,device),"Context");api.check(api.cuCtxPushCurrent(context),"Push");api.check(api.cuStreamCreate(&stream,CU_STREAM_NON_BLOCKING),"Stream");api.check(api.cuEventCreate(&start,0),"Event");api.check(api.cuEventCreate(&stop,0),"Event");engine=new CudaEngine(api);CUcontext old;api.cuCtxPopCurrent(&old);}
 ~Standalone(){if(context){api.cuCtxPushCurrent(context);delete engine;if(input)api.cuMemFree(input);if(output)api.cuMemFree(output);api.cuEventDestroy(start);api.cuEventDestroy(stop);api.cuStreamDestroy(stream);CUcontext old;api.cuCtxPopCurrent(&old);api.cuDevicePrimaryCtxRelease(device);}}
};
FT_API int ft_render(const float* input,float* output,int w,int h,const TextureParams* params,float* ms) {
 try {
  if(!input||!output||!params||w<=0||h<=0||w>32768||h>32768)throw std::runtime_error("Invalid render input");
  static thread_local Standalone gpu;
  gpu.api.check(gpu.api.cuCtxPushCurrent(gpu.context),"Push render");
  struct Pop {CudaApi& a;~Pop(){CUcontext old;a.cuCtxPopCurrent(&old);}} pop{gpu.api};
  size_t bytes=(size_t)w*h*16;
  if(bytes>gpu.capacity){if(gpu.input)gpu.api.cuMemFree(gpu.input);if(gpu.output)gpu.api.cuMemFree(gpu.output);gpu.input=gpu.output=0;gpu.capacity=0;gpu.api.check(gpu.api.cuMemAlloc(&gpu.input,bytes),"Input");gpu.api.check(gpu.api.cuMemAlloc(&gpu.output,bytes),"Output");gpu.capacity=bytes;}
  gpu.api.check(gpu.api.cuMemcpyHtoDAsync(gpu.input,input,bytes,gpu.stream),"Upload");
  gpu.api.check(gpu.api.cuEventRecord(gpu.start,gpu.stream),"Start");
  gpu.engine->render(gpu.input,gpu.output,w,h,*params,gpu.stream);
  gpu.api.check(gpu.api.cuEventRecord(gpu.stop,gpu.stream),"Stop");
  gpu.api.check(gpu.api.cuMemcpyDtoHAsync(output,gpu.output,bytes,gpu.stream),"Download");
  gpu.api.check(gpu.api.cuStreamSynchronize(gpu.stream),"Finish");if(ms)gpu.api.check(gpu.api.cuEventElapsedTime(ms,gpu.start,gpu.stop),"Timing");
  error.clear();return 0;
 }catch(const std::exception& e){error=e.what();return 1;}catch(...){error="Unknown CUDA error";return 1;}
}
FT_API const char* ft_error(){return error.c_str();}
// Test the same pitched BGRA packing/cropping path used by the native Adobe selectors.
FT_API int ft_test_host(const void* input,void* output,int w,int h,int outW,int outH,int srcPitch,int dstPitch,int half,int premult,int cropX,int cropY,const TextureParams* p){
 try {Standalone gpu;gpu.api.check(gpu.api.cuCtxPushCurrent(gpu.context),"Test context");
  struct Pop {CudaApi& a;~Pop(){CUcontext old;a.cuCtxPopCurrent(&old);}} pop{gpu.api};
  gpu.api.check(gpu.api.cuMemAlloc(&gpu.input,(size_t)srcPitch*h),"Pitched input");gpu.api.check(gpu.api.cuMemAlloc(&gpu.output,(size_t)dstPitch*outH),"Pitched output");
  gpu.api.check(gpu.api.cuMemcpyHtoDAsync(gpu.input,input,(size_t)srcPitch*h,gpu.stream),"Pitched upload");
  gpu.engine->renderHost(gpu.input,gpu.output,w,h,outW,outH,srcPitch,dstPitch,half,premult,cropX,cropY,*p,gpu.stream);
  gpu.api.check(gpu.api.cuMemcpyDtoHAsync(output,gpu.output,(size_t)dstPitch*outH,gpu.stream),"Pitched download");gpu.api.check(gpu.api.cuStreamSynchronize(gpu.stream),"Pitched finish");error.clear();return 0;
 }catch(const std::exception& e){error=e.what();return 1;}
}
