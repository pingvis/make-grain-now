#pragma once
#include "texture.h"
enum {MGN_INPUT,MGN_PRESET,MGN_RANDOMIZE,MGN_GRAIN,MGN_SIZE,MGN_HALATION,MGN_BLOOM,MGN_ADVANCED,MGN_COLOR,MGN_HAL_RADIUS,MGN_BLOOM_RADIUS,MGN_THRESHOLD,MGN_SOFTNESS,MGN_SHADOWS,MGN_HIGHLIGHTS,MGN_SEED,MGN_SPACE,MGN_MODEL,MGN_ADVANCED_END,MGN_NUM_PARAMS};
struct CreativePreset {const char* name;float grain,size,color,halation,bloom,halRadius,bloomRadius;};
inline const CreativePreset creativePresets[]={
 {"Fine & gentle",1.2f,.8f,.08f,8,4,6,24},
 {"Soft color",2.5f,1.2f,.35f,15,8,8,32},
 {"Coarse monochrome",4.f,2.6f,0,10,3,7,24},
 {"Warm lights",2.f,1.f,.15f,32,12,12,40},
 {"Dream bloom",1.8f,1.3f,.2f,12,35,10,64},
 {"Gritty color",5.f,2.f,.5f,18,5,8,28}
};
template<class GetFloat,class GetInt> TextureParams readTextureParams(GetFloat f,GetInt i){
 TextureParams p;p.grain=f(MGN_GRAIN)*.01f;p.size=f(MGN_SIZE);p.halation=f(MGN_HALATION)*.01f;p.bloom=f(MGN_BLOOM)*.01f;p.color=f(MGN_COLOR)*.01f;
 p.halRadius=f(MGN_HAL_RADIUS);p.bloomRadius=f(MGN_BLOOM_RADIUS);p.threshold=f(MGN_THRESHOLD)*.01f;p.softness=f(MGN_SOFTNESS)*.01f;
 p.shadows=f(MGN_SHADOWS)*.01f;p.highlights=f(MGN_HIGHLIGHTS)*.01f;p.seed=(uint32_t)f(MGN_SEED);p.linear=i(MGN_SPACE)==2;p.particle=i(MGN_MODEL)==2;return p;
}
