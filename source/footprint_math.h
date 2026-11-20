/* Station-local X/Z bounds, using inverse source yaw. Importer validates inputs. */
typedef struct {
    float minx, maxx, minz, maxz;
} FootprintBounds;
static void footprint_local_point(const float *origin, const float *q, const float *p, float *out) {
    float x = p[0] - origin[0], z = p[2] - origin[2], c = 1 - 2 * q[1] * q[1], s = 2 * q[1] * q[3];
    out[0] = c * x - s * z;
    out[1] = s * x + c * z;
}
static void footprint_extend(FootprintBounds *b, const float *origin, const float *q,
                             const float *p) {
    float local[2];
    footprint_local_point(origin, q, p, local);
    float lx = local[0], lz = local[1];
    if (lx < b->minx)
        b->minx = lx;
    if (lx > b->maxx)
        b->maxx = lx;
    if (lz < b->minz)
        b->minz = lz;
    if (lz > b->maxz)
        b->maxz = lz;
}
