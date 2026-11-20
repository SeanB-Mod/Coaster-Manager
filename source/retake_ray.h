/* Ray: origin xyz, direction xyz. Bounds: center xyz, extents xyz. */
static float management_ray_bounds(const float *ray, const float *bounds) {
    float near = 0.f, far = 10000.f;
    float norm = ray[3] * ray[3] + ray[4] * ray[4] + ray[5] * ray[5];
    if (!(norm > .9f && norm < 1.1f))
        return 10001.f;
    for (unsigned i = 0; i < 3; ++i) {
        float origin = ray[i], direction = ray[i + 3], center = bounds[i], extent = bounds[i + 3];
        if (!(origin >= -1.e8f && origin <= 1.e8f && direction >= -1.f && direction <= 1.f &&
              center >= -1.e8f && center <= 1.e8f && extent > 0.f && extent <= 1.e6f))
            return 10001.f;
        float lo = center - extent, hi = center + extent;
        if (direction > -.000001f && direction < .000001f) {
            if (origin < lo || origin > hi)
                return 10001.f;
            continue;
        }
        float a = (lo - origin) / direction, b = (hi - origin) / direction;
        if (a > b) {
            float t = a;
            a = b;
            b = t;
        }
        if (a > near)
            near = a;
        if (b < far)
            far = b;
        if (near > far)
            return 10001.f;
    }
    return near;
}
