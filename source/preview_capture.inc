static PFRoots preview_capture_roots[6], preview_capture_assets;
static void *preview_capture_title;
static void *preview_capture_sprites[6];
static void *preview_capture_root, *preview_capture_capture, *preview_capture_cancel,
    *preview_capture_registration;
static int preview_capture_hidden, preview_capture_mouse_down, preview_capture_escape_down;
static void *preview_capture_box, *preview_capture_ribbon, *preview_capture_canvas,
    *preview_capture_shadow, *preview_capture_heading_parts[4];
static float preview_capture_price_width(void) {
    /* Project the actual library price control through its canvas camera.
  * Canvas.scaleFactor alone misses the game's camera/projection scaling. */
    void *source =
        library_slot_go[0] && library_slot_build[0] ? library_slot_build[0] : library_price_source;
    if (!source || !browser_alive(source))
        return 0;
    void *rt = browser_optional(source, "get_transform", "UnityEngine.Transform");
    void *box = rt ? browser_optional(rt, "get_rect", "UnityEngine.Rect") : 0;
    if (!box)
        return 0;
    BrowserRect rect = *(BrowserRect *)p_object_unbox(box);
    void *g = browser_optional(source, "get_gameObject", "UnityEngine.GameObject");
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *c = g ? pf_invoke(browser_method(p_object_get_class(g), "GetComponentInParent",
                                           "UnityEngine.Component", 2, cp, 1),
                            g, ca)
                : 0;
    void *root = c ? browser_optional(c, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
    void *camera = mode && *(int *)p_object_unbox(mode) != 0
                       ? browser_optional(root, "get_worldCamera", "UnityEngine.Camera")
                       : 0;
    const char *tp[] = {"UnityEngine.Vector3"};
    void *transform =
        browser_method(p_object_get_class(rt), "TransformPoint", "UnityEngine.Vector3", 1, tp, 1);
    const char *sp[] = {"UnityEngine.Camera", "UnityEngine.Vector3"};
    void *utility = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
    void *project = browser_method(utility, "WorldToScreenPoint", "UnityEngine.Vector2", 2, sp, 0);
    BrowserV2 screen[2];
    for (unsigned i = 0; i < 2; ++i) {
        BrowserV3 local = {rect.x + (i ? rect.w : 0), rect.y + rect.h * .5f, 0};
        void *ta[] = {&local};
        void *wb = pf_invoke(transform, rt, ta);
        if (!wb)
            return 0;
        BrowserV3 world = *(BrowserV3 *)p_object_unbox(wb);
        void *sa[] = {camera, &world};
        void *pb = pf_invoke(project, 0, sa);
        if (!pb)
            return 0;
        screen[i] = *(BrowserV2 *)p_object_unbox(pb);
    }
    float width = screen[1].x - screen[0].x;
    return width >= 60 && width <= 400 ? width : 0;
}
static void preview_capture_size(void) {
    float captureScale = 1;
    void *b = browser_optional(preview_capture_canvas, "get_scaleFactor", "System.Single");
    if (b) {
        float f = *(float *)p_object_unbox(b);
        if (f > .05f && f < 4)
            captureScale = f;
    }
    float width = preview_capture_price_width();
    if (!width)
        width = 120 * captureScale;
    float nativeScale = width / 240, ratio = nativeScale / captureScale;
    BrowserV3 scale = {ratio, ratio, 1};
    browser_set(preview_capture_box, "set_localScale", "UnityEngine.Vector3", &scale);
    /* Match the library: body width minus 20, ribbon shifted 30 left, 120 high. */
    BrowserV2 banner = {640, 120};
    browser_set(preview_capture_ribbon, "set_sizeDelta", "UnityEngine.Vector2", &banner);
    /* Two native units between the paper and ribbon, with the fold slightly raised. */
    BrowserV2 position = {-30, 57 + banner.y * .5f};
    browser_set(preview_capture_ribbon, "set_anchoredPosition", "UnityEngine.Vector2", &position);
    for (unsigned i = 0; i < 4; ++i)
        if (preview_capture_heading_parts[i]) {
            BrowserV2 p = {i == 0   ? 12
                           : i == 1 ? -290
                                    : 278,
                           position.y + (i == 0   ? 10.f
                                         : i == 1 ? 2.f
                                                  : 6.f)};
            browser_set(preview_capture_heading_parts[i], "set_anchoredPosition",
                        "UnityEngine.Vector2", &p);
        }
    BrowserV2 shadowSize = {666, position.y + 59}, shadowPos = {-2, (position.y - 59) * .5f};
    browser_set(preview_capture_shadow, "set_sizeDelta", "UnityEngine.Vector2", &shadowSize);
    browser_set(preview_capture_shadow, "set_anchoredPosition", "UnityEngine.Vector2", &shadowPos);
    logtext("SAVE TEMPLATE: Preview Capture price width px=");
    lognum((UINT_PTR)width);
    logtext(" scale x1000=");
    lognum((UINT_PTR)(ratio * 1000));
    logtext(" header native height=120; library geometry matched");
    nl();
}
static UINT_PTR preview_capture_opened;
static void preview_capture_stop(void) {
    browser_active(preview_capture_root, 0);
    if (preview_capture_hidden) {
        save_restore_ui();
        preview_capture_hidden = 0;
    }
}
static void preview_capture_bank(unsigned bank) {
    browser_current_roots = &preview_capture_roots[bank];
}
static void *preview_capture_clone(void *source) {
    if (!source)
        return 0;
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *a[] = {source};
    void *copy = pf_invoke(browser_method(c, "Instantiate", "UnityEngine.Object", 1, p, 0), 0, a);
    return copy && pf_root(&preview_capture_assets, copy) ? copy : 0;
}
static int preview_capture_construct(void *parent, void *font) {
    library_theme_resolve();
    save_popup_sample_native();
    void *art[] = {save_popup_paper_style.sprite ? save_popup_paper_style.sprite
                                                 : library_theme_sprite[17],
                   library_theme_sprite[0],
                   library_theme_sprite[2],
                   save_popup_close_idle,
                   save_popup_cross_fallback ? save_popup_cross_fallback : library_theme_sprite[16],
                   browser_purchase_sprite ? browser_purchase_sprite : library_theme_sprite[4]};
    for (unsigned i = 0; i < 6; ++i)
        preview_capture_sprites[i] = preview_capture_clone(art[i]);
    preview_capture_bank(0);
    preview_capture_root =
        browser_make("Template snapshot camera UI", parent, 0, 0, 1920, 1080, .5f, .5f);
    void *rt = browser_get(preview_capture_root, "get_transform", "UnityEngine.Transform");
    const char *pp[] = {"UnityEngine.Transform", "System.Boolean"};
    unsigned char no = 0, yes = 1;
    void *pa[] = {0, &no};
    write_void(browser_method(p_object_get_class(rt), "SetParent", "System.Void", 2, pp, 1), rt,
               pa);
    void *canvas = browser_add(preview_capture_root, "UnityEngine", "Canvas");
    preview_capture_canvas = canvas;
    int overlay = 0, order = 500;
    browser_set(canvas, "set_renderMode", "UnityEngine.RenderMode", &overlay);
    browser_set(canvas, "set_overrideSorting", "System.Boolean", &yes);
    browser_set(canvas, "set_sortingOrder", "System.Int32", &order);
    browser_add(preview_capture_root, "UnityEngine.UI", "GraphicRaycaster");
    void *scaler = browser_add(preview_capture_root, "UnityEngine.UI", "CanvasScaler");
    int scaleMode = 1;
    BrowserV2 reference = {1920, 1080};
    browser_set(scaler, "set_uiScaleMode", "UnityEngine.UI.CanvasScaler.ScaleMode", &scaleMode);
    browser_set(scaler, "set_referenceResolution", "UnityEngine.Vector2", &reference);
    /* One container scales paper, controls, header and shadow together. */
    void *container = browser_make("Preview Capture scaled window", rt, 0, 72, 660, 110, .5f, 0);
    preview_capture_box = browser_get(container, "get_transform", "UnityEngine.Transform");
    browser_active(container, 1);
    preview_capture_bank(1);
    void *shadow = library_art(preview_capture_box, "Preview Capture rectangular shadow", -2, 24.5f,
                               666, 167, 0, 0);
    preview_capture_shadow = browser_get(shadow, "get_transform", "UnityEngine.Transform");
    BrowserColor shadowInk = {.17f, .19f, .09f, .60f};
    browser_set(shadow, "set_color", "UnityEngine.Color", &shadowInk);
    preview_capture_bank(0);
    void *box =
        browser_make("Preview Capture parchment", preview_capture_box, 0, 0, 660, 110, .5f, .5f);
    BrowserColor paper = {1, 1, 1, 1};
    void *im = browser_image(box, paper);
    browser_skin_image(im, preview_capture_sprites[0], 0);
    browser_active(box, 1);
    void *bt = browser_get(box, "get_transform", "UnityEngine.Transform");
    if (save_popup_paper_style.sprite) {
        SavePopupSurface style = save_popup_paper_style;
        style.sprite = preview_capture_sprites[0];
        style.material = 0;
        save_popup_apply_surface(im, &style);
    }
    preview_capture_bank(1);
    void *fold = library_art(bt, "Capture close fold", 282, 108, save_popup_close_width,
                             save_popup_close_height, preview_capture_sprites[3], 0);
    browser_set(fold, "set_color", "UnityEngine.Color", &save_popup_close_tint);
    preview_capture_heading_parts[2] = browser_get(fold, "get_transform", "UnityEngine.Transform");
    void *ribbon = library_art(bt, "Preview Capture ribbon", -30, 117, 640, 120,
                               preview_capture_sprites[1], 1);
    preview_capture_ribbon = browser_get(ribbon, "get_transform", "UnityEngine.Transform");
    preview_capture_bank(2);
    void *title = browser_label(bt, font, "Preview Capture", 0, 108, 470, 90, 48);
    preview_capture_title = title;
    BrowserColor white = {1, 1, 1, 1};
    int bold = 1, leftAlign = 513;
    browser_set(title, "set_color", "UnityEngine.Color", &white);
    browser_set(title, "set_fontStyle", "TMPro.FontStyles", &bold);
    browser_set(title, "set_alignment", "TMPro.TextAlignmentOptions", &leftAlign);
    preview_capture_heading_parts[0] = browser_get(title, "get_transform", "UnityEngine.Transform");
    void *cameraImage =
        library_art(bt, "Capture camera", -290, 108, 84, 84, preview_capture_sprites[2], 0);
    preview_capture_heading_parts[1] =
        browser_get(cameraImage, "get_transform", "UnityEngine.Transform");
    preview_capture_bank(3);
    preview_capture_cancel = browser_toggle(bt, font, "", 282, 108, save_popup_close_width,
                                            save_popup_close_height, .5f, .5f);
    void *cancelImage =
        browser_get(preview_capture_cancel, "get_targetGraphic", "UnityEngine.UI.Graphic");
    BrowserColor clear = {1, 1, 1, 0};
    browser_set(cancelImage, "set_sprite", "UnityEngine.Sprite", 0);
    browser_set(cancelImage, "set_color", "UnityEngine.Color", &clear);
    void *cancelTransform =
        browser_get(preview_capture_cancel, "get_transform", "UnityEngine.Transform");
    preview_capture_heading_parts[3] = cancelTransform;
    library_art(cancelTransform, "Capture close cross", 8, 0, 40, 40, preview_capture_sprites[4],
                0);
    preview_capture_bank(4);
    preview_capture_capture = browser_toggle(bt, font, "Save", 0, -3, 240, 82, .5f, .5f);
    void *saveImage =
        browser_get(preview_capture_capture, "get_targetGraphic", "UnityEngine.UI.Graphic");
    browser_skin_image(saveImage, preview_capture_sprites[5], 0);
    const char *lp[] = {"System.Type", "System.Boolean"};
    void *labelType = browser_type("TMPro", "TMP_Text");
    void *la[] = {labelType, &yes};
    void *saveGo = browser_get(preview_capture_capture, "get_gameObject", "UnityEngine.GameObject");
    void *label = pf_invoke(browser_method(p_object_get_class(saveGo), "GetComponentInChildren",
                                           "UnityEngine.Component", 2, lp, 1),
                            saveGo, la);
    BrowserV2 textPos = {0, 8};
    browser_set(browser_get(label, "get_transform", "UnityEngine.Transform"),
                "set_anchoredPosition", "UnityEngine.Vector2", &textPos);
    float size = 34;
    browser_set(label, "set_fontSize", "System.Single", &size);
    browser_set(label, "set_fontStyle", "TMPro.FontStyles", &bold);
    preview_capture_bank(5);
    void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
    preview_capture_registration = rc ? p_object_new(rc) : 0;
    if (!preview_capture_registration || !browser_hold(preview_capture_registration))
        browser_failed = 1;
    else {
        write_void(browser_method(rc, ".ctor", "System.Void", 0, 0, 1),
                   preview_capture_registration, 0);
        const char *rp[] = {"UnityEngine.GameObject"};
        void *ra[] = {preview_capture_root};
        if (!write_void(browser_method(rc, "Register", "System.Void", 1, rp, 1),
                        preview_capture_registration, ra))
            browser_failed = 1;
    }
    browser_active(preview_capture_root, 0);
    return !browser_failed;
}
static int preview_capture_start(void) {
    browser_set(preview_capture_title, "set_text", "System.String",
                p_string_new("Preview Capture"));
    float titleSize = 48;
    browser_set(preview_capture_title, "set_fontSize", "System.Single", &titleSize);
    unsigned char enabled = 1;
    browser_set(preview_capture_capture, "set_interactable", "System.Boolean", &enabled);
    browser_active(preview_capture_root, 0);
    if (!save_hide_ui())
        return 0;
    preview_capture_hidden = 1;
    preview_capture_opened = GetTickCount64();
    unsigned char off = 0;
    browser_set(preview_capture_capture, "SetIsOnWithoutNotify", "System.Boolean", &off);
    browser_set(preview_capture_cancel, "SetIsOnWithoutNotify", "System.Boolean", &off);
    preview_capture_mouse_down = (GetAsyncKeyState(1) & 0x8000) != 0;
    preview_capture_escape_down = (GetAsyncKeyState(27) & 0x8000) != 0;
    browser_active(preview_capture_root, 1);
    preview_capture_size();
    save_log("Preview Capture: active; X/Escape cancel, Save captures.");
    return 1;
}
static void preview_capture_cleanup(void) {
    preview_capture_title = 0;
    preview_capture_stop();
    if (preview_capture_registration) {
        write_void(browser_method(p_object_get_class(preview_capture_registration), "Unregister",
                                  "System.Void", 0, 0, 1),
                   preview_capture_registration, 0);
        preview_capture_registration = 0;
    }
    for (unsigned i = 0; i < 6; ++i)
        save_destroy_roots(&preview_capture_roots[i]);
    for (unsigned i = 0; i < 6; ++i)
        preview_destroy(preview_capture_sprites[i]);
    pf_release(&preview_capture_assets);
    memset(preview_capture_sprites, 0, sizeof(preview_capture_sprites));
    preview_capture_root = preview_capture_capture = preview_capture_cancel = preview_capture_box =
        preview_capture_ribbon = preview_capture_canvas = preview_capture_shadow = 0;
    memset(preview_capture_heading_parts, 0, sizeof(preview_capture_heading_parts));
}
static int preview_capture_ensure(void *parent, void *font) {
    if (preview_capture_root && browser_alive(preview_capture_root))
        return 1;
    int ok = preview_capture_construct(parent, font);
    if (!ok)
        preview_capture_cleanup();
    return ok;
}
static void preview_capture_hide_box(void) {
    browser_active(preview_capture_root, 0);
}
static int preview_capture_choice(void) {
    if (!preview_capture_root || !browser_alive(preview_capture_root))
        return -1;
    int mouse = (GetAsyncKeyState(1) & 0x8000) != 0, escape = (GetAsyncKeyState(27) & 0x8000) != 0;
    int click = mouse && !preview_capture_mouse_down, exit = escape && !preview_capture_escape_down;
    preview_capture_mouse_down = mouse;
    preview_capture_escape_down = escape;
    if (exit || browser_pressed(preview_capture_cancel)) {
        save_log("Preview Capture: cancel requested.");
        return -1;
    }
    if (GetTickCount64() - preview_capture_opened < 250)
        return 0;
    if (browser_pressed(preview_capture_capture)) {
        save_log("Preview Capture: Save requested.");
        return 1;
    }
    if (!click)
        return 0;
    /* TPS may not dispatch toggle events to the independent overlay. Hit-test
  * the same visible controls on a fresh press; never click through the window. */
    void *app = findclass(p_domain_get(), "TPS.Core", "App");
    void *instance = pf_invoke(browser_method(app, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0);
    void *input =
        instance ? browser_optional(instance, "get_InputManager", "TPS.Core.Input.InputManager")
                 : 0;
    void *position = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (!position)
        return 0;
    BrowserV2 point = *(BrowserV2 *)p_object_unbox(position);
    const char *p[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera"};
    void *c = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
    void *m = browser_method(c, "RectangleContainsScreenPoint", "System.Boolean", 3, p, 0);
    void *controls[] = {preview_capture_cancel, preview_capture_capture};
    for (unsigned i = 0; i < 2; ++i) {
        void *rt = browser_get(controls[i], "get_transform", "UnityEngine.Transform");
        void *a[] = {rt, &point, 0};
        void *b = pf_invoke(m, 0, a);
        if (b && *(unsigned char *)p_object_unbox(b)) {
            save_log(i ? "Preview Capture: Save click received."
                       : "Preview Capture: close click received.");
            return i ? 1 : -1;
        }
    }
    return 0;
}
