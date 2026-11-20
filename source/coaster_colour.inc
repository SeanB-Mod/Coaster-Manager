/* Optional track tint: use the same ECS read/write and visual setter as the
 * game's TrackRideColorPickerMenu. All entry points are typed and code-guarded. */
typedef struct {
    int present;
    unsigned char custom;
    float rgba[4];
} CoasterColour;
static CoasterColour colour_selected, colour_pending, colour_saved;
static int colour_valid(const CoasterColour *c) {
    if (!c->present || c->custom > 1)
        return 0;
    for (unsigned i = 0; i < 4; ++i)
        if (!(c->rgba[i] >= 0 && c->rgba[i] <= 1))
            return 0;
    return 1;
}
static void *colour_close(const char *name, const char *ret, const char *arg, unsigned rva,
                          unsigned hash, PFRoots *roots) {
    void *manager = findclass(p_domain_get(), "TPS.Core.Entities", "EntityManager");
    void *method = 0, *it = 0, *m;
    if (!manager || !g_ride_class)
        return 0;
    while ((m = p_class_get_methods(manager, &it)))
        if (equals(p_method_get_name(m), name) && p_method_is_generic(m) &&
            !p_method_is_instance(m) && p_method_get_param_count(m) == 2 &&
            pf_type(p_method_get_param(m, 0), "Unity.Entities.Entity")) {
            if (method)
                return 0;
            method = m;
        }
    void *tc = findclass(p_domain_get(), "System", "Type");
    void *ac = findclass(p_domain_get(), "System", "Array");
    void *rc = findclass(p_domain_get(), "System.Reflection", "MethodInfo");
    if (!method || !tc || !ac || !rc)
        return 0;
    void *type = p_type_get_object(p_class_get_type(g_ride_class));
    void *array = p_array_new(tc, 1);
    const char *sp[] = {"System.Object", "System.Int32"};
    int zero = 0;
    void *set = pf_method(ac, "SetValue", "System.Void", 2, sp, 1), *sa[] = {type, &zero};
    if (!type || !array || !pf_root(roots, type) || !pf_root(roots, array) ||
        !write_void(set, array, sa))
        return 0;
    void *reflection = p_method_get_object(method, manager);
    if (!reflection || !p_class_is_assignable_from(rc, p_object_get_class(reflection)) ||
        !pf_root(roots, reflection))
        return 0;
    const char *mp[] = {"System.Type[]"};
    void *make = pf_method(rc, "MakeGenericMethod", "System.Reflection.MethodInfo", 1, mp, 1);
    void *actual = make ? p_object_get_virtual_method(reflection, make) : 0, *a[] = {array};
    void *closedObject = pf_invoke(actual, reflection, a);
    if (!closedObject || !p_class_is_assignable_from(rc, p_object_get_class(closedObject)) ||
        !pf_root(roots, closedObject))
        return 0;
    void *closed = p_method_get_from_reflection(closedObject);
    const char *params[] = {"Unity.Entities.Entity", arg};
    if (!closed || !p_method_is_inflated(closed) || p_method_is_generic(closed) ||
        p_method_get_class(closed) != manager || p_method_is_instance(closed) ||
        !equals(p_method_get_name(closed), name) || p_method_get_param_count(closed) != 2 ||
        !pf_type(p_method_get_return_type(closed), ret))
        return 0;
    for (unsigned i = 0; i < 2; ++i)
        if (!pf_type(p_method_get_param(closed, i), params[i])) {
            char *actual = p_type_get_name(p_method_get_param(closed, i));
            logtext("FIRST WRITE: COLOUR parameter mismatch: ");
            logtext(name);
            logtext(" actual=");
            logtext(actual ? actual : "unavailable");
            nl();
            if (actual)
                p_free(actual);
            return 0;
        }
    /* Typed, reflected ECS API: validate the complete inflated signature above.
     * The 248/256-byte component bound is checked before buffer use. */
    (void)rva;
    (void)hash;
    void *native = 0;
    unsigned char first = 0;
    if (!orientation_code(closed, &native, &first, 1)) {
        logtext("FIRST WRITE: COLOUR binding is not readable executable GameAssembly code: ");
        logtext(name);
        void *code = 0;
        SIZE_T got = 0;
        if (ReadProcessMemory(GetCurrentProcess(), closed, &code, sizeof(code), &got) &&
            got == sizeof(code)) {
            logtext(" pointer=");
            loghex((UINT_PTR)code);
        }
        nl();
        return 0;
    }
    return closed;
}
static int colour_identity(void *station, const unsigned *expected) {
    if (!station || !g_entity_field || !g_exists_verified)
        return 0;
    unsigned entity[2] = {0};
    p_field_get_value(station, g_entity_field, entity);
    if (!entity[0] || !entity[1] || !pf_same_entity(entity, expected))
        return 0;
    void *a[] = {entity}, *b = pf_invoke(g_exists_method, 0, a);
    return b && pf_class(p_object_get_class(b), "System.Boolean") && p_object_unbox(b) &&
           *(unsigned char *)p_object_unbox(b) == 1;
}
static int colour_component_size(int size) {
    return size == 248 || size == 256;
}
static void *colour_read(void *station, const unsigned *entity, PFRoots *roots) {
    unsigned align = 0;
    if (!colour_identity(station, entity) || !g_ride_class || !p_class_is_valuetype(g_ride_class) ||
        !colour_component_size(p_class_value_size(g_ride_class, &align)))
        return 0;
    void *get = colour_close("TryGetComponentData", "System.Boolean", "TPS.Game.ECTrackRide&",
                             0x4ce370, 0x2eb661b9U, roots);
    unsigned char data[256] = {0};
    void *a[] = {(void *)entity, data}, *b = pf_invoke(get, 0, a);
    if (!b || !pf_class(p_object_get_class(b), "System.Boolean") || !p_object_unbox(b) ||
        *(unsigned char *)p_object_unbox(b) != 1)
        return 0;
    void *boxed = p_value_box(g_ride_class, data);
    return boxed && pf_root(roots, boxed) ? boxed : 0;
}
static int colour_fields(void **has, void **rgba) {
    *has = pf_field(g_ride_class, "HasTint", "System.Boolean");
    *rgba = pf_field(g_ride_class, "TintColor", "UnityEngine.Color");
    return *has && *rgba && write_value_layout(p_class_from_type(p_field_get_type(*has)), 1) &&
           write_value_layout(p_class_from_type(p_field_get_type(*rgba)), 16);
}
static int colour_from_box(void *boxed, CoasterColour *out) {
    memset(out, 0, sizeof(*out));
    void *has = 0, *rgba = 0;
    if (boxed && colour_fields(&has, &rgba)) {
        p_field_get_value(boxed, has, &out->custom);
        p_field_get_value(boxed, rgba, out->rgba);
        out->present = 1;
        if (!colour_valid(out))
            memset(out, 0, sizeof(*out));
    }
    return out->present;
}
static int coaster_colour_capture(void *station, const unsigned *entity, CoasterColour *out) {
    memset(out, 0, sizeof(*out));
    PFRoots roots = {0};
    void *boxed = colour_read(station, entity, &roots);
    colour_from_box(boxed, out);
    pf_release(&roots);
    return out->present;
}
static int coaster_colour_apply(void *station, const unsigned *entity, const CoasterColour *tint) {
    if (!tint->present)
        return 1; /* Older blueprints preserve native defaults. */
    if (!colour_valid(tint))
        return 0;
    PFRoots roots = {0};
    int ok = 0;
    const char *stage = "COLOUR: source/component/visual binding unavailable";
    void *has = 0, *rgba = 0, *bus = 0;
    void *boxed = colour_read(station, entity, &roots);
    void *set = colour_close("SetComponentData", "System.Void", "TPS.Game.ECTrackRide", 0x4c30d0,
                             0x2f7d0b5cU, &roots);
    void *context = write_context(station, entity, &roots, &bus);
    void *visual = context ? pf_ref(context, p_object_get_class(context),
                                    "trackRideVisualComponent", "TPS.Game.TrackRideVisualComponent")
                           : 0;
    if (!boxed) {
        stage = "COLOUR: ride reread failed";
        goto done;
    }
    if (!set) {
        stage = "COLOUR: persistent setter binding failed";
        goto done;
    }
    if (!context) {
        stage = "COLOUR: exact station context unavailable";
        goto done;
    }
    if (!visual || !pf_root(&roots, visual)) {
        stage = "COLOUR: native track visual unavailable";
        goto done;
    }
    if (!colour_fields(&has, &rgba)) {
        stage = "COLOUR: ride tint field schema differs";
        goto done;
    }
    stage = "COLOUR: visual setter or nullable schema differs";
    void *vc = p_object_get_class(visual), *it = 0, *m, *paint = 0;
    while ((m = p_class_get_methods(vc, &it)))
        if (equals(p_method_get_name(m), "SetTrackTintColor") && p_method_is_instance(m) &&
            p_method_get_param_count(m) == 1 &&
            pf_type(p_method_get_return_type(m), "System.Void")) {
            if (paint)
                goto done;
            paint = m;
        }
    void *nullable = paint ? p_class_from_type(p_method_get_param(paint, 0)) : 0;
    void *hv = nullable ? pf_field(nullable, "hasValue", "System.Boolean") : 0;
    void *value = nullable ? pf_field(nullable, "value", "UnityEngine.Color") : 0;
    if (nullable) {
        void *fi = 0, *field = 0;
        unsigned count = 0;
        while ((field = p_class_get_fields(nullable, &fi)) && count++ < 8) {
            logtext("FIRST WRITE: COLOUR nullable field ");
            logtext(p_field_get_name(field));
            logtext(" offset=");
            lognum((UINT_PTR)p_field_get_offset(field));
            char *name = p_type_get_name(p_field_get_type(field));
            logtext(" type=");
            logtext(name ? name : "unavailable");
            if (name)
                p_free(name);
            nl();
        }
    }
    if (!paint || !write_code_guard(paint, 0x2ed07c0, 0xa0, 0x0ed2a425U) ||
        !write_value_layout(nullable, 20) || !hv || !value ||
        p_field_get_offset(hv) != (int)(2 * sizeof(void *)) ||
        p_field_get_offset(value) != (int)(2 * sizeof(void *) + 4))
        goto done;
    logtext("FIRST WRITE: COLOUR nullable size=");
    unsigned align = 0;
    lognum((UINT_PTR)p_class_value_size(nullable, &align));
    logtext(" hasValue offset=");
    lognum((UINT_PTR)p_field_get_offset(hv));
    logtext(" value offset=");
    lognum((UINT_PTR)p_field_get_offset(value));
    nl();
    unsigned char optional[20] = {0};
    optional[0] = tint->custom;
    memcpy(optional + 4, tint->rgba, 16);
    p_field_set_value(boxed, has, (void *)&tint->custom);
    p_field_set_value(boxed, rgba, (void *)tint->rgba);
    stage = "COLOUR: station identity changed before write";
    if (!colour_identity(station, entity))
        goto done;
    void *a[] = {(void *)entity, p_object_unbox(boxed)};
    stage = "COLOUR: persistent component setter failed";
    if (!write_void(set, 0, a))
        goto done;
    void *pa[] = {optional};
    stage = "COLOUR: native visual setter failed";
    if (!write_void(paint, visual, pa))
        goto done;
    stage = "COLOUR: component readback differs";
    CoasterColour check;
    if (!coaster_colour_capture(station, entity, &check) || check.custom != tint->custom)
        goto done;
    for (unsigned i = 0; i < 4; ++i)
        if (check.rgba[i] != tint->rgba[i])
            goto done;
    ok = 1;
    write_log("COLOUR: saved track tint restored and persistent component verified");
done:
    if (!ok)
        write_log(stage);
    pf_release(&roots);
    return ok;
}
