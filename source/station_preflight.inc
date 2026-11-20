/* Read-only, per-F8 inspection. Never dispatch events or call reconstruction. */
typedef struct {
    GCHandle h[32];
    unsigned n;
} PFRoots;
static int pf_root(PFRoots *r, void *o) {
    if (!o || r->n >= 32)
        return 0;
    GCHandle h = p_gchandle_new(o, 1);
    if (!h || p_gchandle_get_target(h) != o)
        return 0;
    r->h[r->n++] = h;
    return 1;
}
static void pf_release(PFRoots *r) {
    while (r->n)
        p_gchandle_free(r->h[--r->n]);
}
static int pf_type(void *t, const char *n) {
    char *s = t ? p_type_get_name(t) : 0;
    int ok = equals(s, n);
    if (s)
        p_free(s);
    return ok;
}
static int pf_class(void *c, const char *n) {
    return c && pf_type(p_class_get_type(c), n);
}
static void *pf_event_class(void) {
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
        for (SIZE_T i = 0; i < classes && scanned < 200000; ++i) {
            ++scanned;
            void *c = p_image_get_class(image, i);
            if (c && equals(p_class_get_name(c), "TrackRevert") &&
                pf_class(c, "TPS.Game.GameEvent.TrackRide.TrackRevert")) {
                cached = c;
                return c;
            }
        }
    }
    return 0;
}
static void *pf_field(void *c, const char *n, const char *t) {
    void *f = p_class_get_field_from_name(c, n);
    return f && p_field_get_parent(f) == c && !(p_field_get_flags(f) & 0x10) &&
                   pf_type(p_field_get_type(f), t)
               ? f
               : 0;
}
static void *pf_ref(void *o, void *c, const char *n, const char *t) {
    void *f = pf_field(c, n, t);
    if (!f)
        return 0;
    void *expected = p_class_from_type(p_field_get_type(f));
    if (!expected || p_class_is_valuetype(expected))
        return 0;
    void *v = 0;
    p_field_get_value(o, f, &v);
    return v && p_class_is_assignable_from(expected, p_object_get_class(v)) ? v : 0;
}
/* Explicit reviewed compiler-name aliases; never match arbitrary closures. */
static int pf_method_name(void *c, const char *actual, const char *requested) {
    if (equals(actual, requested))
        return 1;
    if (pf_class(c, "TPS.Game.ECPTrackRide.<>c__DisplayClass15_0")) {
        if (equals(requested, "<SetComponents>g__Snapshot|30"))
            return equals(actual, "<SetComponents>g__Snapshot|31") || equals(actual, "<SetComponents>g__Snapshot|32");
        if (equals(requested, "<SetComponents>g__Revert|31"))
            return equals(actual, "<SetComponents>g__Revert|32") || equals(actual, "<SetComponents>g__Revert|33");
        if (equals(requested, "<SetComponents>g__OnTrackRevert|13"))
            return equals(actual, "<SetComponents>g__OnTrackRevert|14");
    }
    return pf_class(c, "TPS.Game.ECPTrackRide") &&
           equals(requested, "<SetComponents>g__ToggleTrackSupportArrow|20_26") &&
           (equals(actual, "<SetComponents>g__ToggleTrackSupportArrow|15_27") || equals(actual, "<SetComponents>g__ToggleTrackSupportArrow|15_28"));
}
static void *pf_method(void *c, const char *n, const char *ret, unsigned count, const char **params,
                       int instance) {
    void *it = 0;
    void *m;
    void *found = 0;
    while ((m = p_class_get_methods(c, &it))) {
        if (!pf_method_name(c, p_method_get_name(m), n) || p_method_get_class(m) != c ||
            p_method_is_generic(m) || !!p_method_is_instance(m) != instance ||
            p_method_get_param_count(m) != count || !pf_type(p_method_get_return_type(m), ret))
            continue;
        unsigned k = 0;
        for (; k < count; ++k)
            if (!pf_type(p_method_get_param(m, k), params[k]))
                break;
        if (k == count) {
            if (found)
                return 0;
            found = m;
        }
    }
    return found;
}
static void *pf_invoke(void *m, void *o, void **args) {
    if (!m)
        return 0;
    void *ex = 0;
    void *v = p_runtime_invoke(m, o, args, &ex);
    return ex ? 0 : v;
}
static int pf_entity_field(void *o, void *c, unsigned *out) {
    void *f = pf_field(c, "trackRideEntity", "Unity.Entities.Entity");
    if (!f)
        return 0;
    void *vc = p_class_from_type(p_field_get_type(f));
    unsigned a = 0;
    if (!vc || !p_class_is_valuetype(vc) || p_class_value_size(vc, &a) != 8)
        return 0;
    p_field_get_value(o, f, out);
    return 1;
}
static int pf_same_entity(const unsigned *a, const unsigned *b) {
    return a[0] == b[0] && a[1] == b[1];
}
/* Return context only after inspecting every list entry and confirming one
 * exact entity/item/handler match. Caller keeps roots alive through queries. */
static void *pf_binding(void *bus, void *busClass, void *eventClass, void *station,
                        const unsigned *entity, PFRoots *roots, const char **status) {
    *status = "listener-table-unavailable";
    if (!pf_root(roots, bus))
        return 0;
    void *dict =
        pf_ref(bus, busClass, "_delegates",
               "System.Collections.Generic.Dictionary<System.Type,System.Collections.IList>");
    /* Runtime type names contain a comma-space on some IL2CPP builds. */
    if (!dict)
        dict =
            pf_ref(bus, busClass, "_delegates",
                   "System.Collections.Generic.Dictionary<System.Type, System.Collections.IList>");
    if (!dict || !pf_root(roots, dict))
        return 0;
    void *dc = p_object_get_class(dict);
    const char *dp[] = {"System.Type"};
    void *get = pf_method(dc, "get_Item", "System.Collections.IList", 1, dp, 1);
    void *key = p_type_get_object(p_class_get_type(eventClass));
    if (!key || !pf_root(roots, key)) {
        *status = "event-type-unavailable";
        return 0;
    }
    void *args[] = {key};
    void *list = pf_invoke(get, dict, args);
    if (!list || !pf_root(roots, list)) {
        *status = "track-revert-list-missing-or-getter-unavailable";
        return 0;
    }
    void *lc = p_object_get_class(list);
    if (!pf_class(
            lc,
            "System.Collections.Generic.List<TPS.Core.GameEvents.EventDelegate<TPS.Game.GameEvent.TrackRide.TrackRevert>>")) {
        *status = "unexpected-listener-list-type";
        return 0;
    }
    int count = 0;
    if (!invoke_count(list, lc, &count)) {
        *status = "listener-count-unavailable-or-over-limit";
        return 0;
    }
    const char *ip[] = {"System.Int32"};
    void *item = pf_method(
        lc, "get_Item",
        "TPS.Core.GameEvents.EventDelegate<TPS.Game.GameEvent.TrackRide.TrackRevert>", 1, ip, 1);
    if (!item) {
        *status = "listener-item-signature-unavailable";
        return 0;
    }
    void *delegateClass = findclass(p_domain_get(), "System", "Delegate");
    void *multiClass = findclass(p_domain_get(), "System", "MulticastDelegate");
    void *getMethod = delegateClass ? pf_method(delegateClass, "get_Method",
                                                "System.Reflection.MethodInfo", 0, 0, 1)
                                    : 0;
    if (!delegateClass || !multiClass || !getMethod) {
        *status = "delegate-metadata-unavailable";
        return 0;
    }
    unsigned matches = 0;
    void *context = 0;
    int valid = 0;
    for (int i = 0; i < count; ++i) {
        PFRoots temporary = {0};
        void *ia[] = {&i};
        void *d = pf_invoke(item, list, ia);
        if (!d || !pf_root(&temporary, d) ||
            !pf_class(
                p_object_get_class(d),
                "TPS.Core.GameEvents.EventDelegate<TPS.Game.GameEvent.TrackRide.TrackRevert>")) {
            pf_release(&temporary);
            *status = "listener-read-failed";
            return 0;
        }
        void *mf = pf_field(multiClass, "delegates", "System.Delegate[]");
        void *multi = 0;
        if (!mf) {
            pf_release(&temporary);
            *status = "multicast-layout-unavailable";
            return 0;
        }
        p_field_get_value(d, mf, &multi);
        if (multi) {
            pf_release(&temporary);
            *status = "multicast-listener-not-supported";
            return 0;
        }
        void *target = pf_ref(d, delegateClass, "m_target", "System.Object");
        if (target &&
            (pf_class(p_object_get_class(target), "TPS.Game.ECPTrackRide.<>c__DisplayClass20_0") ||
             pf_class(p_object_get_class(target), "TPS.Game.ECPTrackRide.<>c__DisplayClass15_0"))) {
            if (!pf_root(&temporary, target)) {
                pf_release(&temporary);
                *status = "context-root-failed";
                return 0;
            }
            void *cc = p_object_get_class(target);
            unsigned captured[2] = {0};
            if (!pf_entity_field(target, cc, captured)) {
                pf_release(&temporary);
                *status = "captured-entity-layout-unavailable";
                return 0;
            }
            if (pf_same_entity(captured, entity)) {
                ++matches;
                void *capturedItem =
                    pf_ref(target, cc, "trackRideItem", "TPS.Game.GameItemInstance");
                const char *ep[] = {"TPS.Game.GameEvent.TrackRide.TrackRevert&"};
                void *handler =
                    pf_method(cc, "<SetComponents>g__OnTrackRevert|13", "System.Void", 1, ep, 1);
                void *reflection = pf_invoke(getMethod, d, 0);
                if (reflection && !pf_root(&temporary, reflection)) {
                    pf_release(&temporary);
                    *status = "handler-root-failed";
                    return 0;
                }
                valid = capturedItem == station && handler && reflection &&
                        p_method_get_from_reflection(reflection) == handler;
                if (valid && !context) {
                    if (!pf_root(roots, target)) {
                        pf_release(&temporary);
                        *status = "context-root-failed";
                        return 0;
                    }
                    context = target;
                }
            }
        }
        pf_release(&temporary);
    }
    int after = 0;
    if (!invoke_count(list, lc, &after) || after != count) {
        *status = "listener-list-changed";
        return 0;
    }
    if (matches != 1) {
        *status = matches ? "duplicate-entity-contexts" : "no-matching-entity-context";
        return 0;
    }
    if (!valid || !context) {
        *status = "item-or-handler-mismatch";
        return 0;
    }
    *status = "pass-exact-entity-item-handler";
    return context;
}
static int pf_definition(void *config, void *cls, const char *field, const char *idType,
                         const char *definitionType) {
    void *f = pf_field(cls, field, idType);
    if (!f)
        return 0;
    void *idClass = p_class_from_type(p_field_get_type(f));
    unsigned a = 0;
    if (!idClass || !p_class_is_valuetype(idClass) || p_class_value_size(idClass, &a) != 8)
        return 0;
    unsigned char id[8] = {0};
    p_field_get_value(config, f, id);
    unsigned nonzero = 0;
    for (unsigned k = 0; k < 8; ++k)
        nonzero |= id[k];
    if (!nonzero)
        return 0;
    void *db = findclass(p_domain_get(), "TPS.Game", "LevelDatabaseUtils");
    const char *params[] = {idType};
    void *m = db ? pf_method(db, "Get", definitionType, 1, params, 0) : 0;
    void *args[] = {id};
    void *definition = pf_invoke(m, 0, args);
    void *expected = m ? p_class_from_type(p_method_get_return_type(m)) : 0;
    return definition && expected &&
           p_class_is_assignable_from(expected, p_object_get_class(definition));
}
static void pf_log(const char *check, const char *status) {
    logtext("PREFLIGHT ");
    logtext(check);
    logtext(": ");
    logtext(status);
    nl();
    FlushFileBuffers(g_log);
}
static const char *pf_floor(void *station, PFRoots *roots) {
    void *sc = p_object_get_class(station);
    float position[3] = {0};
    void *pos = get_station_method(sc, "get_WorldPosition", "UnityEngine.Vector3");
    void *floorGetter = get_station_method(sc, "get_FloorArea", "TPS.Game.FloorAreaInstance");
    void *current = pf_invoke(floorGetter, station, 0);
    if (!current || !pf_root(roots, current) || !pos ||
        !station_value(station, pos, "UnityEngine.Vector3", 12, 3, position) ||
        !finite_vector(position, 3)) {
        return "skip-station-floor-or-world-position-unavailable";
    }
    void *fc = p_class_from_type(p_method_get_return_type(floorGetter));
    if (!fc || !p_class_is_assignable_from(fc, p_object_get_class(current))) {
        return "skip-unexpected-station-floor-type";
    }
    void *utils = findclass(p_domain_get(), "TPS.Game", "FloorAreaUtils");
    const char *params[] = {"UnityEngine.Vector3", "System.Boolean", "System.Boolean"};
    void *m = utils ? pf_method(utils, "GetTopMostFloorAreaAtWorldPosition",
                                "TPS.Game.FloorAreaInstance", 3, params, 0)
                    : 0;
    unsigned char allowBlueprint = 0, allowCursor = 0;
    void *args[] = {position, &allowBlueprint, &allowCursor};
    void *floor = pf_invoke(m, 0, args);
    return !floor ? "skip-query-null-exception-or-signature"
                  : (floor == current ? "pass-station-floor-matches-world-query"
                                      : "skip-world-query-differs-from-station-floor");
}
static void run_station_preflight(void *station, const unsigned char *entityBytes) {
    PFRoots roots = {0};
    const char *binding = "event-bus-unavailable";
    void *context = 0;
    unsigned entity[2] = {0};
    memcpy(entity, entityBytes, 8);
    pf_log("begin", "v0.5.5 read-only; location = selected station's current world position");
    if (!pf_root(&roots, station)) {
        pf_log("binding", "station-root-failed");
        goto done;
    }
    void *busClass = findclass(p_domain_get(), "TPS.Core", "GameEvents");
    void *eventClass = pf_event_class();
    void *parent = busClass ? p_class_get_parent(busClass) : 0;
    if (parent && eventClass &&
        pf_class(parent, "TPS.Core.StaticGameSystem<TPS.Core.GameEvents>")) {
        void *f = p_class_get_field_from_name(parent, "<Instance>k__BackingField");
        void *bus = 0;
        if (f && p_field_get_parent(f) == parent && (p_field_get_flags(f) & 0x10) &&
            pf_type(p_field_get_type(f), "TPS.Core.GameEvents")) {
            p_field_static_get_value(f, &bus);
            if (bus && p_object_get_class(bus) == busClass)
                context = pf_binding(bus, busClass, eventClass, station, entity, &roots, &binding);
        }
    }
    pf_log("binding", binding);
    if (context) {
        void *cc = p_object_get_class(context);
        void *config = pf_ref(context, cc, "rideConfig", "TPS.Game.TrackRideConfig");
        void *global = pf_ref(context, cc, "ridesConfig", "TPS.Game.TrackRidesConfig");
        int support = 0, pieceEntity = 0, normal = 0;
        if (config && pf_root(&roots, config)) {
            void *rc = p_object_get_class(config);
            support = pf_definition(config, rc, "SupportDefinition",
                                    "TPS.Game.GameItemDefinitionID", "TPS.Game.GameItemDefinition");
            void *m =
                pf_method(rc, "GetNormalTrackPiece", "TPS.Game.TrackPieceDefinition", 0, 0, 1);
            void *v = pf_invoke(m, config, 0);
            void *expected = m ? p_class_from_type(p_method_get_return_type(m)) : 0;
            normal = v && expected && p_class_is_assignable_from(expected, p_object_get_class(v));
        }
        if (global && pf_root(&roots, global))
            pieceEntity = pf_definition(
                global, p_object_get_class(global), "TrackPieceEntityDefinition",
                "TPS.Game.LightweightEntityDefinitionID", "TPS.Game.LightweightEntityDefinition");
        pf_log("support-definition",
               support ? "pass-resolved" : "skip-missing-config-definition-or-signature");
        pf_log("track-piece-entity-definition",
               pieceEntity ? "pass-resolved" : "skip-missing-config-definition-or-signature");
        pf_log("normal-track-piece-definition",
               normal ? "pass-resolved" : "skip-missing-config-definition-or-signature");
    } else
        pf_log("definitions", "skip-no-validated-station-context");
    pf_log("floor", pf_floor(station, &roots));
done:
    pf_log(
        "blueprint-compatibility",
        "not-checked-no-import-plan; other pieces/decorations and moved destinations unresolved");
    pf_log("placement", "disabled; no events dispatched or reconstruction invoked");
    pf_release(&roots);
    pf_log("end", "complete");
}
