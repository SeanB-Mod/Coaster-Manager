/* Experimental selected-coaster-only capture. Borrow renderers/materials for
 * DrawRenderer commands; never clone gameplay objects or alter live visibility. */
#define ISOLATED_RENDERERS 4096
static void *isolated_renderers[ISOLATED_RENDERERS];
static unsigned isolated_count, isolated_decorations[256][2], isolated_decoration_count;
static PFRoots isolated_renderer_roots[128];
static int isolated_hold_renderer(void *renderer) {
    for (unsigned i = 0; i < isolated_count; ++i)
        if (isolated_renderers[i] == renderer)
            return 1;
    if (isolated_count >= ISOLATED_RENDERERS)
        return 0;
    if (!pf_root(&isolated_renderer_roots[isolated_count / 32], renderer))
        return 0;
    isolated_renderers[isolated_count++] = renderer;
    return 1;
}
static void isolated_clear(void) {
    for (unsigned i = 0; i < 128; ++i)
        pf_release(&isolated_renderer_roots[i]);
    memset(isolated_renderers, 0, sizeof(isolated_renderers));
    isolated_count = 0;
    isolated_decoration_count = 0;
}
static int isolated_add_root_count(void *root, unsigned *matches) {
    *matches = 0;
    if (!root)
        return 0;
    PFRoots roots = {0};
    int ok = 0, count = 0;
    /* Component only exposes generic children overloads in this player.
     * Resolve the non-generic Type overload on its owning GameObject. */
    void *go = pf_class(p_object_get_class(root), "UnityEngine.GameObject")
                   ? root
                   : browser_optional(root, "get_gameObject", "UnityEngine.GameObject");
    if (!go || !pf_root(&roots, go)) {
        save_log("ISOLATED: renderer root GameObject could not be resolved");
        goto done;
    }
    void *type = browser_type("UnityEngine", "Renderer");
    unsigned char yes = 1;
    const char *p[] = {"System.Type", "System.Boolean"};
    void *a[] = {type, &yes};
    void *array = type ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                                  "UnityEngine.Component[]", 2, p, 1),
                                   go, a)
                       : 0;
    if (!array || !pf_root(&roots, array) || !browser_asset_array_count(array, &count) ||
        count > 4096) {
        save_log("ISOLATED: GameObject renderer enumeration failed");
        goto done;
    }
    void *mesh = findclass(p_domain_get(), "UnityEngine", "MeshRenderer");
    void *skin = findclass(p_domain_get(), "UnityEngine", "SkinnedMeshRenderer");
    void *train = browser_type("TPS.Game", "TrainCartVisualComponent");
    for (int i = 0; i < count; ++i) {
        void *renderer = write_array_item(array, i);
        if (!renderer || !browser_alive(renderer))
            continue;
        void *c = p_object_get_class(renderer);
        if ((!mesh || !p_class_is_assignable_from(mesh, c)) &&
            (!skin || !p_class_is_assignable_from(skin, c)))
            continue;
        if (!funfair_property_true(renderer, "get_enabled"))
            continue;
        void *go = browser_optional(renderer, "get_gameObject", "UnityEngine.GameObject");
        if (!go || !funfair_property_true(go, "get_activeInHierarchy"))
            continue;
        if (train) {
            void *ta[] = {train, &yes};
            if (pf_invoke(
                    browser_method(c, "GetComponentInParent", "UnityEngine.Component", 2, p, 1),
                    renderer, ta))
                continue;
        }
        if (!isolated_hold_renderer(renderer))
            goto done;
        ++*matches;
    }
    ok = 1;
done:
    pf_release(&roots);
    return ok;
}
static int isolated_add_root(void *root) {
    unsigned matches = 0;
    return isolated_add_root_count(root, &matches);
}
/* Resolve from the selected item's current visual hierarchy, rather than
 * assuming an event handler's cached visual reference is still its live art. */
static int isolated_pick_visual(void **selected, void *candidate, int active) {
    if (!candidate || !active)
        return 1;
    if (*selected && *selected != candidate)
        return 0;
    *selected = candidate;
    return 1;
}
static void *isolated_live_track_visual(void *station, const unsigned *entity, PFRoots *roots) {
    if (!colour_identity(station, entity))
        return 0;
    void *visual = browser_optional(station, "get_Visual", "TPS.Game.GameItemVisual");
    void *base = findclass(p_domain_get(), "TPS.Game", "GameItemVisual");
    if (!visual || !base || !p_class_is_assignable_from(base, p_object_get_class(visual)) ||
        pf_ref(visual, base, "Item", "TPS.Game.GameItemInstance") != station ||
        !pf_root(roots, visual)) {
        save_log("ISOLATED: current GameItemVisual does not bind the selected station");
        return 0;
    }
    void *owners[] = {browser_optional(visual, "get_gameObject", "UnityEngine.GameObject"),
                      browser_optional(station, "get_GameObject", "UnityEngine.GameObject")};
    void *type = browser_type("TPS.Game", "TrackRideVisualComponent");
    if (!type)
        return 0;
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char no = 0;
    void *selected = 0;
    for (unsigned owner = 0; owner < 2; ++owner) {
        void *go = owners[owner];
        if (!go || (owner && go == owners[0]))
            continue;
        void *a[] = {type, &no};
        void *array = pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                               "UnityEngine.Component[]", 2, p, 1),
                                go, a);
        int count = 0;
        if (!array || !pf_root(roots, array) || !write_array_count(array, &count) || count > 32) {
            save_log("ISOLATED: current station track hierarchy enumeration failed");
            return 0;
        }
        for (int i = 0; i < count; ++i) {
            void *candidate = write_array_item(array, i);
            void *candidateGo =
                candidate ? browser_optional(candidate, "get_gameObject", "UnityEngine.GameObject")
                          : 0;
            int active = candidateGo && browser_alive(candidate) &&
                         funfair_property_true(candidateGo, "get_activeInHierarchy") &&
                         funfair_property_true(candidate, "get_enabled");
            if (!isolated_pick_visual(&selected, candidate, active)) {
                save_log("ISOLATED: current station visual has ambiguous active track components");
                return 0;
            }
        }
    }
    if (!selected || !pf_root(roots, selected)) {
        save_log("ISOLATED: current station track component is inactive or unavailable");
        return 0;
    }
    save_log(
        "ISOLATED: track component resolved from the selected station's current GameItemVisual");
    return selected;
}
static int isolated_collect(void *station, const unsigned *entity) {
    PFRoots roots = {0};
    int ok = 0, count = 0;
    if (!colour_identity(station, entity) || !pf_root(&roots, station))
        return 0;
    void *transform = browser_optional(station, "get_GameObjectTransform", "UnityEngine.Transform");
    if (!transform || !isolated_add_root(transform))
        goto done;
    void *visual = isolated_live_track_visual(station, entity, &roots);
    if (!visual || !isolated_add_root(visual))
        goto done;
    void *utils = findclass(p_domain_get(), "TPS.Game", "TrackRideUtils");
    const char *p[] = {"TPS.Game.GameItemInstance",
                       "System.Collections.Generic.List<TPS.Game.TrackRideUtils.TrackSectionData>",
                       "System.Boolean&"};
    void *method =
        utils ? pf_method(utils, "TryCreateTrackSectionData", "System.Boolean", 3, p, 0) : 0;
    if (!method || !write_code_guard(method, 0x2eac090, 0xe20, 0x6e37b30aU))
        goto done;
    void *lc = p_class_from_type(p_method_get_param(method, 1));
    void *list = write_new_list(lc, &roots);
    unsigned char closed = 0;
    void *a[] = {station, list, &closed};
    void *result = list ? pf_invoke(method, 0, a) : 0;
    if (!result || !pf_class(p_object_get_class(result), "System.Boolean") ||
        !p_object_unbox(result) || !*(unsigned char *)p_object_unbox(result) ||
        !invoke_count(list, lc, &count) || count < 2 || count > 256)
        goto done;
    const char *ip[] = {"System.Int32"};
    void *get = pf_method(lc, "get_Item", "TPS.Game.TrackRideUtils.TrackSectionData", 1, ip, 1);
    if (!get)
        goto done;
    for (int i = 0; i < count; ++i) {
        PFRoots part = {0};
        int valid = 0;
        void *ia[] = {&i};
        void *section = pf_invoke(get, list, ia);
        if (!section ||
            !pf_class(p_object_get_class(section), "TPS.Game.TrackRideUtils.TrackSectionData") ||
            !pf_root(&part, section))
            goto section_done;
        void *c = p_object_get_class(section);
        void *support = pf_ref(section, c, "SupportItem", "TPS.Game.GameItemInstance");
        void *piece =
            pf_ref(section, c, "TrackPieceInstance", "TPS.Game.LightweightEntityInstance");
        if (support) {
            void *rt =
                browser_optional(support, "get_GameObjectTransform", "UnityEngine.Transform");
            if (!rt || !isolated_add_root(rt))
                goto section_done;
        }
        if (piece) {
            void *rt = browser_optional(piece, "get_Transform", "UnityEngine.Transform");
            if (rt && !isolated_add_root(rt))
                goto section_done;
        }
        void *field = pf_field(c, "TrackPiece", "TPS.Game.ECTrackPiece");
        void *data = field ? p_field_get_value_object(field, section) : 0;
        if (data && pf_root(&part, data)) {
            void *dc = p_object_get_class(data);
            void *owner = pf_field(dc, "Station", "Unity.Entities.Entity");
            void *df = pf_field(dc, "TrackPieceDecoration", "Unity.Entities.Entity");
            unsigned source[2] = {0}, decor[2] = {0};
            if (!owner || !df)
                goto section_done;
            p_field_get_value(data, owner, source);
            p_field_get_value(data, df, decor);
            if (source[0] && !pf_same_entity(source, entity))
                goto section_done;
            if (decor[0] && decor[1]) {
                unsigned j = 0;
                for (; j < isolated_decoration_count; ++j)
                    if (pf_same_entity(isolated_decorations[j], decor))
                        break;
                if (j == isolated_decoration_count) {
                    if (j >= 256)
                        goto section_done;
                    memcpy(isolated_decorations[isolated_decoration_count++], decor, 8);
                }
            }
        }
        valid = 1;
    section_done:
        pf_release(&part);
        if (!valid)
            goto done;
    }
    /* Decorations are separate items: match exact live entity ownership, never
     * collect nearby scenery by distance or by shared decoration definition. */
    if (isolated_decoration_count) {
        void *rc = findclass(p_domain_get(), "UnityEngine", "Resources");
        void *type = browser_type("TPS.Game", "GameItemVisual");
        const char *rp[] = {"System.Type"};
        void *ra[] = {type};
        void *all = type ? pf_invoke(browser_method(rc, "FindObjectsOfTypeAll",
                                                    "UnityEngine.Object[]", 1, rp, 0),
                                     0, ra)
                         : 0;
        int n = 0;
        unsigned found[256] = {0};
        if (!all || !pf_root(&roots, all) || !browser_asset_array_count(all, &n))
            goto done;
        for (int i = 0; i < n; ++i) {
            void *v = write_array_item(all, i);
            if (!v || !browser_alive(v))
                continue;
            void *item = pf_ref(v, p_object_get_class(v), "Item", "TPS.Game.GameItemInstance");
            if (!item || !p_class_is_assignable_from(g_gameitem_class, p_object_get_class(item)))
                continue;
            unsigned id[2] = {0};
            p_field_get_value(item, g_entity_field, id);
            for (unsigned j = 0; j < isolated_decoration_count; ++j)
                if (pf_same_entity(id, isolated_decorations[j])) {
                    if (!isolated_add_root(v))
                        goto done;
                    found[j] = 1;
                }
        }
        for (unsigned j = 0; j < isolated_decoration_count; ++j)
            if (!found[j]) {
                save_log("ISOLATED: an attached decoration visual could not be resolved");
                goto done;
            }
    }
    ok = isolated_count > 0 && colour_identity(station, entity);
done:
    pf_release(&roots);
    return ok;
}
typedef struct {
    int (*restore_target)(void *);
    int (*release_resource)(void *);
    void (*destroy_resource)(void *);
} IsolatedCleanupOps;
static int isolated_restore_target(void *previous) {
    void *c = findclass(p_domain_get(), "UnityEngine", "RenderTexture");
    const char *p[] = {"UnityEngine.RenderTexture"};
    void *a[] = {previous};
    return write_void(browser_method(c, "set_active", "System.Void", 1, p, 0), 0, a);
}
static int isolated_release_resource(void *resource) {
    return write_void(
        browser_method(p_object_get_class(resource), "Release", "System.Void", 0, 0, 1), resource,
        0);
}
static const IsolatedCleanupOps isolated_cleanup_ops = {isolated_restore_target,
                                                        isolated_release_resource, preview_destroy};
/* Every failure path restores the borrowed target before disposing owned GPU
 * resources. A failed release must not prevent other resources being released. */
static int isolated_cleanup_resources(void *previous, void *command, void *target, void *texture,
                                      int created, int submitted, const IsolatedCleanupOps *ops) {
    int ok = 1;
    if (submitted && !ops->restore_target(previous))
        ok = 0;
    if (command && !ops->release_resource(command))
        ok = 0;
    if (created && target && !ops->release_resource(target))
        ok = 0;
    if (texture)
        ops->destroy_resource(texture);
    if (target)
        ops->destroy_resource(target);
    return ok;
}
#include "isolated_pipeline.inc"
/* Keep first-pass RGB; obtain alpha from the same isolated geometry without
 * post-processing. Every extra borrowed setting joins the existing journal. */
static int isolated_geometry_mask(void *camera, void *request, void *submit, void *rt,
                                  void *texture, unsigned char *colour, PFRoots *roots) {
    void *go = browser_optional(camera, "get_gameObject", "UnityEngine.GameObject");
    void *type = browser_type("UnityEngine.Rendering.Universal", "UniversalAdditionalCameraData");
    const char *cp[] = {"System.Type"};
    void *ca[] = {type};
    void *data = go && type ? pf_invoke(browser_method(p_object_get_class(go), "GetComponent",
                                                       "UnityEngine.Component", 1, cp, 1),
                                        go, ca)
                            : 0;
    if (!data || !pf_root(roots, data))
        return 0;
    void *pp = browser_optional(data, "get_renderPostProcessing", "System.Boolean");
    if (!pp || !pf_root(roots, pp))
        return 0;
    void *hdr = browser_optional(camera, "get_allowHDR", "System.Boolean");
    if (!hdr || !pf_root(roots, hdr))
        return 0;
    const char *bp[] = {"System.Boolean"};
    void *ps = browser_method(p_object_get_class(data), "set_renderPostProcessing", "System.Void",
                              1, bp, 1);
    void *hs = browser_method(p_object_get_class(camera), "set_allowHDR", "System.Void", 1, bp, 1);
    unsigned char no = 0;
    if (!pp || !hdr || !p_object_unbox(pp) || !p_object_unbox(hdr) ||
        !isolated_change_using(data, ps, 1, 0, p_object_unbox(pp), &no, isolated_apply) ||
        !isolated_change_using(camera, hs, 1, 0, p_object_unbox(hdr), &no, isolated_apply))
        return 0;
    void *args[] = {request};
    if (!write_void(submit, camera, args))
        return 0;
    void *rc = findclass(p_domain_get(), "UnityEngine", "RenderTexture");
    const char *ap[] = {"UnityEngine.RenderTexture"};
    void *aa[] = {rt};
    BrowserRect rect = {0, 0, 960, 540};
    int zero = 0;
    const char *rp[] = {"UnityEngine.Rect", "System.Int32", "System.Int32", "System.Boolean"};
    void *ra[] = {&rect, &zero, &zero, &no};
    if (!write_void(browser_method(rc, "set_active", "System.Void", 1, ap, 0), 0, aa) ||
        !write_void(
            browser_method(p_object_get_class(texture), "ReadPixels", "System.Void", 4, rp, 1),
            texture, ra))
        return 0;
    void *mask = browser_optional(texture, "GetPixels32", "UnityEngine.Color32[]");
    if (!mask || !pf_root(roots, mask) || preview_array_count(mask) != 960ULL * 540)
        return 0;
    return preview_geometry_alpha(colour, (const unsigned char *)mask + 32, 960 * 540);
}
static int isolated_capture(void *station, const unsigned *entity, const WCHAR *path) {
    PFRoots roots = {0};
    void *rt = 0, *texture = 0, *previous = 0;
    int ok = 0, created = 0, submitted = 0;
    const char *stage = "ISOLATED: renderer ownership collection failed";
    if (isolated_busy)
        return 0;
    isolated_busy = 1;
    isolated_clear();
    isolated_request_release();
    if (!isolated_collect(station, entity))
        goto done;
    stage = "ISOLATED: native procedural track ownership unavailable";
    if (!isolated_track_entries(station, entity))
        goto done;
    stage = "ISOLATED: URP request bindings unavailable";
    void *cc = findclass(p_domain_get(), "UnityEngine", "Camera");
    void *camera = pf_invoke(browser_method(cc, "get_main", "UnityEngine.Camera", 0, 0, 0), 0, 0);
    void *pm = findclass(p_domain_get(), "UnityEngine.Rendering", "RenderPipelineManager");
    void *pipeline = pf_invoke(
        browser_method(pm, "get_currentPipeline", "UnityEngine.Rendering.RenderPipeline", 0, 0, 0),
        0, 0);
    const char *requestParams[] = {"System.Object"};
    void *submit =
        browser_method(cc, "SubmitRenderRequestsInternal", "System.Void", 1, requestParams, 1);
    void *requestClass = isolated_full_class(
        "UnityEngine.Rendering.Universal.UniversalRenderPipeline.SingleCameraRequest");
    if (!camera || !pf_root(&roots, camera) || !submit || !pipeline ||
        !pf_class(p_object_get_class(pipeline),
                  "UnityEngine.Rendering.Universal.UniversalRenderPipeline") ||
        !requestClass || p_class_is_valuetype(requestClass))
        goto done;
    void *request = p_object_new(requestClass);
    void *destination = pf_field(requestClass, "destination", "UnityEngine.RenderTexture");
    void *mipField = pf_field(requestClass, "mipLevel", "System.Int32");
    void *faceField = pf_field(requestClass, "face", "UnityEngine.CubemapFace");
    void *sliceField = pf_field(requestClass, "slice", "System.Int32");
    if (!request || !pf_root(&roots, request) || !destination || !mipField || !faceField ||
        !sliceField)
        goto done;
    stage = "ISOLATED: scene render inventory unavailable";
    void *renderers = isolated_resources("UnityEngine", "Renderer");
    void *terrains = isolated_resources("UnityEngine", "Terrain");
    void *dc = findclass(p_domain_get(), "TPS.ProceduralDrawList", "TPSProceduralDrawList");
    void *df = dc ? p_class_get_field_from_name(dc, "s_Requests") : 0;
    void *dictionary = 0;
    if (!df || p_field_get_parent(df) != dc || !(p_field_get_flags(df) & 0x10) ||
        !pf_type(
            p_field_get_type(df),
            "System.Collections.Generic.Dictionary<System.Int64,TPS.ProceduralDrawList.TPSProceduralDrawRequest>"))
        goto done;
    p_field_static_get_value(df, &dictionary);
    if (!renderers || !terrains || !pf_root(&roots, renderers) || !pf_root(&roots, terrains))
        goto done;
    void *keys = 0;
    if (dictionary) {
        if (!pf_root(&roots, dictionary) || !(keys = isolated_dictionary_keys(dictionary, &roots)))
            goto done;
    } else if (!isolated_track_ready(0, isolated_spline_instances)) {
        goto done;
    }
    stage = "ISOLATED: off-screen target creation failed";
    unsigned char no = 0;
    void *rc = findclass(p_domain_get(), "UnityEngine", "RenderTexture");
    rt = rc ? p_object_new(rc) : 0;
    if (!rt || !pf_root(&roots, rt))
        goto done;
    int width = 960, height = 540, depth = 24, format = 0;
    const char *cp[] = {"System.Int32", "System.Int32", "System.Int32",
                        "UnityEngine.RenderTextureFormat"};
    void *ca[] = {&width, &height, &depth, &format};
    if (!write_void(browser_method(rc, ".ctor", "System.Void", 4, cp, 1), rt, ca))
        goto done;
    void *made = browser_optional(rt, "Create", "System.Boolean");
    if (!made || !p_object_unbox(made) || !*(unsigned char *)p_object_unbox(made))
        goto done;
    created = 1;
    stage = "ISOLATED: previous render target could not be read";
    void *activeGetter = browser_method(rc, "get_active", "UnityEngine.RenderTexture", 0, 0, 0);
    void *activeException = 0;
    if (!activeGetter)
        goto done;
    previous = p_runtime_invoke(activeGetter, 0, 0, &activeException);
    if (activeException || (previous && !pf_root(&roots, previous)))
        goto done;
    int zeroRequest = 0, face = -1;
    p_field_set_value_object(request, destination, rt);
    if (pf_ref(request, requestClass, "destination", "UnityEngine.RenderTexture") != rt)
        goto done;
    p_field_set_value(request, mipField, &zeroRequest);
    p_field_set_value(request, faceField, &face);
    p_field_set_value(request, sliceField, &zeroRequest);
    stage = "ISOLATED: native post-process alpha output binding unavailable";
    if (!isolated_alpha_output(&roots))
        goto done;
    stage = "ISOLATED: unrelated renderer suppression failed";
    if (!isolated_hide_renderers(renderers))
        goto done;
    stage = "ISOLATED: terrain suppression failed";
    if (!isolated_hide_terrain(terrains))
        goto done;
    stage = "ISOLATED: procedural draw-list filtering failed";
    if (dictionary && !isolated_hide_procedural(dictionary, keys))
        goto done;
    stage = "ISOLATED: camera background setup failed";
    if (!isolated_camera_background(camera))
        goto done;
    /* URP normally restores targetTexture itself, but retain a second restore
     * for exceptions inside pipeline callbacks. A null backbuffer is valid. */
    stage = "ISOLATED: camera target restoration binding unavailable";
    void *targetGetter =
        browser_method(cc, "get_targetTexture", "UnityEngine.RenderTexture", 0, 0, 1);
    void *targetException = 0;
    void *cameraTarget =
        targetGetter ? p_runtime_invoke(targetGetter, camera, 0, &targetException) : 0;
    const char *targetParams[] = {"UnityEngine.RenderTexture"};
    void *targetSetter = browser_method(cc, "set_targetTexture", "System.Void", 1, targetParams, 1);
    if (!targetGetter || targetException || (cameraTarget && !pf_root(&roots, cameraTarget)) ||
        !isolated_change_using(camera, targetSetter, 5, 0, &cameraTarget, &cameraTarget,
                               isolated_apply))
        goto done;
    stage = "ISOLATED: native pipeline camera render failed";
    void *requestArgs[] = {request};
    submitted = 1;
    if (!write_void(submit, camera, requestArgs))
        goto done;
    stage = "ISOLATED: image readback failed";
    const char *ap[] = {"UnityEngine.RenderTexture"};
    void *aa[] = {rt};
    if (!write_void(browser_method(rc, "set_active", "System.Void", 1, ap, 0), 0, aa))
        goto done;
    void *tc = findclass(p_domain_get(), "UnityEngine", "Texture2D");
    texture = tc ? p_object_new(tc) : 0;
    if (!texture || !pf_root(&roots, texture))
        goto done;
    const char *tcp[] = {"System.Int32", "System.Int32"};
    void *tca[] = {&width, &height};
    if (!write_void(browser_method(tc, ".ctor", "System.Void", 2, tcp, 1), texture, tca))
        goto done;
    BrowserRect rect = {0, 0, 960, 540};
    int zero = 0;
    const char *rp[] = {"UnityEngine.Rect", "System.Int32", "System.Int32", "System.Boolean"};
    void *ra[] = {&rect, &zero, &zero, &no};
    if (!write_void(browser_method(tc, "ReadPixels", "System.Void", 4, rp, 1), texture, ra) ||
        !write_void(browser_method(tc, "Apply", "System.Void", 0, 0, 1), texture, 0))
        goto done;
    void *pixels = browser_optional(texture, "GetPixels32", "UnityEngine.Color32[]");
    if (!pixels || !pf_root(&roots, pixels) || preview_array_count(pixels) != 960ULL * 540)
        goto done;
    unsigned char *px = (unsigned char *)pixels + 32;
    unsigned transparentPixels = 0, visiblePixels = 0;
    for (unsigned i = 0; i < 960 * 540; ++i) {
        if (px[i * 4 + 3] == 0)
            ++transparentPixels;
        if (px[i * 4 + 3] > 0)
            ++visiblePixels;
    }
    if (transparentPixels < 16 || visiblePixels < 16) {
        /* Some shipped URP variants discard alpha despite the asset flag.
         * Preserve rendered RGB and publish an opaque isolated preview instead
         * of blocking blueprint saves or guessing a mask from dark rail RGB. */
        if (isolated_geometry_mask(camera, request, submit, rt, texture, px, &roots)) {
            save_log("ISOLATED: geometry alpha pass accepted; original colour capture retained");
        } else {
            for (unsigned i = 0; i < 960 * 540; ++i)
                px[i * 4 + 3] = 255;
            save_log("ISOLATED: geometry alpha unavailable; opaque preview fallback retained");
        }
        const char *pp[] = {"UnityEngine.Color32[]"};
        void *pa[] = {pixels};
        stage = "ISOLATED: opaque fallback texture update failed";
        if (!write_void(browser_method(tc, "SetPixels32", "System.Void", 1, pp, 1), texture, pa) ||
            !write_void(browser_method(tc, "Apply", "System.Void", 0, 0, 1), texture, 0))
            goto done;
    }
    stage = "ISOLATED: rendering settings restoration failed";
    if (!isolated_restore_using(isolated_apply))
        goto done;
    save_log("ISOLATED: native pipeline rendered; borrowed rendering settings restored");
    unsigned different = 0;
    for (unsigned i = 13; i < 960 * 540; i += 13)
        if (px[i * 4] != px[0] || px[i * 4 + 1] != px[1] || px[i * 4 + 2] != px[2])
            ++different;
    if (different < 16) {
        stage = "ISOLATED: empty image; selected materials may need another render path";
        goto done;
    }
    void *convert = findclass(p_domain_get(), "UnityEngine", "ImageConversion");
    const char *pngParams[] = {"UnityEngine.Texture2D"};
    void *pngArgs[] = {texture};
    void *png = pf_invoke(browser_method(convert, "EncodeToPNG", "System.Byte[]", 1, pngParams, 0),
                          0, pngArgs);
    if (!png || !pf_root(&roots, png))
        goto done;
    unsigned long long bytes = preview_array_count(png);
    stage = "ISOLATED: writing PNG failed";
    if (!colour_identity(station, entity) || bytes < 57 || bytes > 20U * 1024U * 1024U ||
        !save_complete_png((unsigned char *)png + 32, (unsigned)bytes) ||
        !save_write_new(path, (unsigned char *)png + 32, (unsigned)bytes))
        goto done;
    ok = 1;
    save_log("ISOLATED: native URP selected coaster PNG captured");
done:
    if (!isolated_restore_using(isolated_apply)) {
        ok = 0;
        save_log("ISOLATED: restoring one or more borrowed rendering settings failed");
    }
    if (!isolated_cleanup_resources(previous, 0, rt, texture, created, submitted,
                                    &isolated_cleanup_ops)) {
        ok = 0;
        save_log("ISOLATED: GPU cleanup or target restoration was unavailable");
    }
    isolated_request_release();
    pf_release(&roots);
    isolated_clear();
    isolated_busy = 0;
    if (!ok)
        save_log(stage);
    return ok;
}
