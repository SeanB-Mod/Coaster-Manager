#include "../footprint_math.h"
__declspec(dllexport) void Bounds(const float*origin,const float*q,const float*points,int count,float*out){FootprintBounds b={0};for(int i=0;i<count;++i)footprint_extend(&b,origin,q,points+3*i);out[0]=b.minx;out[1]=b.maxx;out[2]=b.minz;out[3]=b.maxz;}
int _fltused;

__declspec(dllexport) void LocalPoint(const float*origin,const float*q,const float*p,float*out){footprint_local_point(origin,q,p,out);}

#include "../preview_rails.h"
__declspec(dllexport) void RailSides(const float*points,int count,float*out){float side[3]={1,0,0};for(int i=0;i<count;++i){preview_rail_side(points+3*((i+count-1)%count),points+3*((i+1)%count),side);for(int k=0;k<3;++k)out[3*i+k]=side[k];}}

#include "../preview_supports.h"
__declspec(dllexport) int SupportTop(const float*points,unsigned count,const float*base,float*out){return preview_support_top((const float(*)[3])points,count,base,out);}

#include "../preview_smooth.h"
__declspec(dllexport) void Smooth(const float*a,const float*b,const float*c,const float*d,float t,float*out){preview_smooth_point(a,b,c,d,t,out);}
__declspec(dllexport) void RotateOffset(const float*q,const float*v,float*out){preview_rotate_offset(q,v,out);}
