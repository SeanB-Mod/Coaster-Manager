/* One synchronous URP request. Every borrowed rendering setting is journalled
 * BEFORE its setter runs, including failed/throwing setter attempts. */
#define ISOLATED_CHANGES (65536 + 8192 + 512 + 4)
typedef struct {
    void *owner, *setter;
    long long key;
    unsigned kind;
    unsigned char before[16];
} IsolatedChange;
static IsolatedChange isolated_changes[ISOLATED_CHANGES];
static unsigned isolated_change_count;
static PFRoots isolated_request_roots[256];
static unsigned isolated_request_root_count;
static long long isolated_track_ids[1024];
static unsigned isolated_track_id_count, isolated_spline_instances;
static int isolated_busy;
static unsigned isolated_change_size(unsigned kind) {
    return (kind == 1 || kind == 6)   ? 1
           : (kind == 2 || kind == 7) ? 4
           : kind == 3                ? 16
           : (kind == 4 || kind == 5) ? 8
                                      : 0;
}
static int isolated_apply(const IsolatedChange *c, const void *data) {
    void *a[] = {(void *)data};
    if (c->kind == 7) {
        int actual = 0, expected = 0;
        memcpy(&expected, data, sizeof(expected));
        p_field_set_value(c->owner, c->setter, (void *)data);
        p_field_get_value(c->owner, c->setter, &actual);
        return actual == expected;
    }
    if (c->kind == 6) {
        unsigned char actual = 0;
        p_field_set_value(c->owner, c->setter, (void *)data);
        p_field_get_value(c->owner, c->setter, &actual);
        return actual == *(const unsigned char *)data;
    }
    if (c->kind == 5) {
        void *reference = 0;
        memcpy(&reference, data, sizeof(reference));
        void *ra[] = {reference};
        return write_void(c->setter, c->owner, ra);
    }
    if (c->kind != 4)
        return write_void(c->setter, c->owner, a);
    void *boxed = 0;
    memcpy(&boxed, data, sizeof(boxed));
    if (!boxed) {
        const char *p[] = {"System.Int64"};
        void *remove =
            browser_method(p_object_get_class(c->owner), "Remove", "System.Boolean", 1, p, 1);
        void *a[] = {(void *)&c->key};
        void *result = pf_invoke(remove, c->owner, a);
        return result && p_object_unbox(result) && *(unsigned char *)p_object_unbox(result) == 1;
    }
    void *da[] = {(void *)&c->key, p_object_unbox(boxed)};
    return da[1] && write_void(c->setter, c->owner, da);
}
typedef int (*IsolatedApply)(const IsolatedChange *, const void *);
static int isolated_change_using(void *owner, void *setter, unsigned kind, long long key,
                                 const void *before, const void *after, IsolatedApply apply) {
    unsigned bytes = isolated_change_size(kind);
    if (!owner || !setter || !before || !after || !bytes ||
        isolated_change_count >= ISOLATED_CHANGES)
        return 0;
    IsolatedChange *c = &isolated_changes[isolated_change_count++];
    c->owner = owner;
    c->setter = setter;
    c->kind = kind;
    c->key = key;
    memcpy(c->before, before, bytes);
    return apply(c, after);
}
static int isolated_restore_using(IsolatedApply apply) {
    int ok = 1;
    while (isolated_change_count) {
        IsolatedChange *c = &isolated_changes[--isolated_change_count];
        if (!apply(c, c->before))
            ok = 0;
        memset(c, 0, sizeof(*c));
    }
    return ok;
}
static int isolated_request_hold(void *o) {
    if (isolated_request_root_count >= 8192 ||
        !pf_root(&isolated_request_roots[isolated_request_root_count / 32], o))
        return 0;
    ++isolated_request_root_count;
    return 1;
}
static void isolated_request_release(void) {
    for (unsigned i = 0; i < 256; ++i)
        pf_release(&isolated_request_roots[i]);
    isolated_request_root_count = 0;
    isolated_track_id_count = 0;
    isolated_spline_instances = 0;
}
/* Resolve nested metadata types without guessing the class_from_name separator. */
static void *isolated_full_class(const char *name) {
    SIZE_T count = 0, scanned = 0;
    void **assemblies = p_domain_get_assemblies(p_domain_get(), &count);
    if (!assemblies || count > 10000)
        return 0;
    for (SIZE_T a = 0; a < count; ++a) {
        void *image = p_assembly_get_image(assemblies[a]);
        SIZE_T classes = image ? p_image_get_class_count(image) : 0;
        if (classes > 200000)
            return 0;
        for (SIZE_T i = 0; i < classes; ++i) {
            if (++scanned > 200000)
                return 0;
            void *c = p_image_get_class(image, i);
            if (pf_class(c, name))
                return c;
        }
    }
    return 0;
}
static int isolated_bool(void *o, const char *getter, unsigned char *out) {
    void *v = browser_optional(o, getter, "System.Boolean");
    if (!v || !p_object_unbox(v))
        return 0;
    *out = *(unsigned char *)p_object_unbox(v);
    return *out <= 1;
}
static void *isolated_resources(const char *ns, const char *name) {
    void *type = browser_type(ns, name),
         *rc = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *a[] = {type};
    return type ? pf_invoke(
                      browser_method(rc, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0),
                      0, a)
                : 0;
}
static int isolated_is_selected(void *renderer) {
    for (unsigned i = 0; i < isolated_count; ++i)
        if (isolated_renderers[i] == renderer)
            return 1;
    return 0;
}
static int isolated_hide_renderers(void *array) {
    int count = 0;
    if (!array || !browser_asset_array_count(array, &count)) {
        save_log("ISOLATED: renderer inventory exceeds bounds or cannot be read");
        return 0;
    }
    const char *p[] = {"System.Boolean"};
    unsigned char no = 0;
    for (int i = 0; i < count; ++i) {
        void *renderer = write_array_item(array, i);
        if (!renderer || !browser_alive(renderer) || isolated_is_selected(renderer))
            continue;
        unsigned char enabled = 0, active = 0;
        void *go = browser_optional(renderer, "get_gameObject", "UnityEngine.GameObject");
        if (!go || !isolated_bool(renderer, "get_enabled", &enabled) ||
            !isolated_bool(go, "get_activeInHierarchy", &active)) {
            save_log("ISOLATED: renderer enabled/active state cannot be read");
            return 0;
        }
        if (!enabled || !active)
            continue;
        /* forceRenderingOff is stripped from the installed player's API.
         * Renderer.enabled exists and affects rendering only, leaving the
         * GameObject, scripts, collider and all scene layers untouched. */
        void *set =
            browser_method(p_object_get_class(renderer), "set_enabled", "System.Void", 1, p, 1);
        if (!isolated_change_using(renderer, set, 1, 0, &enabled, &no, isolated_apply)) {
            save_log("ISOLATED: renderer enabled setter failed; restoring prior settings");
            return 0;
        }
    }
    logtext("SAVE TEMPLATE: ISOLATED scene renderer inventory=");
    lognum((unsigned)count);
    logtext(" selected=");
    lognum(isolated_count);
    nl();
    return 1;
}
static int isolated_hide_terrain(void *array) {
    int count = 0;
    if (!array || !browser_asset_array_count(array, &count) || count > 256)
        return 0;
    const char *get[] = {"get_drawHeightmap", "get_drawTreesAndFoliage"};
    const char *set[] = {"set_drawHeightmap", "set_drawTreesAndFoliage"};
    const char *p[] = {"System.Boolean"};
    unsigned char no = 0;
    for (int i = 0; i < count; ++i) {
        void *terrain = write_array_item(array, i);
        if (!terrain || !browser_alive(terrain))
            continue;
        for (unsigned j = 0; j < 2; ++j) {
            unsigned char before = 0;
            if (!isolated_bool(terrain, get[j], &before))
                return 0;
            if (!before)
                continue;
            void *method =
                browser_method(p_object_get_class(terrain), set[j], "System.Void", 1, p, 1);
            if (!isolated_change_using(terrain, method, 1, 0, &before, &no, isolated_apply))
                return 0;
        }
    }
    return 1;
}
static int isolated_track_id(long long id) {
    if (!id)
        return 1;
    for (unsigned i = 0; i < isolated_track_id_count; ++i)
        if (isolated_track_ids[i] == id)
            return 1;
    if (isolated_track_id_count >= 1024)
        return 0;
    isolated_track_ids[isolated_track_id_count++] = id;
    return 1;
}
static int isolated_track_ids_from(void *o, const char *const *names, unsigned n) {
    void *c = o ? p_object_get_class(o) : 0;
    for (unsigned i = 0; i < n; ++i) {
        void *field = c ? pf_field(c, names[i], "System.Int64") : 0;
        long long id = 0;
        if (!field)
            return 0;
        p_field_get_value(o, field, &id);
        if (!isolated_track_id(id))
            return 0;
    }
    return 1;
}
/* Log exact runtime field types on binding failure rather than guessing
 * or loosening ownership checks for a different native layout. */
static void *isolated_checked_ref(void *owner, void *klass, const char *name, const char *type) {
    void *value = pf_ref(owner, klass, name, type);
    if (value)
        return value;
    logtext("SAVE TEMPLATE: ISOLATED field binding: ");
    logtext(name);
    logtext(" expected=");
    logtext(type);
    void *field = klass ? p_class_get_field_from_name(klass, name) : 0;
    char *actual = field ? p_type_get_name(p_field_get_type(field)) : 0;
    logtext(" actual=");
    logtext(actual ? actual : "missing field");
    logtext(" owner=");
    char *ownerType = klass ? p_type_get_name(p_class_get_type(klass)) : 0;
    logtext(ownerType ? ownerType : "missing owner");
    nl();
    if (actual)
        p_free(actual);
    if (ownerType)
        p_free(ownerType);
    return 0;
}
/* An explicit false evaluator result can mean the spline-instanced renderer
 * is active. Read only its existing owned instance list: do not call getters
 * that can build a cache/root or instantiate new rail objects. */
static int isolated_track_ready(unsigned procedural, unsigned spline) {
    return procedural > 0 || spline > 0;
}
static int isolated_spline_entries(void *visual) {
    PFRoots roots = {0};
    int ok = 0, count = 0;
    void *spline = isolated_checked_ref(visual, p_object_get_class(visual), "_splineInstantiate",
                                        "UnityEngine.Splines.SplineInstantiate");
    if (spline) {
        void *root =
            pf_ref(spline, p_object_get_class(spline), "m_InstancesRoot", "UnityEngine.GameObject");
        unsigned matches = 0;
        logtext("SAVE TEMPLATE: ISOLATED spline root present=");
        lognum(root != 0);
        nl();
        if (root && browser_alive(root)) {
            if (!isolated_add_root_count(root, &matches))
                goto done;
            logtext("SAVE TEMPLATE: ISOLATED spline root eligible renderers=");
            lognum(matches);
            nl();
            if (matches) {
                isolated_spline_instances = matches;
                logtext("SAVE TEMPLATE: ISOLATED owned spline root rail renderers=");
                lognum(matches);
                logtext(" selected renderers=");
                lognum(isolated_count);
                nl();
                ok = 1;
                goto done;
            }
        }
    }
    void *instances =
        spline ? isolated_checked_ref(spline, p_object_get_class(spline), "m_Instances",
                                      "System.Collections.Generic.List<UnityEngine.GameObject>")
               : 0;
    void *lc = instances ? p_object_get_class(instances) : 0;
    const char *p[] = {"System.Int32"};
    void *get = lc ? browser_method(lc, "get_Item", "UnityEngine.GameObject", 1, p, 1) : 0;
    if (!get || !pf_root(&roots, instances) || !invoke_count(instances, lc, &count))
        goto done;
    logtext("SAVE TEMPLATE: ISOLATED spline instance list count=");
    lognum((unsigned)count);
    nl();
    if (count < 1 || count > 4096)
        goto done;
    unsigned visible = 0;
    for (int i = 0; i < count; ++i) {
        void *a[] = {&i};
        void *instance = pf_invoke(get, instances, a);
        unsigned matches = 0;
        if (!instance || !browser_alive(instance) || !isolated_add_root_count(instance, &matches))
            goto done;
        if (matches)
            ++visible;
    }
    if (!visible)
        goto done;
    isolated_spline_instances = visible;
    logtext("SAVE TEMPLATE: ISOLATED owned spline rail instances=");
    lognum(visible);
    logtext(" selected renderers=");
    lognum(isolated_count);
    nl();
    ok = 1;
done:
    if (!ok)
        save_log(
            "ISOLATED: procedural evaluator unavailable and owned spline rail list could not be collected");
    pf_release(&roots);
    return ok;
}
/* The rendering manager is independent of cubic-spline evaluation. The
 * evaluator accessor deliberately gates on BOTH DebugVars, whereas UpdateSpline
 * registers procedural rail draws with UseProceduralTrackRenderer alone. */
static void *isolated_render_manager(PFRoots *roots) {
    void *wc = findclass(p_domain_get(), "TPS.Core", "World");
    void *getWorld = wc ? browser_method(wc, "get_Active", "TPS.Core.World", 0, 0, 0) : 0;
    void *world = pf_invoke(getWorld, 0, 0);
    if (!world || !pf_root(roots, world)) {
        save_log("ISOLATED: active rendering world unavailable");
        return 0;
    }
    void *systems = isolated_checked_ref(world, wc, "GameSystems", "TPS.Core.GameSystems");
    void *dictionary =
        systems ? isolated_checked_ref(
                      systems, p_object_get_class(systems), "_systems",
                      "System.Collections.Generic.Dictionary<System.Type,TPS.Core.IGameSystem>")
                : 0;
    void *mc = findclass(p_domain_get(), "TPS.CoasterRenderer", "CoasterTrackManager");
    void *type = mc ? p_type_get_object(p_class_get_type(mc)) : 0;
    const char *p[] = {"System.Type", "TPS.Core.IGameSystem&"};
    void *get = dictionary ? browser_method(p_object_get_class(dictionary), "TryGetValue",
                                            "System.Boolean", 2, p, 1)
                           : 0;
    if (!get || !type || !pf_root(roots, dictionary) || !pf_root(roots, type)) {
        save_log("ISOLATED: rendering system registry binding unavailable");
        return 0;
    }
    void *manager = 0, *a[] = {type, &manager};
    void *result = pf_invoke(get, dictionary, a);
    if (!result || !pf_class(p_object_get_class(result), "System.Boolean") ||
        !p_object_unbox(result) || !*(unsigned char *)p_object_unbox(result) || !manager ||
        p_object_get_class(manager) != mc || !pf_root(roots, manager)) {
        save_log(
            "ISOLATED: CoasterTrackManager not available in the active world's system registry");
        return 0;
    }
    return manager;
}
static int isolated_track_entries(void *station, const unsigned *entity) {
    PFRoots roots = {0};
    int ok = 0, count = 0;
    const char *stage = "native track context";
    void *visual = isolated_live_track_visual(station, entity, &roots);
    stage = "current station track visual";
    if (!visual)
        goto done;
    stage = "active world rendering manager";
    void *manager = isolated_render_manager(&roots);
    if (!manager)
        goto done;
    /* UpdateSpline's draw registration key is the owning GameObject's instance
     * ID, not the component ID, ECS entity index, or the gated evaluator ID. */
    stage = "selected track GameObject instance ID";
    void *go = browser_optional(visual, "get_gameObject", "UnityEngine.GameObject");
    void *idValue = go ? browser_optional(go, "GetInstanceID", "System.Int32") : 0;
    if (!idValue || !p_object_unbox(idValue))
        goto done;
    int id = 0;
    memcpy(&id, p_object_unbox(idValue), sizeof(id));
    void *result = 0;
    stage = "manager track dictionary field";
    void *tracks = isolated_checked_ref(
        manager, p_object_get_class(manager), "_tracks",
        "System.Collections.Generic.Dictionary<System.Int32,TPS.CoasterRenderer.CoasterTrackInstance>");
    stage = "track dictionary TryGetValue binding";
    const char *gp[] = {"System.Int32", "TPS.CoasterRenderer.CoasterTrackInstance&"};
    void *get = tracks ? browser_method(p_object_get_class(tracks), "TryGetValue", "System.Boolean",
                                        2, gp, 1)
                       : 0;
    if (!get)
        goto done;
    stage = "selected GameObject ID lookup in render manager";
    logtext("SAVE TEMPLATE: ISOLATED rendering GameObject ID=");
    loghex((UINT_PTR)(unsigned)id);
    nl();
    void *track = 0, *ga[] = {&id, &track};
    result = pf_invoke(get, tracks, ga);
    if (!result || !pf_class(p_object_get_class(result), "System.Boolean") ||
        !p_object_unbox(result))
        goto done;
    if (!*(unsigned char *)p_object_unbox(result)) {
        /* A genuinely unregistered procedural track can still use legacy
         * spline instances; a missing evaluation accessor alone proves neither. */
        stage = "owned spline rail instances for unregistered procedural track";
        ok = isolated_spline_entries(visual);
        goto done;
    }
    if (!track ||
        !pf_class(p_object_get_class(track), "TPS.CoasterRenderer.CoasterTrackInstance") ||
        !pf_root(&roots, track))
        goto done;
    stage = "track cap entry ID fields";
    static const char *caps[] = {
        "_capEntryId",       "_constructionCapEntryId",       "_invalidEntryId",
        "_capDetailEntryId", "_constructionCapDetailEntryId", "_invalidDetailEntryId"};
    if (!isolated_track_ids_from(track, caps, 6))
        goto done;
    stage = "track body renderer field";
    void *body = isolated_checked_ref(track, p_object_get_class(track), "_body",
                                      "TPS.CoasterRenderer.CoasterTrackPieceBatchRenderer");
    void *slots =
        body
            ? isolated_checked_ref(
                  body, p_object_get_class(body), "_slots",
                  "System.Collections.Generic.List<TPS.CoasterRenderer.CoasterTrackPieceBatchRenderer.Slot>")
            : 0;
    void *lc = slots ? p_object_get_class(slots) : 0;
    stage = "body slot list/get_Item/count";
    const char *ip[] = {"System.Int32"};
    get = lc ? browser_method(lc, "get_Item",
                              "TPS.CoasterRenderer.CoasterTrackPieceBatchRenderer.Slot", 1, ip, 1)
             : 0;
    if (!get || !invoke_count(slots, lc, &count) || count < 1 || count > 256)
        goto done;
    stage = "body slot entry IDs";
    static const char *entry[] = {"EntryId", "DetailEntryId"};
    for (int i = 0; i < count; ++i) {
        void *sa[] = {&i};
        void *slot = pf_invoke(get, slots, sa);
        if (!slot || !isolated_track_ids_from(slot, entry, 2))
            goto done;
    }
    stage = "no owned draw IDs";
    ok = isolated_track_id_count > 0;
done:
    if (!ok) {
        logtext("SAVE TEMPLATE: ISOLATED ownership lookup stopped: ");
        logtext(stage);
        nl();
    }
    pf_release(&roots);
    return ok;
}
/* Dictionary's concrete KeyCollection name contains nested generic metadata.
 * Validate its declaring dictionary and the exact CopyTo signature instead of
 * guessing the display spelling of that return type. */
static void *isolated_dictionary_keys(void *dictionary, PFRoots *roots) {
    void *c = p_object_get_class(dictionary), *it = 0, *m, *get = 0;
    while ((m = p_class_get_methods(c, &it)))
        if (p_method_get_class(m) == c && equals(p_method_get_name(m), "get_Keys") &&
            p_method_is_instance(m) && !p_method_is_generic(m) && !p_method_get_param_count(m)) {
            if (get)
                return 0;
            get = m;
        }
    void *keys = pf_invoke(get, dictionary, 0);
    int n = 0;
    if (!keys || !pf_root(roots, keys) || !invoke_count(dictionary, c, &n) || n < 0 || n > 8192)
        return 0;
    void *longClass = findclass(p_domain_get(), "System", "Int64");
    void *array = longClass ? p_array_new(longClass, n) : 0;
    const char *p[] = {"System.Int64[]", "System.Int32"};
    void *copy = browser_method(p_object_get_class(keys), "CopyTo", "System.Void", 2, p, 1);
    int zero = 0;
    void *a[] = {array, &zero};
    return array && pf_root(roots, array) && write_void(copy, keys, a) ? array : 0;
}
static int isolated_hide_procedural(void *dictionary, void *keys) {
    const char *gp[] = {"System.Int64"};
    const char *sp[] = {"System.Int64", "TPS.ProceduralDrawList.TPSProceduralDrawRequest"};
    void *c = p_object_get_class(dictionary);
    void *get =
        browser_method(c, "get_Item", "TPS.ProceduralDrawList.TPSProceduralDrawRequest", 1, gp, 1);
    void *set = browser_method(c, "set_Item", "System.Void", 2, sp, 1);
    int n = 0;
    unsigned selected = 0;
    if (!get || !set || !write_array_count_limit(keys, &n, 8192))
        return 0;
    const long long *ids = (const long long *)((const unsigned char *)keys + 32);
    for (int i = 0; i < n; ++i) {
        int keep = 0;
        for (unsigned j = 0; j < isolated_track_id_count; ++j)
            if (ids[i] == isolated_track_ids[j])
                keep = 1;
        void *a[] = {(void *)&ids[i]};
        void *before = pf_invoke(get, dictionary, a);
        void *rc = before ? p_object_get_class(before) : 0;
        void *field = rc ? pf_field(rc, "InstanceCount", "System.Int32") : 0;
        if (!field || !p_class_is_valuetype(rc))
            return 0;
        int count = 0;
        p_field_get_value(before, field, &count);
        if (count < 0)
            return 0;
        if (keep) {
            if (count)
                ++selected;
            continue;
        }
        if (!count)
            continue;
        /* Remove only the draw-list entry; retain its full value, buffers and
         * materials for restoration. Do not enqueue invalid zero-count draws. */
        void *after = 0;
        if (!isolated_request_hold(before))
            return 0;
        if (!isolated_change_using(dictionary, set, 4, ids[i], &before, &after, isolated_apply))
            return 0;
    }
    logtext("SAVE TEMPLATE: ISOLATED native procedural entries selected=");
    lognum(selected);
    nl();
    return isolated_track_ready(selected, isolated_spline_instances);
}
static int isolated_camera_background(void *camera) {
    void *flags = browser_optional(camera, "get_clearFlags", "UnityEngine.CameraClearFlags");
    void *background = browser_optional(camera, "get_backgroundColor", "UnityEngine.Color");
    if (!flags || !background || !p_object_unbox(flags) || !p_object_unbox(background))
        return 0;
    int solid = 2;
    BrowserColor slate = {0, 0, 0, 0};
    void *c = p_object_get_class(camera);
    const char *fp[] = {"UnityEngine.CameraClearFlags"}, *bp[] = {"UnityEngine.Color"};
    void *fs = browser_method(c, "set_clearFlags", "System.Void", 1, fp, 1);
    void *bs = browser_method(c, "set_backgroundColor", "System.Void", 1, bp, 1);
    return isolated_change_using(camera, fs, 2, 0, p_object_unbox(flags), &solid, isolated_apply) &&
           isolated_change_using(camera, bs, 3, 0, p_object_unbox(background), &slate,
                                 isolated_apply);
}

/* Preserve actual geometry alpha through URP post-processing for local UI
 * compositing. The shared asset flag is journalled and restored synchronously. */
static int isolated_alpha_output(PFRoots *roots) {
    void *uc =
        findclass(p_domain_get(), "UnityEngine.Rendering.Universal", "UniversalRenderPipeline");
    void *get =
        uc ? browser_method(uc, "get_asset",
                            "UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset", 0, 0, 0)
           : 0;
    void *asset = pf_invoke(get, 0, 0);
    void *ac = findclass(p_domain_get(), "UnityEngine.Rendering.Universal",
                         "UniversalRenderPipelineAsset");
    void *field = asset && ac && p_class_is_assignable_from(ac, p_object_get_class(asset))
                      ? pf_field(ac, "m_AllowPostProcessAlphaOutput", "System.Boolean")
                      : 0;
    if (!field || !write_value_layout(p_class_from_type(p_field_get_type(field)), 1) ||
        !pf_root(roots, asset))
        return 0;
    unsigned char before = 0, yes = 1;
    p_field_get_value(asset, field, &before);
    if (!(before == 1 || (before == 0 && isolated_change_using(asset, field, 6, 0, &before, &yes,
                                                               isolated_apply))))
        return 0;
    /* The default 32-bit HDR intermediate has no alpha, even when the final
     * target does. Resolve the alpha-capable enum from installed metadata. */
    void *precision = pf_field(ac, "m_HDRColorBufferPrecision",
                               "UnityEngine.Rendering.Universal.HDRColorBufferPrecision");
    void *ec = precision ? p_class_from_type(p_field_get_type(precision)) : 0;
    void *wide = ec ? p_class_get_field_from_name(ec, "_64Bits") : 0;
    void *narrow = ec ? p_class_get_field_from_name(ec, "_32Bits") : 0;
    if (!wide || !narrow || !write_value_layout(ec, 4))
        return 0;
    int original = 0, target = 0, normal = 0;
    p_field_get_value(asset, precision, &original);
    p_field_static_get_value(wide, &target);
    p_field_static_get_value(narrow, &normal);
    if (target == normal || (original != normal && original != target))
        return 0;
    if (original != target &&
        !isolated_change_using(asset, precision, 7, 0, &original, &target, isolated_apply))
        return 0;
    save_log("ISOLATED: alpha processing and 64-bit HDR intermediate requested for capture");
    return 1;
}
