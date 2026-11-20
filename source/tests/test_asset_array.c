#define PREFLIGHT_TEST_ENTRY OriginalPreflightMain
#include "test_preflight.c"
static MO assetArray;
static int assetLength, assetFailure;
static void *asset_find(void *image, const char *ns, const char *name) {
    if (equals(ns,"System") && equals(name,"Array")) return &arrayC;
    return pf_find(image,ns,name);
}
static void *asset_invoke(void *method, void *owner, void **args, void **exception) {
    if (method==&arrayC.countMethod) {
        *exception=assetFailure?(void*)1:0;
        boxedCount.cls=&i32;
        memcpy(boxedCount.bytes,&assetLength,4);
        return &boxedCount;
    }
    return pf_mock_invoke(method,owner,args,exception);
}
void TestMain(void) {
    pf_setup();
    p_class_from_name=asset_find;p_runtime_invoke=asset_invoke;
    arrayC.name="System.Array";arrayC.size=0;
    arrayC.countMethod=(MM){&arrayC,&i32,"get_Length",0};
    assetArray.cls=&arrayC;
    const int lengths[]={0,256,257,1675,65536,65537,-1};
    for(unsigned i=0;i<7;++i) {
        int count=0;assetLength=lengths[i];
        int ui=browser_asset_array_count(&assetArray,&count);
        if(ui!=(assetLength>=0 && assetLength<=65536) || (ui && count!=assetLength)) ExitProcess(10+i);
        int config=write_array_count(&assetArray,&count);
        if(config!=(assetLength>=0 && assetLength<=256)) ExitProcess(20+i);
    }
    assetFailure=1;int count=0;
    if(browser_asset_array_count(&assetArray,&count))ExitProcess(30);
    assetFailure=0;assetArray.cls=&i32;
    if(browser_asset_array_count(&assetArray,&count)||browser_asset_array_count(0,&count)||liveHandles)ExitProcess(31);
    ExitProcess(0);
}
