#include "footprint_math.h"
static PFRoots footprint_roots;
#define CURVE_PREVIEW_MAX 240
static unsigned footprint_curve_count;
static float footprint_curve[CURVE_PREVIEW_MAX][3];
static void *footprint_go, *footprint_material, *footprint_unparented;
static UINT_PTR footprint_due;
static void footprint_destroy(void *object) {
    if (!object)
        return;
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *a[] = {object};
    write_void(pf_method(c, "Destroy", "System.Void", 1, p, 0), 0, a);
}
static void *footprint_wire_mesh;
static void footprint_badge_clear(void);
static void footprint_clear(void) {
    footprint_badge_clear();
    footprint_destroy(footprint_wire_mesh);
    footprint_wire_mesh = 0;
    footprint_destroy(footprint_unparented);
    footprint_unparented = 0;
    footprint_destroy(footprint_go);
    footprint_destroy(footprint_material);
    footprint_go = footprint_material = 0;
    footprint_due = 0;
    pf_release(&footprint_roots);
}
static int footprint_set(void *object, void *c, const char *name, const char *type, void *value) {
    const char *p[] = {type};
    void *a[] = {value};
    return write_void(pf_method(c, name, "System.Void", 1, p, 1), object, a);
}
#include "preview_rails.h"
#include "preview_supports.h"
#include "preview_smooth.h"
static void *preview_get(void *object, const char *name, const char *result) {
    if (!object)
        return 0;
    for (void *c = p_object_get_class(object); c; c = p_class_get_parent(c)) {
        void *m = pf_method(c, name, result, 0, 0, 1);
        if (m)
            return pf_invoke(m, object, 0);
    }
    return 0;
}
#include "preview_native_supports.inc"
#define PREVIEW_RENDER_MAX (CURVE_PREVIEW_MAX * 2)
static unsigned preview_render_count;
static float preview_render[PREVIEW_RENDER_MAX][3];
static float preview_support_lines[WRITE_MAX_SUPPORTS * 2][3];
static unsigned preview_support_vertices;
static float preview_left[PREVIEW_RENDER_MAX][3], preview_right[PREVIEW_RENDER_MAX][3];
static void *footprint_child(void *parent, void *gc, const char *name) {
    void *go = p_object_new(gc);
    footprint_unparented = go;
    if (!go || !pf_root(&footprint_roots, go))
        return 0;
    const char *cp[] = {"System.String"};
    void *ca[] = {p_string_new(name)};
    if (!write_void(pf_method(gc, ".ctor", "System.Void", 1, cp, 1), go, ca))
        return 0;
    void *tr = pf_invoke(pf_method(gc, "get_transform", "UnityEngine.Transform", 0, 0, 1), go, 0);
    if (!tr || !pf_root(&footprint_roots, tr))
        return 0;
    const char *pp[] = {"UnityEngine.Transform", "System.Boolean"};
    unsigned char no = 0;
    void *pa[] = {parent, &no};
    if (!write_void(pf_method(p_object_get_class(tr), "SetParent", "System.Void", 2, pp, 1), tr,
                    pa))
        return 0;
    footprint_unparented = 0;
    return go;
}
static void *footprint_component(void *go, void *gc, void *c) {
    if (!c)
        return 0;
    void *type = p_type_get_object(p_class_get_type(c));
    const char *p[] = {"System.Type"};
    void *a[] = {type};
    void *o = pf_invoke(pf_method(gc, "AddComponent", "UnityEngine.Component", 1, p, 1), go, a);
    return o && pf_root(&footprint_roots, o) ? o : 0;
}
static PFRoots footprint_badge_roots;
static void *footprint_badge_menu, *footprint_badge_group;
static int footprint_badge_owned;
static float footprint_badge_alpha;
static unsigned footprint_badge_attempts;
static UINT_PTR footprint_badge_due;
static void footprint_badge_clear(void) {
    if (footprint_badge_owned)
        footprint_destroy(footprint_badge_group);
    else if (footprint_badge_group) {
        void *current = preview_get(footprint_badge_group, "get_alpha", "System.Single");
        if (current && pf_class(p_object_get_class(current), "System.Single") &&
            *(float *)p_object_unbox(current) == 0)
            footprint_set(footprint_badge_group,
                          findclass(p_domain_get(), "UnityEngine", "CanvasGroup"), "set_alpha",
                          "System.Single", &footprint_badge_alpha);
    }
    footprint_badge_owned = 0;
    footprint_badge_menu = footprint_badge_group = 0;
    footprint_badge_attempts = 0;
    footprint_badge_due = 0;
    pf_release(&footprint_badge_roots);
}
static int footprint_badge_owner(void *owner) {
    if (owner == placement_wait_item)
        return owner != 0;
    if (!owner || !placement_wait_item || !g_entity_field || !g_managed_fields_verified ||
        !g_managed_class || !p_class_is_assignable_from(g_managed_class, p_object_get_class(owner)))
        return 0;
    unsigned a[2] = {0}, b[2] = {0};
    p_field_get_value(owner, g_entity_field, a);
    p_field_get_value(placement_wait_item, g_entity_field, b);
    return a[0] && a[1] && pf_same_entity(a, b);
}
static void footprint_badge_tick(void) {
    if (footprint_badge_group) {
        void *owner = pf_ref(footprint_badge_menu,
                             findclass(p_domain_get(), "TPS.Game.UI", "ExhibitBuzzRatingMenu"),
                             "_item", "TPS.Core.Entities.ManagedEntity");
        if (!footprint_badge_owner(owner)) {
            footprint_badge_clear();
            footprint_badge_attempts = 10;
        }
        return;
    }
    if (footprint_badge_attempts >= 10 || GetTickCount64() < footprint_badge_due)
        return;
    ++footprint_badge_attempts;
    footprint_badge_due = GetTickCount64() + 500;
    PFRoots roots = {0};
    void *resources = findclass(p_domain_get(), "UnityEngine", "Resources");
    void *c = findclass(p_domain_get(), "TPS.Game.UI", "ExhibitBuzzRatingMenu");
    if (!c)
        return;
    void *type = p_type_get_object(p_class_get_type(c));
    const char *p[] = {"System.Type"};
    void *a[] = {type};
    void *all = pf_invoke(
        pf_method(resources, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0), 0, a);
    int count = 0;
    if (all && pf_root(&roots, all) && write_array_count(all, &count)) {
        if (footprint_badge_attempts == 10) {
            logtext("FOOTPRINT BADGE: menus scanned=");
            lognum((unsigned)count);
            nl();
        }
        for (int i = 0; i < count; ++i) {
            void *menu = write_array_item(all, i);
            void *owner = pf_ref(menu, c, "_item", "TPS.Core.Entities.ManagedEntity");
            if (!footprint_badge_owner(owner)) {
                if (footprint_badge_attempts == 10 && i < 8) {
                    logtext("FOOTPRINT BADGE: owner=");
                    logtext(owner ? p_class_get_name(p_object_get_class(owner)) : "null");
                    nl();
                }
                continue;
            }
            if (footprint_badge_attempts == 1 || footprint_badge_attempts == 10)
                placement_log("BADGE STAGE: exact station owner matched");
            void *go = preview_get(menu, "get_gameObject", "UnityEngine.GameObject");
            if (!go) {
                if (footprint_badge_attempts == 1 || footprint_badge_attempts == 10)
                    placement_log("BADGE STAGE FAIL: inherited GameObject getter returned null");
                continue;
            }
            if (!pf_root(&roots, go)) {
                placement_log("BADGE STAGE FAIL: GameObject retention failed");
                break;
            }
            void *gc = findclass(p_domain_get(), "UnityEngine", "GameObject"),
                 *cc = findclass(p_domain_get(), "UnityEngine", "CanvasGroup");
            if (!cc) {
                placement_log("BADGE STAGE FAIL: CanvasGroup class unavailable");
                break;
            }
            void *t = p_type_get_object(p_class_get_type(cc));
            void *aa[] = {t};
            void *get = pf_method(gc, "GetComponent", "UnityEngine.Component", 1, p, 1);
            void *group = pf_invoke(get, go, aa);
            int owned = 0;
            float original = 1;
            if (group) {
                void *opacity = preview_get(group, "get_alpha", "System.Single");
                if (!opacity || !pf_class(p_object_get_class(opacity), "System.Single")) {
                    placement_log("BADGE STAGE FAIL: existing opacity unavailable");
                    break;
                }
                original = *(float *)p_object_unbox(opacity);
                if (!(original >= 0 && original <= 1)) {
                    placement_log("BADGE STAGE FAIL: existing opacity invalid");
                    break;
                }
            } else {
                void *add = pf_method(gc, "AddComponent", "UnityEngine.Component", 1, p, 1);
                if (!add) {
                    placement_log("BADGE STAGE FAIL: AddComponent signature unavailable");
                    break;
                }
                void *exception = 0;
                group = p_runtime_invoke(add, go, aa, &exception);
                if (exception) {
                    placement_log("BADGE STAGE FAIL: AddComponent raised managed exception");
                    group = 0;
                }
                owned = group != 0;
            }
            if (!group) {
                placement_log("BADGE STAGE FAIL: CanvasGroup unavailable");
                break;
            }
            if (!pf_root(&footprint_badge_roots, group) || !pf_root(&footprint_badge_roots, menu)) {
                placement_log("BADGE STAGE FAIL: CanvasGroup retention failed");
                if (owned)
                    footprint_destroy(group);
                pf_release(&footprint_badge_roots);
                break;
            }
            footprint_badge_owned = owned;
            footprint_badge_alpha = original;
            placement_log(owned ? "BADGE STAGE: temporary CanvasGroup created"
                                : "BADGE STAGE: existing CanvasGroup opacity preserved");
            footprint_badge_group = group;
            footprint_badge_menu = menu;
            float hidden = 0;
            unsigned char no = 0;
            int alpha = footprint_set(group, cc, "set_alpha", "System.Single", &hidden),
                raycasts =
                    !owned || footprint_set(group, cc, "set_blocksRaycasts", "System.Boolean", &no),
                interaction =
                    !owned || footprint_set(group, cc, "set_interactable", "System.Boolean", &no);
            if (!alpha)
                placement_log("BADGE STAGE FAIL: opacity setter failed");
            if (!raycasts)
                placement_log("BADGE STAGE FAIL: raycast setter failed");
            if (!interaction)
                placement_log("BADGE STAGE FAIL: interaction setter failed");
            if (!alpha || !raycasts || !interaction) {
                footprint_badge_clear();
                footprint_badge_attempts = 10;
            } else {
                void *opacity = preview_get(group, "get_alpha", "System.Single");
                if (opacity && pf_class(p_object_get_class(opacity), "System.Single")) {
                    logtext("BADGE STAGE: opacity readback=");
                    float value = *(float *)p_object_unbox(opacity);
                    logtext(value == 0 ? "zero" : "nonzero");
                    nl();
                }
                placement_log("FOOTPRINT: exact station rating badge hidden until preview ends");
            }
            break;
        }
    }
    if (!footprint_badge_group && footprint_badge_attempts == 10) {
        placement_log(
            "FOOTPRINT: station buzz badge hide incomplete after bounded lookup; see BADGE STAGE diagnostics");
    }
    pf_release(&roots);
}
static int footprint_rail(void *parent, void *gc, void *lc, float points[][3]) {
    void *go = footprint_child(parent, gc, "Coaster preview rail");
    void *line = go ? footprint_component(go, gc, lc) : 0;
    if (!line)
        return 0;
    void *rc = findclass(p_domain_get(), "UnityEngine", "Renderer");
    int count = (int)preview_render_count;
    unsigned char no = 0, yes = 1;
    float width = .085f, color[4] = {1, .82f, .12f, .95f};
    if (!footprint_set(line, rc, "set_sharedMaterial", "UnityEngine.Material",
                       footprint_material) ||
        !footprint_set(line, lc, "set_positionCount", "System.Int32", &count) ||
        !footprint_set(line, lc, "set_loop", "System.Boolean", &yes) ||
        !footprint_set(line, lc, "set_useWorldSpace", "System.Boolean", &no) ||
        !footprint_set(line, lc, "set_startWidth", "System.Single", &width) ||
        !footprint_set(line, lc, "set_endWidth", "System.Single", &width) ||
        !footprint_set(line, lc, "set_startColor", "UnityEngine.Color", color) ||
        !footprint_set(line, lc, "set_endColor", "UnityEngine.Color", color))
        return 0;
    const char *p[] = {"System.Int32", "UnityEngine.Vector3"};
    void *set = pf_method(lc, "SetPosition", "System.Void", 2, p, 1);
    for (int i = 0; i < count; ++i) {
        void *a[] = {&i, points[i]};
        if (!write_void(set, line, a))
            return 0;
    }
    return 1;
}
static int footprint_guides(void *parent, void *gc) {
    PFRoots temp = {0};
    int ok = 0;
    void *go = footprint_child(parent, gc, "Coaster preview cross ties and height guides");
    if (!go)
        return 0;
    void *mc = findclass(p_domain_get(), "UnityEngine", "Mesh");
    void *mf = footprint_component(go, gc, findclass(p_domain_get(), "UnityEngine", "MeshFilter"));
    void *mr =
        footprint_component(go, gc, findclass(p_domain_get(), "UnityEngine", "MeshRenderer"));
    if (!mc || !mf || !mr)
        goto done;
    footprint_wire_mesh = p_object_new(mc);
    if (!footprint_wire_mesh || !pf_root(&footprint_roots, footprint_wire_mesh) ||
        !write_void(pf_method(mc, ".ctor", "System.Void", 0, 0, 1), footprint_wire_mesh, 0))
        goto done;
    preview_support_vertices = 0;
    float rotation[4];
    memcpy(rotation, write_source_rotation, 16);
    void *prefabRoot = 0;
    void *supportPrefab = preview_support_prefab(placement_wait_item, &temp, &prefabRoot);
    unsigned nativeCount = 0;
    for (int i = 0; i < write_count; ++i) {
        float world[3], local[2], base[3], height;
        memcpy(world, write_fixture[i].position, 12);
        footprint_local_point(write_source_position, rotation, world, local);
        base[0] = local[0];
        base[1] = world[1] - write_source_position[1] + .18f;
        base[2] = local[1];
        float offset[3], top[3];
        if (supportPrefab && preview_native_support_top(supportPrefab, prefabRoot,
                                                        write_fixture[i].height, offset)) {
            float supportRotation[4], rotated[3], worldTop[3], topXZ[2];
            memcpy(supportRotation, write_fixture[i].rotation, 16);
            preview_rotate_offset(supportRotation, offset, rotated);
            for (unsigned k = 0; k < 3; ++k)
                worldTop[k] = world[k] + rotated[k];
            footprint_local_point(write_source_position, rotation, worldTop, topXZ);
            top[0] = topXZ[0];
            top[1] = worldTop[1] - write_source_position[1];
            top[2] = topXZ[1];
            ++nativeCount;
        } else {
            if (!preview_support_top(footprint_curve, footprint_curve_count, base, &height))
                continue;
            memcpy(top, base, 12);
            top[1] = height;
        }
        memcpy(preview_support_lines[preview_support_vertices++], base, 12);
        memcpy(preview_support_lines[preview_support_vertices++], top, 12);
    }
    placement_log(
        nativeCount ? "FOOTPRINT: native prefab height attachments used for support guides"
                    : "FOOTPRINT: native support prefab unavailable; sampled-height fallback used");
    unsigned count = preview_support_vertices * 5;
    for (unsigned i = 0; i < preview_render_count; i += 8)
        count += 2;
    void *vc = findclass(p_domain_get(), "UnityEngine", "Vector3"),
         *ic = findclass(p_domain_get(), "System", "Int32"),
         *cc = findclass(p_domain_get(), "UnityEngine", "Color32");
    if (!write_value_layout(vc, 12) || !write_value_layout(cc, 4) || !write_value_layout(ic, 4))
        goto done;
    void *vertices = p_array_new(vc, count), *indices = p_array_new(ic, count),
         *colors = p_array_new(cc, count);
    if (!vertices || !indices || !colors || !pf_root(&temp, vertices) || !pf_root(&temp, indices) ||
        !pf_root(&temp, colors))
        goto done;
    void *arrays[] = {vertices, indices, colors};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned long long length = 0;
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), (unsigned char *)arrays[i] + 24, &length, 8,
                               &got) ||
            got != 8 || length != count)
            goto done;
    }
    float (*v)[3] = (float (*)[3])((unsigned char *)vertices + 32);
    unsigned *ix = (unsigned *)((unsigned char *)indices + 32);
    unsigned char *rgba = (unsigned char *)colors + 32;
    unsigned n = 0;
    for (unsigned i = 0; i < preview_render_count; i += 8) {
        memcpy(v[n++], preview_left[i], 12);
        memcpy(v[n++], preview_right[i], 12);
    }
    unsigned tieCount = n;
    for (unsigned i = 0; i < preview_support_vertices; ++i)
        memcpy(v[n++], preview_support_lines[i], 12);
    unsigned feetStart = n;
    for (unsigned i = 0; i < preview_support_vertices; i += 2) {
        const float *base = preview_support_lines[i];
        float corners[4][3] = {{base[0] - .24f, base[1], base[2] - .24f},
                               {base[0] + .24f, base[1], base[2] - .24f},
                               {base[0] + .24f, base[1], base[2] + .24f},
                               {base[0] - .24f, base[1], base[2] + .24f}};
        for (unsigned k = 0; k < 4; ++k) {
            memcpy(v[n++], corners[k], 12);
            memcpy(v[n++], corners[(k + 1) % 4], 12);
        }
    }

    for (unsigned i = 0; i < n; ++i) {
        ix[i] = i;
        rgba[i * 4] = 255;
        rgba[i * 4 + 1] = 215;
        rgba[i * 4 + 2] = 70;
        rgba[i * 4 + 3] = (unsigned char)(i < tieCount ? 170 : i < feetStart ? 145 : 200);
    }
    if (!footprint_set(footprint_wire_mesh, mc, "set_vertices", "UnityEngine.Vector3[]",
                       vertices) ||
        !footprint_set(footprint_wire_mesh, mc, "set_colors32", "UnityEngine.Color32[]", colors))
        goto done;
    const char *ip[] = {"System.Int32[]", "UnityEngine.MeshTopology", "System.Int32",
                        "System.Boolean"};
    int lines = 3, submesh = 0;
    unsigned char yes = 1;
    void *ia[] = {indices, &lines, &submesh, &yes};
    if (!write_void(pf_method(mc, "SetIndices", "System.Void", 4, ip, 1), footprint_wire_mesh, ia))
        goto done;
    if (!footprint_set(mf, p_object_get_class(mf), "set_sharedMesh", "UnityEngine.Mesh",
                       footprint_wire_mesh) ||
        !footprint_set(mr, findclass(p_domain_get(), "UnityEngine", "Renderer"),
                       "set_sharedMaterial", "UnityEngine.Material", footprint_material))
        goto done;
    ok = 1;
done:
    pf_release(&temp);
    return ok;
}
static int footprint_route(void *parent, void *gc, void *lc) {
    if (footprint_curve_count < 2)
        return 1;
    float side[3] = {1, 0, 0};
    preview_render_count = footprint_curve_count * 2;
    for (unsigned i = 0; i < footprint_curve_count; ++i)
        for (unsigned j = 0; j < 2; ++j)
            preview_smooth_point(
                footprint_curve[(i + footprint_curve_count - 1) % footprint_curve_count],
                footprint_curve[i], footprint_curve[(i + 1) % footprint_curve_count],
                footprint_curve[(i + 2) % footprint_curve_count], .5f * j,
                preview_render[2 * i + j]);
    for (unsigned i = 0; i < preview_render_count; ++i) {
        unsigned previous = i ? i - 1 : preview_render_count - 1,
                 next = (i + 1) % preview_render_count;
        preview_rail_side(preview_render[previous], preview_render[next], side);
        for (unsigned k = 0; k < 3; ++k) {
            preview_left[i][k] = preview_render[i][k] - .35f * side[k];
            preview_right[i][k] = preview_render[i][k] + .35f * side[k];
        }
    }
    return footprint_rail(parent, gc, lc, preview_left) &&
           footprint_rail(parent, gc, lc, preview_right) && footprint_guides(parent, gc);
}
static void footprint_start(void *station) {
    footprint_clear();
    if (!station || write_count < 1 || write_count > WRITE_MAX_SUPPORTS)
        return;
    void *sc = p_object_get_class(station);
    void *tm = get_station_method(sc, "get_GameObjectTransform", "UnityEngine.Transform");
    void *parent = tm ? pf_invoke(tm, station, 0) : 0;
    if (!parent || !pf_root(&footprint_roots, parent))
        goto fail;
    void *gc = findclass(p_domain_get(), "UnityEngine", "GameObject");
    footprint_go = gc ? p_object_new(gc) : 0;
    if (!footprint_go || !pf_root(&footprint_roots, footprint_go))
        goto fail;
    const char *cp[] = {"System.String"};
    void *ca[] = {p_string_new("Coaster template footprint guide")};
    if (!write_void(pf_method(gc, ".ctor", "System.Void", 1, cp, 1), footprint_go, ca))
        goto fail;
    void *transform = pf_invoke(pf_method(gc, "get_transform", "UnityEngine.Transform", 0, 0, 1),
                                footprint_go, 0);
    if (!transform || !pf_root(&footprint_roots, transform))
        goto fail;
    void *tc = p_object_get_class(transform);
    const char *pp[] = {"UnityEngine.Transform", "System.Boolean"};
    unsigned char no = 0, yes = 1;
    void *pa[] = {parent, &no};
    if (!write_void(pf_method(tc, "SetParent", "System.Void", 2, pp, 1), transform, pa))
        goto fail;
    float zero[3] = {0, 0, 0}, one[3] = {1, 1, 1}, identity[4] = {0, 0, 0, 1};
    if (!footprint_set(transform, tc, "set_localPosition", "UnityEngine.Vector3", zero) ||
        !footprint_set(transform, tc, "set_localRotation", "UnityEngine.Quaternion", identity) ||
        !footprint_set(transform, tc, "set_localScale", "UnityEngine.Vector3", one))
        goto fail;
    void *lc = findclass(p_domain_get(), "UnityEngine", "LineRenderer");
    if (!lc)
        goto fail;
    void *type = p_type_get_object(p_class_get_type(lc));
    const char *ap[] = {"System.Type"};
    void *aa[] = {type};
    void *line = pf_invoke(pf_method(gc, "AddComponent", "UnityEngine.Component", 1, ap, 1),
                           footprint_go, aa);
    if (!line || !pf_root(&footprint_roots, line))
        goto fail;
    void *shaderClass = findclass(p_domain_get(), "UnityEngine", "Shader");
    void *sa[] = {p_string_new("Sprites/Default")};
    void *shader = pf_invoke(pf_method(shaderClass, "Find", "UnityEngine.Shader", 1, cp, 0), 0, sa);
    if (!shader || !pf_root(&footprint_roots, shader))
        goto fail;
    void *mc = findclass(p_domain_get(), "UnityEngine", "Material");
    if (!mc)
        goto fail;
    footprint_material = p_object_new(mc);
    if (!footprint_material || !pf_root(&footprint_roots, footprint_material))
        goto fail;
    const char *mp[] = {"UnityEngine.Shader"};
    void *ma[] = {shader};
    if (!write_void(pf_method(mc, ".ctor", "System.Void", 1, mp, 1), footprint_material, ma))
        goto fail;
    void *rc = findclass(p_domain_get(), "UnityEngine", "Renderer");
    if (!footprint_set(line, rc, "set_sharedMaterial", "UnityEngine.Material", footprint_material))
        goto fail;
    int count = 4;
    float width = .10f, color[4] = {.45f, 1, .04f, .70f};
    if (!footprint_set(line, lc, "set_positionCount", "System.Int32", &count) ||
        !footprint_set(line, lc, "set_loop", "System.Boolean", &yes) ||
        !footprint_set(line, lc, "set_useWorldSpace", "System.Boolean", &no) ||
        !footprint_set(line, lc, "set_startWidth", "System.Single", &width) ||
        !footprint_set(line, lc, "set_endWidth", "System.Single", &width) ||
        !footprint_set(line, lc, "set_startColor", "UnityEngine.Color", color) ||
        !footprint_set(line, lc, "set_endColor", "UnityEngine.Color", color))
        goto fail;
    FootprintBounds b = {0};
    float q[4];
    memcpy(q, write_source_rotation, 16);
    for (int i = 0; i < write_count; ++i) {
        float point[3];
        memcpy(point, write_fixture[i].position, 12);
        footprint_extend(&b, write_source_position, q, point);
    }
    for (unsigned i = 0; i < footprint_curve_count; ++i) {
        float x = footprint_curve[i][0], z = footprint_curve[i][2];
        if (x < b.minx)
            b.minx = x;
        if (x > b.maxx)
            b.maxx = x;
        if (z < b.minz)
            b.minz = z;
        if (z > b.maxz)
            b.maxz = z;
    }
    b.minx -= 2;
    b.maxx += 2;
    b.minz -= 2;
    b.maxz += 2;
    float corners[4][3] = {{b.minx, .18f, b.minz},
                           {b.maxx, .18f, b.minz},
                           {b.maxx, .18f, b.maxz},
                           {b.minx, .18f, b.maxz}};
    const char *sp[] = {"System.Int32", "UnityEngine.Vector3"};
    void *set = pf_method(lc, "SetPosition", "System.Void", 2, sp, 1);
    for (int i = 0; i < 4; ++i) {
        void *a[] = {&i, corners[i]};
        if (!write_void(set, line, a))
            goto fail;
    }
    if (!footprint_route(transform, gc, lc))
        goto fail;
    placement_log(
        footprint_curve_count
            ? "FOOTPRINT: saved native 3D spline samples plus padded boundary active"
            : "FOOTPRINT: boundary only; save this coaster again to capture native spline preview");
    return;
fail:
    footprint_clear();
    placement_log("FOOTPRINT unavailable: normal station placement retained");
}
static void footprint_tick(void) {
    if (!footprint_go || GetTickCount64() < footprint_due)
        return;
    footprint_due = GetTickCount64() + 100;
    void *c = findclass(p_domain_get(), "TPS.Game", "BuildLogic");
    void *m =
        c ? pf_method(c, "GetCursorModePlaceItem", "TPS.Game.CursorModePlaceItem", 0, 0, 0) : 0;
    void *cursor = m ? pf_invoke(m, 0, 0) : 0;
    void *get = cursor ? get_station_method(p_object_get_class(cursor), "get_Item",
                                            "TPS.Game.GameItemInstance")
                       : 0;
    void *item = get ? pf_invoke(get, cursor, 0) : 0;
    if (item != placement_wait_item)
        footprint_clear();
    else
        footprint_badge_tick();
}
