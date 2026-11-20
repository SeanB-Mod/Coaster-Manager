/* Sequential, selected-blueprint import experiment. F8 never reaches this entrypoint. */
#include "write_fixture.h"
static int write_attempted;
static int write_load_selection(void);
static int write_configuration_matches(void *config, PFRoots *roots);
static unsigned write_hash(const unsigned char *b, unsigned n) {
    unsigned h = 2166136261U;
    for (unsigned i = 0; i < n; ++i)
        h = (h ^ b[i]) * 16777619U;
    return h;
}
static int write_code_guard(void *m, unsigned rva, unsigned n, unsigned expected) {
    static unsigned char bytes[0x4000];
    /* Mapped installed Windows build, 30 October 2026. Typed callers and all
     * original data/entity/floor checks remain; no signature-only bypass. */
    static const unsigned current[][6] = {
        {0x2ed5d90U,0x20U,0xd775f8e0U,0x2f0e3a0U,32U,0xafe60ab5U},
        {0x2ed5db0U,0x20U,0xc48ff079U,0x2f0e3c0U,32U,0x63948c72U},
        {0x2a50df0U,0x410U,0x23f267eU,0x2a803d0U,1024U,0xdf47506U},
        {0x2ea1e00U,0xf0U,0xbaab1121U,0x2ed8920U,304U,0x87bb5b2eU},
        {0x130a40b0U,0x1430U,0x8ecf1a69U,0x12e733d0U,5120U,0xfb435c8dU},
        {0x35a8270U,0x80U,0x70f24cfU,0x35e4fd0U,128U,0x9c38aa17U},
        {0x2ebb430U,0x120U,0xf10947f3U,0x2ef3840U,240U,0xa3633691U},
        {0x2ead6b0U,0x1270U,0x621a450eU,0x2ee4440U,5680U,0x89115670U},
        {0x2ea8180U,0x778U,0xd2d4a218U,0x2ede920U,1936U,0x77055982U},
        {0x2ea8910U,0x18c2U,0xd29e9f1dU,0x2edf0b0U,9760U,0x36ee4878U},
        {0x2eac090U,0xe20U,0x6e37b30aU,0x2ee2bb0U,4336U,0xca1de530U},
        {0x2ed07c0U,0xa0U,0xed2a425U,0x2f08e50U,160U,0x717b0c34U},
    };
    void *candidate = 0;
    HMODULE currentGame = GetModuleHandleW(L"GameAssembly.dll");
    if (orientation_code(m, &candidate, bytes, 1)) {
        for (unsigned i = 0; i < sizeof(current)/sizeof(current[0]); ++i) {
            if (rva != current[i][0] || n != current[i][1] || expected != current[i][2] ||
                candidate != (void *)((UINT_PTR)currentGame + current[i][3])) continue;
            return current[i][4] <= sizeof(bytes) &&
                   orientation_code(m, &candidate, bytes, current[i][4]) &&
                   candidate == (void *)((UINT_PTR)currentGame + current[i][3]) &&
                   write_hash(bytes, current[i][4]) == current[i][5];
        }
    }

    /* Two Point Museum Windows 11-Nov-2026 assembly, independently checked
     * against the supplied game binary. The ten Test 1 construction mappings
     * are instruction-identical modulo relocated targets. BuildSpline differs
     * materially, so Test 5 adds a separately exact-byte guarded, version-specific
     * call for statistics refresh. The old three-argument managed signature
     * remains required and the call is allowed only after queue, entity and
     * snapshot verification. Its new body has not been proven behaviourally
     * identical; this is a controlled in-game test candidate, not a bypass.
     * Experimental opt-in for FinishPlaceItem: source method name, static
     * signature and current code pointer were independently resolved against
     * metadata+method pointers. Exact current build 5120-byte code fingerprint
     * is checked and the caller still requires an exact placed station,
     * active placement cursor and game-owned placement history. This function
     * was NOT instruction-equivalence verified because of heavy obfuscation.
     * Any different build retains the manual right-click fallback. */
    static const unsigned november2026[][6] = {
        {0x2ed5d90U, 0x20U, 0xd775f8e0U, 0x2f25f20U, 32U, 0x6857e7d5U}, /* ToggleEntryArrow */
        {0x2ed5db0U, 0x20U, 0xc48ff079U, 0x2f25f40U, 32U, 0xd532e652U}, /* ToggleExitArrow */
        {0x2a50df0U, 0x410U, 0x23f267eU, 0x2a96b20U, 1024U, 0xd3d5cbc6U}, /* OnClickBuyRide */
        {0x2ea1e00U, 0xf0U, 0xbaab1121U, 0x2eef140U, 304U, 0xfff77a89U}, /* <SetComponents>g__ToggleTrackSupportArrow|15_28 */
        {0x130a40b0U, 0x1430U, 0x8ecf1a69U, 0x12a0d7f0U, 5120U, 0x13c18306U}, /* FinishPlaceItem: experimental */
        {0x35a8270U, 0x80U, 0x70f24cfU, 0x35fc8a0U, 128U, 0x29a18be3U}, /* ClearSupport */
        {0x2ebb430U, 0x120U, 0xf10947f3U, 0x2f0b1c0U, 240U, 0xb2cfc71dU}, /* MarkTrackRideForRebuild */
        {0x2ead6b0U, 0x1270U, 0x621a450eU, 0x2efac70U, 0x170bU, 0x30468546U}, /* BuildSpline: Test 5 guarded stat refresh */
        {0x2ea8180U, 0x778U, 0xd2d4a218U, 0x2ef5150U, 1936U, 0xcc6592c9U}, /* <SetComponents>g__Snapshot|32 */
        {0x2ea8910U, 0x18c2U, 0xd29e9f1dU, 0x2ef58e0U, 9760U, 0x42c3f28fU}, /* <SetComponents>g__Revert|33 */
        {0x2eac090U, 0xe20U, 0x6e37b30aU, 0x2ef93e0U, 4336U, 0x5777cb17U}, /* TryCreateTrackSectionData */
        {0x2ed07c0U, 0xa0U, 0xed2a425U, 0x2f209d0U, 160U, 0x96d6bd5cU}, /* SetTrackTintColor */
    };
    if (orientation_code(m, &candidate, bytes, 1)) {
        for (unsigned i = 0; i < sizeof(november2026)/sizeof(november2026[0]); ++i) {
            if (rva != november2026[i][0] || n != november2026[i][1] ||
                expected != november2026[i][2] ||
                candidate != (void *)((UINT_PTR)currentGame + november2026[i][3]))
                continue;
            return november2026[i][4] <= sizeof(bytes) &&
                   orientation_code(m, &candidate, bytes, november2026[i][4]) &&
                   candidate == (void *)((UINT_PTR)currentGame + november2026[i][3]) &&
                   write_hash(bytes, november2026[i][4]) == november2026[i][5];
        }
    }

    void *code = 0;
    HMODULE game = GetModuleHandleW(L"GameAssembly.dll");
    if (n > sizeof(bytes) || !orientation_code(m, &code, bytes, n))
        return 0;
    unsigned actual = write_hash(bytes, n);
    if (code == (void *)((UINT_PTR)game + rva) && actual == expected)
        return 1;
    /* Reviewed 12.1 equivalents. Exact method pointer, span and hash required;
     * a failed old-build guard never authorizes an arbitrary new body. */
    static const unsigned reviewed[][5] = {
        {0x2ed5d90U, 0x20U, 0xd775f8e0U, 0x2ed4f00U, 0xc06fcd67U},
        {0x2ed5db0U, 0x20U, 0xc48ff079U, 0x2ed4f20U, 0x1f27b7ecU},
        {0x2a50df0U, 0x410U, 0x23f267eU, 0x2a4e220U, 0xf302d750U},
        {0x2ea1e00U, 0xf0U, 0xbaab1121U, 0x2ea04d0U, 0x740590eaU},
        {0x130a40b0U, 0x1430U, 0x8ecf1a69U, 0x129624d0U, 0x893fb241U},
        {0x35a8270U, 0x80U, 0x70f24cfU, 0x35a7480U, 0x8336a5f3U},
        {0x2ebb430U, 0x120U, 0xf10947f3U, 0x2eba870U, 0xccd7bc7bU},
        {0x2ead6b0U, 0x1270U, 0x621a450eU, 0x2eac190U, 0x6ff52c00U},
        {0x2ea8180U, 0x778U, 0xd2d4a218U, 0x2ea69f0U, 0xb77f2aacU},
        {0x2ea8910U, 0x18c2U, 0xd29e9f1dU, 0x2ea7180U, 0xea28809eU},
        {0x2eac090U, 0xe20U, 0x6e37b30aU, 0x2eaa900U, 0x1aae1aa7U},
        {0x2ed07c0U, 0xa0U, 0xed2a425U, 0x2ecf9b0U, 0x5593e982U},
    };
    for (unsigned i = 0; i < sizeof(reviewed) / sizeof(reviewed[0]); ++i)
        if (rva == reviewed[i][0] && n == reviewed[i][1] && expected == reviewed[i][2] &&
            code == (void *)((UINT_PTR)game + reviewed[i][3]) && actual == reviewed[i][4])
            return 1;
    return 0;
}
static void write_log(const char *s) {
    logtext("FIRST WRITE: ");
    logtext(s);
    nl();
    FlushFileBuffers(g_log);
}
static int write_void(void *m, void *o, void **args) {
    if (!m)
        return 0;
    void *ex = 0;
    p_runtime_invoke(m, o, args, &ex);
    return !ex;
}
static int write_value_layout(void *c, int size) {
    unsigned a = 0;
    return c && p_class_is_valuetype(c) && p_class_value_size(c, &a) == size;
}
static int write_schema(void *c, const RecordField *schema, unsigned n, int size) {
    if (!write_value_layout(c, size))
        return 0;
    void *it = 0;
    void *f;
    unsigned fields = 0;
    while ((f = p_class_get_fields(c, &it)))
        if (!(p_field_get_flags(f) & 0x10))
            ++fields;
    if (fields != n)
        return 0;
    for (unsigned i = 0; i < n; ++i) {
        f = pf_field(c, schema[i].name, schema[i].type);
        if (!f || !write_value_layout(p_class_from_type(p_field_get_type(f)), schema[i].size))
            return 0;
        if (equals(schema[i].type, "TPS.Core.Utils.ManagedEntityID") ||
            equals(schema[i].type, "TPS.Game.GameItemDefinitionID")) {
            void *vc = p_class_from_type(p_field_get_type(f));
            void *vi = 0;
            void *vf;
            unsigned count = 0;
            while ((vf = p_class_get_fields(vc, &vi))) {
                if (p_field_get_flags(vf) & 0x10)
                    continue;
                ++count;
                if (p_field_get_parent(vf) != vc ||
                    (!pf_type(p_field_get_type(vf), "System.Int64") &&
                     !pf_type(p_field_get_type(vf), "System.UInt64")))
                    return 0;
            }
            if (count != 1)
                return 0;
        }
    }
    return 1;
}
static int write_set(void *o, void *c, const RecordField *schema, unsigned field, void *value) {
    void *f = pf_field(c, schema[field].name, schema[field].type);
    if (!f)
        return 0;
    p_field_set_value(o, f, value);
    return 1;
}
/* Destination yaw rotates both world positions and world rotations. The
 * floor remains unrotated, so native LocalRotation receives the same yaw. */
static float write_target_rotations[WRITE_MAX_SUPPORTS][4], write_target_angles[WRITE_MAX_SUPPORTS];
static double write_delta[4] = {0, 0, 0, 1};
static double write_delta_degrees;
static double write_atan_unit(double x) {
    const double quarter = 0.78539816339744830962;
    double offset = 0;
    if (x > 0.4142135623730950488) {
        x = (x - 1) / (x + 1);
        offset = quarter;
    }
    double term = x, sum = x;
    for (unsigned n = 1; n < 24; ++n) {
        term *= -x * x;
        sum += term / (2 * n + 1);
    }
    return offset + sum;
}
static double write_atan2(double y, double x) {
    const double pi = 3.14159265358979323846;
    double ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ax == 0 && ay == 0)
        return 0;
    double a = ay > ax ? pi / 2 - write_atan_unit(ax / ay) : write_atan_unit(ay / ax);
    if (x < 0)
        a = pi - a;
    return y < 0 ? -a : a;
}
static int write_target_frame(unsigned long long id, const float *p, const float *q) {
    if (id == WRITE_SOURCE_ID || !finite_vector(p, 3) || !finite_vector(q, 4))
        return 0;
    float source[4];
    memcpy(source, write_source_rotation, 16);
    double s[4], t[4];
    if (!normalized_quaternion(source, s) || !normalized_quaternion(q, t) || t[0] < -0.00001 ||
        t[0] > 0.00001 || t[2] < -0.00001 || t[2] > 0.00001)
        return 0;
    double dot = 0;
    for (unsigned i = 0; i < 4; ++i)
        dot += s[i] * t[i];
    if (dot > 0.9999999999 || dot < -0.9999999999) {
        write_delta[0] = write_delta[1] = write_delta[2] = 0;
        write_delta[3] = 1;
        write_delta_degrees = 0;
        return 1;
    }
    write_delta[0] = write_delta[2] = 0;
    write_delta[1] = t[1] * s[3] - t[3] * s[1];
    write_delta[3] = t[3] * s[3] + t[1] * s[1];
    write_delta_degrees = 2 * write_atan2(write_delta[1], write_delta[3]) * 57.2957795130823208768;
    return 1;
}
static int write_plan_points(const float *target, float points[][3]) {
    double b = write_delta[1], w = write_delta[3], cosine = 1 - 2 * b * b, sine = 2 * b * w;
    for (unsigned i = 0; i < (unsigned)write_count; ++i) {
        float original[3], q[4], angle = 0;
        memcpy(original, write_fixture[i].position, 12);
        memcpy(q, write_fixture[i].rotation, 16);
        memcpy(&angle, &write_fixture[i].angle, 4);
        if (write_delta_degrees == 0) {
            for (unsigned k = 0; k < 3; ++k)
                points[i][k] = original[k] + target[k] - write_source_position[k];
            memcpy(write_target_rotations[i], q, 16);
            write_target_angles[i] = angle;
        } else {
            double x = (double)original[0] - write_source_position[0],
                   z = (double)original[2] - write_source_position[2];
            points[i][0] = (float)(target[0] + cosine * x + sine * z);
            points[i][1] = original[1] + target[1] - write_source_position[1];
            points[i][2] = (float)(target[2] - sine * x + cosine * z);
            double r[4];
            if (!normalized_quaternion(q, r))
                return 0;
            write_target_rotations[i][0] = (float)(w * r[0] + b * r[2]);
            write_target_rotations[i][1] = (float)(w * r[1] + b * r[3]);
            write_target_rotations[i][2] = (float)(w * r[2] - b * r[0]);
            write_target_rotations[i][3] = (float)(w * r[3] - b * r[1]);
            double a = angle + write_delta_degrees;
            while (a < 0)
                a += 360;
            while (a >= 360)
                a -= 360;
            write_target_angles[i] = (float)a;
        }
        if (!finite_vector(points[i], 3) || !finite_vector(write_target_rotations[i], 4))
            return 0;
    }
    return 1;
}
/* Close only the game's instance Raise<T>(ref T) method. Reflection and
 * Type[] initialization follow the previously tested snapshot getter path. */
static void *write_raise_method(void *busClass, void *eventClass, PFRoots *roots) {
    void *it = 0;
    void *m;
    void *generic = 0;
    while ((m = p_class_get_methods(busClass, &it))) {
        if (equals(p_method_get_name(m), "Raise") && p_method_get_class(m) == busClass &&
            p_method_is_instance(m) && p_method_is_generic(m) && p_method_get_param_count(m) == 1 &&
            pf_type(p_method_get_return_type(m), "System.Void")) {
            if (generic)
                return 0;
            generic = m;
        }
    }
    void *systemType = findclass(p_domain_get(), "System", "Type");
    void *arrayClass = findclass(p_domain_get(), "System", "Array");
    void *reflectionClass = findclass(p_domain_get(), "System.Reflection", "MethodInfo");
    if (!generic || !systemType || !arrayClass || !reflectionClass)
        return 0;
    const char *setParams[] = {"System.Object", "System.Int32"};
    void *set = pf_method(arrayClass, "SetValue", "System.Void", 2, setParams, 1);
    void *type = p_type_get_object(p_class_get_type(eventClass));
    void *array = p_array_new(systemType, 1);
    if (!set || !type || !array || !pf_root(roots, type) || !pf_root(roots, array))
        return 0;
    int index = 0;
    void *setArgs[] = {type, &index};
    if (!write_void(set, array, setArgs))
        return 0;
    void *reflection = p_method_get_object(generic, busClass);
    if (!reflection ||
        !p_class_is_assignable_from(reflectionClass, p_object_get_class(reflection)) ||
        !pf_root(roots, reflection))
        return 0;
    const char *makeParams[] = {"System.Type[]"};
    void *make = pf_method(reflectionClass, "MakeGenericMethod", "System.Reflection.MethodInfo", 1,
                           makeParams, 1);
    void *actualMake = make ? p_object_get_virtual_method(reflection, make) : 0;
    void *args[] = {array};
    void *closedObject = pf_invoke(actualMake, reflection, args);
    if (!closedObject ||
        !p_class_is_assignable_from(reflectionClass, p_object_get_class(closedObject)) ||
        !pf_root(roots, closedObject))
        return 0;
    void *closed = p_method_get_from_reflection(closedObject);
    if (!closed || !p_method_is_inflated(closed) || p_method_get_class(closed) != busClass ||
        !p_method_is_instance(closed) || p_method_is_generic(closed) ||
        !equals(p_method_get_name(closed), "Raise") || p_method_get_param_count(closed) != 1 ||
        !pf_type(p_method_get_param(closed, 0), "TPS.Game.GameEvent.TrackRide.TrackRevert&") ||
        !pf_type(p_method_get_return_type(closed), "System.Void"))
        return 0;
    void *code = 0;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), closed, &code, sizeof(code), &got) ||
        got != sizeof(code) || !is_executable_game_pointer(code))
        return 0;
    void *f = pf_field(eventClass, "TrackRideEntity", "Unity.Entities.Entity");
    return f && write_value_layout(eventClass, 8) &&
                   write_value_layout(p_class_from_type(p_field_get_type(f)), 8)
               ? closed
               : 0;
}
static void *write_event_bus(PFRoots *roots) {
    void *bc = findclass(p_domain_get(), "TPS.Core", "GameEvents");
    void *parent = bc ? p_class_get_parent(bc) : 0;
    if (!parent || !pf_class(parent, "TPS.Core.StaticGameSystem<TPS.Core.GameEvents>"))
        return 0;
    void *f = p_class_get_field_from_name(parent, "<Instance>k__BackingField");
    void *bus = 0;
    if (!f || p_field_get_parent(f) != parent || !(p_field_get_flags(f) & 0x10) ||
        !pf_type(p_field_get_type(f), "TPS.Core.GameEvents"))
        return 0;
    p_field_static_get_value(f, &bus);
    return bus && p_object_get_class(bus) == bc && (!roots || pf_root(roots, bus)) ? bus : 0;
}
static void *write_context(void *station, const unsigned *entity, PFRoots *roots, void **outBus) {
    void *bc = findclass(p_domain_get(), "TPS.Core", "GameEvents");
    void *ec = pf_event_class();
    void *parent = bc ? p_class_get_parent(bc) : 0;
    if (!parent || !ec || !pf_class(parent, "TPS.Core.StaticGameSystem<TPS.Core.GameEvents>"))
        return 0;
    void *f = p_class_get_field_from_name(parent, "<Instance>k__BackingField");
    void *bus = 0;
    if (!f || p_field_get_parent(f) != parent || !(p_field_get_flags(f) & 0x10) ||
        !pf_type(p_field_get_type(f), "TPS.Core.GameEvents"))
        return 0;
    p_field_static_get_value(f, &bus);
    if (!bus || p_object_get_class(bus) != bc)
        return 0;
    *outBus = bus;
    const char *status = 0;
    void *c = pf_binding(bus, bc, ec, station, entity, roots, &status);
    pf_log("write-binding", status);
    return c;
}
static int write_normal_matches(void *config, PFRoots *roots) {
    void *cc = p_object_get_class(config);
    void *m = pf_method(cc, "GetNormalTrackPiece", "TPS.Game.TrackPieceDefinition", 0, 0, 1);
    void *d = pf_invoke(m, config, 0);
    void *expected = m ? p_class_from_type(p_method_get_return_type(m)) : 0;
    if (!d || !expected || !p_class_is_assignable_from(expected, p_object_get_class(d)) ||
        !pf_root(roots, d))
        return 0;
    void *getter = get_station_method(p_object_get_class(d), "get_ID", "System.Int64");
    void *box = pf_invoke(getter, d, 0);
    if (!box || !pf_class(p_object_get_class(box), "System.Int64") ||
        !write_value_layout(p_object_get_class(box), 8))
        return 0;
    void *v = p_object_unbox(box);
    long long id = 0;
    if (!v)
        return 0;
    memcpy(&id, v, 8);
    logtext("FIRST WRITE: NORMAL PIECE source=");
    if (WRITE_NORMAL_ID < 0)
        logtext("-");
    lognum(
        (WRITE_NORMAL_ID < 0 ? (UINT_PTR)(-(WRITE_NORMAL_ID + 1)) + 1 : (UINT_PTR)WRITE_NORMAL_ID));
    logtext(" target=");
    if (id < 0)
        logtext("-");
    lognum((id < 0 ? (UINT_PTR)(-(id + 1)) + 1 : (UINT_PTR)id));
    nl();
    return id == WRITE_NORMAL_ID;
}
static void *write_snapshot_data(void *snapshot, void *f, PFRoots *roots) {
    void *d = p_field_get_value_object(f, snapshot);
    void *c = p_class_from_type(p_field_get_type(f));
    return d && p_object_get_class(d) == c && pf_root(roots, d) ? d : 0;
}
static int write_counts(void *data, void *dc, int *supports, int *pieces) {
    void *s = pf_ref(data, dc, "TrackSupportData",
                     "System.Collections.Generic.List<TPS.Game.TrackSupportSnapshotData>");
    void *p = pf_ref(data, dc, "TrackPieceData",
                     "System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>");
    PFRoots lists = {0};
    int ok = s && p && pf_root(&lists, s) && pf_root(&lists, p) &&
             invoke_count(s, p_object_get_class(s), supports) &&
             invoke_count(p, p_object_get_class(p), pieces);
    pf_release(&lists);
    return ok;
}
static void *write_new_list(void *c, PFRoots *roots) {
    void *ctor = pf_method(c, ".ctor", "System.Void", 0, 0, 1);
    if (!ctor)
        return 0;
    void *o = p_object_new(c);
    return o && p_object_get_class(o) == c && pf_root(roots, o) && write_void(ctor, o, 0) ? o : 0;
}
static void *write_make_data(void *dc, float points[][3], unsigned long long floorId,
                             PFRoots *roots) {
    void *sf = pf_field(dc, "TrackSupportData",
                        "System.Collections.Generic.List<TPS.Game.TrackSupportSnapshotData>");
    void *pf = pf_field(dc, "TrackPieceData",
                        "System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>");
    if (!sf || !pf || !write_value_layout(dc, 16))
        return 0;
    void *fieldIter = 0;
    void *each;
    unsigned fieldCount = 0;
    while ((each = p_class_get_fields(dc, &fieldIter)))
        if (!(p_field_get_flags(each) & 0x10))
            ++fieldCount;
    if (fieldCount != 2)
        return 0;
    void *sclass = p_class_from_type(p_field_get_type(sf));
    void *pclass = p_class_from_type(p_field_get_type(pf));
    const char *sp[] = {"TPS.Game.TrackSupportSnapshotData"};
    const char *pp[] = {"TPS.Game.TrackPieceSnapshotData"};
    void *sa = pf_method(sclass, "Add", "System.Void", 1, sp, 1);
    void *pa = pf_method(pclass, "Add", "System.Void", 1, pp, 1);
    void *sc = sa ? p_class_from_type(p_method_get_param(sa, 0)) : 0;
    void *pc = pa ? p_class_from_type(p_method_get_param(pa, 0)) : 0;
    if (!sa || !pa || !write_schema(sc, supportSchema, 11, 80) ||
        !write_schema(pc, pieceSchema, 4, 32))
        return 0;
    unsigned char zero[80] = {0};
    void *data = p_value_box(dc, zero);
    if (!data || p_object_get_class(data) != dc || !pf_root(roots, data))
        return 0;
    void *sl = write_new_list(sclass, roots);
    void *pl = write_new_list(pclass, roots);
    if (!sl || !pl)
        return 0;
    /* Reference fields use the object setter. SnapshotData itself is a value
   * type and is assigned later using the value setter and an unboxed pointer. */
    p_field_set_value_object(data, sf, sl);
    p_field_set_value_object(data, pf, pl);
    for (int i = 0; i < write_count; ++i) {
        PFRoots recordRoots = {0};
        void *s = p_value_box(sc, zero);
        if (!s || !pf_root(&recordRoots, s)) {
            pf_release(&recordRoots);
            return 0;
        }
        int height = write_fixture[i].height;
        unsigned char valid = 1;
        int ok = 1;
        ok = ok && write_set(s, sc, supportSchema, 0, &i) &&
             write_set(s, sc, supportSchema, 1, points[i]);
        ok = ok && write_set(s, sc, supportSchema, 2, write_target_rotations[i]) &&
             write_set(s, sc, supportSchema, 3, &write_target_angles[i]);
        ok = ok && write_set(s, sc, supportSchema, 4, points[i]) &&
             write_set(s, sc, supportSchema, 5, &floorId);
        ok = ok && write_set(s, sc, supportSchema, 6, &valid) &&
             write_set(s, sc, supportSchema, 7, &height);
        for (unsigned k = 8; k < 11; ++k)
            ok = ok && write_set(s, sc, supportSchema, k, &write_fixture[i].modifiers[k - 8]);
        void *sv = ok ? p_object_unbox(s) : 0;
        void *args[] = {sv};
        ok = ok && sv && write_void(sa, sl, args);
        void *p = ok ? p_value_box(pc, zero) : 0;
        if (!p || !pf_root(&recordRoots, p))
            ok = 0;
        long long pieceId = write_fixture[i].piece, decoration = write_fixture[i].decoration;
        float decorationPos = 0;
        memcpy(&decorationPos, &write_fixture[i].decorationPosition, 4);
        ok = ok && write_set(p, pc, pieceSchema, 0, &i) &&
             write_set(p, pc, pieceSchema, 1, &pieceId) &&
             write_set(p, pc, pieceSchema, 2, &decoration) &&
             write_set(p, pc, pieceSchema, 3, &decorationPos);
        void *pv = ok ? p_object_unbox(p) : 0;
        args[0] = pv;
        ok = ok && pv && write_void(pa, pl, args);
        pf_release(&recordRoots);
        if (!ok)
            return 0;
    }
    int s = 0, p = 0;
    return write_counts(data, dc, &s, &p) && s == write_count && p == write_count ? data : 0;
}
/* Topmost lookup can return scenery/platform floors over the target floor.
 * Ask the native floor's exact grid containment before treating that as outside. */
static int write_floor_contains(void *floor, float point[3]) {
    void *utils = findclass(p_domain_get(), "TPS.Game", "FloorAreaUtils");
    const char *params[] = {"TPS.Game.FloorAreaInstance", "UnityEngine.Vector3"};
    void *m = utils ? pf_method(utils, "ContainsWorldPosition", "System.Boolean", 2, params, 0) : 0;
    void *args[] = {floor, point};
    void *result = floor && finite_vector(point, 3) ? pf_invoke(m, 0, args) : 0;
    void *value = result && pf_class(p_object_get_class(result), "System.Boolean")
                      ? p_object_unbox(result) : 0;
    return value && *(unsigned char *)value == 1;
}
static int write_same_floor_points(void *floor, float points[][3]) {
    void *utils = findclass(p_domain_get(), "TPS.Game", "FloorAreaUtils");
    const char *params[] = {"UnityEngine.Vector3", "System.Boolean", "System.Boolean"};
    void *m = utils ? pf_method(utils, "GetTopMostFloorAreaAtWorldPosition",
                                "TPS.Game.FloorAreaInstance", 3, params, 0)
                    : 0;
    if (!m)
        return 0;
    unsigned char no = 0;
    for (unsigned i = 0; i < (unsigned)write_count; ++i) {
        void *args[] = {points[i], &no, &no};
        if (pf_invoke(m, 0, args) != floor && !write_floor_contains(floor, points[i])) {
            logtext("FIRST WRITE: FLOOR: support index outside target floor=");
            lognum(i);
            nl();
            return 0;
        }
    }
    return 1;
}
static int write_restore_snapshot(void *snapshot, void *field, void *oldData,
                                  unsigned char oldFlag) {
    void *v = p_object_unbox(oldData);
    if (!v)
        return 0;
    p_field_set_value(snapshot, field, v);
    p_field_set_value(snapshot, g_snapshot_flag_field, &oldFlag);
    unsigned char restored[16] = {0}, flag = 0;
    p_field_get_value(snapshot, field, restored);
    p_field_get_value(snapshot, g_snapshot_flag_field, &flag);
    for (unsigned i = 0; i < 16; ++i)
        if (restored[i] != ((unsigned char *)v)[i])
            return 0;
    return flag == oldFlag;
}
/* Track the exact delayed wrapper; absence is followed by live-buffer checks,
 * and is not alone treated as proof that its handler executed. */
static void *write_pending_ticket;
static void *write_queue_list(void *bus, const char *name) {
    void *c = p_object_get_class(bus);
    return pf_ref(bus, c, name,
                  "System.Collections.Generic.List<TPS.Core.GameEvents.IDelayedGameEvent>");
}
static int write_queue_items(void *list, void *ticket, void **last, int *contains, int *count,
                             int describe) {
    if (!list)
        return 0;
    void *c = p_object_get_class(list);
    if (!invoke_count(list, c, count) || *count > 256)
        return 0;
    const char *ip[] = {"System.Int32"};
    void *get = pf_method(c, "get_Item", "TPS.Core.GameEvents.IDelayedGameEvent", 1, ip, 1);
    if (!get)
        return 0;
    *contains = 0;
    *last = 0;
    for (int i = 0; i < *count; ++i) {
        void *args[] = {&i};
        void *item = pf_invoke(get, list, args);
        if (!item)
            return 0;
        if (item == ticket)
            *contains = 1;
        *last = item;
        if (describe) {
            char *name = p_type_get_name(p_class_get_type(p_object_get_class(item)));
            logtext("FIRST WRITE: PENDING EVENT ");
            lognum((UINT_PTR)i);
            logtext(": ");
            logtext(name ? name : "<unresolved>");
            nl();
            if (name)
                p_free(name);
        }
    }
    int after = -1;
    return invoke_count(list, c, &after) && after == *count;
}
static int write_queue_state(void *bus, void *ticket, int *waiting, int *active, int describe) {
    void *c = p_object_get_class(bus);
    void *f = pf_field(c, "_processingEvents", "System.Boolean");
    void *g = pf_field(c, "_processingEvent", "System.Boolean");
    unsigned char processing = 1, event = 1;
    if (!f || !g)
        return 0;
    p_field_get_value(bus, f, &processing);
    p_field_get_value(bus, g, &event);
    if (processing || event)
        return 0;
    const char *names[] = {"_raisedEvents", "_raisingEvents", "_raisedImmediateEvents"};
    *waiting = 0;
    *active = 0;
    for (unsigned i = 0; i < 3; ++i) {
        int count = 0, contains = 0;
        void *last = 0;
        if (!write_queue_items(write_queue_list(bus, names[i]), ticket, &last, &contains, &count,
                               describe))
            return 0;
        *waiting += count;
        *active |= contains;
    }
    return 1;
}
/* Only the observed museum stat notification may coexist with an import.
 * Never drain, delete or reorder the game's events. All unknown wrappers block.
 * After dispatch, require exactly one appended TrackRevert in the incoming list. */
static int write_import_queue_ready(void *bus, void **submitted) {
    int total = 0, active = 0;
    if (submitted)
        *submitted = 0;
    if (!write_queue_state(bus, 0, &total, &active, 0))
        return 0;
    const char *names[] = {"_raisedEvents", "_raisingEvents", "_raisedImmediateEvents"};
    void *found = 0;
    for (unsigned q = 0; q < 3; ++q) {
        void *list = write_queue_list(bus, names[q]);
        int count = 0;
        if (!list || !invoke_count(list, p_object_get_class(list), &count) || count < 0 ||
            count > 256)
            return 0;
        const char *ip[] = {"System.Int32"};
        void *get = pf_method(p_object_get_class(list), "get_Item",
                              "TPS.Core.GameEvents.IDelayedGameEvent", 1, ip, 1);
        if (!get)
            return 0;
        for (int i = 0; i < count; ++i) {
            void *a[] = {&i};
            void *item = pf_invoke(get, list, a);
            if (!item)
                return 0;
            void *c = p_object_get_class(item);
            if (pf_class(
                    c,
                    "TPS.Core.GameEvents.DelayedGameEvent<TPS.Game.GameEvent.MuseumEvent.StatChanged>"))
                continue;
            if (submitted && q == 0 && i == count - 1 && !found &&
                pf_class(
                    c,
                    "TPS.Core.GameEvents.DelayedGameEvent<TPS.Game.GameEvent.TrackRide.TrackRevert>")) {
                found = item;
                continue;
            }
            return 0;
        }
        int after = -1;
        if (!invoke_count(list, p_object_get_class(list), &after) || after != count)
            return 0;
    }
    int after = 0;
    if (!write_queue_state(bus, 0, &after, &active, 0) || after != total)
        return 0;
    if (submitted) {
        if (!found)
            return 0;
        *submitted = found;
    }
    return 1;
}
/* Queued operations retain their managed roots until verification or timeout. */
static PFRoots write_pending_roots;
#include "coaster_colour.inc"
static void *write_pending_bus, *write_pending_context, *write_pending_station,
    *write_pending_snapshot, *write_pending_data_field, *write_pending_dc,
    *write_pending_snap_method;
static void *write_pending_cancel_data;
static unsigned char write_pending_cancel_flag;
static unsigned write_pending_entity[2];
static UINT_PTR write_queued_tick;
static long write_queued_callback;
static int write_pending;
/* A failed write retains its exact event bus until an explicit fresh retry proves it idle. */
static PFRoots write_retry_roots;
static void *write_retry_bus;
static int write_retry_missing;
static void write_release_retry(void) {
    pf_release(&write_retry_roots);
    write_retry_bus = 0;
    write_retry_missing = 0;
}
static void write_retain_retry(void *bus) {
    if (write_retry_bus == bus && bus)
        return;
    if (write_retry_bus || !bus || !pf_root(&write_retry_roots, bus)) {
        write_retry_missing = 1;
        return;
    }
    write_retry_bus = bus;
}
static int write_retry_idle(void) {
    if (write_pending || write_retry_missing || (write_attempted && !write_retry_bus))
        return 0;
    if (!write_retry_bus)
        return 1;
    int total = 0, active = 0;
    return write_queue_state(write_retry_bus, 0, &total, &active, 0) && total == 0 && !active;
}
static int write_apply_plan(void *snapMethod, void *raiseMethod, void *bus, void *context,
                            void *station, const unsigned *entity, void *floor, float points[][3],
                            void *snapshot, void *dataField, void *oldData, unsigned char oldFlag,
                            void *plan, void *dc, PFRoots *roots) {
    if (write_attempted)
        return 0;
    write_attempted = 1;
    int dispatched = 0, ok = 0;
    write_log("STAGE 1: capture current target tracks to verify emptiness");
    if (!write_void(snapMethod, context, 0)) {
        write_log("STOP: target snapshot capture exception");
        goto finish;
    }
    void *empty = write_snapshot_data(snapshot, dataField, roots);
    int supports = 0, pieces = 0;
    if (!empty || !write_counts(empty, dc, &supports, &pieces) || supports != 0 || pieces != 0) {
        write_log("SKIP: target is not confirmed empty; no reconstruction queued");
        goto finish;
    }
    unsigned now[2] = {0};
    p_field_get_value(station, g_entity_field, now);
    if (!pf_same_entity(now, entity) || !write_same_floor_points(floor, points)) {
        write_log("STOP: target identity or floors changed");
        goto finish;
    }
    void *value = p_object_unbox(plan);
    if (!value) {
        write_log("STOP: planned snapshot unbox failed");
        goto finish;
    }
    p_field_set_value(snapshot, dataField, value);
    unsigned char yes = 1;
    p_field_set_value(snapshot, g_snapshot_flag_field, &yes);
    void *staged = write_snapshot_data(snapshot, dataField, roots);
    if (!staged || !write_counts(staged, dc, &supports, &pieces) || supports != write_count ||
        pieces != write_count) {
        write_log("STOP: staged snapshot readback differs");
        goto finish;
    }
    write_log(
        "STAGE 2: queue TrackRevert for the exact target entity; game owns reconstruction timing");
    dispatched = 1;
    void *eventArgs[] = {(void *)entity};
    if (!write_void(raiseMethod, bus, eventArgs)) {
        write_log(
            "DISPATCH FAILED/UNKNOWN: event raise exception; discard test save; no automatic rollback");
        goto finish;
    }
    ok = 1;
finish:
    if (!dispatched) {
        write_log(write_restore_snapshot(snapshot, dataField, oldData, oldFlag)
                      ? "Original target snapshot restored; reconstruction was not queued"
                      : "STOP: original snapshot restoration failed; discard test save");
    }
    return ok;
}
static int write_verify_live(void *snapMethod, void *context, void *snapshot, void *dataField,
                             void *dc, PFRoots *roots) {
    write_log("STAGE 3: capture resulting live tracks after submitted wrapper left the game queue");
    if (!write_void(snapMethod, context, 0)) {
        write_log("UNVERIFIED: post-write snapshot exception; discard test save");
        return 0;
    }
    void *actual = write_snapshot_data(snapshot, dataField, roots);
    int supports = 0, pieces = 0;
    if (!actual || !write_counts(actual, dc, &supports, &pieces) || supports != write_count ||
        pieces != write_count) {
        write_log("UNVERIFIED: live support/piece counts differ; discard test save");
        return 0;
    }
    logtext("FIRST WRITE: VERIFIED live supports=");
    lognum((UINT_PTR)supports);
    logtext(" pieces=");
    lognum((UINT_PTR)pieces);
    nl();
    write_log(
        "COUNT CHECK PASSED: expected live supports and pieces; geometry/buildability still require inspection");
    return 1;
}
static int write_queue_count(void *getter, void *bus, int *out) {
    void *box = pf_invoke(getter, bus, 0);
    if (!box) {
        write_log("QUEUE READ FAILED: getter returned null or raised a managed exception");
        return 0;
    }
    void *cls = p_object_get_class(box);
    if (!pf_class(cls, "System.Int32")) {
        write_log("QUEUE READ FAILED: boxed result is not System.Int32");
        return 0;
    }
    unsigned alignment = 0;
    int bytes = p_class_value_size(cls, &alignment);
    logtext("FIRST WRITE: QUEUE result value-type=");
    lognum((UINT_PTR)p_class_is_valuetype(cls));
    logtext(" size=");
    if (bytes < 0)
        logtext("-");
    lognum((UINT_PTR)(bytes < 0 ? -(long long)bytes : bytes));
    nl();
    if (!write_value_layout(cls, 4)) {
        write_log("QUEUE READ FAILED: System.Int32 value layout is not four bytes");
        return 0;
    }
    void *v = p_object_unbox(box);
    if (!v) {
        write_log("QUEUE READ FAILED: boxed System.Int32 could not be unboxed");
        return 0;
    }
    memcpy(out, v, 4);
    logtext("FIRST WRITE: QUEUE pending-event count=");
    if (*out < 0)
        logtext("-");
    lognum((UINT_PTR)(*out < 0 ? -(long long)*out : *out));
    nl();
    if (*out < 0 || *out > 100000) {
        write_log("QUEUE READ FAILED: pending count is outside validated range 0..100000");
        return 0;
    }
    return 1;
}
/* Caller supplies reviewed static marker and BuildSpline methods after queue
 * completion. Recheck station identity immediately before the one-shot call. */
static int write_recalculation_dispatch(void *station, const unsigned *entity, void *method,
                                        void *splineMethod) {
    if (!station || !method || !splineMethod || !entity || !entity[0] || !entity[1] ||
        !g_entity_field)
        return 0;
    unsigned current[2] = {0};
    p_field_get_value(station, g_entity_field, current);
    if (!pf_same_entity(current, entity))
        return 0;
    unsigned char rebuild = 1;
    void *args[] = {(void *)entity, &rebuild};
    if (!write_void(method, 0, args))
        return 0;
    int startIndex = 0;
    unsigned char refresh = 1;
    void *splineArgs[] = {station, &startIndex, &refresh};
    return write_void(splineMethod, 0, splineArgs);
}
static int write_request_recalculation(void *station, const unsigned *entity) {
    void *c = findclass(p_domain_get(), "TPS.Game", "TrackRideUtils");
    const char *params[] = {"Unity.Entities.Entity", "System.Boolean"};
    void *m = c ? pf_method(c, "MarkTrackRideForRebuild", "System.Void", 2, params, 0) : 0;
    if (!write_code_guard(m, 0x2ebb430, 0x120, 0xf10947f3U)) {
        write_log("STATS REFRESH SKIP: native rebuild marker differs from reviewed binary");
        return 0;
    }
    const char *splineParams[] = {"TPS.Game.GameItemInstance", "System.Int32", "System.Boolean"};
    void *spline = c ? pf_method(c, "BuildSpline", "System.Void", 3, splineParams, 0) : 0;
    if (!write_code_guard(spline, 0x2ead6b0, 0x1270, 0x621a450eU)) {
        write_log("STATS REFRESH SKIP: native spline refresh differs from reviewed binary");
        return 0;
    }
    if (!write_recalculation_dispatch(station, entity, m, spline)) {
        write_log(
            "STATS REFRESH unavailable: station identity changed or native refresh call failed");
        return 0;
    }
    write_log(
        "STATS REFRESH: exact verified coaster spline rebuilt once through native calculation-dirty path");
    return 1;
}
static void write_finish_pending(int verified) {
    logtext("FIRST WRITE: queued-to-verification ms=");
    lognum(GetTickCount64() - write_queued_tick);
    nl();
    /* Release operation-owned references before enabling the next request. */
    if (!verified)
        write_retain_retry(write_pending_bus);
    placement_import_finished(verified, write_pending_context);
    write_pending = 0;
    write_pending_ticket = 0;
    pf_release(&write_pending_roots);
    write_pending_bus = write_pending_context = write_pending_station = write_pending_snapshot = 0;
    write_pending_data_field = write_pending_dc = write_pending_snap_method =
        write_pending_cancel_data = 0;
    write_pending_cancel_flag = 0;
    if (verified) {
        write_release_retry();
        write_attempted = 0;
        write_log("READY: next import allowed into a fresh empty station");
    } else {
        write_attempted = 1;
        write_log(
            "Previous outcome unverified; fresh placement retry available after its event queue is idle");
    }
}
static void write_phase_time(const char *phase, long long start) {
    log_stamp();
    logtext("IMPORT PERF phase=");
    logtext(phase);
    logtext(" us=");
    lognum(perf_us(start));
    nl();
}
static void service_pending_write(void) {
    if (write_pending != 1)
        return;
    int verified = 0;
    if (GetTickCount64() - write_queued_tick > 10000) {
        write_log(
            "UNVERIFIED: submitted event was not verified within 10 seconds; queued outcome unknown; discard test save");
        goto finish;
    }
    if (__atomic_load_n(&g_hook_count, __ATOMIC_RELAXED) - write_queued_callback < 2)
        return;
    int waiting = 0, active = 0;
    if (!write_pending_ticket ||
        !write_queue_state(write_pending_bus, write_pending_ticket, &waiting, &active, 0)) {
        write_log("UNVERIFIED: exact event queue state unavailable; discard test save");
        goto finish;
    }
    if (active)
        return;
    write_log(
        "Submitted wrapper is absent from waiting/processing lists; verifying live track (absence alone is not handler acknowledgement)");
    write_pending = 2;
    log_stamp();
    logtext("IMPORT PERF queue_wait_ms=");
    lognum(GetTickCount64() - write_queued_tick);
    nl();
    long long phaseStart = perf_now();
    unsigned now[2] = {0};
    p_field_get_value(write_pending_station, g_entity_field, now);
    void *args[] = {write_pending_entity};
    void *exists = pf_invoke(g_exists_method, 0, args);
    void *ev = exists ? p_object_unbox(exists) : 0;
    if (!pf_same_entity(now, write_pending_entity) || !ev || *(unsigned char *)ev != 1) {
        write_log("UNVERIFIED: target entity changed or disappeared; discard test save");
        goto finish;
    }
    if (write_verify_live(write_pending_snap_method, write_pending_context, write_pending_snapshot,
                          write_pending_data_field, write_pending_dc, &write_pending_roots)) {
        /* Counts/identity were just verified. A second diagnostic snapshot
         * walk + JSON export is optional tooling, not an import safety check. */
        verified = 1;
    }
    write_phase_time("live-snapshot-verification", phaseStart);
    phaseStart = perf_now();
    /* Reconstruction and verification borrow the cancellation snapshot. Restore
   * its pre-import data/flag only after the exact queue is idle and identity
   * was revalidated. This does not dispatch Revert or modify live track. */
    if (!write_pending_cancel_data ||
        !write_restore_snapshot(write_pending_snapshot, write_pending_data_field,
                                write_pending_cancel_data, write_pending_cancel_flag)) {
        verified = 0;
        write_log("UNVERIFIED: native cancellation baseline restoration failed; discard test save");
    } else {
        write_log(
            "Native construction cancellation baseline restored after verification; live track retained");
        write_phase_time("restore-cancellation-baseline", phaseStart);
        phaseStart = perf_now();
        if (verified &&
            !coaster_colour_apply(write_pending_station, write_pending_entity, &colour_pending))
            write_log("COLOUR: restoration unavailable; imported track retained");
        write_phase_time("restore-colour", phaseStart);
        phaseStart = perf_now();
        if (verified && !write_request_recalculation(write_pending_station, write_pending_entity))
            write_log(
                "Track import verified; stats refresh was unavailable and was not retried automatically");
        write_phase_time("native-spline-refresh", phaseStart);
    }
finish:
    write_finish_pending(verified);
    write_log("POST-QUEUE CHECK END; inspect log and coaster before manual save");
}
static void run_first_write(void *station) {
    long long prepareStart = perf_now();
    PFRoots roots = {0};
    void *bus = 0;
    void *snapshot = 0;
    void *dataField = 0;
    void *oldData = 0;
    unsigned char oldFlag = 0;
    write_log("BEGIN selected-template position-and-heading import experiment; no automatic save");
    if (write_pending) {
        write_log(
            "SKIP: previous import is still pending verification; wait for POST-QUEUE CHECK END");
        return;
    }
    if (write_attempted) {
        write_log(
            "SKIP: previous write was not verified; return to selection mode and start a fresh library placement");
        return;
    }
    if (!write_load_selection()) {
        write_log("SKIP: no valid selected template; target untouched");
        return;
    }
    if (!station || !g_managed_fields_verified || !g_snapshot_reader_verified ||
        !g_snapshot_flag_verified || !g_support_world_verified ||
        !p_class_is_assignable_from(g_gameitem_class, p_object_get_class(station)) ||
        !pf_root(&roots, station)) {
        write_log("SKIP: station/export/orientation guards unavailable");
        goto done;
    }
    unsigned entity[2] = {0};
    unsigned long long id = 0;
    p_field_get_value(station, g_entity_field, entity);
    p_field_get_value(station, g_id_field, &id);
    if (!entity[0] || !entity[1] || !g_exists_verified) {
        write_log("SKIP: entity unavailable");
        goto done;
    }
    void *ea[] = {entity};
    void *exists = pf_invoke(g_exists_method, 0, ea);
    void *ev = exists ? p_object_unbox(exists) : 0;
    if (!ev || *(unsigned char *)ev != 1 || !probe_component_presence((unsigned char *)entity)) {
        write_log("SKIP: entity/components missing");
        goto done;
    }
    station_frame(station);
    floor_frame(station);
    if (!g_frame || !g_floor_frame_read || !write_target_frame(id, g_origin, g_rotation)) {
        write_log("SKIP: target must differ from source, have an upright station rotation");
        goto done;
    }
    for (unsigned i = 0; i < 4; ++i) {
        double wanted = i == 3 ? 1 : 0, delta = g_floor_rotation[i] - wanted;
        if (delta > 0.00001 || delta < -0.00001) {
            write_log("SKIP: rotated target floor unsupported");
            goto done;
        }
    }
    void *context = write_context(station, entity, &roots, &bus);
    if (!context) {
        write_log("SKIP: exact target binding unavailable");
        goto done;
    }
    void *cc = p_object_get_class(context);
    void *config = pf_ref(context, cc, "rideConfig", "TPS.Game.TrackRideConfig");
    void *global = pf_ref(context, cc, "ridesConfig", "TPS.Game.TrackRidesConfig");
    if (!config || !global || !pf_root(&roots, config) || !pf_root(&roots, global) ||
        (WRITE_NORMAL_ID && !write_normal_matches(config, &roots)) ||
        !write_configuration_matches(config, &roots) ||
        !pf_definition(config, p_object_get_class(config), "SupportDefinition",
                       "TPS.Game.GameItemDefinitionID", "TPS.Game.GameItemDefinition") ||
        !pf_definition(global, p_object_get_class(global), "TrackPieceEntityDefinition",
                       "TPS.Game.LightweightEntityDefinitionID",
                       "TPS.Game.LightweightEntityDefinition")) {
        write_log("SKIP: selected template piece/configuration definitions do not match");
        goto done;
    }
    void *floorGetter = get_station_method(p_object_get_class(station), "get_FloorArea",
                                           "TPS.Game.FloorAreaInstance");
    void *floor = pf_invoke(floorGetter, station, 0);
    if (!floor || !pf_root(&roots, floor) ||
        !p_class_is_assignable_from(g_managed_class, p_object_get_class(floor)) ||
        !equals(pf_floor(station, &roots), "pass-station-floor-matches-world-query")) {
        write_log("SKIP: target floor unresolved");
        goto done;
    }
    unsigned long long floorId = 0;
    p_field_get_value(floor, g_id_field, &floorId);
    static float points[WRITE_MAX_SUPPORTS][3];
    if (!floorId || !write_plan_points(g_origin, points) ||
        !write_same_floor_points(floor, points)) {
        write_log("SKIP: one or more planned supports are outside the target floor");
        goto done;
    }
    void *snapMethod = pf_method(cc, "<SetComponents>g__Snapshot|30", "System.Void", 0, 0, 1);
    void *revertMethod = pf_method(cc, "<SetComponents>g__Revert|31", "System.Void", 0, 0, 1);
    if (!write_code_guard(snapMethod, 0x2ea8180, 0x778, WRITE_SNAPSHOT_HASH) ||
        !write_code_guard(revertMethod, 0x2ea8910, 0x18c2, WRITE_REVERT_HASH)) {
        write_log("SKIP: snapshot/restoration code differs from reviewed binary");
        goto done;
    }
    void *raise = write_raise_method(p_object_get_class(bus), pf_event_class(), &roots);
    void *pendingGetter =
        pf_method(p_object_get_class(bus), "get_NumEventsPending", "System.Int32", 0, 0, 1);
    if (!raise || !pendingGetter) {
        write_log("SKIP: queued TrackRevert dispatch or queue-status signature unavailable");
        goto done;
    }
    int queuedBefore = -1;
    if (!write_queue_count(pendingGetter, bus, &queuedBefore)) {
        write_log(
            "SKIP: event queue read failed; see QUEUE diagnostic above; no target snapshot changes");
        goto done;
    }
    int queueTotal = 0, queueActive = 0;
    if (!write_queue_state(bus, 0, &queueTotal, &queueActive, 1)) {
        write_log(
            "SKIP: queue processing state or wrapper list unavailable; no target snapshot changes");
        goto done;
    }
    if (!write_import_queue_ready(bus, 0)) {
        write_log(
            "SKIP: queued events contain an unsupported or active operation; target unchanged");
        goto done;
    }
    snapshot = pf_invoke(g_snapshot_reader, 0, ea);
    if (!snapshot || !p_class_is_assignable_from(g_snapshot_class, p_object_get_class(snapshot)) ||
        !pf_root(&roots, snapshot)) {
        write_log("SKIP: target snapshot unavailable");
        goto done;
    }
    dataField = pf_field(g_snapshot_class, "SnapshotData", "TPS.Game.TrackRideSnapshotData");
    if (!dataField) {
        write_log("SKIP: snapshot data field unavailable");
        goto done;
    }
    void *dc = p_class_from_type(p_field_get_type(dataField));
    if (!write_value_layout(dc, 16)) {
        write_log("SKIP: snapshot value layout differs");
        goto done;
    }
    oldData = write_snapshot_data(snapshot, dataField, &roots);
    p_field_get_value(snapshot, g_snapshot_flag_field, &oldFlag);
    if (!oldData || oldFlag > 1) {
        write_log("SKIP: cannot preserve existing snapshot");
        goto done;
    }
    long long constructionStart = perf_now();
    void *plan = write_make_data(dc, points, floorId, &roots);
    write_phase_time("managed-plan-construction", constructionStart);
    if (!plan) {
        write_log("SKIP: managed import plan construction failed; target untouched");
        goto done;
    }
    if (write_apply_plan(snapMethod, raise, bus, context, station, entity, floor, points, snapshot,
                         dataField, oldData, oldFlag, plan, dc, &roots)) {
        void *last = 0;
        if (!write_import_queue_ready(bus, &last) || !pf_root(&roots, last)) {
            write_log(
                "DISPATCH OUTCOME UNVERIFIED: exact submitted wrapper unavailable; discard test save");
            goto done;
        }
        colour_pending = colour_selected;
        write_pending_ticket = last;
        write_pending_roots = roots;
        roots.n = 0;
        write_pending_bus = bus;
        write_pending_context = context;
        write_pending_station = station;
        write_pending_snapshot = snapshot;
        write_pending_cancel_data = oldData;
        write_pending_cancel_flag = oldFlag;
        write_pending_data_field = dataField;
        write_pending_dc = dc;
        write_pending_snap_method = snapMethod;
        memcpy(write_pending_entity, entity, 8);
        write_queued_tick = GetTickCount64();
        write_queued_callback = __atomic_load_n(&g_hook_count, __ATOMIC_RELAXED);
        write_pending = 1;
        write_log("QUEUED: wait for POST-QUEUE CHECK END; do not save yet");
    }
done:
    if (write_attempted && !write_pending)
        write_retain_retry(bus);
    pf_release(&roots);
    write_phase_time("prepare-and-dispatch-total", prepareStart);
    write_log("END; inspect log and coaster before any manual save");
}

#include "import_plan.inc"

#include "import_compatibility.inc"
