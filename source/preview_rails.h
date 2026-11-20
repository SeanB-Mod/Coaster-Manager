/* Stable horizontal side for a paired-rail visual; preserves side at vertical tangents. */
static void preview_rail_side(const float *before, const float *after, float *side) {
    float x = after[0] - before[0], z = after[2] - before[2], n = x * x + z * z;
    if (n < .000001f)
        return;
    union {
        float f;
        unsigned u;
    } inv;
    inv.f = n;
    inv.u = 0x5f3759dfU - (inv.u >> 1);
    float y = inv.f;
    y = y * (1.5f - .5f * n * y * y);
    y = y * (1.5f - .5f * n * y * y);
    float sx = z * y, sz = -x * y;
    if (sx * side[0] + sz * side[2] < 0) {
        sx = -sx;
        sz = -sz;
    }
    side[0] = sx;
    side[1] = 0;
    side[2] = sz;
}
