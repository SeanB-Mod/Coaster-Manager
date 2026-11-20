/* Native Unity UI proof, adapted from Expanded Colour Picker drawer.h.
 * New Toggle controls have no purchase listeners. Existing Buy controls are untouched. */
typedef struct {
    float x, y;
} BrowserV2;
typedef struct {
    float r, g, b, a;
} BrowserColor;
static PFRoots browser_roots, browser_button_roots[32], browser_decorations, browser_skin_roots;
static void *library_price_source;
static void *browser_purchase_sprite, *browser_negative_sprite, *browser_tab_sprite,
    *browser_tab_selected_sprite, *browser_paper_sprite, *browser_header_sprite,
    *browser_close_sprite;
static void *browser_paper_material, *browser_header_material, *browser_paper_rect;
typedef struct {
    float x, y, w, h;
} BrowserRect;
static float browser_width = 2100, browser_height = 1500;
static void library_list_cleanup(void);
static void library_theme_cleanup(void);
static int management_busy(void);
static int management_dialog_open(void);
static int management_construct(void *parent, void *font);
static void management_cleanup(void);
static PFRoots *browser_current_roots = &browser_roots;
static int browser_hold(void *o) {
    return pf_root(browser_current_roots, o);
}
static int browser_skin_hold(void *o) {
    for (unsigned i = 0; i < browser_skin_roots.n; ++i)
        if (p_gchandle_get_target(browser_skin_roots.h[i]) == o)
            return 1;
    return pf_root(&browser_skin_roots, o);
}
static void *browser_menu, *browser_panel, *browser_status, *browser_close, *browser_build;
static void *browser_rows[32], *browser_toggles[32];
typedef struct {
    void *row;
    long long definition;
} BrowserBinding;
static BrowserBinding browser_bindings[32];
static unsigned browser_count;
static int browser_bindings_match(const BrowserBinding *a, unsigned na, const BrowserBinding *b,
                                  unsigned nb) {
    if (na != nb)
        return 0;
    for (unsigned i = 0; i < na; ++i)
        if (a[i].row != b[i].row || a[i].definition != b[i].definition)
            return 0;
    return 1;
}
static int browser_failed, browser_open;
static long long browser_definition;
/* Read-only display cost from the selected game-native station purchase row.
 * Never used by placement, blueprint data, or construction billing. */
static int library_station_build_cost = -1;
static UINT_PTR browser_next;
static void browser_log(const char *s) {
    log_stamp();
    logtext("TEMPLATE UI: ");
    logtext(s);
    nl();
}
static void *browser_method(void *c, const char *n, const char *r, unsigned count, const char **p,
                            int instance) {
    for (; c; c = p_class_get_parent(c)) {
        void *m = pf_method(c, n, r, count, p, instance);
        if (m)
            return m;
    }
    return 0;
}
/* Resource inventories can exceed the import/config 256-item limit in a
 * populated museum. Only UI discovery uses this bounded larger allowance. */
static int browser_asset_array_count(void *array, int *out) {
    return write_array_count_limit(array, out, 65536);
}
static void *browser_get(void *o, const char *n, const char *r) {
    void *m = o ? browser_method(p_object_get_class(o), n, r, 0, 0, 1) : 0;
    void *v = pf_invoke(m, o, 0);
    if (!v)
        browser_failed = 1;
    return v;
}
static int browser_set(void *o, const char *n, const char *t, void *v) {
    const char *p[] = {t};
    void *m = o ? browser_method(p_object_get_class(o), n, "System.Void", 1, p, 1) : 0;
    void *a[] = {v};
    if (!write_void(m, o, a)) {
        browser_failed = 1;
        return 0;
    }
    return 1;
}
static int browser_alive(void *o) {
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "op_Implicit", "System.Boolean", 1, p, 0) : 0;
    void *a[] = {o};
    void *b = pf_invoke(m, 0, a);
    return b && pf_class(p_object_get_class(b), "System.Boolean") &&
           *(unsigned char *)p_object_unbox(b) == 1;
}
static void *browser_type(const char *ns, const char *name) {
    void *c = findclass(p_domain_get(), ns, name);
    return c ? p_type_get_object(p_class_get_type(c)) : 0;
}
static void *browser_add(void *g, const char *ns, const char *name) {
    if (!g) {
        browser_failed = 1;
        return 0;
    }
    const char *p[] = {"System.Type"};
    void *m =
        browser_method(p_object_get_class(g), "AddComponent", "UnityEngine.Component", 1, p, 1);
    void *t = browser_type(ns, name);
    void *a[] = {t};
    void *o = t ? pf_invoke(m, g, a) : 0;
    if (!o || !browser_hold(o)) {
        browser_failed = 1;
        return 0;
    }
    return o;
}
static void *browser_make(const char *name, void *parent, float x, float y, float w, float h,
                          float anchorX, float anchorY) {
    void *c = findclass(p_domain_get(), "UnityEngine", "GameObject");
    void *g = c ? p_object_new(c) : 0;
    if (!g || !browser_hold(g)) {
        browser_failed = 1;
        return 0;
    }
    const char *p[] = {"System.String"};
    void *m = pf_method(c, ".ctor", "System.Void", 1, p, 1);
    void *s = p_string_new(name);
    void *a[] = {s};
    if (!s || !write_void(m, g, a)) {
        browser_failed = 1;
        return 0;
    }
    unsigned char off = 0;
    browser_set(g, "SetActive", "System.Boolean", &off);
    void *rect = browser_add(g, "UnityEngine", "RectTransform");
    const char *pp[] = {"UnityEngine.Transform", "System.Boolean"};
    void *pa[] = {parent, &off};
    m = rect ? browser_method(p_object_get_class(rect), "SetParent", "System.Void", 2, pp, 1) : 0;
    if (!write_void(m, rect, pa))
        browser_failed = 1;
    BrowserV2 anchor = {anchorX, anchorY}, pivot = {.5f, .5f}, size = {w, h}, pos = {x, y};
    browser_set(rect, "set_anchorMin", "UnityEngine.Vector2", &anchor);
    browser_set(rect, "set_anchorMax", "UnityEngine.Vector2", &anchor);
    browser_set(rect, "set_pivot", "UnityEngine.Vector2", &pivot);
    browser_set(rect, "set_sizeDelta", "UnityEngine.Vector2", &size);
    browser_set(rect, "set_anchoredPosition", "UnityEngine.Vector2", &pos);
    void *pg = browser_get(parent, "get_gameObject", "UnityEngine.GameObject");
    void *layer = pg ? browser_get(pg, "get_layer", "System.Int32") : 0;
    if (layer)
        browser_set(g, "set_layer", "System.Int32", p_object_unbox(layer));
    return g;
}
static void browser_active(void *g, int on) {
    unsigned char v = (unsigned char)on;
    if (g)
        browser_set(g, "SetActive", "System.Boolean", &v);
}
static void *browser_image(void *g, BrowserColor color) {
    void *i = browser_add(g, "UnityEngine.UI", "Image");
    browser_set(i, "set_color", "UnityEngine.Color", &color);
    return i;
}
static void *browser_label(void *parent, void *font, const char *text, float x, float y, float w,
                           float h, float size) {
    void *g = browser_make("TPM template label", parent, x, y, w, h, .5f, .5f);
    if (!g)
        return 0;
    void *t = browser_add(g, "TMPro", "TextMeshProUGUI");
    BrowserColor white = {.16f, .39f, .53f, 1};
    unsigned char off = 0;
    int alignment = 514;
    browser_set(t, "set_font", "TMPro.TMP_FontAsset", font);
    browser_set(t, "set_fontSize", "System.Single", &size);
    browser_set(t, "set_color", "UnityEngine.Color", &white);
    browser_set(t, "set_raycastTarget", "System.Boolean", &off);
    browser_set(t, "set_alignment", "TMPro.TextAlignmentOptions", &alignment);
    browser_set(t, "set_richText", "System.Boolean", &off);
    browser_set(t, "set_text", "System.String", p_string_new(text));
    browser_active(g, 1);
    return t;
}
static void *browser_toggle(void *parent, void *font, const char *text, float x, float y, float w,
                            float h, float ax, float ay) {
    void *g = browser_make("TPM Templates button", parent, x, y, w, h, ax, ay);
    if (!g)
        return 0;
    BrowserColor blue = {1, .62f, .12f, 1};
    void *i = browser_image(g, blue);
    if (browser_purchase_sprite) {
        int sliced = 1;
        BrowserColor white = {1, 1, 1, 1};
        browser_set(i, "set_sprite", "UnityEngine.Sprite", browser_purchase_sprite);
        browser_set(i, "set_type", "UnityEngine.UI.Image.Type", &sliced);
        browser_set(i, "set_color", "UnityEngine.Color", &white);
    }
    void *t = browser_add(g, "UnityEngine.UI", "Toggle");
    browser_set(t, "set_targetGraphic", "UnityEngine.UI.Graphic", i);
    unsigned char off = 0;
    browser_set(t, "SetIsOnWithoutNotify", "System.Boolean", &off);
    void *transform = browser_get(g, "get_transform", "UnityEngine.Transform");
    void *label = browser_label(transform, font, text, 0, 0, w, h, 28);
    BrowserColor ink = {1, 1, 1, 1};
    if (equals(text, "X"))
        ink = (BrowserColor){.9f, .12f, .02f, 1};
    browser_set(label, "set_color", "UnityEngine.Color", &ink);
    browser_active(g, 1);
    return t;
}
static void browser_danger(void *toggle) {
    if (!toggle)
        return;
    void *graphic = browser_get(toggle, "get_targetGraphic", "UnityEngine.UI.Graphic");
    if (!graphic)
        return;
    BrowserColor red = {1, .45f, .30f, 1};
    int sliced = 1;
    browser_set(graphic, "set_sprite", "UnityEngine.Sprite",
                browser_negative_sprite ? browser_negative_sprite : browser_purchase_sprite);
    browser_set(graphic, "set_type", "UnityEngine.UI.Image.Type", &sliced);
    if (browser_negative_sprite)
        red = (BrowserColor){1, 1, 1, 1};
    browser_set(graphic, "set_color", "UnityEngine.Color", &red);
}
static void *browser_optional(void *o, const char *n, const char *r) {
    int old = browser_failed;
    void *v = browser_get(o, n, r);
    browser_failed = old;
    return v;
}
static void *browser_button_sprite(void *button, const char *state) {
    void *d = button ? pf_ref(button, p_object_get_class(button), "_stateData",
                              "TPS.Core.UI.UIButton.StateData")
                     : 0;
    void *normal =
        d ? pf_ref(d, p_object_get_class(d), state, "TPS.Core.UI.UIButton.ImageData") : 0;
    return normal ? pf_ref(normal, p_object_get_class(normal), "Idle", "UnityEngine.Sprite") : 0;
}
static void browser_skin_image(void *image, void *sprite, void *material) {
    if (!image || !sprite)
        return;
    BrowserColor white = {1, 1, 1, 1};
    int sliced = 1;
    browser_set(image, "set_sprite", "UnityEngine.Sprite", sprite);
    browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &sliced);
    browser_set(image, "set_color", "UnityEngine.Color", &white);
    if (material)
        browser_set(image, "set_material", "UnityEngine.Material", material);
}
static void browser_capture_skin(void *row) {
    if (browser_purchase_sprite)
        return;
    void *button = pf_ref(row, p_object_get_class(row), "_buttonBuyRide", "TPS.Core.UI.UIButton");
    void *sprite = browser_button_sprite(button, "Normal");
    if (sprite && browser_skin_hold(sprite))
        browser_purchase_sprite = sprite;
    /* UIButton state data can be a value type: sample its rendered background directly. */
    void *g = button ? browser_optional(button, "get_gameObject", "UnityEngine.GameObject") : 0;
    if (g) {
        const char *ip[] = {"System.Type", "System.Boolean"};
        unsigned char yes = 1;
        void *t = browser_type("UnityEngine.UI", "Image");
        void *ia[] = {t, &yes};
        PFRoots roots = {0};
        void *images = pf_invoke(browser_method(p_object_get_class(g), "GetComponentsInChildren",
                                                "UnityEngine.Component[]", 2, ip, 1),
                                 g, ia);
        int count = 0;
        float largest = 0;
        if (images && pf_root(&roots, images) && write_array_count(images, &count))
            for (int i = 0; i < count; ++i) {
                void *im = write_array_item(images, i);
                void *rt = browser_optional(im, "get_rectTransform", "UnityEngine.RectTransform");
                void *box = browser_optional(rt, "get_rect", "UnityEngine.Rect");
                void *sp = browser_optional(im, "get_sprite", "UnityEngine.Sprite");
                if (!box || !sp)
                    continue;
                BrowserRect b = *(BrowserRect *)p_object_unbox(box);
                if (b.w < 100 || b.h < 40 || b.w * b.h <= largest)
                    continue;
                if (browser_skin_hold(sp) && browser_skin_hold(im)) {
                    largest = b.w * b.h;
                    browser_purchase_sprite = sp;
                    library_price_source = im;
                }
            }
        pf_release(&roots);
    }
    sprite = browser_button_sprite(button, "Negative");
    if (sprite && browser_skin_hold(sprite))
        browser_negative_sprite = sprite;
}
/* Sample only native artwork. Do not clone native purchase scripts or listeners. */
static void browser_capture_panel(void *owner) {
    int old = browser_failed;
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine.UI", "Image");
    void *a[] = {t, &yes};
    void *m = browser_method(p_object_get_class(owner), "GetComponentsInChildren",
                             "UnityEngine.Component[]", 2, p, 1);
    void *array = pf_invoke(m, owner, a);
    PFRoots roots = {0};
    int count = 0;
    float largest = 0;
    if (array && pf_root(&roots, array) && browser_asset_array_count(array, &count))
        for (int k = 0; k < count; ++k) {
            void *i = write_array_item(array, k);
            void *rect = browser_optional(i, "get_rectTransform", "UnityEngine.RectTransform");
            void *box = browser_optional(rect, "get_rect", "UnityEngine.Rect");
            void *sprite = browser_optional(i, "get_overrideSprite", "UnityEngine.Sprite");
            if (!sprite)
                sprite = browser_optional(i, "get_sprite", "UnityEngine.Sprite");
            if (!box || !sprite)
                continue;
            BrowserRect r = *(BrowserRect *)p_object_unbox(box);
            /* The native parchment is a large named Background; exclude the full-screen dimmer. */
            void *name = browser_optional(i, "get_name", "System.String");
            const WCHAR *n = name ? p_string_chars(name) : 0;

            if (n && p_string_length(name) == 10 && n[0] == 'B' && n[1] == 'a' && n[2] == 'c' &&
                n[3] == 'k' && n[4] == 'g' && n[5] == 'r' && n[6] == 'o' && n[7] == 'u' &&
                n[8] == 'n' && n[9] == 'd' && r.w > 500 && r.h > 500 && r.w * r.h > largest) {
                largest = r.w * r.h;
                browser_paper_sprite = sprite;
                browser_paper_rect = rect;
                browser_paper_material =
                    browser_optional(i, "get_material", "UnityEngine.Material");
                browser_width = r.w;
                browser_height = r.h;
            }
            if (n && p_string_length(name) == 10 && n[0] == 'B' && n[1] == 'a' && n[2] == 'c' &&
                n[3] == 'k' && n[4] == 'g' && n[5] == 'r' && n[6] == 'o' && n[7] == 'u' &&
                n[8] == 'n' && n[9] == 'd' && r.w > 500 && r.h > 60 && r.h < 180) {
                browser_header_sprite = sprite;
                browser_header_material =
                    browser_optional(i, "get_material", "UnityEngine.Material");
            }
            if (n && p_string_length(name) == 10 && n[0] == 'B' && n[1] == 'a' && n[2] == 'c' &&
                n[3] == 'k' && n[4] == 'g' && n[5] == 'r' && n[6] == 'o' && n[7] == 'u' &&
                n[8] == 'n' && n[9] == 'd' && r.w > 75 && r.w < 120 && r.h > 60 && r.h < 110)
                browser_close_sprite = sprite;
        }
    if (pf_class(p_object_get_class(owner), "TPS.Game.UI.RideBuildingMenu")) {
        void *group =
            pf_ref(owner, p_object_get_class(owner), "_tabGroup", "TPS.Core.UI.UIButtonGroup");
        void *buttons =
            group ? pf_ref(group, p_object_get_class(group), "_buttons", "TPS.Core.UI.UIButton[]")
                  : 0;
        int tabs = 0;
        if (buttons && pf_root(&roots, buttons) && browser_asset_array_count(buttons, &tabs) &&
            tabs) {
            void *button = write_array_item(buttons, 0);
            browser_tab_sprite = browser_button_sprite(button, "Normal");
            browser_tab_selected_sprite = browser_button_sprite(button, "Selected");
        }
    }
    void *assets[] = {browser_paper_sprite,  browser_paper_rect,         browser_paper_material,
                      browser_header_sprite, browser_header_material,    browser_close_sprite,
                      browser_tab_sprite,    browser_tab_selected_sprite};
    for (unsigned i = 0; i < 8; ++i)
        if (assets[i] && !browser_skin_hold(assets[i]))
            browser_failed = 1;
    pf_release(&roots);
    browser_failed = old;
    browser_log(browser_paper_sprite ? "THEME: native parchment found"
                                     : "THEME: native parchment unavailable");
    logtext("TEMPLATE UI: native paper dimensions ");
    lognum((unsigned)browser_width);
    logtext(" x ");
    lognum((unsigned)browser_height);
    nl();
    browser_log(browser_header_sprite ? "THEME: native ribbon found"
                                      : "THEME: native ribbon unavailable");
}
#include "blueprint_side_buttons.inc"
static int browser_pressed(void *t) {
    if (!t)
        return 0;
    void *b = browser_get(t, "get_isOn", "System.Boolean");
    void *v = b ? p_object_unbox(b) : 0;
    if (!v || *(unsigned char *)v != 1)
        return 0;
    unsigned char off = 0;
    browser_set(t, "SetIsOnWithoutNotify", "System.Boolean", &off);
    return 1;
}
/* Retitle only the exact English native header while this integration owns
 * the menu. Keep the original managed text rooted and restore it on teardown. */
static PFRoots browser_native_title_roots;
static void *browser_native_title, *browser_native_title_original;
static int browser_plain_text(void *text, const char *wanted) {
    if (!text || p_string_length(text) != (int)length(wanted))
        return 0;
    const WCHAR *chars = p_string_chars(text);
    for (unsigned i = 0; wanted[i]; ++i)
        if (!chars || chars[i] != (WCHAR)(unsigned char)wanted[i])
            return 0;
    return 1;
}
static void browser_native_title_restore(void) {
    if (browser_native_title && browser_alive(browser_native_title)) {
        void *text = browser_optional(browser_native_title, "get_text", "System.String");
        if (browser_plain_text(text, "Funfair Templates"))
            browser_set(browser_native_title, "set_text", "System.String",
                        browser_native_title_original);
    }
    browser_native_title = browser_native_title_original = 0;
    pf_release(&browser_native_title_roots);
}
static void browser_native_title_update(void *menu) {
    int previous = browser_failed;
    PFRoots roots = {0};
    void *go = browser_optional(menu, "get_gameObject", "UnityEngine.GameObject");
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *type = browser_type("TMPro", "TMP_Text");
    void *ca[] = {type, &yes};
    void *all = go && type
                    ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                               "UnityEngine.Component[]", 2, cp, 1),
                                go, ca)
                    : 0;
    int count = 0;
    if (all && pf_root(&roots, all) && write_array_count_limit(all, &count, 1024))
        for (int i = 0; i < count; ++i) {
            void *label = write_array_item(all, i);
            void *text = label ? browser_optional(label, "get_text", "System.String") : 0;
            if (!browser_plain_text(text, "Funfair Blueprints"))
                continue;
            if (pf_root(&browser_native_title_roots, label) &&
                pf_root(&browser_native_title_roots, text)) {
                browser_native_title = label;
                browser_native_title_original = text;
                browser_set(label, "set_text", "System.String", p_string_new("Funfair Templates"));
            } else
                pf_release(&browser_native_title_roots);
            break;
        }
    pf_release(&roots);
    browser_failed = previous;
}
static void library_clear_assets(void);
static void library_cleanup(void);
static void browser_cleanup(void) {
    browser_native_title_restore();
    management_cleanup();
    library_list_cleanup();
    library_cleanup();
    library_theme_cleanup();
    browser_side_cleanup();
    /* Deactivate all owned GameObjects before deferred destruction. Never destroy menu/rows. */
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "Destroy", "System.Void", 1, p, 0) : 0;
    for (unsigned group = 0; group < 34; ++group) {
        PFRoots *r = group == 0    ? &browser_roots
                     : group == 33 ? &browser_decorations
                                   : &browser_button_roots[group - 1];
        for (unsigned i = 0; i < r->n; ++i) {
            void *o = p_gchandle_get_target(r->h[i]);
            if (o && pf_class(p_object_get_class(o), "UnityEngine.GameObject") &&
                browser_alive(o)) {
                browser_active(o, 0);
                void *a[] = {o};
                write_void(m, 0, a);
            }
        }
        pf_release(r);
    }
    pf_release(&browser_skin_roots);
    browser_negative_sprite = browser_tab_selected_sprite = browser_tab_sprite =
        browser_purchase_sprite = browser_paper_sprite = browser_header_sprite =
            browser_close_sprite = 0;
    browser_paper_material = browser_header_material = browser_paper_rect = 0;
    browser_current_roots = &browser_roots;
    browser_menu = browser_panel = browser_status = browser_close = browser_build = 0;
    memset(browser_rows, 0, sizeof(browser_rows));
    memset(browser_toggles, 0, sizeof(browser_toggles));
    browser_count = 0;
    browser_failed = browser_open = 0;
    browser_definition = 0;
    library_station_build_cost = -1;
}
static void *browser_row_definition(void *row, PFRoots *roots) {
    void *c = p_object_get_class(row);
    void *data = pf_ref(row, c, "_currentData", "TPS.Game.UI.RideBuildingMenu.ItemData");
    if (!data || !pf_root(roots, data))
        return 0;
    void *available = pf_ref(data, p_object_get_class(data), "AvailalableExhibitRideBlueprint",
                             "TPS.Game.AvailableExhibitRideBlueprint");
    if (!available || !pf_root(roots, available))
        return 0;
    void *bp = pf_invoke(get_station_method(p_object_get_class(available), "get_Definition",
                                            "TPS.Game.ExhibitRideBlueprintDefinition"),
                         available, 0);
    if (!bp || !pf_root(roots, bp))
        return 0;
    void *d = pf_invoke(get_station_method(p_object_get_class(bp), "get_ItemDefinition",
                                           "TPS.Game.GameItemDefinition"),
                        bp, 0);
    return d && pf_root(roots, d) ? d : 0;
}
static int browser_type_allowed(int value, const int *allowed) {
    for (unsigned i = 0; i < 6; ++i)
        if (value == allowed[i])
            return 1;
    return 0;
}
/* Track rides also include preset flumes and ring rides. Only the supported editable
 * coaster families get a blueprint button, independent of translated names. */
static int browser_is_roller_coaster(void *definition, PFRoots *roots) {
    if (!definition || !placement_has_track_proxy(definition, roots))
        return 0;
    void *enumClass = findclass(p_domain_get(), "TPS.Game", "TrackRideType");
    if (!enumClass || !write_value_layout(enumClass, 4))
        return 0;
    const char *names[] = {"Steel", "SteelInverse", "Spinning",
                           "Baby",  "Steeplechase", "Amusement"};
    int allowed[6] = {0};
    for (unsigned i = 0; i < 6; ++i) {
        void *f = p_class_get_field_from_name(enumClass, names[i]);
        if (!f || p_field_get_parent(f) != enumClass || (p_field_get_flags(f) & 0x50) != 0x50 ||
            !pf_type(p_field_get_type(f), "TPS.Game.TrackRideType"))
            return 0;
        p_field_static_get_value(f, &allowed[i]);
        for (unsigned j = 0; j < i; ++j)
            if (allowed[i] == allowed[j])
                return 0;
    }
    void *base = findclass(p_domain_get(), "TPS.Game", "EntityDefinition");
    void *array = base ? pf_ref(definition, base, "Components", "TPS.Game.IComponentProxy[]") : 0;
    int count = 0;
    if (!array || !pf_root(roots, array) || !write_array_count(array, &count))
        return 0;
    for (int i = 0; i < count; ++i) {
        void *proxy = write_array_item(array, i);
        if (!proxy || !pf_class(p_object_get_class(proxy), "TPS.Game.ECPTrackRide"))
            continue;
        PFRoots temp = {0};
        if (!pf_root(&temp, proxy))
            return 0;
        void *f = pf_field(p_object_get_class(proxy), "trackRideType", "TPS.Game.TrackRideType");
        int value = 0;
        int valid = f && write_value_layout(p_class_from_type(p_field_get_type(f)), 4);
        if (valid)
            p_field_get_value(proxy, f, &value);
        pf_release(&temp);
        return valid && browser_type_allowed(value, allowed);
    }
    return 0;
}
#include "library_theme.inc"
#include "library_json.h"
#include "blueprint_library.inc"
static void browser_open_for_row(void *row);

static void browser_open_for_row(void *row) {
    PFRoots roots = {0};
    void *c = p_object_get_class(row);
    void *data = pf_ref(row, c, "_currentData", "TPS.Game.UI.RideBuildingMenu.ItemData");
    if (data && !pf_root(&roots, data)) {
        pf_release(&roots);
        return;
    }
    void *available =
        data ? pf_ref(data, p_object_get_class(data), "AvailalableExhibitRideBlueprint",
                      "TPS.Game.AvailableExhibitRideBlueprint")
             : 0;
    if (available && !pf_root(&roots, available)) {
        pf_release(&roots);
        return;
    }
    void *blueprint =
        available ? pf_invoke(get_station_method(p_object_get_class(available), "get_Definition",
                                                 "TPS.Game.ExhibitRideBlueprintDefinition"),
                              available, 0)
                  : 0;
    if (blueprint && !pf_root(&roots, blueprint)) {
        pf_release(&roots);
        return;
    }
    void *definition =
        blueprint
            ? pf_invoke(get_station_method(p_object_get_class(blueprint), "get_ItemDefinition",
                                           "TPS.Game.GameItemDefinition"),
                        blueprint, 0)
            : 0;
    long long id = 0;
    if (!definition || !pf_root(&roots, definition) || !write_definition_id(definition, &id) ||
        !browser_is_roller_coaster(definition, &roots)) {
        pf_release(&roots);
        return;
    }
    /* Snapshot the current station price from its live native purchase row.
     * The template's saved displayStats.cost is track-only, and must not be edited.
     * Use exactly the same BuildCost field used by the normal purchase guard. */
    int liveStationCost = -1;
    if (!data || !write_read_limit(data, p_object_get_class(data), "BuildCost",
                                   "System.Int32", &liveStationCost, 4) ||
        liveStationCost < 0)
        liveStationCost = -1;
    pf_release(&roots);
    browser_definition = id;
    library_station_build_cost = liveStationCost;
    if (liveStationCost < 0)
        browser_log("PRICE: native station BuildCost unavailable; showing unknown totals");
    else {
        log_stamp();
        logtext("TEMPLATE UI: PRICE: live station BuildCost=");
        lognum((unsigned)liveStationCost);
        nl();
    }
    library_type_header(row);
    long long scanStart = perf_now();
    library_open_scan(id);
    UINT_PTR scanUs = perf_us(scanStart);
    long long renderStart = perf_now();
    library_display();
    log_stamp();
    logtext("PERF OPEN catalogue_us=");
    lognum(scanUs);
    logtext(" render_us=");
    lognum(perf_us(renderStart));
    perf_memory();
    nl();
    browser_open = 1;
    browser_side_sync();
    browser_active(browser_panel, 1);
    browser_log("OPEN: coaster-specific panel; native purchase buttons unchanged");
}
static void blueprint_browser_service(void) {
    if (p_string_new && p_thread_current()) {
        library_scroll_rows_tick();
        browser_side_sync();
    }
    if (!p_string_new || GetTickCount64() < browser_next || !p_thread_current())
        return;
    browser_next = GetTickCount64() + 100;
    library_repair_artwork(0);
    PFRoots roots = {0};
    void *menu = funfair_menu(&roots);
    if (!menu) {
        if (browser_open && browser_panel && browser_alive(browser_panel))
            browser_active(browser_panel, 0);
        browser_open = 0;
        pf_release(&roots);
        return;
    }
    void *owner = browser_get(menu, "get_gameObject", "UnityEngine.GameObject");
    const char *p[] = {"System.Type", "System.Boolean"};
    void *m = owner ? browser_method(p_object_get_class(owner), "GetComponentsInChildren",
                                     "UnityEngine.Component[]", 2, p, 1)
                    : 0;
    void *t = browser_type("TPS.Game.UI", "RideBuildingMenu_Item");
    unsigned char off = 0;
    void *a[] = {t, &off};
    void *array = pf_invoke(m, owner, a);
    int count = 0;
    if (!array || !pf_root(&roots, array) || !write_array_count(array, &count)) {
        browser_cleanup();
        pf_release(&roots);
        return;
    }
    BrowserBinding candidates[32];
    unsigned candidateCount = 0;
    for (int i = 0; i < count; ++i) {
        void *row = write_array_item(array, i);
        if (!row || !pf_class(p_object_get_class(row), "TPS.Game.UI.RideBuildingMenu_Item") ||
            !funfair_property_true(row, "get_isActiveAndEnabled"))
            continue;
        PFRoots check = {0};
        void *d = browser_row_definition(row, &check);
        long long id = 0;
        int coaster = d && write_definition_id(d, &id) && browser_is_roller_coaster(d, &check);
        pf_release(&check);
        if (!coaster)
            continue;
        if (candidateCount >= 32) {
            browser_cleanup();
            pf_release(&roots);
            return;
        }
        candidates[candidateCount++] = (BrowserBinding){row, id};
    }
    if (browser_menu &&
        (!browser_alive(browser_menu) || browser_menu != menu ||
         !browser_bindings_match(browser_bindings, browser_count, candidates, candidateCount))) {
        browser_log("REFRESH: visible coaster row bindings changed; rebuilding owned buttons");
        browser_cleanup();
    }
    if (!candidateCount) {
        pf_release(&roots);
        return;
    }
    if (!browser_menu) {
        browser_menu = menu;
        if (!pf_root(&browser_roots, menu)) {
            browser_cleanup();
            pf_release(&roots);
            return;
        }
        browser_native_title_update(menu);
        void *font = 0;
        for (unsigned i = 0; i < candidateCount; ++i) {
            void *row = candidates[i].row;
            void *label = pf_ref(row, p_object_get_class(row), "_nameLabel", "TMPro.TMP_Text");
            if (!font && label)
                font = browser_get(label, "get_font", "TMPro.TMP_FontAsset");
            if (!font)
                continue;
            browser_current_roots = &browser_button_roots[browser_count];
            if (!browser_hold(row)) {
                browser_failed = 1;
                break;
            }
            browser_rows[browser_count] = row;
            browser_capture_skin(row);
            /* Each row owns a bounded group of roots for its added controls. */
            browser_bindings[browser_count] = candidates[i];
            browser_toggles[browser_count] = browser_side_button(menu, row, font, browser_count);
            ++browser_count;
        }
        browser_current_roots = &browser_roots;
        if (!font || !browser_count) {
            browser_cleanup();
            pf_release(&roots);
            return;
        }
        if (!pf_root(&browser_roots, font)) {
            browser_cleanup();
            pf_release(&roots);
            return;
        }
        browser_capture_panel(menu);
        library_theme_resolve();
        browser_width = 2000;
        browser_height = 1340;
        void *parent = browser_paper_rect
                           ? browser_paper_rect
                           : browser_get(menu, "get_transform", "UnityEngine.Transform");
        browser_current_roots = &browser_roots;
        browser_panel = browser_make("Coaster type template library", parent, 0, 0, browser_width,
                                     browser_height, .5f, .5f);
        library_chrome_bank();
        void *canvas = browser_add(browser_panel, "UnityEngine", "Canvas");
        unsigned char override = 1;
        int sorting = 100;
        browser_set(canvas, "set_overrideSorting", "System.Boolean", &override);
        browser_set(canvas, "set_sortingOrder", "System.Int32", &sorting);
        browser_add(browser_panel, "UnityEngine.UI", "GraphicRaycaster");
        void *panelTransform = browser_get(browser_panel, "get_transform", "UnityEngine.Transform");
        library_chrome_bank();
        void *shield = library_art(panelTransform, "Library window input shield", 0, 40,
                                   browser_width + 20, browser_height + 120, 0, 0);
        BrowserColor clear = {1, 1, 1, 0};
        browser_set(shield, "set_color", "UnityEngine.Color", &clear);
        browser_set(shield, "set_raycastTarget", "System.Boolean", &override);
        library_chrome_bank();
        void *shadow = library_art(panelTransform, "Library rectangular shadow", -4, 18,
                                   browser_width + 20, browser_height + 54, 0, 0);
        BrowserColor dark = {.17f, .19f, .09f, .60f};
        browser_set(shadow, "set_color", "UnityEngine.Color", &dark);
        library_chrome_bank();
        library_paper_image = library_art(panelTransform, "Library parchment", 0, 0, browser_width,
                                          browser_height, library_theme_sprite[17], 1);
        unsigned char yes = 1;
        browser_set(library_paper_image, "set_raycastTarget", "System.Boolean", &yes);
        library_theme_paper(library_paper_image);
        library_chrome_bank();
        browser_status = browser_label(panelTransform, font, "", 0, -browser_height / 2 + 36,
                                       browser_width - 120, 44, 25);
        library_chrome_bank();
        browser_close = browser_toggle(panelTransform, font, "", browser_width / 2 - 52,
                                       browser_height / 2 + 46, 84, 76, .5f, .5f);
        void *closeGraphic =
            browser_get(browser_close, "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(closeGraphic,
                           browser_close_sprite ? browser_close_sprite : library_theme_sprite[19],
                           0);
        library_chrome_bank();
        void *head =
            library_art(panelTransform, "Native library blue ribbon", -30, browser_height / 2 + 40,
                        browser_width - 20, 120, library_theme_sprite[0], 1);
        library_ribbon_image = head;
        library_chrome_bank();
        library_heading = browser_label(panelTransform, font, "Coaster Templates", 40,
                                        browser_height / 2 + 50, browser_width - 420, 90, 48);
        BrowserColor white = {1, 1, 1, 1};
        int left = 513, bold = 1;
        browser_set(library_heading, "set_color", "UnityEngine.Color", &white);
        browser_set(library_heading, "set_alignment", "TMPro.TextAlignmentOptions", &left);
        browser_set(library_heading, "set_fontStyle", "TMPro.FontStyles", &bold);
        library_chrome_bank();
        void *closeCross =
            library_art(panelTransform, "Close library", browser_width / 2 - 44,
                        browser_height / 2 + 46, 40, 40, library_theme_sprite[16], 0);
        library_theme_close(closeGraphic, closeCross);
        library_chrome_bank();
        void *columns = library_art(panelTransform, "Native list column banner", 0,
                                    browser_height / 2 - 60, browser_width - 90, 58, 0, 0);
        BrowserColor blue = {.40f, .73f, .82f, 1};
        browser_set(columns, "set_color", "UnityEngine.Color", &blue);
        const char *labels[] = {"Preview", "Name", "Buzz", "Thrill", "Ferocity", "Nausea", "Cost"};
        float xs[] = {-722, -330, -70, 125, 300, 435, 625};
        float ws[] = {456, 300, 190, 190, 130, 130, 220};
        for (unsigned i = 0; i < 7; ++i) {
            library_chrome_bank();
            void *label = browser_label(panelTransform, font, labels[i], xs[i],
                                        browser_height / 2 - 60, ws[i], 52, 27);
            browser_set(label, "set_color", "UnityEngine.Color", &white);
            browser_set(label, "set_fontStyle", "TMPro.FontStyles", &bold);
        }
        library_chrome_bank();
        library_art(panelTransform, "Coaster type square frame", -browser_width / 2 + 64,
                    browser_height / 2 + 42, 165, 165, 0, 0);
        library_chrome_bank();
        void *iconBack =
            library_art(panelTransform, "Coaster icon square background", -browser_width / 2 + 64,
                        browser_height / 2 + 42, 150, 150, 0, 0);
        BrowserColor iconInk = {.22f, .29f, .27f, 1};
        browser_set(iconBack, "set_color", "UnityEngine.Color", &iconInk);
        library_chrome_bank();
        library_type_image =
            library_art(panelTransform, "Coaster type icon", -browser_width / 2 + 64,
                        browser_height / 2 + 42, 135, 135, 0, 0);
        browser_set(library_type_image, "set_preserveAspect", "System.Boolean", &yes);
        /* Internal build signal is hidden; each row owns its visible price button. */
        library_chrome_bank();
        browser_build = browser_toggle(panelTransform, font, "", 0, 0, 1, 1, .5f, .5f);
        browser_active(browser_get(browser_build, "get_gameObject", "UnityEngine.GameObject"), 0);
        browser_current_roots = &browser_roots;
        if (!library_list_construct(panelTransform, font) ||
            !management_construct(panelTransform, font))
            browser_failed = 1;
        library_chrome_bank();
        void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
        library_registration = rc ? p_object_new(rc) : 0;
        if (library_registration && browser_hold(library_registration)) {
            write_void(browser_method(rc, ".ctor", "System.Void", 0, 0, 1), library_registration,
                       0);
            const char *rp[] = {"UnityEngine.GameObject"};
            void *ra[] = {browser_panel};
            if (!write_void(browser_method(rc, "Register", "System.Void", 1, rp, 1),
                            library_registration, ra))
                browser_failed = 1;
        } else
            browser_failed = 1;
        browser_active(browser_panel, 0);
        if (browser_failed) {
            browser_log("DISABLED: UI construction failed; removing owned controls");
            browser_cleanup();
        } else
            browser_log("READY: visible rows have Templates buttons (UI proof)");
    }
    pf_release(&roots);
    if (!browser_menu || browser_failed)
        return;
    browser_side_hover_update();
    for (unsigned i = 0; i < browser_count; ++i)
        if (browser_pressed(browser_toggles[i]))
            browser_open_for_row(browser_rows[i]);
    if (browser_open && !management_dialog_open() && browser_pressed(browser_close)) {
        browser_active(browser_panel, 0);
        browser_open = 0;
        browser_side_sync();
    }
    if (browser_open && !management_busy()) {
        library_list_tick();
    }
    if (browser_open && browser_pressed(browser_build)) {
        if (library_load_plan()) {
            browser_active(browser_panel, 0);
            browser_open = 0;
            browser_log("BUILD: invoking existing guarded Funfair placement flow");
            write_library_selection = 1;
            placement_expected_definition = browser_definition;
            placement_report();
            placement_expected_definition = 0;
            write_library_selection = 0;
            browser_side_sync(); /* Restore native-menu tabs after a refused start. */
        } else {
            browser_log("BUILD SKIP: library validation failed or operation pending");
            browser_set(
                browser_status, "set_text", "System.String",
                p_string_new(
                    "Template could not be built.\n\nThe layout is invalid, changed, or a build is in progress.\n\nSee the mod log."));
        }
    }
}
