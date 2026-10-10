#define NOMINMAX
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "PrSDKAESupport.h"
#include "parameters.h"
#include "cuda_engine.h"
#include <vector>
#include <algorithm>
#include <mutex>
#include <memory>
#include <cstdio>
#include <cmath>

template<class T> struct Suite {SPBasicSuite* basic;const char* name;long version;const T* p=nullptr;
 Suite(PF_InData* in,const char* n,long v):basic(in->pica_basicP),name(n),version(v){if(basic->AcquireSuite(n,v,(const void**)&p)||!p)throw std::runtime_error("Adobe suite unavailable");}
 ~Suite(){basic->ReleaseSuite(name,version);}const T* operator->(){return p;}
};
static PF_Err paramsSetup(PF_InData* in_data,PF_OutData* out){PF_ParamDef def{};
 PF_ADD_POPUPX("Look",customLook,2,lookNames,PF_ParamFlag_SUPERVISE,diskId(MGN_PRESET));
 PF_ADD_POPUPX("Grain type",2,2,"Gaussian (fast)|Simulated (slow)",0,diskId(MGN_MODEL));
 PF_ADD_FLOAT_SLIDERX("Overall strength",0,100,0,100,100,1,PF_ValueDisplayFlag_NONE,PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS,diskId(MGN_STRENGTH));
 PF_ADD_BUTTON("Explore","New look",0,PF_ParamFlag_SUPERVISE|PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_RANDOMIZE));
 PF_ADD_BUTTON("Variation","New variation",0,PF_ParamFlag_SUPERVISE|PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_VARIATION));
 PF_ADD_TOPICX("Advanced",PF_ParamFlag_START_COLLAPSED,diskId(MGN_ADVANCED));
#define SLIDER(label,id,max,defaultValue) PF_ADD_FLOAT_SLIDERX(label,0,max,0,max,defaultValue,1,PF_ValueDisplayFlag_NONE,PF_ParamFlag_SUPERVISE,diskId(id))
 PF_ADD_POPUPX("Grain preset",5,1,"Custom|Fine|Soft color|Dense color|Coarse monochrome",PF_ParamFlag_SUPERVISE,diskId(MGN_GRAIN_PRESET));
 SLIDER("Grain amount",MGN_GRAIN,15,2.5);PF_ADD_FLOAT_SLIDERX("Grain size",.25,8,.25,4,1.2,2,PF_ValueDisplayFlag_NONE,PF_ParamFlag_SUPERVISE,diskId(MGN_SIZE));
 SLIDER("Grain color",MGN_COLOR,100,35);
 PF_ADD_CHECKBOXX("Lock grain settings",FALSE,PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_LOCK_GRAIN));
 PF_ADD_POPUPX("Halation preset",5,1,"Custom|Off|Tight|Soft|Wide",PF_ParamFlag_SUPERVISE,diskId(MGN_HAL_PRESET));
 SLIDER("Halation amount",MGN_HALATION,100,15);SLIDER("Halation radius",MGN_HAL_RADIUS,80,8);
 PF_ADD_CHECKBOXX("Lock halation settings",FALSE,PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_LOCK_HAL));
 PF_ADD_POPUPX("Bloom preset",5,1,"Custom|Off|Gentle|Soft diffusion|Dreamy",PF_ParamFlag_SUPERVISE,diskId(MGN_BLOOM_PRESET));
 SLIDER("Bloom amount",MGN_BLOOM,100,8);SLIDER("Bloom radius",MGN_BLOOM_RADIUS,200,32);
 PF_ADD_CHECKBOXX("Lock bloom settings",FALSE,PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_LOCK_BLOOM));
 SLIDER("Highlight threshold",MGN_THRESHOLD,150,70);SLIDER("Highlight softness",MGN_SOFTNESS,100,20);
 SLIDER("Grain in shadows",MGN_SHADOWS,200,70);SLIDER("Grain in highlights",MGN_HIGHLIGHTS,200,35);
 PF_ADD_FLOAT_SLIDERX("Variation range",0,100,0,100,25,0,PF_ValueDisplayFlag_NONE,PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_VARIATION_AMOUNT));
 PF_ADD_BUTTON("Pattern","New grain pattern",0,PF_ParamFlag_SUPERVISE|PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_PATTERN));
 PF_ADD_FLOAT_SLIDERX("Grain seed",0,1000000,0,10000,731,0,PF_ValueDisplayFlag_NONE,PF_ParamFlag_CANNOT_INTERP,diskId(MGN_SEED));
 PF_ADD_FLOAT_SLIDERX("Explore seed",0,1000000,0,10000,7919,0,PF_ValueDisplayFlag_NONE,PF_ParamFlag_CANNOT_INTERP|PF_ParamFlag_CANNOT_TIME_VARY,diskId(MGN_EXPLORE_SEED));
 PF_ADD_POPUPX("Input encoding",2,1,"Rec.709 / display gamma|Linear RGB",0,diskId(MGN_SPACE));
 PF_END_TOPIC(diskId(MGN_ADVANCED_END));out->num_params=MGN_NUM_PARAMS;return PF_Err_NONE;
#undef SLIDER
}
static TextureParams readParams(PF_ParamDef* params[]){return readTextureParams([&](int id){return (float)params[id]->u.fs_d.value;},[&](int id){return params[id]->u.pd.value;});}
static void frameParams(PF_InData* in,TextureParams& p){p.frame=in->time_step?(uint32_t)(in->current_time/in->time_step):0;p.scale=(float)in->height/1080.f*in->downsample_y.num/in->downsample_y.den; p.scale=std::max(p.scale,.001f);}
static PF_Err changed(PF_InData*,PF_OutData* out,PF_ParamDef* params[],PF_UserChangedParamExtra* extra){
 const int changedIndex=extra->param_index;
 auto set=[&](int i,float value){params[i]->u.fs_d.value=value;params[i]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;};
 auto popup=[&](int i,int value){params[i]->u.pd.value=(short)value;params[i]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;};
 auto custom=[&](){popup(MGN_PRESET,customLook);};
 auto grain=[&](float amount,float size,float color,float shadows,float highlights){set(MGN_GRAIN,amount);set(MGN_SIZE,size);set(MGN_COLOR,color*100);set(MGN_SHADOWS,shadows*100);set(MGN_HIGHLIGHTS,highlights*100);popup(MGN_GRAIN_PRESET,1);};
 auto halation=[&](float amount,float radius){set(MGN_HALATION,amount);set(MGN_HAL_RADIUS,radius);popup(MGN_HAL_PRESET,1);};
 auto bloom=[&](float amount,float radius){set(MGN_BLOOM,amount);set(MGN_BLOOM_RADIUS,radius);popup(MGN_BLOOM_PRESET,1);};
 auto next=[](unsigned& state){state=state*1664525U+1013904223U;return state;};
 unsigned state=(unsigned)params[MGN_EXPLORE_SEED]->u.fs_d.value;
 const bool lockGrain=params[MGN_LOCK_GRAIN]->u.bd.value!=0,lockHal=params[MGN_LOCK_HAL]->u.bd.value!=0,lockBloom=params[MGN_LOCK_BLOOM]->u.bd.value!=0;
 if(changedIndex==MGN_PRESET||changedIndex==MGN_RANDOMIZE){
  int index=params[MGN_PRESET]->u.pd.value-1;
  const bool exploring=changedIndex==MGN_RANDOMIZE;
  if(exploring&&lockGrain&&lockHal&&lockBloom)return PF_Err_NONE;
  if(exploring){
   if(index>=0&&index<creativePresetCount)index=(index+1+next(state)%(creativePresetCount-1))%creativePresetCount;
   else index=next(state)%creativePresetCount;
   set(MGN_EXPLORE_SEED,(float)(state%1000000));
  }
  if(index<0||index>=creativePresetCount)return PF_Err_NONE;
  const auto& p=creativePresets[index];
  if(!exploring||!lockGrain)grain(p.grain,p.size,p.color,p.shadows,p.highlights);
  if(!exploring||!lockHal)halation(p.halation,p.halRadius);
  if(!exploring||!lockBloom)bloom(p.bloom,p.bloomRadius);
  // Extraction is shared by both glow modules. Preserve it if either is locked.
  if(!exploring||(!lockHal&&!lockBloom)){set(MGN_THRESHOLD,p.threshold*100);set(MGN_SOFTNESS,p.softness*100);}
  popup(MGN_PRESET,exploring&&(lockGrain||lockHal||lockBloom)?customLook:index+1);
  if(exploring&&!lockGrain){unsigned seed=(unsigned)params[MGN_SEED]->u.fs_d.value;set(MGN_SEED,(float)((seed+1+next(state)%999999)%1000000));set(MGN_EXPLORE_SEED,(float)(state%1000000));}
 }else if(changedIndex==MGN_VARIATION){
  const float range=std::clamp((float)params[MGN_VARIATION_AMOUNT]->u.fs_d.value*.01f,0.f,1.f);
  if(range==0|| (lockGrain&&lockHal&&lockBloom))return PF_Err_NONE;
  auto vary=[&](int i,float fraction,float minimum,float maximum){float u=(next(state)>>8)*(1.f/16777216.f);set(i,std::clamp((float)params[i]->u.fs_d.value*(1+(u*2-1)*fraction*range),minimum,maximum));};
  if(!lockGrain){vary(MGN_GRAIN,.35f,0,15);vary(MGN_SIZE,.25f,.25f,8);vary(MGN_COLOR,.25f,0,100);vary(MGN_SHADOWS,.2f,0,200);vary(MGN_HIGHLIGHTS,.2f,0,200);popup(MGN_GRAIN_PRESET,1);}
  if(!lockHal){vary(MGN_HALATION,.35f,0,100);vary(MGN_HAL_RADIUS,.25f,0,80);popup(MGN_HAL_PRESET,1);}
  if(!lockBloom){vary(MGN_BLOOM,.35f,0,100);vary(MGN_BLOOM_RADIUS,.25f,0,200);popup(MGN_BLOOM_PRESET,1);}
  if(!lockHal&&!lockBloom){vary(MGN_THRESHOLD,.08f,0,150);vary(MGN_SOFTNESS,.2f,0,100);}
  set(MGN_EXPLORE_SEED,(float)(state%1000000));custom();
 }else if(changedIndex==MGN_PATTERN){
  unsigned seed=(unsigned)params[MGN_SEED]->u.fs_d.value,stateSeed=seed;unsigned step=next(stateSeed)%999999;set(MGN_SEED,(float)((seed+1+step)%1000000));
 }else if(changedIndex==MGN_GRAIN_PRESET){
  int index=params[MGN_GRAIN_PRESET]->u.pd.value-2;
  if(index>=0&&index<4){auto p=grainPresets[index];grain(p.grain,p.size,p.color,p.shadows,p.highlights);popup(MGN_GRAIN_PRESET,index+2);}custom();
 }else if(changedIndex==MGN_HAL_PRESET||changedIndex==MGN_BLOOM_PRESET){
  int index=params[changedIndex]->u.pd.value-2;
  if(index>=0&&index<4){auto p=changedIndex==MGN_HAL_PRESET?halationPresets[index]:bloomPresets[index];if(changedIndex==MGN_HAL_PRESET)halation(p.amount,p.radius);else bloom(p.amount,p.radius);popup(changedIndex,index+2);}custom();
 }else{
  switch(changedIndex){
  case MGN_GRAIN:case MGN_SIZE:case MGN_COLOR:case MGN_SHADOWS:case MGN_HIGHLIGHTS:popup(MGN_GRAIN_PRESET,1);custom();break;
  case MGN_HALATION:case MGN_HAL_RADIUS:popup(MGN_HAL_PRESET,1);custom();break;
  case MGN_BLOOM:case MGN_BLOOM_RADIUS:popup(MGN_BLOOM_PRESET,1);custom();break;
  case MGN_THRESHOLD:case MGN_SOFTNESS:popup(MGN_HAL_PRESET,1);popup(MGN_BLOOM_PRESET,1);custom();break;
  default:return PF_Err_NONE;
  }
 }
 out->out_flags|=PF_OutFlag_REFRESH_UI|PF_OutFlag_FORCE_RERENDER;return PF_Err_NONE;
}
// CPU-visible host frames still execute CUDA, with a transfer cost. Native GPU selectors below avoid it.
static PF_Err renderWorld(PF_InData* in,PF_EffectWorld* src,PF_EffectWorld* dst,TextureParams p){
 int format=8;
 if(in->appl_id=='PrMr'){Suite<PF_PixelFormatSuite1> suite(in,kPFPixelFormatSuite,kPFPixelFormatSuiteVersion1);PrPixelFormat f;auto err=suite->GetPixelFormat(dst,&f);if(err)return err;format=f==PrPixelFormat_BGRA_4444_32f?132:f==PrPixelFormat_BGRA_4444_8u?108:0;}
 else {Suite<PF_WorldSuite2> suite(in,kPFWorldSuite,kPFWorldSuiteVersion2);PF_PixelFormat f;auto err=suite->PF_GetPixelFormat(src,&f);if(err)return err;format=f==PF_PixelFormat_ARGB128?32:f==PF_PixelFormat_ARGB64?16:8;}
 if(!format)return PF_Err_BAD_CALLBACK_PARAM;
 const bool pr=format>100,premult=!pr;const int depth=pr?format-100:format,w=src->width,h=src->height;
 if(!w||!h)return PF_Err_NONE;
 const int bytes=depth==32?16:depth==16?8:4;
 int cropX=dst->origin_x-src->origin_x,cropY=dst->origin_y-src->origin_y;
 const bool bypass=p.mix==0||(p.grain==0&&p.halation==0&&p.bloom==0);
 if(bypass){for(int y=0;y<dst->height;y++)for(int x=0;x<dst->width;x++){
  int sx=x+cropX,sy=y+cropY;char* q=(char*)dst->data+y*dst->rowbytes+x*bytes;
  if(sx<0||sy<0||sx>=w||sy>=h)memset(q,0,bytes);else memcpy(q,(char*)src->data+sy*src->rowbytes+sx*bytes,bytes);
 }return PF_Err_NONE;}
 std::vector<float> rgba((size_t)w*h*4),result(rgba.size());
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  float* v=&rgba[((size_t)y*w+x)*4];char* row=(char*)src->data+y*src->rowbytes;
  if(pr&&depth==32){const float* q=(float*)row+x*4;v[0]=q[2];v[1]=q[1];v[2]=q[0];v[3]=q[3];}
  else if(pr){const unsigned char* q=(unsigned char*)row+x*4;v[0]=q[2]/255.f;v[1]=q[1]/255.f;v[2]=q[0]/255.f;v[3]=q[3]/255.f;}
  else if(depth==32){auto q=((PF_PixelFloat*)row)[x];v[0]=q.red;v[1]=q.green;v[2]=q.blue;v[3]=q.alpha;}
  else if(depth==16){auto q=((PF_Pixel16*)row)[x];v[0]=q.red/32768.f;v[1]=q.green/32768.f;v[2]=q.blue/32768.f;v[3]=q.alpha/32768.f;}
  else {auto q=((PF_Pixel8*)row)[x];v[0]=q.red/255.f;v[1]=q.green/255.f;v[2]=q.blue/255.f;v[3]=q.alpha/255.f;}
  if(premult&&v[3]>0)for(int c=0;c<3;c++)v[c]/=v[3];
 }
 frameParams(in,p);p.originX=(float)src->origin_x;p.originY=(float)src->origin_y;
 if(ft_render(rgba.data(),result.data(),w,h,&p,nullptr))throw std::runtime_error(ft_error());
 for(int y=0;y<dst->height;y++)for(int x=0;x<dst->width;x++){
  int sx=x+cropX,sy=y+cropY;if(sx<0||sy<0||sx>=w||sy>=h){memset((char*)dst->data+y*dst->rowbytes+x*bytes,0,bytes);continue;}
  float* v=&result[((size_t)sy*w+sx)*4];char* row=(char*)dst->data+y*dst->rowbytes;
  if(v[3]<=0||bypass){memcpy(row+x*bytes,(char*)src->data+sy*src->rowbytes+sx*bytes,bytes);continue;}
  if(premult)for(int c=0;c<3;c++)v[c]*=v[3];
  auto quant=[](float f,int max){return (unsigned int)std::lround(std::clamp(f,0.f,1.f)*max);};
  if(pr&&depth==32){float* q=(float*)row+x*4;q[0]=v[2];q[1]=v[1];q[2]=v[0];q[3]=v[3];}
  else if(pr){auto q=(unsigned char*)row+x*4;q[0]=(unsigned char)quant(v[2],255);q[1]=(unsigned char)quant(v[1],255);q[2]=(unsigned char)quant(v[0],255);q[3]=((unsigned char*)src->data+sy*src->rowbytes+sx*4)[3];}
  else if(depth==32){auto& q=((PF_PixelFloat*)row)[x];q.red=v[0];q.green=v[1];q.blue=v[2];q.alpha=v[3];}
  else if(depth==16){auto& q=((PF_Pixel16*)row)[x];q.red=(A_u_short)quant(v[0],32768);q.green=(A_u_short)quant(v[1],32768);q.blue=(A_u_short)quant(v[2],32768);q.alpha=((PF_Pixel16*)((char*)src->data+sy*src->rowbytes))[sx].alpha;}
  else {auto& q=((PF_Pixel8*)row)[x];q.red=(A_u_char)quant(v[0],255);q.green=(A_u_char)quant(v[1],255);q.blue=(A_u_char)quant(v[2],255);q.alpha=((PF_Pixel8*)((char*)src->data+sy*src->rowbytes))[sx].alpha;}
 }
 return PF_Err_NONE;
}
struct PreRender {TextureParams p;};
static void deletePreRender(void* p){delete (PreRender*)p;}
struct ParameterCheckouts {
 PF_InData* in;std::vector<PF_ParamDef> values;std::vector<int> active;
 explicit ParameterCheckouts(PF_InData* i):in(i),values(MGN_NUM_PARAMS){active.reserve(MGN_NUM_PARAMS);}
 ~ParameterCheckouts(){checkin();}
 PF_Err checkout(int id){auto e=PF_CHECKOUT_PARAM(in,id,in->current_time,in->time_step,in->time_scale,&values[id]);if(!e)active.push_back(id);return e;}
 PF_Err checkin(){PF_Err first=PF_Err_NONE;for(int id:active){auto e=PF_CHECKIN_PARAM(in,&values[id]);if(!first)first=e;}active.clear();return first;}
};
static PF_Err preRender(PF_InData* in,PF_PreRenderExtra* extra){
 std::unique_ptr<PreRender> data(new PreRender);ParameterCheckouts values(in);std::vector<PF_ParamDef*> params(MGN_NUM_PARAMS);
 for(int i=1;i<MGN_NUM_PARAMS;i++){if(!isRenderParameter(i))continue;params[i]=&values.values[i];auto e=values.checkout(i);if(e)return e;}
 data->p=readParams(params.data());
 auto check=values.checkin();if(check)return check;
 auto req=extra->input->output_request;
 // Query upstream bounds first: preceding effects can expand or translate the source.
 // Request every color channel because highlight extraction needs full RGB even for single-channel output.
 req.rect={};req.channel_mask=PF_ChannelMask_ARGB;req.preserve_rgb_of_zero_alpha=TRUE;
 PF_CheckoutResult bounds{},result{};auto err=extra->cb->checkout_layer(in->effect_ref,0,0,&req,in->current_time,in->time_step,in->time_scale,&bounds);if(err)return err;
 req.rect=bounds.max_result_rect;
 err=extra->cb->checkout_layer(in->effect_ref,0,1,&req,in->current_time,in->time_step,in->time_scale,&result);if(err)return err;
 auto requested=extra->input->output_request.rect;
 auto available=result.result_rect;
 available.left=std::max(available.left,requested.left);available.top=std::max(available.top,requested.top);
 available.right=std::min(available.right,requested.right);available.bottom=std::min(available.bottom,requested.bottom);
 if(available.right<=available.left||available.bottom<=available.top)available={};
 extra->output->result_rect=available;extra->output->max_result_rect=result.max_result_rect;
 if(extra->input->what_gpu==PF_GPU_Framework_CUDA)extra->output->flags|=PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;
 extra->output->pre_render_data=data.release();extra->output->delete_pre_render_data_func=deletePreRender;return PF_Err_NONE;
}
struct GPUState {CudaApi api;CUcontext context;CUstream stream;std::unique_ptr<CudaEngine> engine;std::mutex lock;SPBasicSuite* basic=nullptr;PF_ProgPtr effectRef=nullptr;
 ~GPUState(){if(basic)basic->ReleaseSuite(kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1);}
};
struct ContextScope {CudaApi& api;ContextScope(CudaApi& a,CUcontext ctx):api(a){api.check(api.cuCtxPushCurrent(ctx),"Push Adobe context");}~ContextScope(){CUcontext ctx;api.cuCtxPopCurrent(&ctx);}};
static PF_Err gpuSetup(PF_InData* in,PF_OutData* out,PF_GPUDeviceSetupExtra* extra){
 out->out_flags2=0;if(extra->input->what_gpu!=PF_GPU_Framework_CUDA)return PF_Err_NONE;
 Suite<PF_GPUDeviceSuite1> suite(in,kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1);PF_GPUDeviceInfo info{};
 auto err=suite->GetDeviceInfo(in->effect_ref,extra->input->device_index,&info);if(err)return err;
 std::unique_ptr<GPUState> state(new GPUState);state->context=(CUcontext)info.contextPV;state->stream=(CUstream)info.command_queuePV;
 const PF_GPUDeviceSuite1* retained=nullptr;if(in->pica_basicP->AcquireSuite(kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1,(const void**)&retained))return PF_Err_INTERNAL_STRUCT_DAMAGED;state->basic=in->pica_basicP;
 ContextScope ctx(state->api,state->context);auto device=extra->input->device_index;const PF_GPUDeviceSuite1* s=suite.p;auto owner=state.get();owner->effectRef=in->effect_ref;
 // PF_ProgPtr belongs to the current selector. Never retain the setup reference for later allocations.
 state->engine.reset(new CudaEngine(state->api,[s,owner,device](size_t n){void* p=nullptr;if(s->AllocateDeviceMemory(owner->effectRef,device,n,&p))throw std::runtime_error("Adobe GPU allocation failed");return (CUdeviceptr)p;},[s,owner,device](CUdeviceptr p){s->FreeDeviceMemory(owner->effectRef,device,(void*)p);}));
 extra->output->gpu_data=state.release();out->out_flags2=PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;return PF_Err_NONE;
}
static PF_Err smartRender(PF_InData* in,PF_SmartRenderExtra* extra,bool gpu){
 PF_EffectWorld *src=nullptr,*dst=nullptr;auto err=extra->cb->checkout_layer_pixels(in->effect_ref,1,&src);if(err)return err;
 err=extra->cb->checkout_output(in->effect_ref,&dst);
 try {if(!err&&src&&dst){auto p=((PreRender*)extra->input->pre_render_data)->p;
  if(!gpu){err=renderWorld(in,src,dst,p);}
  else {GPUState* state=(GPUState*)extra->input->gpu_data;if(!state)throw std::runtime_error("CUDA device state missing");std::lock_guard<std::mutex> guard(state->lock);state->effectRef=in->effect_ref;ContextScope ctx(state->api,state->context);
   Suite<PF_GPUDeviceSuite1> suite(in,kPFGPUDeviceSuite,kPFGPUDeviceSuiteVersion1);void *a=nullptr,*b=nullptr;
   err=suite->GetGPUWorldData(in->effect_ref,src,&a);if(!err)err=suite->GetGPUWorldData(in->effect_ref,dst,&b);
   frameParams(in,p);p.originX=(float)src->origin_x;p.originY=(float)src->origin_y;
   if(!err){state->engine->renderHost((CUdeviceptr)a,(CUdeviceptr)b,src->width,src->height,dst->width,dst->height,src->rowbytes,dst->rowbytes,0,1,dst->origin_x-src->origin_x,dst->origin_y-src->origin_y,p,state->stream);state->api.check(state->api.cuStreamSynchronize(state->stream),"Finish Adobe render");}
  }
 }}catch(...){extra->cb->checkin_layer_pixels(in->effect_ref,1);throw;}
 auto check=extra->cb->checkin_layer_pixels(in->effect_ref,1);return err?err:check;
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr ptr,PF_PluginDataCB2 cb,SPBasicSuite*,const char*,const char*){
 PF_Err result=PF_Err_INVALID_CALLBACK;
 PF_REGISTER_EFFECT_EXT2(ptr,cb,"Make Grain Now","MGN Film Texture","Film Texture",AE_RESERVED_INFO,"EffectMain","");
 return result;
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd,PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_LayerDef* output,void* extra){
 try{switch(cmd){
 case PF_Cmd_ABOUT:snprintf(out->return_msg,sizeof(out->return_msg),"Make Grain Now 0.2\rPreset-driven CUDA grain, halation and bloom. Simulated grain uses experimental particles, not calibrated film stocks.");break;
 case PF_Cmd_GLOBAL_SETUP:
  out->my_version=PF_VERSION(1,0,0,PF_Stage_DEVELOP,2);out->out_flags=PF_OutFlag_DEEP_COLOR_AWARE|PF_OutFlag_NON_PARAM_VARY;
  out->out_flags2=PF_OutFlag2_FLOAT_COLOR_AWARE|PF_OutFlag2_SUPPORTS_SMART_RENDER|PF_OutFlag2_SUPPORTS_GPU_RENDER_F32|PF_OutFlag2_SUPPORTS_THREADED_RENDERING|PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;
  if(in->appl_id=='PrMr'){Suite<PF_PixelFormatSuite1> suite(in,kPFPixelFormatSuite,kPFPixelFormatSuiteVersion1);suite->ClearSupportedPixelFormats(in->effect_ref);suite->AddSupportedPixelFormat(in->effect_ref,PrPixelFormat_BGRA_4444_32f);suite->AddSupportedPixelFormat(in->effect_ref,PrPixelFormat_BGRA_4444_8u);}break;
 case PF_Cmd_PARAMS_SETUP:return paramsSetup(in,out);
 case PF_Cmd_USER_CHANGED_PARAM:return changed(in,out,params,(PF_UserChangedParamExtra*)extra);
 case PF_Cmd_RENDER:return renderWorld(in,&params[0]->u.ld,output,readParams(params));
 case PF_Cmd_SMART_PRE_RENDER:return preRender(in,(PF_PreRenderExtra*)extra);
 case PF_Cmd_SMART_RENDER:return smartRender(in,(PF_SmartRenderExtra*)extra,false);
 case PF_Cmd_SMART_RENDER_GPU:return smartRender(in,(PF_SmartRenderExtra*)extra,true);
 case PF_Cmd_GPU_DEVICE_SETUP:return gpuSetup(in,out,(PF_GPUDeviceSetupExtra*)extra);
 case PF_Cmd_GPU_DEVICE_SETDOWN:{auto e=(PF_GPUDeviceSetdownExtra*)extra;auto s=(GPUState*)e->input->gpu_data;if(s){ {std::lock_guard<std::mutex> guard(s->lock);s->effectRef=in->effect_ref;ContextScope ctx(s->api,s->context);s->engine.reset();}delete s;}break;}
 default:break;}return PF_Err_NONE;
 }catch(const std::exception& e){snprintf(out->return_msg,sizeof(out->return_msg),"Make Grain Now: %.210s",e.what());out->out_flags|=PF_OutFlag_DISPLAY_ERROR_MESSAGE;return PF_Err_INTERNAL_STRUCT_DAMAGED;}catch(...){return PF_Err_INTERNAL_STRUCT_DAMAGED;}
}
