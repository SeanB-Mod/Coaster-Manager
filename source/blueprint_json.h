/* CRT-free bounded JSON helpers. Also compiled by the host verification harness. */
#define JSON_CAPACITY (4U * 1024U * 1024U)
static char jbuf[JSON_CAPACITY];
static unsigned jpos;
static int jok;
static void js(const char *s) {
    while (*s) {
        if (jpos >= JSON_CAPACITY - 1) {
            jok = 0;
            return;
        }
        jbuf[jpos++] = *s++;
    }
    jbuf[jpos] = 0;
}
static void ju(unsigned long long x) {
    char rev[24], out[24];
    unsigned n = 0, k = 0;
    do {
        rev[n++] = (char)('0' + x % 10);
        x /= 10;
    } while (x);
    while (n)
        out[k++] = rev[--n];
    out[k] = 0;
    js(out);
}
static void ji(long long x) {
    if (x < 0) {
        js("-");
        ju(0ULL - (unsigned long long)x);
    } else
        ju((unsigned long long)x);
}
static void jq(const char *s) {
    const char *hex = "0123456789abcdef";
    js("\"");
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        char b[7] = {0};
        if (c == '"' || c == '\\') {
            b[0] = '\\';
            b[1] = (char)c;
            js(b);
        } else if (c < 32 || c >= 127) {
            b[0] = '\\';
            b[1] = 'u';
            b[2] = '0';
            b[3] = '0';
            b[4] = hex[c >> 4];
            b[5] = hex[c & 15];
            js(b);
        } else {
            b[0] = (char)c;
            js(b);
        }
    }
    js("\"");
}
/* Nine significant decimal digits suffice to round-trip finite IEEE binary32.
 * Raw source float bit patterns are exported as well. */
static int jf(float f) {
    unsigned bits = 0;
    for (unsigned i = 0; i < 4; ++i)
        ((unsigned char *)&bits)[i] = ((const unsigned char *)&f)[i];
    if ((bits & 0x7f800000U) == 0x7f800000U) {
        jok = 0;
        return 0;
    }
    if (bits >> 31)
        js("-");
    double x = (double)f;
    if (x < 0)
        x = -x;
    if (x == 0) {
        js("0");
        return 1;
    }
    int e = 0;
    while (x >= 10) {
        x /= 10;
        ++e;
    }
    while (x < 1) {
        x *= 10;
        --e;
    }
    unsigned long long digits = (unsigned long long)(x * 100000000.0 + 0.5);
    if (digits >= 1000000000ULL) {
        digits /= 10;
        ++e;
    }
    char b[12];
    for (int i = 8; i >= 0; --i) {
        b[i + 1] = (char)('0' + digits % 10);
        digits /= 10;
    }
    b[0] = b[1];
    b[1] = '.';
    b[10] = 0;
    js(b);
    js("e");
    ji(e);
    return 1;
}
static void jfloatbits(float f) {
    unsigned bits = 0;
    for (unsigned i = 0; i < 4; ++i)
        ((unsigned char *)&bits)[i] = ((const unsigned char *)&f)[i];
    const char *hex = "0123456789abcdef";
    char b[11];
    b[0] = '0';
    b[1] = 'x';
    for (unsigned i = 0; i < 8; ++i)
        b[i + 2] = hex[(bits >> ((7 - i) * 4)) & 15];
    b[10] = 0;
    jq(b);
}
static void inverse_point(const float *p, const float *origin, const float *q, float *out) {
    double x = (double)p[0] - origin[0], y = (double)p[1] - origin[1], z = (double)p[2] - origin[2];
    double a = -q[0], b = -q[1], c = -q[2], w = q[3];
    double tx = 2 * (b * z - c * y), ty = 2 * (c * x - a * z), tz = 2 * (a * y - b * x);
    out[0] = (float)(x + w * tx + b * tz - c * ty);
    out[1] = (float)(y + w * ty + c * tx - a * tz);
    out[2] = (float)(z + w * tz + a * ty - b * tx);
}
static int normalized_quaternion(const float *q, double *out) {
    double norm = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned bits = 0;
        for (unsigned k = 0; k < 4; ++k)
            ((unsigned char *)&bits)[k] = ((const unsigned char *)&q[i])[k];
        if ((bits & 0x7f800000U) == 0x7f800000U)
            return 0;
        norm += (double)q[i] * q[i];
    }
    if (norm < 0.999 || norm > 1.001)
        return 0;
    double inv = 1;
    for (unsigned i = 0; i < 4; ++i)
        inv *= 1.5 - 0.5 * norm * inv * inv;
    for (unsigned i = 0; i < 4; ++i)
        out[i] = q[i] * inv;
    return 1;
}
/* Candidate only: inverse(stationWorld) * sourceRotation. Source Rotation's
 * coordinate space still needs a controlled in-game comparison. */
static int relative_quaternion(const float *station, const float *source, float *out) {
    double s[4], r[4];
    if (!normalized_quaternion(station, s) || !normalized_quaternion(source, r))
        return 0;
    double a = -s[0], b = -s[1], c = -s[2], w = s[3];
    out[0] = (float)(w * r[0] + a * r[3] + b * r[2] - c * r[1]);
    out[1] = (float)(w * r[1] - a * r[2] + b * r[3] + c * r[0]);
    out[2] = (float)(w * r[2] + a * r[1] - b * r[0] + c * r[3]);
    out[3] = (float)(w * r[3] - a * r[0] - b * r[1] - c * r[2]);
    return 1;
}
