#include "../blueprint_json.h"
int _fltused=0;
__declspec(dllexport) const char* FormatFloat(float x){jpos=0;jok=1;jbuf[0]=0;jf(x);return jok?jbuf:0;}
__declspec(dllexport) void TransformPoint(const float*p,const float*o,const float*q,float*out){inverse_point(p,o,q,out);}
__declspec(dllexport) int BufferBoundary(void){jpos=0;jok=1;jbuf[0]=0;for(unsigned i=0;i<JSON_CAPACITY;++i)js("a");return !jok&&jpos==JSON_CAPACITY-1&&jbuf[JSON_CAPACITY-1]==0;}
__declspec(dllexport) const char* FormatIds(void){jpos=0;jok=1;jbuf[0]=0;js("[");ji((-9223372036854775807LL)-1);js(",");ju(18446744073709551615ULL);js(",");jq("quote\" slash\\ line\n");js("]");return jbuf;}
__declspec(dllexport) const char* FormatBits(float x){jpos=0;jok=1;jbuf[0]=0;jfloatbits(x);return jbuf;}
__declspec(dllexport) int RelativeRotation(const float*s,const float*r,float*out){return relative_quaternion(s,r,out);}
