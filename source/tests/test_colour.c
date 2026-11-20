#define PREFLIGHT_TEST_ENTRY OriginalPreflightMain
#include "test_preflight.c"
static MC rideColourClass, rgbaClass;
static struct {
    MC *cls;
    unsigned char bytes[256];
} rideColourBox;
void TestMain(void) {
    pf_setup();
    g_ride_class = &rideColourClass;
    rideColourClass.name = "TPS.Game.ECTrackRide";
    rideColourClass.size = 248;
    rgbaClass.name = "UnityEngine.Color";
    rgbaClass.size = 16;
    boolean.name = "System.Boolean";
    boolean.size = 1;
    rideColourClass.fieldsCount = 2;
    rideColourClass.fields[0] = (MF){"HasTint", &boolean, &rideColourClass, 16, 0};
    rideColourClass.fields[1] = (MF){"TintColor", &rgbaClass, &rideColourClass, 20, 0};
    rideColourBox.cls = &rideColourClass;
    memset(rideColourBox.bytes, 0xa5, 248);
    float rgba[4] = {0.125f, 0.25f, 0.5f, 1};
    CoasterColour tint;
    for (unsigned custom = 0; custom < 2; ++custom) {
        rideColourBox.bytes[16] = (unsigned char)custom;
        memcpy(rideColourBox.bytes + 20, rgba, 16);
        if (!colour_from_box(&rideColourBox, &tint) || tint.custom != custom)
            ExitProcess(10 + custom);
        for (unsigned i = 0; i < 4; ++i)
            if (tint.rgba[i] != rgba[i])
                ExitProcess(20 + i);
    }
    rideColourBox.bytes[16] = 2;
    if (colour_from_box(&rideColourBox, &tint) || tint.present)
        ExitProcess(30);
    rideColourBox.bytes[16] = 1;
    rgba[0] = 2;
    memcpy(rideColourBox.bytes + 20, rgba, 16);
    if (colour_from_box(&rideColourBox, &tint) || tint.present)
        ExitProcess(31);
    if (colour_from_box(0, &tint) || tint.present || liveHandles)
        ExitProcess(32);
    if (!colour_component_size(248) || !colour_component_size(256) ||
        colour_component_size(247) || colour_component_size(249) ||
        colour_component_size(257) || colour_component_size(-1)) ExitProcess(40);
    contextC.name="TPS.Game.ECPTrackRide.<>c__DisplayClass15_0";
    if (!pf_method_name(&contextC,"<SetComponents>g__Snapshot|31","<SetComponents>g__Snapshot|30") ||
        !pf_method_name(&contextC,"<SetComponents>g__Revert|32","<SetComponents>g__Revert|31") ||
        !pf_method_name(&contextC,"<SetComponents>g__OnTrackRevert|14","<SetComponents>g__OnTrackRevert|13") ||
        !pf_method_name(&contextC,"<SetComponents>g__Snapshot|32","<SetComponents>g__Snapshot|30") ||
        !pf_method_name(&contextC,"<SetComponents>g__Revert|33","<SetComponents>g__Revert|31") ||
        pf_method_name(&contextC,"<SetComponents>g__Snapshot|99","<SetComponents>g__Snapshot|30")) ExitProcess(41);
    contextC.name="Other.<>c__DisplayClass15_0";
    if(pf_method_name(&contextC,"<SetComponents>g__Snapshot|31","<SetComponents>g__Snapshot|30")) ExitProcess(42);
    rideColourClass.size=256;
    rgba[0]=0.125f;memcpy(rideColourBox.bytes+20,rgba,16);rideColourBox.bytes[16]=1;
    if(!colour_from_box(&rideColourBox,&tint)||!tint.present)ExitProcess(43);
    ExitProcess(0);
}
