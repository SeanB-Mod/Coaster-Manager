/* Read-only runtime confirmation. Never invoke the writer or indirect targets. */
static int orientation_contains(const char *s, const char *part) {
    if (!s)
        return 0;
    for (; *s; ++s)
        if (begins(s, part))
            return 1;
    return 0;
}
static int orientation_slot(const unsigned char *b, unsigned n, unsigned off, UINT_PTR base,
                            int jump, UINT_PTR *slot) {
    unsigned len = jump ? 7U : 6U;
    if (!b || !slot || off > n || n - off < len)
        return 0;
    unsigned d = off;
    if (jump) {
        if (b[d++] != 0x48)
            return 0;
    }
    if (b[d++] != 0xff || b[d++] != (jump ? 0x25 : 0x15))
        return 0;
    unsigned u = (unsigned)b[d] | ((unsigned)b[d + 1] << 8) | ((unsigned)b[d + 2] << 16) |
                 ((unsigned)b[d + 3] << 24);
    long long disp = (u & 0x80000000U) ? (long long)u - 0x100000000LL : (long long)u;
    *slot = (UINT_PTR)((long long)base + off + len + disp);
    return 1;
}
static int orientation_code(void *m, void **code, unsigned char *b, unsigned n) {
    SIZE_T got = 0;
    *code = 0;
    return m && ReadProcessMemory(GetCurrentProcess(), m, code, sizeof(*code), &got) &&
           got == sizeof(*code) && is_executable_game_pointer(*code) &&
           ReadProcessMemory(GetCurrentProcess(), *code, b, n, &got) && got == n;
}
static int orientation_layout(const unsigned char *b, unsigned n) {
    return n >= 0x3f3 && b[0x320] == 0x48 && b[0x321] == 0x8d && b[0x322] == 0x54 &&
           b[0x323] == 0x24 && b[0x324] == 0x70 && b[0x360] == 0x48 && b[0x361] == 0x8d &&
           b[0x362] == 0x54 && b[0x363] == 0x24 && b[0x364] == 0x48 && b[0x3ef] == 0x0f &&
           b[0x3f0] == 0x29 && b[0x3f1] == 0x4d && b[0x3f2] == 0x40;
}
static void *orientation_writer(void) {
    static void *cached;
    if (cached)
        return cached;
    SIZE_T count = 0;
    void **assemblies = p_domain_get_assemblies(p_domain_get(), &count);
    if (!assemblies || count > 10000)
        return 0;
    unsigned scanned = 0;
    for (SIZE_T a = 0; a < count && scanned < 200000; ++a) {
        void *image = p_assembly_get_image(assemblies[a]);
        if (!image)
            continue;
        SIZE_T classes = p_image_get_class_count(image);
        if (classes > 200000)
            continue;
        for (SIZE_T c = 0; c < classes && scanned < 200000; ++c) {
            ++scanned;
            void *cls = p_image_get_class(image, c);
            if (!cls)
                continue;
            if (!equals(p_class_get_name(cls), "<>c__DisplayClass20_0") &&
                !equals(p_class_get_name(cls), "<>c__DisplayClass15_0"))
                continue;
            char *name = p_type_get_name(p_class_get_type(cls));
            int match = orientation_contains(name, "ECPTrackRide.") &&
                        (equals(name, "TPS.Game.ECPTrackRide.<>c__DisplayClass20_0") ||
                         equals(name, "TPS.Game.ECPTrackRide.<>c__DisplayClass15_0"));
            if (name)
                p_free(name);
            if (!match)
                continue;
            void *m = p_class_get_method_from_name(cls, "<SetComponents>g__Snapshot|30", 0);
            if (!m)
                m = p_class_get_method_from_name(cls, "<SetComponents>g__Snapshot|31", 0);
            if (!m)
                m = p_class_get_method_from_name(cls, "<SetComponents>g__Snapshot|32", 0);
            if (m && p_method_get_class(m) == cls && p_method_get_param_count(m) == 0 &&
                p_method_is_instance(m) && !p_method_is_generic(m)) {
                char *ret = p_type_get_name(p_method_get_return_type(m));
                int ok = equals(ret, "System.Void");
                if (ret)
                    p_free(ret);
                if (ok) {
                    cached = m;
                    return m;
                }
            }
        }
    }
    return 0;
}
static int orientation_getter_slot(void *cls, const char *name, UINT_PTR *slot) {
    void *m = p_class_get_method_from_name(cls, name, 2);
    if (!m || p_method_get_class(m) != cls || p_method_get_param_count(m) != 2 ||
        p_method_is_instance(m) || p_method_is_generic(m))
        return 0;
    unsigned char b[7];
    void *code = 0;
    return orientation_code(m, &code, b, sizeof(b)) &&
           orientation_slot(b, sizeof(b), 0, (UINT_PTR)code, 1, slot);
}
static void verify_support_orientation(void) {
    g_support_world_verified = 0;
    void *m = orientation_writer();
    unsigned char b[1024];
    void *code = 0;
    UINT_PTR vector = 0, rotation = 0, worldVector = 0, worldRotation = 0, localVector = 0,
             localRotation = 0;
    if (!orientation_code(m, &code, b, sizeof(b)) || !orientation_layout(b, sizeof(b)) ||
        !orientation_slot(b, sizeof(b), 0x325, (UINT_PTR)code, 0, &vector) ||
        !orientation_slot(b, sizeof(b), 0x365, (UINT_PTR)code, 0, &rotation)) {
        logtext(
            "ORIENTATION VERIFY SKIP: captured writer identity or layout unavailable. Native data retained.\r\n");
        return;
    }
    void *cls = findclass(p_domain_get(), "UnityEngine", "Transform");
    if (!cls || !orientation_getter_slot(cls, "get_position_Injected", &worldVector) ||
        !orientation_getter_slot(cls, "get_rotation_Injected", &worldRotation) ||
        !orientation_getter_slot(cls, "get_localPosition_Injected", &localVector) ||
        !orientation_getter_slot(cls, "get_localRotation_Injected", &localRotation) ||
        vector != worldVector || rotation != worldRotation || vector == localVector ||
        rotation == localRotation) {
        logtext(
            "ORIENTATION VERIFY SKIP: world/local Transform getter slots do not match tested writer. Native data retained.\r\n");
        return;
    }
    g_support_world_verified = 1;
    logtext(
        "ORIENTATION VERIFIED: support Rotation uses Transform world rotation; WorldPosition uses Transform world position.\r\n");
}
