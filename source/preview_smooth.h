/* Closed Catmull-Rom interpolation, two subdivisions per saved sample. */
static void preview_smooth_point(const float *a, const float *b, const float *c, const float *d,
                                 float t, float *out) {
    for (unsigned k = 0; k < 3; ++k) {
        float t2 = t * t, t3 = t2 * t;
        out[k] =
            .5f * ((2 * b[k]) + (-a[k] + c[k]) * t + (2 * a[k] - 5 * b[k] + 4 * c[k] - d[k]) * t2 +
                   (-a[k] + 3 * b[k] - 3 * c[k] + d[k]) * t3);
    }
}
static void preview_rotate_offset(const float *q, const float *v, float *out) {
    float tx = 2 * (q[1] * v[2] - q[2] * v[1]), ty = 2 * (q[2] * v[0] - q[0] * v[2]),
          tz = 2 * (q[0] * v[1] - q[1] * v[0]);
    out[0] = v[0] + q[3] * tx + q[1] * tz - q[2] * ty;
    out[1] = v[1] + q[3] * ty + q[2] * tx - q[0] * tz;
    out[2] = v[2] + q[3] * tz + q[0] * ty - q[1] * tx;
}
