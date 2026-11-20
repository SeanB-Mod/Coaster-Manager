/* Separate ownership: the confirmed coaster-list controls remain unchanged. */
static PFRoots save_ui_assets;
static void *save_ui_yellow_idle, *save_ui_yellow_hover;
static BrowserColor save_ui_yellow_tint = {1, 1, 1, 1}, save_ui_yellow_hover_tint = {1, 1, 1, 1};
static UINT_PTR save_ui_yellow_retry;

static void *save_ui_icon;
static int save_ui_icon_attempted;
static void *save_ui_square_idle, *save_ui_square_hover;
static int save_ui_square_attempted;
static int save_ui_square_owned[2];
static void *save_ui_canvas[3], *save_ui_go[3], *save_ui_registration[3];
static void *save_ui_idle[3], *save_ui_over[3];
static int save_ui_visual = -2;
static void *save_ui_centered_rect;
static BrowserV2 save_ui_center_original;
static void save_ui_restore_center(void) {
    if (save_ui_centered_rect && browser_alive(save_ui_centered_rect))
        browser_set(save_ui_centered_rect, "set_anchoredPosition", "UnityEngine.Vector2",
                    &save_ui_center_original);
    save_ui_centered_rect = 0;
}
static void save_ui_hover_reset(void) {
    save_ui_visual = -2;
}
static void save_destroy_roots(PFRoots *r);
static void save_ui_button_cleanup(void) {
    save_ui_restore_center();
    save_ui_hover_reset();
    for (unsigned i = 0; i < 3; ++i) {
        if (save_ui_registration[i])
            write_void(browser_method(p_object_get_class(save_ui_registration[i]), "Unregister",
                                      "System.Void", 0, 0, 1),
                       save_ui_registration[i], 0);
        save_destroy_roots(&save_slot_roots[i]);
        save_ui_registration[i] = save_ui_canvas[i] = save_ui_go[i] = save_slot_donor[i] =
            save_slot_toggle[i] = save_ui_idle[i] = save_ui_over[i] = 0;
    }
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = browser_method(c, "Destroy", "System.Void", 1, p, 0);
    void *sprites[] = {save_ui_square_idle, save_ui_square_hover};
    for (unsigned k = 0; k < 2; ++k)
        if (save_ui_square_owned[k] && sprites[k]) {
            void *a[] = {sprites[k]};
            write_void(m, 0, a);
            save_ui_square_owned[k] = 0;
        }
    pf_release(&save_ui_assets);
    save_ui_icon = 0;
    save_ui_icon_attempted = 0;
    save_ui_yellow_idle = save_ui_yellow_hover = 0;
    save_ui_yellow_retry = 0;
    save_ui_square_idle = save_ui_square_hover = 0;
    save_ui_square_attempted = 0;
}
static void save_ui_find_icon(void) {
    if (save_ui_icon || save_ui_icon_attempted)
        return;
    save_ui_icon_attempted = 1;
    PFRoots roots = {0};
    /* Resolve the exact loaded asset, including inactive menus and atlas sub-sprites. */
    static const char target[] = "UI_Atlas_Icons_Inspectors_ButtonSave_Idle";
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
            ac ? pf_invoke(browser_method(ac, "get_Length", "System.Int32", 0, 0, 1), array, 0) : 0;
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
            if (match && pf_root(&save_ui_assets, icon)) {
                save_ui_icon = icon;
                break;
            }
        }
    }
    pf_release(&roots);
    browser_log(save_ui_icon ? "SIDE BUTTON: exact Save sprite found; white tint"
                             : "SIDE BUTTON: exact Save sprite not loaded; Save text fallback");
}
static void save_ui_hover_update(void) {
    int previous = browser_failed;
    int visualIndex = -1;
    BrowserV2 mouse = {0};
    void *root = 0;
    void *c = findclass(p_domain_get(), "TPS.Core", "App");
    void *app = c ? pf_invoke(browser_method(c, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0) : 0;
    void *input =
        app ? browser_optional(app, "get_InputManager", "TPS.Core.Input.InputManager") : 0;
    void *position = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (position)
        mouse = *(BrowserV2 *)p_object_unbox(position);
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *canvas = pf_invoke(browser_method(p_object_get_class(save_owner), "GetComponentInParent",
                                            "UnityEngine.Component", 2, cp, 1),
                             save_owner, ca);
    root = canvas ? browser_optional(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *camera = 0;
    void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
    if (mode && *(int *)p_object_unbox(mode) != 0)
        camera = browser_optional(root, "get_worldCamera", "UnityEngine.Camera");
    if (position && root && !save_open) {
        void *rc = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
        const char *hp[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2",
                            "UnityEngine.Camera"};
        void *hit =
            rc ? browser_method(rc, "RectangleContainsScreenPoint", "System.Boolean", 3, hp, 0) : 0;
        for (unsigned i = 0; i < 3; ++i) {
            void *g = save_ui_go[i];
            if (!g || !funfair_property_true(g, "get_activeInHierarchy"))
                continue;
            void *rt = browser_optional(g, "get_transform", "UnityEngine.Transform");
            void *args[] = {rt, &mouse, camera};
            void *b = pf_invoke(hit, 0, args);
            if (b && pf_class(p_object_get_class(b), "System.Boolean") &&
                *(unsigned char *)p_object_unbox(b)) {
                visualIndex = (int)i;
                break;
            }
        }
    }
    if (save_ui_visual != visualIndex) {
        save_ui_visual = visualIndex;
        for (unsigned i = 0; i < 3; ++i) {
            if (!save_slot_toggle[i] || !browser_alive(save_slot_toggle[i]))
                continue;
            void *graphic = browser_optional(save_slot_toggle[i], "get_targetGraphic",
                                             "UnityEngine.UI.Graphic");
            void *sprite =
                visualIndex == (int)i && save_ui_over[i] ? save_ui_over[i] : save_ui_idle[i];
            if (sprite) {
                browser_set(graphic, "set_sprite", "UnityEngine.Sprite", sprite);
                BrowserColor white = {1, 1, 1, 1};
                browser_set(graphic, "set_color", "UnityEngine.Color",
                            i == 0 && save_ui_square_idle
                                ? &white
                                : (visualIndex == (int)i ? &save_ui_yellow_hover_tint
                                                         : &save_ui_yellow_tint));
            }
            void *rt = browser_optional(save_ui_go[i], "get_transform", "UnityEngine.Transform");
            float factor = visualIndex == (int)i ? 1.14f : 1.f;
            BrowserV3 scale = {factor, factor, 1};
            browser_set(rt, "set_localScale", "UnityEngine.Vector3", &scale);
        }
    }
    browser_failed = previous;
}

static void save_ui_button_decorate(float edge, unsigned index) {
    save_ui_find_icon();
    save_ui_go[index] = save_button_go;
    unsigned char yes = 1, no = 0;
    void *rt = browser_get(save_button_go, "get_transform", "UnityEngine.Transform");
    void *parent = browser_get(rt, "get_parent", "UnityEngine.Transform");
    const char *cp[] = {"System.Type", "System.Boolean"};
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *go = browser_get(parent, "get_gameObject", "UnityEngine.GameObject");
    void *pc = pf_invoke(browser_method(p_object_get_class(go), "GetComponentInParent",
                                        "UnityEngine.Component", 2, cp, 1),
                         go, ca);
    void *layer = pc ? browser_optional(pc, "get_sortingLayerID", "System.Int32") : 0;
    void *cv = browser_add(save_button_go, "UnityEngine", "Canvas");
    save_ui_canvas[index] = cv;
    if (layer)
        browser_set(cv, "set_sortingLayerID", "System.Int32", p_object_unbox(layer));
    /* Inherit the owning menu order so native tooltips render above Save.
   * Keep the local canvas/raycaster for the existing input registration. */
    browser_set(cv, "set_overrideSorting", "System.Boolean", &no);
    browser_add(save_button_go, "UnityEngine.UI", "GraphicRaycaster");
    void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
    void *registration = rc ? p_object_new(rc) : 0;
    if (registration && browser_hold(registration)) {
        save_ui_registration[index] = registration;
        write_void(pf_method(rc, ".ctor", "System.Void", 0, 0, 1), registration, 0);
        browser_set(registration, "Register", "UnityEngine.GameObject", save_button_go);
    } else
        browser_failed = 1;
    if (save_ui_icon) {
        void *icon =
            browser_make("Save Template icon",
                         browser_get(save_button_go, "get_transform", "UnityEngine.Transform"), 0,
                         0, edge * .58f, edge * .58f, .5f, .5f);
        BrowserColor white = {1, 1, 1, 1};
        void *image = browser_image(icon, white);
        int simple = 0;
        browser_set(image, "set_sprite", "UnityEngine.Sprite", save_ui_icon);
        browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &simple);
        browser_set(image, "set_preserveAspect", "System.Boolean", &yes);
        browser_set(image, "set_raycastTarget", "System.Boolean", &no);
        /* Info-menu artwork must inherit the native scroll viewport clipping. */
        unsigned char maskable = index == 2;
        browser_set(image, "set_maskable", "System.Boolean", &maskable);
        browser_active(icon, 1);
    } else
        save_log("Save icon not loaded yet.");
}

static void *save_ui_font(void *owner) {
    int previous = browser_failed;
    PFRoots roots = {0};
    void *font = 0;
    void *go = browser_optional(owner, "get_gameObject", "UnityEngine.GameObject");
    const char *p[] = {"System.Type", "System.Boolean"};
    void *t = browser_type("TMPro", "TMP_Text");
    unsigned char yes = 1;
    void *a[] = {t, &yes};
    void *array = go ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                                "UnityEngine.Component[]", 2, p, 1),
                                 go, a)
                     : 0;
    int count = 0;
    if (array && pf_root(&roots, array) && write_array_count(array, &count))
        for (int i = 0; i < count && !font; ++i) {
            void *text = write_array_item(array, i);
            if (text)
                font = browser_optional(text, "get_font", "TMPro.TMP_FontAsset");
        }
    pf_release(&roots);
    browser_failed = previous;
    return font;
}

static void save_ui_find_yellow(void) {
    if (save_ui_yellow_idle || GetTickCount64() < save_ui_yellow_retry)
        return;
    save_ui_yellow_retry = GetTickCount64() + 5000;
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("TPS.Game", "CoasterControlsUI");
    void *a[] = {t, &yes};
    void *array =
        t ? pf_invoke(browser_method(c, "FindObjectsOfType", "UnityEngine.Object[]", 2, p, 0), 0, a)
          : 0;
    int count = 0;
    if (array && pf_root(&roots, array) && write_array_count(array, &count))
        for (int i = 0; i < count; ++i) {
            void *owner = write_array_item(array, i);
            if (!owner)
                continue;
            void *button = pf_ref(owner, p_object_get_class(owner), "_addPillarButton",
                                  "TPS.Core.UI.UIButton");
            if (!button)
                continue;
            void *data = pf_ref(button, p_object_get_class(button), "_stateData",
                                "TPS.Core.UI.UIButton.StateData");
            void *state = data ? pf_ref(data, p_object_get_class(data), "Normal",
                                        "TPS.Core.UI.UIButton.ImageData")
                               : 0;
            void *idle =
                state ? pf_ref(state, p_object_get_class(state), "Idle", "UnityEngine.Sprite") : 0;
            if (!idle || !pf_root(&save_ui_assets, idle))
                continue;
            save_ui_yellow_idle = idle;
            void *over = pf_ref(state, p_object_get_class(state), "Hover", "UnityEngine.Sprite");
            if (over && pf_root(&save_ui_assets, over))
                save_ui_yellow_hover = over;
            void *f = pf_field(p_object_get_class(state), "IdleTint", "UnityEngine.Color");
            if (f)
                p_field_get_value(state, f, &save_ui_yellow_tint);
            f = pf_field(p_object_get_class(state), "HoverTint", "UnityEngine.Color");
            if (f)
                p_field_get_value(state, f, &save_ui_yellow_hover_tint);
            save_log(
                "Yellow save artwork resolved from CoasterControlsUI Add Support; green tick artwork excluded.");
            break;
        }
    pf_release(&roots);
}
/* Verified in UI_Atlas_Shared_Buttons.spriteatlasv2. Orange is the game's
 * gold/yellow variant of GreenSquarePrimaryButtonLargeBase, not a long button. */
static void save_ui_find_square(void) {
    if (save_ui_square_attempted)
        return;
    save_ui_square_attempted = 1;
    PFRoots roots = {0};
    static const char *names[] = {"UI_T_Inspectors_OrangeSquarePrimaryButtonLargeBase_Idle",
                                  "UI_T_Inspectors_OrangeSquarePrimaryButtonLargeBase_Hover"};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *t = browser_type("UnityEngine", "Sprite");
    void *a[] = {t};
    void *array =
        t ? pf_invoke(browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0), 0,
                      a)
          : 0;
    int count = 0;
    if (array && pf_root(&roots, array)) {
        void *ac = findclass(p_domain_get(), "System", "Array");
        void *box = pf_invoke(browser_method(ac, "get_Length", "System.Int32", 0, 0, 1), array, 0);
        if (box)
            count = *(int *)p_object_unbox(box);
        if (count < 0 || count > 65536)
            count = 0;
        for (int i = 0; i < count && (!save_ui_square_idle || !save_ui_square_hover); ++i) {
            void *sprite = write_array_item(array, i);
            void *label = sprite ? browser_optional(sprite, "get_name", "System.String") : 0;
            if (!label)
                continue;
            int n = p_string_length(label);
            const WCHAR *text = p_string_chars(label);
            for (unsigned k = 0; k < 2; ++k) {
                if (!text || n != (int)length(names[k]))
                    continue;
                int match = 1;
                for (int j = 0; j < n; ++j)
                    if (text[j] != (WCHAR)(unsigned char)names[k][j]) {
                        match = 0;
                        break;
                    }
                if (match && pf_root(&save_ui_assets, sprite)) {
                    if (k)
                        save_ui_square_hover = sprite;
                    else
                        save_ui_square_idle = sprite;
                    break;
                }
            }
        }
    }
    /* An atlas entry need not already have a loaded Sprite object. Resolve it
   * from the loaded atlas directly if Resources did not return the named pair. */
    if (!save_ui_square_idle || !save_ui_square_hover) {
        void *at = browser_type("UnityEngine.U2D", "SpriteAtlas");
        void *aa[] = {at};
        void *atlases = at ? pf_invoke(browser_method(c, "FindObjectsOfTypeAll",
                                                      "UnityEngine.Object[]", 1, p, 0),
                                       0, aa)
                           : 0;
        int n = 0;
        if (atlases && pf_root(&roots, atlases) && write_array_count(atlases, &n))
            for (int i = 0; i < n; ++i) {
                void *atlas = write_array_item(atlases, i);
                void *label = atlas ? browser_optional(atlas, "get_name", "System.String") : 0;
                static const char target[] = "UI_Atlas_Shared_Buttons";
                if (!label || p_string_length(label) != (int)(sizeof(target) - 1))
                    continue;
                const WCHAR *chars = p_string_chars(label);
                int match = chars != 0;
                for (unsigned j = 0; match && j < sizeof(target) - 1; ++j)
                    if (chars[j] != (WCHAR)(unsigned char)target[j])
                        match = 0;
                if (!match)
                    continue;
                const char *sp[] = {"System.String"};
                void *method = browser_method(p_object_get_class(atlas), "GetSprite",
                                              "UnityEngine.Sprite", 1, sp, 1);
                for (unsigned k = 0; k < 2; ++k) {
                    if (k ? save_ui_square_hover != 0 : save_ui_square_idle != 0)
                        continue;
                    void *name = p_string_new(names[k]);
                    if (!name || !pf_root(&roots, name))
                        continue;
                    void *args[] = {name};
                    void *sprite = pf_invoke(method, atlas, args);
                    if (sprite && pf_root(&save_ui_assets, sprite)) {
                        if (k)
                            save_ui_square_hover = sprite;
                        else
                            save_ui_square_idle = sprite;
                        save_ui_square_owned[k] = 1;
                    }
                }
                break;
            }
    }
    pf_release(&roots);
    save_log(save_ui_square_idle
                 ? "Construction button: exact native OrangeSquarePrimaryButtonLargeBase found."
                 : "Construction square sprite not loaded; retaining yellow fallback.");
}
static int save_ui_slot(unsigned index, void *owner, void *donor) {
    if (save_slot_donor[index] != donor || !save_ui_go[index] ||
        !browser_alive(save_ui_go[index])) {
        UINT_PTR started = GetTickCount64();
        if (index == 2)
            save_ui_restore_center();
        if (save_ui_registration[index])
            write_void(browser_method(p_object_get_class(save_ui_registration[index]), "Unregister",
                                      "System.Void", 0, 0, 1),
                       save_ui_registration[index], 0);
        save_destroy_roots(&save_slot_roots[index]);
        save_ui_registration[index] = save_ui_canvas[index] = save_ui_go[index] = 0;
        browser_current_roots = &save_slot_roots[index];
        void *font = save_ui_font(owner);
        void *dr = browser_get(donor, "get_transform", "UnityEngine.Transform");
        void *box = browser_get(dr, "get_rect", "UnityEngine.Rect");
        if (!font || !box) {
            browser_current_roots = &browser_roots;
            return 0;
        }
        BrowserRect r = *(BrowserRect *)p_object_unbox(box);
        float edge = r.h;
        float width = index == 0 ? r.w : edge;
        void *parent = browser_get(dr, "get_parent", "UnityEngine.Transform");
        BrowserBounds bounds = {0};
        void *pb = browser_get(parent, "get_rect", "UnityEngine.Rect");
        if (!pb || !browser_side_bounds(parent, dr, &bounds)) {
            browser_current_roots = &browser_roots;
            return 0;
        }
        BrowserRect pr = *(BrowserRect *)p_object_unbox(pb);
        float x = bounds.center.x + bounds.size.x / 2 + width / 2 + 12 - (pr.x + pr.w / 2),
              y = bounds.center.y - (pr.y + pr.h / 2);
        save_ui_find_icon();
        void *toggle =
            browser_toggle(parent, font, save_ui_icon ? "" : "Save", x, y, width, edge, .5f, .5f);
        void *g = browser_get(toggle, "get_gameObject", "UnityEngine.GameObject");
        void *graphic = browser_get(toggle, "get_targetGraphic", "UnityEngine.UI.Graphic");
        /* This owned button's hover sprites/tint/scale are managed above. Disable
     * Selectable's default colour tween so it cannot animate the same graphic. */
        int transitionNone = 0;
        browser_set(toggle, "set_transition", "UnityEngine.UI.Selectable.Transition",
                    &transitionNone);
        /* Disabling Selectable does not clear an already-started renderer tween.
     * Stop it and restore the renderer multiplier; Image keeps native art tint. */
        BrowserColor rendererWhite = {1, 1, 1, 1};
        float immediate = 0;
        unsigned char ignoreTime = 1, includeAlpha = 1;
        const char *fadeParams[] = {"UnityEngine.Color", "System.Single", "System.Boolean",
                                    "System.Boolean"};
        void *fadeArgs[] = {&rendererWhite, &immediate, &ignoreTime, &includeAlpha};
        if (!write_void(browser_method(p_object_get_class(graphic), "CrossFadeColor", "System.Void",
                                       4, fadeParams, 1),
                        graphic, fadeArgs))
            browser_failed = 1;
        unsigned char yes = 1;
        void *layout = browser_add(g, "UnityEngine.UI", "LayoutElement");
        browser_set(layout, "set_ignoreLayout", "System.Boolean", &yes);
        /* Slot 2 is the scrolling Info page. Preserve clipping there while
         * keeping the non-scrolling construction buttons' existing behaviour. */
        unsigned char maskable = index == 2;
        browser_set(graphic, "set_maskable", "System.Boolean", &maskable);
        save_ui_find_yellow();
        if (save_ui_yellow_idle) {
            browser_skin_image(graphic, save_ui_yellow_idle, 0);
            browser_set(graphic, "set_color", "UnityEngine.Color", &save_ui_yellow_tint);
            save_ui_idle[index] = save_ui_yellow_idle;
            save_ui_over[index] = save_ui_yellow_hover;
        } else {
            browser_set(graphic, "set_sprite", "UnityEngine.Sprite", 0);
            BrowserColor yellow = {1, .82f, 0, 1};
            browser_set(graphic, "set_color", "UnityEngine.Color", &yellow);
            save_log("Yellow artwork pending; yellow fill used instead of a green tick sprite.");
        }
        save_button_go = g;
        save_ui_button_decorate(edge, index);
        save_slot_toggle[index] = toggle;
        save_slot_donor[index] = donor;
        if (index == 2) {
            void *position = browser_get(dr, "get_anchoredPosition", "UnityEngine.Vector2");
            if (position && browser_hold(dr)) {
                save_ui_center_original = *(BrowserV2 *)p_object_unbox(position);
                save_ui_centered_rect = dr;
                float shift = (edge + 12) / 2;
                BrowserV2 main = save_ui_center_original;
                main.x -= shift;
                browser_set(dr, "set_anchoredPosition", "UnityEngine.Vector2", &main);
                void *sr = browser_get(g, "get_transform", "UnityEngine.Transform");
                BrowserV2 side = {x - shift, y};
                browser_set(sr, "set_anchoredPosition", "UnityEngine.Vector2", &side);
            }
        }
        browser_current_roots = &browser_roots;
        save_log(
            "Save button attached as independent sibling; native action hover/click isolated.");
        logtext("SAVE TEMPLATE: button preparation ms=");
        lognum(GetTickCount64() - started);
        nl();
    }
    if (index == 0)
        save_ui_find_square();
    if (!save_ui_yellow_idle)
        save_ui_find_yellow();
    if (save_ui_yellow_idle && save_ui_idle[index] != save_ui_yellow_idle &&
        !(index == 0 && save_ui_square_idle)) {
        void *graphic =
            browser_get(save_slot_toggle[index], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(graphic, save_ui_yellow_idle, 0);
        browser_set(graphic, "set_color", "UnityEngine.Color", &save_ui_yellow_tint);
        save_ui_idle[index] = save_ui_yellow_idle;
        save_ui_over[index] = save_ui_yellow_hover;
        save_ui_visual = -2;
    }
    if (index == 0 && save_ui_square_idle && save_ui_idle[index] != save_ui_square_idle) {
        void *graphic =
            browser_get(save_slot_toggle[index], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(graphic, save_ui_square_idle, 0);
        BrowserColor white = {1, 1, 1, 1};
        int simple = 0;
        browser_set(graphic, "set_type", "UnityEngine.UI.Image.Type", &simple);
        browser_set(graphic, "set_color", "UnityEngine.Color", &white);
        save_ui_idle[index] = save_ui_square_idle;
        save_ui_over[index] = save_ui_square_hover;
        save_ui_visual = -2;
    }
    browser_active(save_ui_go[index], save_phase < 2);
    return save_slot_toggle[index] != 0;
}
