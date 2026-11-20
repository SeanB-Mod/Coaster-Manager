/* Centre-cropped 16:9 bilinear RGBA resampling; no game dependencies. */
static int preview_resample(const unsigned char *src, unsigned w, unsigned h, unsigned char *out) {
    if (!src || !out || !w || !h || w > 4096 || h > 4096)
        return 0;
    float cw = (float)w, ch = (float)h;
    if (cw * 9 > ch * 16)
        cw = ch * 16 / 9;
    else
        ch = cw * 9 / 16;
    float ox = ((float)w - cw) / 2, oy = ((float)h - ch) / 2;
    for (unsigned y = 0; y < 540; ++y) {
        float sy = oy + ((float)y + .5f) * ch / 540 - .5f;
        if (sy < 0)
            sy = 0;
        if (sy > h - 1)
            sy = (float)(h - 1);
        unsigned y0 = (unsigned)sy, y1 = y0 + 1 < h ? y0 + 1 : y0;
        float fy = sy - y0;
        for (unsigned x = 0; x < 960; ++x) {
            float sx = ox + ((float)x + .5f) * cw / 960 - .5f;
            if (sx < 0)
                sx = 0;
            if (sx > w - 1)
                sx = (float)(w - 1);
            unsigned x0 = (unsigned)sx, x1 = x0 + 1 < w ? x0 + 1 : x0;
            float fx = sx - x0;
            for (unsigned c = 0; c < 4; ++c) {
                float a = src[(y0 * w + x0) * 4 + c] * (1 - fx) + src[(y0 * w + x1) * 4 + c] * fx,
                      b = src[(y1 * w + x0) * 4 + c] * (1 - fx) + src[(y1 * w + x1) * 4 + c] * fx;
                out[(y * 960 + x) * 4 + c] = (unsigned char)(a * (1 - fy) + b * fy + .5f);
            }
        }
    }
    return 1;
}
