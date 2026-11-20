#ifndef COASTER_PREVIEW_BACKGROUND_H
#define COASTER_PREVIEW_BACKGROUND_H
/* Accept alpha only from a separate geometry render, never from RGB darkness.
 * Reject empty/fully covered masks without touching the colour capture. */
static int preview_geometry_alpha(unsigned char *colour, const unsigned char *mask,
                                  unsigned count) {
    unsigned clear = 0, solid = 0;
    if (!colour || !mask || count < 32)
        return 0;
    for (unsigned i = 0; i < count; ++i) {
        clear += mask[i * 4 + 3] == 0;
        solid += mask[i * 4 + 3] == 255;
    }
    if (clear < 16 || solid < 16)
        return 0;
    for (unsigned i = 0; i < count; ++i)
        colour[i * 4 + 3] = mask[i * 4 + 3];
    return 1;
}
/* UUIDs are random when created; deriving an index from their canonical form
 * gives each blueprint a stable random backdrop without extra mutable metadata. */
static unsigned preview_background_index(const unsigned short *uuid) {
    unsigned hash = 2166136261U;
    if (!uuid)
        return 0;
    for (unsigned i = 0; i < 32 && uuid[i]; ++i) {
        unsigned c = uuid[i];
        if (c >= 'A' && c <= 'F')
            c += 'a' - 'A';
        hash = (hash ^ c) * 16777619U;
    }
    return hash % 8;
}
/* JPEG ignores alpha. Flatten explicitly so transparent RGB never leaks into
 * the Steam poster, while opaque/dark rail pixels remain unchanged. */
static int preview_flatten_black(unsigned char *rgba, unsigned count) {
    int changed = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char *p = rgba + 4 * i;
        unsigned a = p[3];
        if (a == 255)
            continue;
        changed = 1;
        for (unsigned c = 0; c < 3; ++c)
            p[c] = (unsigned char)((p[c] * a + 127) / 255);
        p[3] = 255;
    }
    return changed;
}
#endif
