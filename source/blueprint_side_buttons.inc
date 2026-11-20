/* Reuse the proven process-lifetime metadata cache, never cache live objects. */
static void *save_cached_class(void *, const char *, const char *);
static void *save_cached_method(void *, const char *, const char *, unsigned, const char **, int);
static void *save_cached_get(void *, const char *, const char *);
static void *save_cached_optional(void *, const char *, const char *);
static int save_cached_set(void *, const char *, const char *, void *);
static int save_cached_alive(void *);
static int save_cached_true(void *, const char *);
#define findclass save_cached_class
#define browser_method save_cached_method
#define browser_get save_cached_get
#define browser_optional save_cached_optional
#define browser_set save_cached_set
#define browser_alive save_cached_alive
#define funfair_property_true save_cached_true
/* Change 1 only: outside-panel buttons. Native price transforms/listeners untouched.
 * Eyedropper is 46x46 under a 2x scale in the picker: 92x92 native menu units. */
#define BROWSER_SIDE_EDGE 92.f
static void *browser_side_canvas[32], *browser_side_go[32], *browser_side_registration[32],
    *browser_side_icon;
static void *browser_side_group[32], *browser_side_native_groups[32];
static struct {
    BrowserV2 position;
    float alpha;
    int initialized, shown, interactive;
} browser_side_last[32];
/* Reusable Unity assets outlive the transient coaster-menu UI. Keep GC handles
 * for the tiny group of verified icons/prefabs, not button instances or tooltip
 * text. Per-menu handles are still released by browser_side_cleanup(). */
static PFRoots browser_side_assets;
static PFRoots browser_side_shared_assets;
static int browser_side_icon_attempted;
static void *browser_side_tip[32], *browser_side_tip_prefab, *browser_side_tip_console;
static int browser_side_tip_attempted;
static void *browser_side_hover, *browser_side_sorted_instance;
static UINT_PTR browser_side_hover_started;
typedef struct {
    float x, y, z;
} BrowserV3;
typedef struct {
    BrowserV3 center, size;
} BrowserBounds;
static void browser_side_find_icon(void) {
    if (browser_side_icon && !browser_alive(browser_side_icon)) {
        /* A scene transition unloaded the cached icon; rebuild the small
         * shared asset cache on the next discovery pass. */
        pf_release(&browser_side_shared_assets);
        browser_side_icon = browser_side_tip_prefab = browser_side_tip_console = 0;
        browser_side_icon_attempted = browser_side_tip_attempted = 0;
    }
    if (browser_side_icon || browser_side_icon_attempted)
        return;
    browser_side_icon_attempted = 1;
    PFRoots roots = {0};
    /* Resolve the exact loaded asset, including inactive menus and atlas sub-sprites. */
    static const char target[] = "UI_Generic_T_Spritesheet_Building_buttonIcon_PlacementTools";
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *m = c ? pf_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0) : 0;
    void *t = browser_type("UnityEngine", "Sprite");
    void *a[] = {t};
    void *array = t ? pf_invoke(m, 0, a) : 0;
    int count = 0;
    if (array && pf_root(&roots, array)) {
        void *ac = findclass(p_domain_get(), "System", "Array");
        void *box =
            ac ? pf_invoke(pf_method(ac, "get_Length", "System.Int32", 0, 0, 1), array, 0) : 0;
        if (box && pf_class(p_object_get_class(box), "System.Int32"))
            count = *(int *)p_object_unbox(box);
        if (count < 0 || count > 65536)
            count = 0;
        for (int i = 0; i < count; ++i) {
            void *icon = write_array_item(array, i);
            void *name = icon ? browser_optional(icon, "get_name", "System.String") : 0;
            if (!name || p_string_length(name) != (int)(sizeof(target) - 1))
                continue;
            const WCHAR *chars = p_string_chars(name);
            int match = chars != 0;
            for (unsigned j = 0; match && j < sizeof(target) - 1; ++j)
                if (chars[j] != (WCHAR)(unsigned char)target[j])
                    match = 0;
            if (match && pf_root(&browser_side_shared_assets, icon)) {
                browser_side_icon = icon;
                break;
            }
        }
    }
    pf_release(&roots);
    browser_log(browser_side_icon
                    ? "SIDE BUTTON: exact PlacementTools sprite found; white tint"
                    : "SIDE BUTTON: exact PlacementTools sprite not loaded; BP fallback");
}
static int browser_side_point(void *parent, void *child, BrowserV3 point, BrowserV3 *out) {
    const char *p[] = {"UnityEngine.Vector3"};
    void *a[] = {&point};
    void *b = pf_invoke(
        browser_method(p_object_get_class(child), "TransformPoint", "UnityEngine.Vector3", 1, p, 1),
        child, a);
    if (!b || !pf_class(p_object_get_class(b), "UnityEngine.Vector3"))
        return 0;
    BrowserV3 world = *(BrowserV3 *)p_object_unbox(b);
    void *args[] = {&world};
    b = pf_invoke(browser_method(p_object_get_class(parent), "InverseTransformPoint",
                                 "UnityEngine.Vector3", 1, p, 1),
                  parent, args);
    if (!b || !pf_class(p_object_get_class(b), "UnityEngine.Vector3"))
        return 0;
    *out = *(BrowserV3 *)p_object_unbox(b);
    return 1;
}
static int browser_side_bounds(void *parent, void *child, BrowserBounds *out) {
    /* Use this RectTransform's own rectangle, never the bounds of scrolled descendants. */
    void *b = browser_optional(child, "get_rect", "UnityEngine.Rect");
    if (!b || !pf_class(p_object_get_class(b), "UnityEngine.Rect"))
        return 0;
    BrowserRect r = *(BrowserRect *)p_object_unbox(b);
    BrowserV3 low = {r.x, r.y, 0}, high = {r.x + r.w, r.y + r.h, 0}, a, bound;
    if (r.w <= 0 || r.h <= 0 || !browser_side_point(parent, child, low, &a) ||
        !browser_side_point(parent, child, high, &bound))
        return 0;
    out->center = (BrowserV3){(a.x + bound.x) / 2, (a.y + bound.y) / 2, 0};
    out->size = (BrowserV3){bound.x - a.x, bound.y - a.y, 0};
    return out->size.x > 0 && out->size.y > 0;
}
static void browser_side_style(void *toggle, void *row) {
    void *button = pf_ref(row, p_object_get_class(row), "_buttonBuyRide", "TPS.Core.UI.UIButton");
    /* Use the native yellow Normal artwork, as on Add Support and Edit.
   * _childGraphic is the text/icon, so it is not a background donor. */
    void *sprite = browser_button_sprite(button, "Normal");
    BrowserColor color = {1, 1, 1, 1};
    void *data = button ? pf_ref(button, p_object_get_class(button), "_stateData",
                                 "TPS.Core.UI.UIButton.StateData")
                        : 0;
    void *state =
        data ? pf_ref(data, p_object_get_class(data), "Normal", "TPS.Core.UI.UIButton.ImageData")
             : 0;
    void *field = state ? pf_field(p_object_get_class(state), "IdleTint", "UnityEngine.Color") : 0;
    if (field)
        p_field_get_value(state, field, &color);
    void *target = browser_get(toggle, "get_targetGraphic", "UnityEngine.UI.Graphic");
    if (sprite) {
        browser_skin_hold(sprite);
        browser_skin_image(target, sprite, 0);
        browser_set(target, "set_color", "UnityEngine.Color", &color);
    } else {
        BrowserColor green = {1, .82f, 0, 1};
        browser_set(target, "set_sprite", "UnityEngine.Sprite", 0);
        browser_set(target, "set_color", "UnityEngine.Color", &green);
        browser_log("SIDE BUTTON: yellow fallback; native Normal artwork unavailable");
    }
}
/* Exact types only: item/room tooltip subclasses contain placeholder widgets. */
static int browser_side_plain_tooltip(void *prefab) {
    if (!prefab)
        return 0;
    void *c = p_object_get_class(prefab);
    return pf_class(c, "TPS.Game.UI.Tooltip") || pf_class(c, "TPS.Game.UI.TooltipAnimated") ||
           pf_class(c, "TPS.Game.UI.TooltipMain") || pf_class(c, "TPS.Game.UI.TooltipWithTitle");
}
static void browser_side_tooltip(void *g, unsigned index) {
    int previous = browser_failed;
    PFRoots roots = {0};
    if (browser_side_tip_prefab &&
        (!browser_alive(browser_side_tip_prefab) ||
         (browser_side_tip_console && !browser_alive(browser_side_tip_console)))) {
        /* Prefabs can be invalidated by scene changes even when the managed
         * wrapper remains rooted. Drop only shared assets and rediscover. */
        pf_release(&browser_side_shared_assets);
        browser_side_icon = browser_side_tip_prefab = browser_side_tip_console = 0;
        browser_side_icon_attempted = browser_side_tip_attempted = 0;
    }
    if (!browser_side_tip_attempted) {
        browser_side_tip_attempted = 1;
        void *c = findclass(p_domain_get(), "UnityEngine", "Object");
        const char *p[] = {"System.Type", "System.Boolean"};
        void *t = browser_type("TPS.Game.UI", "TooltipSpawner");
        unsigned char yes = 1;
        void *a[] = {t, &yes};
        void *array =
            t ? pf_invoke(c ? pf_method(c, "FindObjectsOfType", "UnityEngine.Object[]", 2, p, 0)
                            : 0,
                          0, a)
              : 0;
        int count = 0;
        if (array && pf_root(&roots, array)) {
            void *ac = findclass(p_domain_get(), "System", "Array");
            void *length =
                ac ? pf_invoke(pf_method(ac, "get_Length", "System.Int32", 0, 0, 1), array, 0) : 0;
            if (length && pf_class(p_object_get_class(length), "System.Int32"))
                count = *(int *)p_object_unbox(length);
            if (count < 0 || count > 8192)
                count = 0;
        }
        logtext("TEMPLATE UI: tooltip donors scanned=");
        lognum((unsigned)count);
        nl();
        for (int i = 0; i < count; ++i) {
            void *donor = write_array_item(array, i);
            if (!donor)
                continue;
            void *prefab =
                pf_ref(donor, p_object_get_class(donor), "_prefab", "TPS.Game.UI.Tooltip");
            if (!browser_side_plain_tooltip(prefab) || !pf_root(&browser_side_shared_assets, prefab))
                continue;
            browser_side_tip_prefab = prefab;
            logtext("TEMPLATE UI: simple tooltip template=");
            logtext(p_class_get_name(p_object_get_class(prefab)));
            nl();
            void *console =
                pf_ref(donor, p_object_get_class(donor), "_prefabConsole", "TPS.Game.UI.Tooltip");
            if (browser_side_plain_tooltip(console) && pf_root(&browser_side_shared_assets, console))
                browser_side_tip_console = console;
            else
                browser_side_tip_console = prefab;
            break;
        }
        /* Some simple templates are loaded assets without a scene spawner. */
        if (!browser_side_tip_prefab) {
            void *resources = findclass(p_domain_get(), "UnityEngine", "Resources");
            const char *rp[] = {"System.Type"};
            void *tt = browser_type("TPS.Game.UI", "Tooltip");
            void *ra[] = {tt};
            void *all = tt ? pf_invoke(resources ? pf_method(resources, "FindObjectsOfTypeAll",
                                                             "UnityEngine.Object[]", 1, rp, 0)
                                                 : 0,
                                       0, ra)
                           : 0;
            int n = 0;
            if (all && pf_root(&roots, all)) {
                void *ac = findclass(p_domain_get(), "System", "Array");
                void *length =
                    ac ? pf_invoke(pf_method(ac, "get_Length", "System.Int32", 0, 0, 1), all, 0)
                       : 0;
                if (length && pf_class(p_object_get_class(length), "System.Int32"))
                    n = *(int *)p_object_unbox(length);
                if (n < 0 || n > 8192)
                    n = 0;
            }
            for (int i = 0; i < n; ++i) {
                void *prefab = write_array_item(all, i);
                if (!browser_side_plain_tooltip(prefab) || !pf_root(&browser_side_shared_assets, prefab))
                    continue;
                browser_side_tip_prefab = browser_side_tip_console = prefab;
                logtext("TEMPLATE UI: loaded simple tooltip template=");
                logtext(p_class_get_name(p_object_get_class(prefab)));
                nl();
                break;
            }
        }
        browser_log(browser_side_tip_prefab ? "SIDE BUTTON: native Templates tooltip ready"
                                            : "SIDE BUTTON: native tooltip prefab unavailable");
    }
    if (browser_side_tip_prefab) {
        void *parent = browser_get(browser_get(g, "get_transform", "UnityEngine.Transform"),
                                   "get_parent", "UnityEngine.Transform");
        void *host = browser_make("Template tooltip host", parent, 0, 0, 0, 0, .5f, .5f);
        browser_active(host, 1);
        void *tip = browser_add(host, "TPS.Game.UI", "TooltipSpawner");
        void *c = tip ? p_object_get_class(tip) : 0;
        void *f = c ? pf_field(c, "_prefab", "TPS.Game.UI.Tooltip") : 0;
        if (f)
            p_field_set_value_object(tip, f, browser_side_tip_prefab);
        f = c ? pf_field(c, "_prefabConsole", "TPS.Game.UI.Tooltip") : 0;
        if (f && browser_side_tip_console)
            p_field_set_value_object(tip, f, browser_side_tip_console);
        typedef struct {
            void *dev;
            int term, padding;
        } TooltipText;
        TooltipText text = {p_string_new("Templates"), 0, 0};
        f = c ? pf_field(c, "_tooltip", "TPS.Core.Localisation.LocalisedString") : 0;
        if (f && text.dev && write_value_layout(p_class_from_type(p_field_get_type(f)), 16) &&
            pf_root(&browser_side_assets, text.dev)) {
            p_field_set_value(tip, f, &text);
            browser_side_tip[index] = tip;
            unsigned char mouse = 0;
            f = pf_field(c, "_anchorToMouse", "System.Boolean");
            if (f)
                p_field_set_value(tip, f, &mouse);
            void *buttonTransform = browser_get(g, "get_transform", "UnityEngine.Transform");
            void *anchor =
                browser_make("Template tooltip fixed anchor", buttonTransform, 0, 8, 0, 0, .5f, 1);
            browser_active(anchor, 1);
            void *anchorTransform = browser_get(anchor, "get_transform", "UnityEngine.Transform");
            f = pf_field(c, "_anchorTransform", "UnityEngine.Transform");
            if (f)
                p_field_set_value_object(tip, f, anchorTransform);
            BrowserV3 offset = {0, 0, 0};
            f = pf_field(c, "_anchorOffset", "UnityEngine.Vector3");
            if (f)
                p_field_set_value(tip, f, &offset);
            write_void(browser_method(c, "Reset", "System.Void", 0, 0, 1), tip, 0);
        }
    }
    pf_release(&roots);
    browser_failed = previous;
}
static void *browser_side_button(void *menu, void *row, void *font, unsigned index) {
    browser_side_find_icon();
    void *parent = browser_get(menu, "get_transform", "UnityEngine.Transform");
    void *toggle = browser_toggle(parent, font, browser_side_icon ? "" : "BP", 0, 0,
                                  BROWSER_SIDE_EDGE, BROWSER_SIDE_EDGE, .5f, .5f);
    void *g = browser_get(toggle, "get_gameObject", "UnityEngine.GameObject");
    browser_side_go[index] = g;
    /* Independent sorting canvases do not reliably inherit the native fade.
     * Mirror the row's CanvasGroups explicitly, excluding our own ancestry. */
    void *group = browser_add(g, "UnityEngine", "CanvasGroup");
    browser_side_group[index] = group;
    unsigned char independent = 1;
    browser_set(group, "set_ignoreParentGroups", "System.Boolean", &independent);
    const char *gp[] = {"System.Type", "System.Boolean"};
    void *gt = browser_type("UnityEngine", "CanvasGroup");
    unsigned char includeInactive = 1;
    void *ga[] = {gt, &includeInactive};
    void *rowGo = browser_optional(row, "get_gameObject", "UnityEngine.GameObject");
    void *groups =
        rowGo ? pf_invoke(browser_method(p_object_get_class(rowGo), "GetComponentsInParent",
                                         "UnityEngine.Component[]", 2, gp, 1),
                          rowGo, ga)
              : 0;
    int groupCount = 0;
    if (groups && write_array_count_limit(groups, &groupCount, 32) && browser_hold(groups))
        browser_side_native_groups[index] = groups;
    else
        browser_failed = 1;
    float hidden = 0;
    browser_set(group, "set_alpha", "System.Single", &hidden);
    browser_side_style(toggle, row);
    /* Direct child of the menu, outside the list mask. Own sorting/raycaster only. */
    void *canvas = browser_add(g, "UnityEngine", "Canvas");
    browser_side_canvas[index] = canvas;
    unsigned char yes = 1;
    int order = 50;
    browser_set(canvas, "set_overrideSorting", "System.Boolean", &yes);
    browser_set(canvas, "set_sortingOrder", "System.Int32", &order);
    browser_add(g, "UnityEngine.UI", "GraphicRaycaster");
    void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
    void *registration = rc ? p_object_new(rc) : 0;
    if (registration && browser_hold(registration)) {
        browser_side_registration[index] = registration;
        if (!write_void(pf_method(rc, ".ctor", "System.Void", 0, 0, 1), registration, 0))
            browser_failed = 1;
        browser_set(registration, "Register", "UnityEngine.GameObject", g);
    } else
        browser_failed = 1;
    if (browser_side_icon) {
        void *rt = browser_get(g, "get_transform", "UnityEngine.Transform");
        void *icon = browser_make("Template template icon", rt, 0, 0, 54, 54, .5f, .5f);
        BrowserColor white = {1, 1, 1, 1};
        void *image = browser_image(icon, white);
        int simple = 0;
        unsigned char no = 0;
        browser_set(image, "set_sprite", "UnityEngine.Sprite", browser_side_icon);
        browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &simple);
        browser_set(image, "set_preserveAspect", "System.Boolean", &yes);
        browser_set(image, "set_raycastTarget", "System.Boolean", &no);
        browser_active(icon, 1);
    }
    browser_side_tooltip(g, index);
    return toggle;
}
static void browser_side_hover_update(void) {
    int previous = browser_failed;
    void *hover = 0;
    unsigned hoverIndex = 0;
    BrowserV2 mouse = {0};
    void *root = 0;
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "TPS.Core", "App");
    void *app = c ? pf_invoke(pf_method(c, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0) : 0;
    void *input =
        app ? browser_optional(app, "get_InputManager", "TPS.Core.Input.InputManager") : 0;
    void *position = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (position)
        mouse = *(BrowserV2 *)p_object_unbox(position);
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *canvas =
        pf_invoke(browser_method(p_object_get_class(browser_menu), "GetComponentInParent",
                                 "UnityEngine.Component", 2, cp, 1),
                  browser_menu, ca);
    root = canvas ? browser_optional(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *camera = 0;
    void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
    if (mode && *(int *)p_object_unbox(mode) != 0)
        camera = browser_optional(root, "get_worldCamera", "UnityEngine.Camera");
    if (position && root && !browser_open) {
        void *rc = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
        const char *hp[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2",
                            "UnityEngine.Camera"};
        void *hit =
            rc ? pf_method(rc, "RectangleContainsScreenPoint", "System.Boolean", 3, hp, 0) : 0;
        for (unsigned i = 0; i < browser_count; ++i) {
            void *g = browser_side_go[i];
            void *tip = browser_side_tip[i];
            if (!g || !tip || !funfair_property_true(g, "get_activeInHierarchy"))
                continue;
            void *rt = browser_optional(g, "get_transform", "UnityEngine.Transform");
            void *args[] = {rt, &mouse, camera};
            void *b = pf_invoke(hit, 0, args);
            if (b && pf_class(p_object_get_class(b), "System.Boolean") &&
                *(unsigned char *)p_object_unbox(b)) {
                hover = tip;
                hoverIndex = i;
                break;
            }
        }
    }
    if (hover != browser_side_hover) {
        if (browser_side_hover && browser_alive(browser_side_hover))
            write_void(browser_method(p_object_get_class(browser_side_hover), "Reset",
                                      "System.Void", 0, 0, 1),
                       browser_side_hover, 0);
        browser_side_hover = hover;
        browser_side_hover_started = GetTickCount64();
    }
    if (hover && GetTickCount64() - browser_side_hover_started >= 250) {
        void *f = pf_field(p_object_get_class(hover), "_cursorTime", "System.Single");
        float elapsed = .3f;
        if (f)
            p_field_set_value(hover, f, &elapsed);
        BrowserV3 point = {mouse.x, mouse.y, 0};
        int controller = 1;
        void *rt = browser_optional(root, "get_transform", "UnityEngine.Transform");
        const char *p[] = {"UnityEngine.Transform", "UnityEngine.Vector3",
                           "TPS.Core.PlatformUtils.ControllerType"};
        void *a[] = {rt, &point, &controller};
        write_void(browser_method(p_object_get_class(hover), "CursorOver", "System.Void", 3, p, 1),
                   hover, a);
        int tooltipLayer = 0, tooltipOrder = 70;
        void *buttonCanvas = browser_side_canvas[hoverIndex];
        void *layerBox =
            buttonCanvas ? browser_optional(buttonCanvas, "get_sortingLayerID", "System.Int32") : 0;
        if (layerBox)
            tooltipLayer = *(int *)p_object_unbox(layerBox);
        void *orderBox =
            buttonCanvas ? browser_optional(buttonCanvas, "get_sortingOrder", "System.Int32") : 0;
        if (orderBox)
            tooltipOrder = *(int *)p_object_unbox(orderBox) + 20;
        void *instance =
            pf_ref(hover, p_object_get_class(hover), "_tooltipInstance", "TPS.Game.UI.Tooltip");
        if (instance && browser_alive(instance)) {
            const char *ap[] = {"System.Type", "System.Boolean"};
            void *at = browser_type("UnityEngine", "Canvas");
            void *aa[] = {at, &yes};
            /* This game's IL2CPP build exposes the non-generic Type overload on
       * GameObject, not on the Tooltip Component. The previous lookup returned
       * no method, so no tooltip canvas was ever promoted. */
            void *tooltipGo =
                browser_optional(instance, "get_gameObject", "UnityEngine.GameObject");
            void *method =
                tooltipGo ? browser_method(p_object_get_class(tooltipGo), "GetComponentsInChildren",
                                           "UnityEngine.Component[]", 2, ap, 1)
                          : 0;
            void *array = pf_invoke(method, tooltipGo, aa);
            int count = 0;
            if (array && pf_root(&roots, array) && write_array_count(array, &count)) {
                for (int i = 0; i < count; ++i) {
                    void *cv = write_array_item(array, i);
                    browser_set(cv, "set_overrideSorting", "System.Boolean", &yes);
                    browser_set(cv, "set_sortingLayerID", "System.Int32", &tooltipLayer);
                    browser_set(cv, "set_sortingOrder", "System.Int32", &tooltipOrder);
                }
                if (!count) {
                    void *g = tooltipGo;
                    const char *addParams[] = {"System.Type"};
                    void *addArgs[] = {at};
                    void *cv =
                        g ? pf_invoke(browser_method(p_object_get_class(g), "AddComponent",
                                                     "UnityEngine.Component", 1, addParams, 1),
                                      g, addArgs)
                          : 0;
                    if (cv) {
                        browser_set(cv, "set_overrideSorting", "System.Boolean", &yes);
                        browser_set(cv, "set_sortingLayerID", "System.Int32", &tooltipLayer);
                        browser_set(cv, "set_sortingOrder", "System.Int32", &tooltipOrder);
                        count = 1;
                    }
                }
                if (browser_side_sorted_instance != instance) {
                    browser_side_sorted_instance = instance;
                    logtext("TEMPLATE UI: tooltip canvas count=");
                    lognum((unsigned)count);
                    logtext(" order=");
                    lognum((unsigned)tooltipOrder);
                    nl();
                }
            }
        }
    }
    pf_release(&roots);
    browser_failed = previous;
}
static float browser_side_alpha(unsigned index) {
    void *row = browser_rows[index];
    if (!row || !browser_alive(row) || !funfair_property_true(row, "get_isActiveAndEnabled"))
        return 0;
    void *array = browser_side_native_groups[index];
    int count = 0;
    if (!array || !write_array_count_limit(array, &count, 32))
        return 0;
    float alpha = 1;
    for (int j = 0; j < count; ++j) {
        void *group = write_array_item(array, j);
        if (!group || !browser_alive(group) ||
            !funfair_property_true(group, "get_isActiveAndEnabled"))
            continue;
        void *box = browser_optional(group, "get_alpha", "System.Single");
        if (!box || !p_object_unbox(box))
            return 0;
        float value = *(float *)p_object_unbox(box);
        if (!(value >= 0 && value <= 1))
            return 0;
        alpha *= value;
        if (funfair_property_true(group, "get_ignoreParentGroups"))
            break;
    }
    return alpha;
}
static void browser_side_sync(void) {
    if (!browser_menu || !browser_alive(browser_menu))
        return;
    void *parent = browser_get(browser_menu, "get_transform", "UnityEngine.Transform");
    void *viewport = pf_ref(browser_menu, p_object_get_class(browser_menu), "_scrollView",
                            "UnityEngine.RectTransform");
    BrowserBounds view = {0};
    int valid = parent && viewport && browser_side_bounds(parent, viewport, &view);
    void *box = parent ? browser_optional(parent, "get_rect", "UnityEngine.Rect") : 0;
    BrowserV2 origin = {0, 0};
    if (box) {
        BrowserRect r = *(BrowserRect *)p_object_unbox(box);
        origin = (BrowserV2){r.x + r.w / 2, r.y + r.h / 2};
    }
    float right = view.center.x + view.size.x / 2 + 24;
    /* Resolve the surrounding panel edge without including any descendant rows. */
    void *scope = viewport;
    for (unsigned depth = 0; valid && scope && depth < 5; ++depth) {
        BrowserBounds outer = {0};
        if (browser_side_bounds(parent, scope, &outer) && outer.size.x >= view.size.x &&
            outer.size.x <= view.size.x + 300) {
            float edge = outer.center.x + outer.size.x / 2 + 8;
            if (edge > right)
                right = edge;
        }
        if (scope == parent)
            break;
        scope = browser_optional(scope, "get_parent", "UnityEngine.Transform");
    }
    for (unsigned i = 0; i < browser_count; ++i) {
        void *rt = browser_optional(browser_rows[i], "get_transform", "UnityEngine.Transform");
        BrowserBounds row = {0};
        int visible = !browser_open && valid && rt && browser_side_bounds(parent, rt, &row) &&
                      row.center.y >= view.center.y - view.size.y / 2 + BROWSER_SIDE_EDGE / 2 &&
                      row.center.y <= view.center.y + view.size.y / 2 - BROWSER_SIDE_EDGE / 2;
        if (visible) { /* Inset from the detected edge so the button straddles the paper like the picker tabs. */
            BrowserV2 pos = {right - BROWSER_SIDE_EDGE * .10f - origin.x,
                             row.center.y - origin.y - 6.f};
            void *tr = browser_get(browser_side_go[i], "get_transform", "UnityEngine.Transform");
            if (!browser_side_last[i].initialized || pos.x != browser_side_last[i].position.x ||
                pos.y != browser_side_last[i].position.y) {
                browser_set(tr, "set_anchoredPosition", "UnityEngine.Vector2", &pos);
                browser_side_last[i].position = pos;
            }
        }
        float alpha = visible ? browser_side_alpha(i) : 0;
        unsigned char interactive = (unsigned char)(visible && alpha >= .99f);
        int shown = visible && alpha > .001f;
        if (!browser_side_last[i].initialized || alpha != browser_side_last[i].alpha)
            browser_set(browser_side_group[i], "set_alpha", "System.Single", &alpha);
        if (!browser_side_last[i].initialized || interactive != browser_side_last[i].interactive) {
            browser_set(browser_side_group[i], "set_interactable", "System.Boolean", &interactive);
            browser_set(browser_side_group[i], "set_blocksRaycasts", "System.Boolean",
                        &interactive);
        }
        if (!browser_side_last[i].initialized || shown != browser_side_last[i].shown)
            browser_active(browser_side_go[i], shown);
        browser_side_last[i].initialized = 1;
        browser_side_last[i].alpha = alpha;
        browser_side_last[i].shown = shown;
        browser_side_last[i].interactive = interactive;
    }
}
static void browser_side_cleanup(void) {
    memset(browser_side_last, 0, sizeof(browser_side_last));
    browser_side_hover = 0;
    browser_side_sorted_instance = 0;
    browser_side_hover_started = 0;
    memset(browser_side_canvas, 0, sizeof(browser_side_canvas));
    memset(browser_side_group, 0, sizeof(browser_side_group));
    memset(browser_side_native_groups, 0, sizeof(browser_side_native_groups));
    for (unsigned i = 0; i < 32; ++i) {
        void *tip = browser_side_tip[i];
        if (tip && browser_alive(tip))
            write_void(browser_method(p_object_get_class(tip), "Reset", "System.Void", 0, 0, 1),
                       tip, 0);
        void *r = browser_side_registration[i];
        if (r)
            write_void(browser_method(p_object_get_class(r), "Unregister", "System.Void", 0, 0, 1),
                       r, 0);
    }
    memset(browser_side_tip, 0, sizeof(browser_side_tip));
    /* Preserve only alive shared native icon/prefabs across row-menu refreshes;
     * do NOT retain owned controls, tooltip strings, row bindings or registrations.
     * Their per-menu roots are released below as before. */
    memset(browser_side_registration, 0, sizeof(browser_side_registration));
    pf_release(&browser_side_assets);
    memset(browser_side_go, 0, sizeof(browser_side_go));
}

#undef findclass
#undef browser_method
#undef browser_get
#undef browser_optional
#undef browser_set
#undef browser_alive
#undef funfair_property_true
