#pragma once
#include "texture.h"
// Indices follow the UI. Persistent disk IDs never follow a UI rearrangement.
enum {
 MGN_INPUT,MGN_PRESET,MGN_MODEL,MGN_STRENGTH,MGN_RANDOMIZE,MGN_VARIATION,MGN_ADVANCED,
 MGN_GRAIN_PRESET,MGN_GRAIN,MGN_SIZE,MGN_COLOR,MGN_LOCK_GRAIN,
 MGN_HAL_PRESET,MGN_HALATION,MGN_HAL_RADIUS,MGN_LOCK_HAL,
 MGN_BLOOM_PRESET,MGN_BLOOM,MGN_BLOOM_RADIUS,MGN_LOCK_BLOOM,
 MGN_THRESHOLD,MGN_SOFTNESS,MGN_SHADOWS,MGN_HIGHLIGHTS,MGN_VARIATION_AMOUNT,
 MGN_PATTERN,MGN_SEED,MGN_EXPLORE_SEED,MGN_SPACE,MGN_ADVANCED_END,MGN_NUM_PARAMS
};
inline constexpr int parameterDiskIds[]={
 0,1,17,19,2,20,7,22,3,4,8,25,23,5,9,26,24,6,10,27,11,12,13,14,28,21,15,29,16,18
};
static_assert(sizeof(parameterDiskIds)/sizeof(parameterDiskIds[0])==MGN_NUM_PARAMS,"Parameter ID map mismatch");
inline constexpr int diskId(int index){return parameterDiskIds[index];}
inline constexpr bool isRenderParameter(int index){
 switch(index){
 case MGN_GRAIN:case MGN_SIZE:case MGN_COLOR:case MGN_HALATION:case MGN_HAL_RADIUS:
 case MGN_BLOOM:case MGN_BLOOM_RADIUS:case MGN_THRESHOLD:case MGN_SOFTNESS:
 case MGN_SHADOWS:case MGN_HIGHLIGHTS:case MGN_SEED:case MGN_SPACE:case MGN_MODEL:case MGN_STRENGTH:return true;
 default:return false;
 }
}
struct CreativePreset {
 const char* name;float grain,size,color,halation,bloom,halRadius,bloomRadius;
 float threshold=.7f,softness=.2f,shadows=.7f,highlights=.35f;
};
inline constexpr CreativePreset creativePresets[]={
 // Keep the original six choices and their seven style values in the same order.
 {"Fine & gentle",1.2f,.8f,.08f,8,4,6,24},
 {"Soft color",2.5f,1.2f,.35f,15,8,8,32},
 {"Coarse monochrome",4.f,2.6f,0,10,3,7,24},
 {"Warm lights",2.f,1.f,.15f,32,12,12,40},
 {"Dream bloom",1.8f,1.3f,.2f,12,35,10,64},
 {"Gritty color",5.f,2.f,.5f,18,5,8,28},
 {"Clean detail",.9f,.7f,.08f,3,2,4,20,.82f,.16f,.7f,.35f},
 {"Soft portrait",1.7f,1.1f,.18f,10,9,7,40,.78f,.25f,.7f,.35f},
 {"Night lights",2.8f,1.5f,.22f,28,10,10,44,.88f,.18f,.95f,.22f},
 {"Subtle warmth",1.5f,1.f,.12f,20,5,8,28,.75f,.2f,.7f,.35f},
 {"Dreamy monochrome",2.8f,1.8f,0,10,24,9,64,.74f,.3f,.8f,.25f},
 {"Heavy texture",6.f,2.8f,.25f,8,3,6,24,.8f,.16f,1.f,.2f}
};
inline constexpr int creativePresetCount=sizeof(creativePresets)/sizeof(creativePresets[0]);
inline constexpr int customLook=creativePresetCount+1;
inline constexpr const char* lookNames="Fine & gentle|Soft color|Coarse monochrome|Warm lights|Dream bloom|Gritty color|Clean detail|Soft portrait|Night lights|Subtle warmth|Dreamy monochrome|Heavy texture|Custom";
struct GrainPreset {float grain,size,color,shadows,highlights;};
inline constexpr GrainPreset grainPresets[]={
 {1.2f,.8f,.08f,.7f,.35f},{2.5f,1.2f,.35f,.7f,.35f},
 {3.4f,1.6f,.3f,.9f,.25f},{4.f,2.6f,0,.7f,.35f}
};
struct GlowPreset {float amount,radius;};
inline constexpr GlowPreset halationPresets[]={{0,8},{18,5},{15,9},{22,18}};
inline constexpr GlowPreset bloomPresets[]={{0,32},{4,24},{12,48},{35,80}};
template<class GetFloat,class GetInt> TextureParams readTextureParams(GetFloat f,GetInt i){
 TextureParams p;p.grain=f(MGN_GRAIN)*.01f;p.size=f(MGN_SIZE);p.halation=f(MGN_HALATION)*.01f;p.bloom=f(MGN_BLOOM)*.01f;p.color=f(MGN_COLOR)*.01f;
 p.halRadius=f(MGN_HAL_RADIUS);p.bloomRadius=f(MGN_BLOOM_RADIUS);p.threshold=f(MGN_THRESHOLD)*.01f;p.softness=f(MGN_SOFTNESS)*.01f;
 p.shadows=f(MGN_SHADOWS)*.01f;p.highlights=f(MGN_HIGHLIGHTS)*.01f;p.seed=(uint32_t)f(MGN_SEED);p.linear=i(MGN_SPACE)==2;p.particle=i(MGN_MODEL)==2;p.mix=f(MGN_STRENGTH)*.01f;return p;
}
