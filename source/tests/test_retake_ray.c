#include <assert.h>
#include <math.h>
#include "../retake_ray.h"
int main(void) {
    float b[6] = {0, 0, 0, 1, 1, 1};
    float r[6] = {0, 0, -5, 0, 0, 1};
    assert(management_ray_bounds(r, b) == 4);
    r[2] = 5;
    r[5] = -1;
    assert(management_ray_bounds(r, b) == 4);
    r[0] = 2;
    assert(management_ray_bounds(r, b) == 10001);
    r[0] = 1;
    assert(management_ray_bounds(r, b) == 4);
    r[0] = 0;
    r[2] = 0;
    assert(management_ray_bounds(r, b) == 0);
    r[2] = 5;
    r[5] = 1;
    assert(management_ray_bounds(r, b) == 10001);
    r[2] = -20000;
    assert(management_ray_bounds(r, b) == 10001);
    r[2] = -5;
    b[3] = NAN;
    assert(management_ray_bounds(r, b) == 10001);
    b[3] = 1;
    r[5] = NAN;
    assert(management_ray_bounds(r, b) == 10001);
    r[5] = 0;
    assert(management_ray_bounds(r, b) == 10001);
    return 0;
}
