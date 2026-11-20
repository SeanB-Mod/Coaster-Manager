#include "../coaster_selection_probe.c"
__declspec(dllexport) int ResizePreview(const unsigned char *data, unsigned w, unsigned h,
                                        unsigned char *out) {
    return preview_resample(data, w, h, out);
}

__declspec(dllexport) unsigned BackgroundIndex(const unsigned short *id) {
    return preview_background_index(id);
}
__declspec(dllexport) int FlattenBlack(unsigned char *rgba, unsigned count) {
    return preview_flatten_black(rgba, count);
}
__declspec(dllexport) int GeometryAlpha(unsigned char *colour, const unsigned char *mask,
                                        unsigned count) {
    return preview_geometry_alpha(colour, mask, count);
}
