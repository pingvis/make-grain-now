// Exercise the shipped effect entry point using SDK-shaped callbacks and actual CUDA.
// This is a mock adapter test, not proof that an Adobe host accepts or schedules it.
#define NOMINMAX
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "PrSDKGPUFilter.h"
#include "PrSDKGPUDeviceSuite.h"
#include "PrSDKPPixSuite.h"
#include "PrSDKVideoSegmentSuite.h"
#include "parameters.h"
#include "cuda_engine.h"
#include <vector>
#include <unordered_set>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <cstdlib>

using Entry=PF_Err(*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::vector<PF_ParamDef> definitions(1);
static int suiteRefs=0,checkedOut=0,checkedIn=0,failParam=-1,pixelCheckins=0;
static PF_PixelFormat pixelFormat=PF_PixelFormat_ARGB128;
static PF_WorldSuite2 worldSuite{};
static PF_GPUDeviceSuite1 gpuSuite{};
static PF_EffectWorld *sourceWorld=nullptr,*outputWorld=nullptr;
static CudaApi* api=nullptr;
static CUcontext context=nullptr;
static CUstream stream=nullptr;
static std::unordered_set<CUdeviceptr> allocations;
static bool expanded=false;
static PF_ProgPtr activeEffectRef=nullptr;
static decltype(&cuModuleUnload) realUnload=nullptr;
static int unloadedModules=0;
static CUresult CUDAAPI failFunction(CUfunction*,CUmodule,const char*){return CUDA_ERROR_NOT_FOUND;}
static CUresult CUDAAPI trackUnload(CUmodule m){++unloadedModules;return realUnload(m);}
static A_u_long metadata(HMODULE module,const char* key){
 auto resource=FindResourceA(module,MAKEINTRESOURCEA(16000),"PiPL");require(resource!=nullptr,"Missing PiPL");
 auto* data=(const unsigned char*)LockResource(LoadResource(module,resource));auto size=SizeofResource(module,resource);
 for(DWORD i=0;i+16<=size;i++)if(!memcmp(data+i,key,4)){A_u_long value;memcpy(&value,data+i+12,sizeof(value));return value;}
 throw std::runtime_error("PiPL numeric property missing");
}

static SPErr acquire(const char* name,int32,const void** p){
 if(!strcmp(name,kPFWorldSuite))*p=&worldSuite;
 else if(!strcmp(name,kPFGPUDeviceSuite))*p=&gpuSuite;
 else return -1;
 ++suiteRefs;return 0;
}
static SPErr release(const char*,int32){--suiteRefs;return 0;}
static PF_Err add(PF_ProgPtr,PF_ParamIndex,PF_ParamDefPtr p){definitions.push_back(*p);return 0;}
static PF_Err checkout(PF_ProgPtr,PF_ParamIndex i,A_long,A_long,A_u_long,PF_ParamDef* p){
 if(i==failParam)return PF_Err_BAD_CALLBACK_PARAM;
 *p=definitions.at(i);++checkedOut;return 0;
}
static PF_Err checkin(PF_ProgPtr,PF_ParamDef*){++checkedIn;return 0;}
static PF_Err layer(PF_ProgPtr,PF_ParamIndex,A_long id,const PF_RenderRequest* req,A_long,A_long,A_u_long,PF_CheckoutResult* result){
 PF_LRect expected{};expected.left=expanded?-11:0;expected.top=expanded?-7:0;expected.right=expanded?142:131;expected.bottom=expanded?100:93;
 require(req->channel_mask==PF_ChannelMask_ARGB,"Full RGB needed for highlight extraction");
 if(id==0){require(req->rect.right==0&&req->rect.bottom==0,"Bounds probe should request no pixels");result->max_result_rect=expected;}
 else {require(id==1&&memcmp(&req->rect,&expected,sizeof(expected))==0,"Full upstream bounds not requested");result->result_rect=result->max_result_rect=expected;}
 return 0;
}
static PF_Err inputPixels(PF_ProgPtr,A_long id,PF_EffectWorld** p){require(id==1,"Pixels checkout did not use its pre-render ID");*p=sourceWorld;return 0;}
static PF_Err outputPixels(PF_ProgPtr,PF_EffectWorld** p){*p=outputWorld;return 0;}
static PF_Err checkinPixels(PF_ProgPtr,A_long){++pixelCheckins;return 0;}
static PF_Err deviceInfo(PF_ProgPtr,A_u_long,PF_GPUDeviceInfo* p){p->device_framework=PF_GPU_Framework_CUDA;p->compatibleB=TRUE;p->contextPV=context;p->command_queuePV=stream;return 0;}
static PF_Err allocate(PF_ProgPtr ref,A_u_long,size_t n,void** p){require(ref==activeEffectRef,"GPU allocation used a stale selector effect reference");CUdeviceptr v=0;auto e=api->cuMemAlloc(&v,n);if(e!=CUDA_SUCCESS)return PF_Err_OUT_OF_MEMORY;allocations.insert(v);*p=(void*)v;return 0;}
static PF_Err freeDevice(PF_ProgPtr ref,A_u_long,void* p){require(ref==activeEffectRef,"GPU free used a stale selector effect reference");require(allocations.erase((CUdeviceptr)p)==1,"GPU allocation double free");return api->cuMemFree((CUdeviceptr)p)==CUDA_SUCCESS?0:PF_Err_INTERNAL_STRUCT_DAMAGED;}
static PF_Err gpuPixels(PF_ProgPtr,PF_EffectWorld* w,void** p){*p=w->data;return 0;}

template<class Pixel> static void integerSelectorTests(Entry entry,PF_InData in,PF_ParamDef** params,int maxValue,PF_PixelFormat format){
 const int w=77,h=53,stride=w+7;std::vector<Pixel> src(stride*h),dst(stride*h);
 in.width=w;in.height=h;pixelFormat=format;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto& p=src[y*stride+x];p.alpha=(decltype(p.alpha))(y<3?0:maxValue*.6);p.red=(decltype(p.red))(maxValue*.25);p.green=(decltype(p.green))(maxValue*.15);p.blue=(decltype(p.blue))(maxValue*.08);}
 PF_EffectWorld input{},output{};input.width=output.width=w;input.height=output.height=h;input.rowbytes=output.rowbytes=stride*sizeof(Pixel);input.data=(PF_PixelPtr)src.data();output.data=(PF_PixelPtr)dst.data();definitions[0].u.ld=input;
 PF_OutData out{};require(entry(PF_Cmd_RENDER,&in,&out,params,&output,nullptr)==0,out.return_msg);
 std::vector<float> rgba(w*h*4),reference(rgba.size());
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto v=src[y*stride+x];auto* p=&rgba[(y*w+x)*4];p[3]=v.alpha/(float)maxValue;p[0]=v.red/(float)maxValue;p[1]=v.green/(float)maxValue;p[2]=v.blue/(float)maxValue;if(p[3])for(int c=0;c<3;c++)p[c]/=p[3];}
 TextureParams p;p.scale=h/1080.f;require(ft_render(rgba.data(),reference.data(),w,h,&p,nullptr)==0,"Integer reference failed");
 auto quant=[maxValue](float f){return (int)std::lround(std::clamp(f,0.f,1.f)*maxValue);};
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto a=src[y*stride+x],b=dst[y*stride+x];require(a.alpha==b.alpha,"Integer selector changed alpha");
  if(!a.alpha)require(memcmp(&a,&b,sizeof(Pixel))==0,"Integer hidden RGB changed");
  else {auto* r=&reference[(y*w+x)*4];require(std::abs((int)b.red-quant(r[0]*r[3]))<=1&&std::abs((int)b.green-quant(r[1]*r[3]))<=1&&std::abs((int)b.blue-quant(r[2]*r[3]))<=1,"Integer selector differs from float reference");}}
 float amounts[3]={(float)definitions[MGN_GRAIN].u.fs_d.value,(float)definitions[MGN_HALATION].u.fs_d.value,(float)definitions[MGN_BLOOM].u.fs_d.value};
 definitions[MGN_GRAIN].u.fs_d.value=definitions[MGN_HALATION].u.fs_d.value=definitions[MGN_BLOOM].u.fs_d.value=0;
 output.origin_x=-3;output.origin_y=-2;output.width=w+6;output.height=h+4;output.rowbytes=(output.width+5)*sizeof(Pixel);std::vector<Pixel> cropped((output.width+5)*output.height);memset(cropped.data(),255,cropped.size()*sizeof(Pixel));output.data=(PF_PixelPtr)cropped.data();
 require(entry(PF_Cmd_RENDER,&in,&out,params,&output,nullptr)==0,"Integer cropped bypass failed");
 Pixel zero{};
 for(int y=0;y<output.height;y++)for(int x=0;x<output.width;x++){int sx=x-3,sy=y-2;const auto& expected=(sx<0||sy<0||sx>=w||sy>=h)?zero:src[sy*stride+sx];require(memcmp(&cropped[y*(output.width+5)+x],&expected,sizeof(Pixel))==0,"Integer bypass/crop differs from source bytes");}
 definitions[MGN_GRAIN].u.fs_d.value=amounts[0];definitions[MGN_HALATION].u.fs_d.value=amounts[1];definitions[MGN_BLOOM].u.fs_d.value=amounts[2];pixelFormat=PF_PixelFormat_ARGB128;
}

static PrSDKGPUDeviceSuite prDevice{};
static PrSDKPPixSuite prPPix{};
static PrSDKVideoSegmentSuite prSegment{};
static SPBasicSuite prBasic{};
static bool failPrParam=false,prDouble=false;
struct PrFrame {CUdeviceptr data=0;int w,h,pitch;PrPixelFormat format;};
static PrFrame* frame(PPixHand p){return reinterpret_cast<PrFrame*>(p);}
static SPErr prAcquire(const char* name,int32,const void** p){
 if(!strcmp(name,kPrSDKGPUDeviceSuite))*p=&prDevice;
 else if(!strcmp(name,kPrSDKPPixSuite))*p=&prPPix;
 else if(!strcmp(name,kPrSDKVideoSegmentSuite))*p=&prSegment;
 else return -1;
 ++suiteRefs;return 0;
}
static SPBasicSuite* getBasic(){return &prBasic;}
static prSuiteError prCreate(csSDK_uint32,PrPixelFormat format,int w,int h,int,int,prFieldType,PPixHand* out){
 auto p=new PrFrame;p->w=w;p->h=h;p->pitch=(w+5)*(format==PrPixelFormat_GPU_BGRA_4444_16f?8:16);p->format=format;
 if(api->cuMemAlloc(&p->data,(size_t)p->pitch*h)!=CUDA_SUCCESS){delete p;return suiteError_Fail;}*out=reinterpret_cast<PPixHand>(p);return 0;
}
static prSuiteError prDispose(PPixHand f){auto p=frame(f);api->cuMemFree(p->data);delete p;return 0;}
static prSuiteError prParam(csSDK_int32,csSDK_int32 i,PrTime,PrParam* p){
 if(failPrParam)return suiteError_Fail;
 const auto& d=definitions.at(i+1);
 if(d.param_type==PF_Param_POPUP){p->mType=kPrParamType_Int16;p->mInt16=(csSDK_int16)d.u.pd.value;}
 else if(prDouble){p->mType=kPrParamType_Float64;p->mFloat64=d.u.fs_d.value;}
 else {p->mType=kPrParamType_Float32;p->mFloat32=(float)d.u.fs_d.value;}return 0;
}
static void benchmarkPremiere(PrGPUFilter& filter,PrGPUFilterInstance& inst){
 const char* outputDir=std::getenv("MGN_OUTPUT_DIR");
 std::ofstream report(std::string(outputDir?outputDir:"tests/outputs")+"/native-gpu-benchmark.json");
 report<<"{\"scope\":\"Shipped Premiere CUDA adapter with mock SDK callbacks and GPU-resident frames; not Adobe playback, decoding or grading\",\"measurements\":[";
 bool first=true;
 for(int model:{1,2})for(int h:{1080,1440,2160}){
  definitions[MGN_MODEL].u.pd.value=model;int w=h*16/9;PPixHand input=nullptr,output=nullptr;
  require(prCreate(0,PrPixelFormat_GPU_BGRA_4444_32f,w,h,1,1,prFieldsNone,&input)==0,"Benchmark frame allocation failed");auto src=frame(input);
  std::vector<float> pixels((size_t)src->pitch*h/4,.5f);for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto* p=&pixels[y*src->pitch/4+x*4];p[3]=1;if(x>w/3&&x<w/2&&y>h/3&&y<h/2)p[0]=p[1]=p[2]=1.2f;}
  api->check(api->cuMemcpyHtoDAsync(src->data,pixels.data(),(size_t)src->pitch*h,stream),"Benchmark upload");
  PrGPUFilterRenderParams r{};r.inRenderWidth=w;r.inRenderHeight=h;r.inRenderPARNum=r.inRenderPARDen=1;r.inRenderTicksPerFrame=1;
  require(filter.Render(&inst,&r,&input,1,&output)==0,"Benchmark warmup failed");std::vector<double> times;
  for(int i=0;i<8;i++){r.inClipTime=i;auto start=std::chrono::steady_clock::now();require(filter.Render(&inst,&r,&input,1,&output)==0,"Benchmark render failed");times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
  std::sort(times.begin(),times.end());double median=(times[3]+times[4])*.5;
  if(!first)report<<',';first=false;report<<"{\"model\":\""<<(model==1?"fast":"particle")<<"\",\"width\":"<<w<<",\"height\":"<<h<<",\"adapter_median_ms\":"<<median<<",\"maximum_ms\":"<<times.back()<<'}';
  std::cout<<"Native mock CUDA adapter: "<<(model==1?"fast":"particle")<<' '<<w<<'x'<<h<<" median "<<median<<" ms (GPU resident, including pack/unpack and synchronization).\n";
  prDispose(output);prDispose(input);
 }definitions[MGN_MODEL].u.pd.value=1;report<<"]}";
}
static void premiereTests(HMODULE module){
 auto entry=reinterpret_cast<PrGPUFilterEntry>(GetProcAddress(module,"xGPUFilterEntry"));require(entry!=nullptr,"Missing native Premiere GPU entry");
 prBasic.AcquireSuite=prAcquire;prBasic.ReleaseSuite=release;
 prDevice.GetDeviceInfo=[](csSDK_uint32,csSDK_uint32,PrGPUDeviceInfo* p)->prSuiteError{p->outDeviceFramework=PrGPUDeviceFramework_CUDA;p->outContextHandle=context;p->outCommandQueueHandle=stream;return 0;};
 prDevice.AllocateDeviceMemory=[](csSDK_uint32,size_t n,void** p)->prSuiteError{return allocate(nullptr,0,n,p);};
 prDevice.FreeDeviceMemory=[](csSDK_uint32,void* p)->prSuiteError{return freeDevice(nullptr,0,p);};
 prDevice.CreateGPUPPix=prCreate;prDevice.GetGPUPPixData=[](PPixHand f,void** p)->prSuiteError{*p=(void*)frame(f)->data;return 0;};
 prPPix.Dispose=prDispose;prPPix.GetBounds=[](PPixHand f,prRect* p)->prSuiteError{*p={0,0,frame(f)->w,frame(f)->h};return 0;};
 prPPix.GetRowBytes=[](PPixHand f,csSDK_int32* p)->prSuiteError{*p=frame(f)->pitch;return 0;};
 prPPix.GetPixelFormat=[](PPixHand f,PrPixelFormat* p)->prSuiteError{*p=frame(f)->format;return 0;};prSegment.GetParam=prParam;
 PlugUtilFuncs utilities{};utilities.getSPBasicSuite=getBasic;piSuites suites{};suites.utilFuncs=&utilities;
 PrGPUFilter filter{};PrGPUFilterInfo info{};csSDK_int32 index=0;
 require(entry(PrSDKGPUFilterInterfaceVersion,&index,1,&suites,&filter,&info)==0,"Premiere GPU entry registration failed");
 require(info.outInterfaceVersion<=PrSDKGPUFilterInterfaceVersion,"Premiere GPU interface version mismatch");
 int baseline=suiteRefs;
 for(bool doubles:{false,true}){
  prDouble=doubles;PrGPUFilterInstance inst{};inst.piSuites=&suites;require(filter.CreateInstance(&inst)==0,"Premiere CUDA instance failed");
  PPixHand input=nullptr,output=nullptr;require(prCreate(0,PrPixelFormat_GPU_BGRA_4444_32f,131,93,1,1,prFieldsNone,&input)==0,"Premiere input allocation failed");
  auto f=frame(input);std::vector<float> src((size_t)f->pitch*93/4,0),rgba(131*93*4),reference(rgba.size());
  for(int y=0;y<93;y++)for(int x=0;x<131;x++){auto* q=&src[y*f->pitch/4+x*4];q[0]=.1f;q[1]=.25f;q[2]=.5f;q[3]=y<3?0:.7f;auto* r=&rgba[(y*131+x)*4];r[0]=q[2];r[1]=q[1];r[2]=q[0];r[3]=q[3];}
  api->check(api->cuMemcpyHtoDAsync(f->data,src.data(),(size_t)f->pitch*93,stream),"Premiere test upload");
  PrGPUFilterRenderParams r{};r.inRenderWidth=131;r.inRenderHeight=93;r.inRenderPARNum=r.inRenderPARDen=1;r.inRenderTicksPerFrame=1;r.inClipTime=1;
  require(filter.Render(&inst,&r,&input,1,&output)==0,"Premiere native render failed");
  auto dest=frame(output);std::vector<float> pixels((size_t)dest->pitch*93/4);api->check(api->cuMemcpyDtoHAsync(pixels.data(),dest->data,(size_t)dest->pitch*93,stream),"Premiere test download");api->check(api->cuStreamSynchronize(stream),"Premiere test finish");
  TextureParams p;p.scale=93/1080.f;p.frame=1;require(ft_render(rgba.data(),reference.data(),131,93,&p,nullptr)==0,"Premiere reference failed");
  for(int y=0;y<93;y++)for(int x=0;x<131;x++){auto* a=&pixels[y*dest->pitch/4+x*4];auto* b=&reference[(y*131+x)*4];require(std::abs(a[2]-b[0])<2e-6f&&std::abs(a[1]-b[1])<2e-6f&&std::abs(a[0]-b[2])<2e-6f&&a[3]==b[3],"Premiere selector differs from standalone reference");}
  prDispose(output);output=nullptr;failPrParam=true;require(filter.Render(&inst,&r,&input,1,&output)!=0&&output==nullptr,"Premiere error leaked newly allocated output");failPrParam=false;
  require(filter.Render(&inst,&r,&input,1,&output)==0,"Premiere did not recover after failed parameter checkout");prDispose(output);prDispose(input);
  if(!doubles)benchmarkPremiere(filter,inst);
  require(filter.DisposeInstance(&inst)==0&&inst.ioPrivatePluginData==nullptr,"Premiere instance teardown failed");require(suiteRefs==baseline&&allocations.empty(),"Premiere instance leaked suite or memory");
 }
 std::cout<<"Passed: shipped Premiere Mercury entry, float32/float64 parameters, popup indexing, real CUDA render parity, frame seed, alpha and error/teardown cleanup. Mock callbacks; NOT Premiere host verification.\n";
}

int main(int argc,char** argv){try{
 require(argc==2,"Pass the exact workspace .aex path");
 auto module=LoadLibraryA(argv[1]);require(module!=nullptr,"Cannot load workspace plugin");
 auto entry=(Entry)GetProcAddress(module,"EffectMain");require(entry!=nullptr,"Missing EffectMain");
 SPBasicSuite basic{};basic.AcquireSuite=acquire;basic.ReleaseSuite=release;
 worldSuite.PF_GetPixelFormat=[](const PF_EffectWorld*,PF_PixelFormat* p)->PF_Err{*p=pixelFormat;return 0;};
 PF_InData in{};in.appl_id='FXTC';in.pica_basicP=&basic;in.width=131;in.height=93;in.time_step=1;in.time_scale=50;in.downsample_x=in.downsample_y={1,1};
 in.inter.add_param=add;in.inter.checkout_param=checkout;in.inter.checkin_param=checkin;
 PF_OutData out{};
 require(entry(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr)==0,"Global setup failed");
 require((out.out_flags2&PF_OutFlag2_SUPPORTS_GPU_RENDER_F32)!=0,"CUDA support not advertised");
 require(metadata(module,"OLGe")==static_cast<A_u_long>(out.out_flags)&&metadata(module,"2LGe")==static_cast<A_u_long>(out.out_flags2)&&metadata(module,"REVe")==out.my_version,"PiPL flags/version disagree with native setup");
 require(entry(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr)==0,"Parameters setup failed");
 require(out.num_params==MGN_NUM_PARAMS&&definitions.size()==MGN_NUM_PARAMS,"Parameter count mismatch");
 std::vector<PF_ParamDef*> params;for(auto& d:definitions)params.push_back(&d);
 require(definitions[MGN_ADVANCED].flags&PF_ParamFlag_START_COLLAPSED,"Advanced group is not collapsed");
 PF_UserChangedParamExtra changed{};changed.param_index=MGN_RANDOMIZE;
 int initial=definitions[MGN_PRESET].u.pd.value;
 require(entry(PF_Cmd_USER_CHANGED_PARAM,&in,&out,params.data(),nullptr,&changed)==0,"Randomize failed");
 require(definitions[MGN_PRESET].u.pd.value!=initial,"Randomize did not change preset");
 require(definitions[MGN_SEED].uu.change_flags&PF_ChangeFlag_CHANGED_VALUE,"Random seed not marked for saving");
 definitions[MGN_GRAIN].u.fs_d.value=2.5;definitions[MGN_SIZE].u.fs_d.value=1.2;definitions[MGN_COLOR].u.fs_d.value=15;
 definitions[MGN_HALATION].u.fs_d.value=15;definitions[MGN_BLOOM].u.fs_d.value=8;definitions[MGN_HAL_RADIUS].u.fs_d.value=8;definitions[MGN_BLOOM_RADIUS].u.fs_d.value=32;definitions[MGN_SEED].u.fs_d.value=731;
 const int w=131,h=93,pitch=(w+7)*16;std::vector<PF_PixelFloat> src((w+7)*h),dst((w+7)*h);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto& v=src[y*(w+7)+x];v.alpha=y<3?0.f:.35f+(x%7)*.09f;v.red=.5f*v.alpha;v.green=.25f*v.alpha;v.blue=.1f*v.alpha;if(y==45&&x>50&&x<70)v.red=v.green=v.blue=1.5f*v.alpha;}
 PF_EffectWorld input{},output{};input.width=output.width=w;input.height=output.height=h;input.rowbytes=output.rowbytes=pitch;input.data=(PF_PixelPtr)src.data();output.data=(PF_PixelPtr)dst.data();definitions[0].u.ld=input;
 require(entry(PF_Cmd_RENDER,&in,&out,params.data(),&output,nullptr)==0,out.return_msg);
 require(suiteRefs==0,"CPU render leaked suite");
 for(int y=0;y<h;y++)for(int x=0;x<w;x++)require(src[y*(w+7)+x].alpha==dst[y*(w+7)+x].alpha,"CPU selector changed alpha");
 std::vector<float> rgba(w*h*4),reference(w*h*4);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto v=src[y*(w+7)+x];auto* q=&rgba[(y*w+x)*4];q[0]=v.alpha?v.red/v.alpha:v.red;q[1]=v.alpha?v.green/v.alpha:v.green;q[2]=v.alpha?v.blue/v.alpha:v.blue;q[3]=v.alpha;}
 TextureParams p;p.scale=h/1080.f;require(ft_render(rgba.data(),reference.data(),w,h,&p,nullptr)==0,"Reference CUDA failed");
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto v=dst[y*(w+7)+x];auto* q=&reference[(y*w+x)*4];require(std::abs(v.red-q[0]*v.alpha)<2e-6f&&std::abs(v.green-q[1]*v.alpha)<2e-6f&&std::abs(v.blue-q[2]*v.alpha)<2e-6f,"CPU selector differs from core");}
 PF_PreRenderInput preIn{};PF_PreRenderOutput preOut{};PF_PreRenderCallbacks preCB{};preCB.checkout_layer=layer;preIn.what_gpu=PF_GPU_Framework_CUDA;
 preIn.output_request.rect={0,0,w,h};PF_PreRenderExtra pre{&preIn,&preOut,&preCB};
 require(entry(PF_Cmd_SMART_PRE_RENDER,&in,&out,nullptr,nullptr,&pre)==0,"Pre-render failed");
 require(checkedIn==checkedOut,"Pre-render leaked successful parameter checkouts");
 // Source bounds can exceed the original layer; requested output must still honor ROI.
 {expanded=true;PF_PreRenderOutput roiOut{};preIn.output_request.rect={11,7,86,62};PF_PreRenderExtra roi{&preIn,&roiOut,&preCB};
  require(entry(PF_Cmd_SMART_PRE_RENDER,&in,&out,nullptr,nullptr,&roi)==0,"Expanded upstream pre-render failed");
  require(memcmp(&roiOut.result_rect,&preIn.output_request.rect,sizeof(PF_LRect))==0&&roiOut.max_result_rect.left==-11&&roiOut.max_result_rect.right==142,"SmartFX ROI/bounds contract failed");
  roiOut.delete_pre_render_data_func(roiOut.pre_render_data);expanded=false;preIn.output_request.rect={0,0,w,h};}
 CudaApi cuda;api=&cuda;CUdevice device=0;cuda.check(cuda.cuDeviceGet(&device,0),"test device");cuda.check(cuda.cuDevicePrimaryCtxRetain(&context,device),"test context");cuda.check(cuda.cuCtxPushCurrent(context),"test push");cuda.check(cuda.cuStreamCreate(&stream,CU_STREAM_NON_BLOCKING),"test stream");
 gpuSuite.GetDeviceInfo=deviceInfo;gpuSuite.AllocateDeviceMemory=allocate;gpuSuite.FreeDeviceMemory=freeDevice;gpuSuite.GetGPUWorldData=gpuPixels;
 PF_GPUDeviceSetupInput setupIn{};setupIn.what_gpu=PF_GPU_Framework_CUDA;PF_GPUDeviceSetupOutput setupOut{};PF_GPUDeviceSetupExtra setup{&setupIn,&setupOut};
 in.effect_ref=activeEffectRef=reinterpret_cast<PF_ProgPtr>(1);
 require(entry(PF_Cmd_GPU_DEVICE_SETUP,&in,&out,nullptr,nullptr,&setup)==0,"GPU selector setup failed");
 CUdeviceptr a=0,b=0;cuda.check(cuda.cuMemAlloc(&a,pitch*h),"test source");cuda.check(cuda.cuMemAlloc(&b,pitch*h),"test destination");std::vector<float> bgra((w+7)*h*4),gpuResult(bgra.size());
 for(size_t i=0;i<src.size();i++){bgra[i*4]=src[i].blue;bgra[i*4+1]=src[i].green;bgra[i*4+2]=src[i].red;bgra[i*4+3]=src[i].alpha;}
 cuda.check(cuda.cuMemcpyHtoDAsync(a,bgra.data(),pitch*h,stream),"test upload");input.data=(PF_PixelPtr)a;output.data=(PF_PixelPtr)b;sourceWorld=&input;outputWorld=&output;
 PF_SmartRenderInput smartIn{};smartIn.pre_render_data=preOut.pre_render_data;smartIn.gpu_data=setupOut.gpu_data;PF_SmartRenderCallbacks smartCB{};smartCB.checkout_layer_pixels=inputPixels;smartCB.checkout_output=outputPixels;smartCB.checkin_layer_pixels=checkinPixels;PF_SmartRenderExtra smart{&smartIn,&smartCB};
 in.effect_ref=activeEffectRef=reinterpret_cast<PF_ProgPtr>(2);
 require(entry(PF_Cmd_SMART_RENDER_GPU,&in,&out,nullptr,nullptr,&smart)==0,out.return_msg);
 cuda.check(cuda.cuMemcpyDtoHAsync(gpuResult.data(),b,pitch*h,stream),"test download");cuda.check(cuda.cuStreamSynchronize(stream),"test finish");
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto v=dst[y*(w+7)+x];auto* q=&gpuResult[(y*(w+7)+x)*4];require(std::abs(v.red-q[2])<2e-6f&&std::abs(v.green-q[1])<2e-6f&&std::abs(v.blue-q[0])<2e-6f&&v.alpha==q[3],"Native GPU selector differs from CPU selector");}
 require(pixelCheckins==1,"Smart render did not check in input");
 in.effect_ref=activeEffectRef=reinterpret_cast<PF_ProgPtr>(3);
 PF_GPUDeviceSetdownInput downIn{};downIn.gpu_data=setupOut.gpu_data;PF_GPUDeviceSetdownExtra down{&downIn};require(entry(PF_Cmd_GPU_DEVICE_SETDOWN,&in,&out,nullptr,nullptr,&down)==0,"GPU teardown failed");require(allocations.empty()&&suiteRefs==0,"GPU teardown leaked host memory or suites");
 in.effect_ref=activeEffectRef=nullptr;
 preOut.delete_pre_render_data_func(preOut.pre_render_data);
 premiereTests(module);
 {auto get=cuda.cuModuleGetFunction;auto unload=cuda.cuModuleUnload;realUnload=unload;cuda.cuModuleGetFunction=failFunction;cuda.cuModuleUnload=trackUnload;
  bool failed=false;try{CudaEngine test(cuda);}catch(const std::exception&){failed=true;}
  cuda.cuModuleGetFunction=get;cuda.cuModuleUnload=unload;require(failed&&unloadedModules==1,"Failed kernel resolution leaked its CUDA module");}
 cuda.cuMemFree(a);cuda.cuMemFree(b);cuda.cuStreamDestroy(stream);CUcontext old;cuda.cuCtxPopCurrent(&old);cuda.cuDevicePrimaryCtxRelease(device);
 integerSelectorTests<PF_Pixel8>(entry,in,params.data(),255,PF_PixelFormat_ARGB32);
 integerSelectorTests<PF_Pixel16>(entry,in,params.data(),32768,PF_PixelFormat_ARGB64);
 failParam=MGN_HALATION;checkedIn=checkedOut=0;preOut={};
 require(entry(PF_Cmd_SMART_PRE_RENDER,&in,&out,nullptr,nullptr,&pre)!=0,"Injected checkout error was swallowed");
 require(checkedIn==checkedOut,"Failed pre-render leaked checked-out parameters");
 FreeLibrary(module);
 std::cout<<"Passed: shipped AE selectors, PiPL flag/version consistency, controls, randomization flags, 8/16/32-bit CPU rendering, CPU/CUDA parity, alpha, cropped bypass, expanded upstream bounds/ROI, suite/allocation cleanup and injected checkout/kernel-resolution failures. Mock SDK callbacks; NOT Adobe host verification.\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
