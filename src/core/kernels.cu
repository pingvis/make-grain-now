// Original implementation. Fast grain is an artistic approximation, not calibrated stock grain.
struct Params {
 float grain,size,color,halation,halRadius,bloom,bloomRadius,threshold,softness;
 float shadows,highlights,scale,originX,originY; unsigned seed,frame; int linear,particle;float mix;
};
__device__ unsigned hash(unsigned x) { x ^= x>>16; x*=0x7feb352dU; x^=x>>15; x*=0x846ca68bU; return x^(x>>16); }
__device__ float rnd(unsigned s) { return (hash(s)>>8)*(1.f/16777216.f); }
__device__ float normal(unsigned s) { return sqrtf(-2.f*logf(fmaxf(rnd(s),1.e-7f)))*cosf(6.2831853f*rnd(s^0xa51cd321U)); }
__device__ unsigned spatial(int x,int y,unsigned s) { return hash((unsigned)x*0x9e3779b9U ^ (unsigned)y*0x85ebca6bU ^ s); }
__device__ float fastNoise(float x,float y,unsigned s) {
 int ix=(int)floorf(x),iy=(int)floorf(y); float fx=x-ix,fy=y-iy;
 // Smooth interpolation and variance compensation avoid grid lines and brightness pumping.
 fx=fx*fx*(3.f-2.f*fx); fy=fy*fy*(3.f-2.f*fy);
 float a=(1-fx)*(1-fy),b=fx*(1-fy),c=(1-fx)*fy,d=fx*fy;
 return (a*normal(spatial(ix,iy,s))+b*normal(spatial(ix+1,iy,s))+
 c*normal(spatial(ix,iy+1,s))+d*normal(spatial(ix+1,iy+1,s)))*rsqrtf(a*a+b*b+c*c+d*d);
}
__device__ float particleNoise(float x,float y,float density,float radius,unsigned seed) {
 // Boolean coverage by Poisson-distributed discs, optical filtering by sample integration.
 // Bounded equal radii and eight samples are a research preview, not the full Newson renderer.
 float cell=2.f*radius,lambda=-logf(1.f-density)*4.f/3.14159265f;
 float result=0;
 for(int sample=0;sample<8;sample++) {
   unsigned jitter=spatial((int)floorf(x),(int)floorf(y),seed^((sample+1)*0x618da7U));
   float px=x+.35f*normal(jitter),py=y+.35f*normal(jitter^0x98123U);
   int cx=(int)floorf(px/cell),cy=(int)floorf(py/cell); bool covered=false;
   float localX=px-cx*cell,localY=py-cy*cell;
   float reach=radius+2.e-7f*(fabsf(px)+fabsf(py)+cell+1.f);
   for(int oy=-1;oy<=1&&!covered;oy++) for(int ox=-1;ox<=1&&!covered;ox++) {
     // A disc center anywhere in this cell must still reach the optical sample.
     // Keep a small conservative margin for float rounding at cell boundaries.
     float gapX=ox<0?localX:ox>0?cell-localX:0.f;
     float gapY=oy<0?localY:oy>0?cell-localY:0.f;
     if(gapX*gapX+gapY*gapY>reach*reach)continue;
     unsigned s=spatial(cx+ox,cy+oy,seed); float u=rnd(s),prob=expf(-lambda),cdf=prob;
     int count=0; while(u>cdf && count<24) { count++; prob*=lambda/count; cdf+=prob; }
     for(int j=0;j<count;j++) {
       unsigned ps=hash(s ^ ((j+1)*0xd168aae1U));
       float dx=px-(cx+ox+rnd(ps))*cell,dy=py-(cy+oy+rnd(ps^0x6591bU))*cell;
       if(dx*dx+dy*dy<radius*radius) {covered=true;break;}
     }
   }
   result+=covered?1.f:0.f;
 }
 return (result/8.f-density)/sqrtf(density*(1-density));
}
__device__ float decode(float v,int linear) { return linear?v:copysignf(powf(fabsf(v),2.4f),v); }
__device__ float encode(float v,int linear) { return linear?v:copysignf(powf(fabsf(v),1.f/2.4f),v); }
__device__ float4 at(const float4* a,int w,int h,int x,int y) {return a[max(0,min(h-1,y))*w+max(0,min(w-1,x))];}
__device__ float4 lerp4(float4 a,float4 b,float t) {return make_float4(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t);}
__device__ float4 sample4(const float4* a,int w,int h,float x,float y) {
 int ix=(int)floorf(x),iy=(int)floorf(y);
 return lerp4(lerp4(at(a,w,h,ix,iy),at(a,w,h,ix+1,iy),x-ix),lerp4(at(a,w,h,ix,iy+1),at(a,w,h,ix+1,iy+1),x-ix),y-iy);
}
extern "C" __global__ void extract(const float4* src,float4* dst,int w,int h,int qw,int qh,Params p) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=qw||y>=qh)return;
 float4 sum=make_float4(0,0,0,0); int count=0;
 for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++) {
  int sx=x*4+dx,sy=y*4+dy; if(sx>=w||sy>=h)continue;
  float4 v=src[sy*w+sx];float alpha=fmaxf(0.f,fminf(1.f,v.w));
  float r=decode(v.x,p.linear),g=decode(v.y,p.linear),b=decode(v.z,p.linear);
  float lum=fmaxf(0.f,.2126f*r+.7152f*g+.0722f*b);
  float t=decode(p.threshold,0),k=fmaxf(.0001f,p.softness);
  float mask=fminf(1.f,fmaxf(0.f,(lum-t+k)/(2*k))); mask=mask*mask*(3-2*mask);
  sum.x+=fmaxf(r,0.f)*mask*alpha;sum.y+=fmaxf(g,0.f)*mask*alpha;sum.z+=fmaxf(b,0.f)*mask*alpha;sum.w+=lum*mask*alpha;count++;
 }
 dst[y*qw+x]=make_float4(sum.x/count,sum.y/count,sum.z/count,sum.w/count);
}
extern "C" __global__ void blur(const float4* src,float4* dst,int w,int h,float radius,int vertical) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=w||y>=h)return;
 float4 sum=make_float4(0,0,0,0);float norm=0;
 int support=(int)ceilf(3.f*radius);float sigma=fmaxf(radius,.125f);
 for(int k=-support;k<=support;k++) {float wt=expf(-.5f*k*k/(sigma*sigma));
  float4 v=at(src,w,h,x+(vertical?0:k),y+(vertical?k:0));
  sum.x+=wt*v.x;sum.y+=wt*v.y;sum.z+=wt*v.z;sum.w+=wt*v.w;norm+=wt;
 }
 dst[y*w+x]=make_float4(sum.x/norm,sum.y/norm,sum.z/norm,sum.w/norm);
}
extern "C" __global__ void down(const float4* src,float4* dst,int w,int h,int dw,int dh,int factor) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=dw||y>=dh)return;
 float4 sum=make_float4(0,0,0,0);int count=0;
 for(int dy=0;dy<factor;dy++)for(int dx=0;dx<factor;dx++){int sx=x*factor+dx,sy=y*factor+dy;if(sx>=w||sy>=h)continue;float4 v=src[sy*w+sx];sum.x+=v.x;sum.y+=v.y;sum.z+=v.z;sum.w+=v.w;count++;}
 dst[y*dw+x]=make_float4(sum.x/count,sum.y/count,sum.z/count,sum.w/count);
}
extern "C" __global__ void compose(const float4* src,float4* dst,const float4* hal,const float4* bloom,int w,int h,int hw,int hh,int bw,int bh,int hf,int bf,Params p) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=w||y>=h)return;
 float4 v=src[y*w+x];if(v.w<=0.f || p.mix==0.f || (p.grain==0.f&&p.halation==0.f&&p.bloom==0.f)){dst[y*w+x]=v;return;}
 float4 original=v;
 float4 hv=p.halation!=0?sample4(hal,hw,hh,(x+.5f)/(4.f*hf)-.5f,(y+.5f)/(4.f*hf)-.5f):make_float4(0,0,0,0);
 float4 bv=p.bloom!=0?sample4(bloom,bw,bh,(x+.5f)/(4.f*bf)-.5f,(y+.5f)/(4.f*bf)-.5f):make_float4(0,0,0,0);
 float r=v.x,g=v.y,b=v.z;
 // Warm compact halo, plus neutral broad bloom; no transform of the underlying grade.
 if(p.halation!=0||p.bloom!=0) {
 r=decode(r,p.linear);g=decode(g,p.linear);b=decode(b,p.linear);
 r+=p.halation*hv.w +p.bloom*bv.x;g+=p.halation*.16f*hv.w+p.bloom*bv.y;b+=p.halation*.025f*hv.w+p.bloom*bv.z;
 v.x=encode(r,p.linear);v.y=encode(g,p.linear);v.z=encode(b,p.linear);}
 if(p.grain>0) {
  float lum=fmaxf(0.f,fminf(1.f,p.linear?encode(.2126f*r+.7152f*g+.0722f*b,0):.2126f*v.x+.7152f*v.y+.0722f*v.z));
  float response=lum<.5f?p.shadows+(1-p.shadows)*lum*2:1+(p.highlights-1)*(lum-.5f)*2;
  unsigned s=hash(p.seed ^ hash(p.frame));float pitch=fmaxf(.45f,p.size*p.scale);
  float px=x+p.originX+.5f,py=y+p.originY+.5f;
  float n=p.particle?particleNoise(px/p.scale,py/p.scale,fmaxf(.05f,fminf(.95f,lum)),fmaxf(.25f,p.size*.45f),s):fastNoise(px/pitch,py/pitch,s);
  float cr=n,cg=n,cb=n;
  if(p.color>0){cr=fastNoise(px/pitch,py/pitch,s^0x12773U);cg=fastNoise(px/pitch,py/pitch,s^0x91331U);cb=fastNoise(px/pitch,py/pitch,s^0x34573U);}
  float amp=p.grain*response;
  if(p.linear) {float er=encode(v.x,0),eg=encode(v.y,0),eb=encode(v.z,0);v.x=decode(er+amp*(n*(1-p.color)+cr*p.color),0);v.y=decode(eg+amp*(n*(1-p.color)+cg*p.color),0);v.z=decode(eb+amp*(n*(1-p.color)+cb*p.color),0);}
  else {v.x+=amp*(n*(1-p.color)+cr*p.color);v.y+=amp*(n*(1-p.color)+cg*p.color);v.z+=amp*(n*(1-p.color)+cb*p.color);}
 }
 if(p.mix!=1.f){v.x=original.x+(v.x-original.x)*p.mix;v.y=original.y+(v.y-original.y)*p.mix;v.z=original.z+(v.z-original.z)*p.mix;}
 dst[y*w+x]=v;
}
__device__ float halfFloat(unsigned short h) {
 unsigned s=(h&0x8000U)<<16,e=(h>>10)&31,m=h&1023;
 if(e==0)return copysignf(ldexpf((float)m,-24),s?-1.f:1.f);
 if(e==31)return __uint_as_float(s|0x7f800000U|(m<<13));
 return __uint_as_float(s|((e+112)<<23)|(m<<13));
}
__device__ unsigned short floatHalf(float v) {
 unsigned bits=__float_as_uint(v),s=(bits>>16)&0x8000U,m=bits&0x7fffffU;int e=((bits>>23)&255)-127+15;
 if(e>=31)return (unsigned short)(s|0x7c00U|(((bits>>23)&255)==255&&m?0x200:0));
 if(e<=0){if(e<-10)return (unsigned short)s;m|=0x800000U;int shift=14-e;unsigned val=m>>shift,rem=m&((1U<<shift)-1);if(rem>(1U<<(shift-1))||(rem==(1U<<(shift-1))&&(val&1)))val++;return (unsigned short)(s|val);}
 unsigned val=((unsigned)e<<10)|(m>>13),rem=m&8191;if(rem>4096||(rem==4096&&(val&1)))val++;return (unsigned short)(s|val);
}
// GPU host buffers: BGRA float/half, row pitch may include padding.
extern "C" __global__ void pack(const char* host,float4* rgba,int w,int h,int pitch,int half,int premult) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=w||y>=h)return;
 float4 v;
 if(half){const unsigned short* q=(const unsigned short*)(host+y*pitch)+x*4;v=make_float4(halfFloat(q[2]),halfFloat(q[1]),halfFloat(q[0]),halfFloat(q[3]));}
 else {float4 q=((const float4*)(host+y*pitch))[x];v=make_float4(q.z,q.y,q.x,q.w);}
 if(premult&&v.w>0){v.x/=v.w;v.y/=v.w;v.z/=v.w;}rgba[y*w+x]=v;
}
extern "C" __global__ void unpack(const float4* rgba,const char* original,char* host,int w,int h,int outW,int outH,int srcPitch,int dstPitch,int half,int premult,int cropX,int cropY,int bypass) {
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=outW||y>=outH)return;
 int sx=x+cropX,sy=y+cropY;if(sx<0||sx>=w||sy<0||sy>=h){int n=half?8:16;char* b=host+y*dstPitch+x*n;for(int i=0;i<n;i++)b[i]=0;return;}
 if(bypass){int n=half?8:16;const char* a=original+sy*srcPitch+sx*n;char* b=host+y*dstPitch+x*n;for(int i=0;i<n;i++)b[i]=a[i];return;}
 float4 v=rgba[sy*w+sx];
 if(v.w<=0){int n=half?8:16;const char* a=original+sy*srcPitch+sx*n;char* b=host+y*dstPitch+x*n;for(int i=0;i<n;i++)b[i]=a[i];return;}
 if(premult){v.x*=v.w;v.y*=v.w;v.z*=v.w;}
 if(half){unsigned short* q=(unsigned short*)(host+y*dstPitch)+x*4;q[0]=floatHalf(v.z);q[1]=floatHalf(v.y);q[2]=floatHalf(v.x);q[3]=floatHalf(v.w);}
 else {((float4*)(host+y*dstPitch))[x]=make_float4(v.z,v.y,v.x,v.w);}
}
