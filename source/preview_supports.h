/* Ground anchors come from the import plan; heights interpolate the saved spline.
   Reject distant segments rather than inventing a support beneath unrelated track. */
static int preview_support_top(const float points[][3], unsigned count, const float *base,
                               float *height) {
    float best = 1.0f, top = base[1];
    int found = 0;
    for (unsigned i = 0; i < count; ++i) {
        const float *a = points[i], *b = points[(i + 1) % count];
        float dx = b[0] - a[0], dz = b[2] - a[2], length = dx * dx + dz * dz;
        float t = length > .000001f ? ((base[0] - a[0]) * dx + (base[2] - a[2]) * dz) / length : 0;
        if (t < 0)
            t = 0;
        if (t > 1)
            t = 1;
        float x = a[0] + t * dx - base[0], z = a[2] + t * dz - base[2], distance = x * x + z * z,
              y = a[1] + t * (b[1] - a[1]);
        if (y <= base[1] + .1f)
            continue;
        if (distance < best - .00001f || (found && distance <= best + .00001f && y > top)) {
            best = distance;
            top = y;
            found = 1;
        }
    }
    if (found)
        *height = top;
    return found;
}
