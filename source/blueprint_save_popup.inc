/* Save popup artwork is separately owned; list and station buttons stay locked. */
static PFRoots save_popup_assets, save_popup_extra_assets, save_popup_roots[6];
static int save_popup_hold(void *o) {
    if (!o)
        return 0;
    PFRoots *banks[] = {&save_popup_assets, &save_popup_extra_assets};
    for (unsigned b = 0; b < 2; ++b)
        for (unsigned i = 0; i < banks[b]->n; ++i)
            if (p_gchandle_get_target(banks[b]->h[i]) == o)
                return 1;
    return pf_root(save_popup_assets.n < 32 ? &save_popup_assets : &save_popup_extra_assets, o);
}
typedef struct {
    void *sprite, *material;
    BrowserColor tint;
    float ppu, basePPU;
    int type;
} SavePopupSurface;
static SavePopupSurface save_popup_paper_style, save_popup_header_style, save_popup_input_style,
    save_popup_close_style;
static void *save_popup_header_font;
static float save_popup_header_size = 48;
static int save_popup_header_font_style = 1;
static void save_popup_read_surface(void *image, SavePopupSurface *out) {
    out->tint = (BrowserColor){1, 1, 1, 1};
    out->ppu = 1;
    out->type = 1;
    out->sprite = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
    if (out->sprite)
        save_popup_hold(out->sprite);
    out->material = browser_optional(image, "get_material", "UnityEngine.Material");
    if (out->material)
        save_popup_hold(out->material);
    void *b = browser_optional(image, "get_color", "UnityEngine.Color");
    if (b)
        out->tint = *(BrowserColor *)p_object_unbox(b);
    b = browser_optional(image, "get_pixelsPerUnitMultiplier", "System.Single");
    if (b)
        out->ppu = *(float *)p_object_unbox(b);
    b = browser_optional(image, "get_pixelsPerUnit", "System.Single");
    if (b)
        out->basePPU = *(float *)p_object_unbox(b);
    b = browser_optional(image, "get_type", "UnityEngine.UI.Image.Type");
    if (b)
        out->type = *(int *)p_object_unbox(b);
}
static void save_popup_apply_surface(void *image, SavePopupSurface *style) {
    if (!style->sprite)
        return;
    browser_set(image, "set_sprite", "UnityEngine.Sprite", style->sprite);
    if (style->material)
        browser_set(image, "set_material", "UnityEngine.Material", style->material);
    browser_set(image, "set_color", "UnityEngine.Color", &style->tint);
    browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &style->type);
    browser_set(image, "set_pixelsPerUnitMultiplier", "System.Single", &style->ppu);
}
static void *save_popup_sprite[15];
static unsigned char save_popup_owned[15];
static float save_popup_action_edge = 108, save_popup_close_width = 84,
             save_popup_close_height = 76;
static void *save_popup_toggle[3], *save_popup_go[3], *save_close;
static int save_popup_visual = -2;
static void *save_popup_cross_fallback;
static void *save_popup_fold_texture, *save_popup_fold_sprite;
static int save_popup_native_fold;
static void *save_popup_paper_image;
static void *save_popup_paper_material, *save_popup_close_idle, *save_popup_close_hover;
static BrowserColor save_popup_close_tint = {1, 1, 1, 1};
static const char *save_popup_names[] = {
    "UI_T_Inspectors_CreateTemplatePanelBase_Opaque",
    "UI_T_Inspectors_HeaderBase",
    "UI_T_Inspectors_HeaderIconBase",
    "Borelli_Icons_Menu_Tab_Thrill_Coaster",
    "UI_T_Inspectors_CloseButton",
    "UI_T_Inspectors_GreenSquarePrimaryButtonLargeBase_Idle",
    "UI_T_Inspectors_GreenSquarePrimaryButtonLargeBase_Hovered",
    "UI_T_Inspectors_RedSquarePrimaryButtonLargeBase_Idle",
    "UI_T_Inspectors_RedSquarePrimaryButtonLargeBase_Hover",
    "UI_Generic_T_Spritesheet_buttonIcons_Cross",
    "UI_Generic_T_Spritesheet_buttonIcons_Tick",
    "UI_T_Inspectors_HeaderTape",
    "UI_T_Inspectors_PrimaryPanelBaseRight_Opaque",
    "UI_T_Inspectors_RedSquarePrimaryButtonLargeBase_Selected",
    "UI_T_Inspectors_GreenSquarePrimaryButtonLargeBase_Selected"};
static int save_popup_name(void *label, const char *name) {
    if (!label || p_string_length(label) != (int)length(name))
        return 0;
    const WCHAR *s = p_string_chars(label);
    if (!s)
        return 0;
    for (unsigned i = 0; name[i]; ++i)
        if (s[i] != (WCHAR)(unsigned char)name[i])
            return 0;
    return 1;
}
static int save_popup_contains(void *label, const char *part) {
    if (!label)
        return 0;
    int n = p_string_length(label), len = (int)length(part);
    const WCHAR *w = p_string_chars(label);
    if (!w)
        return 0;
    for (int i = 0; i + len <= n; ++i) {
        int j = 0;
        while (j < len && w[i + j] == (WCHAR)(unsigned char)part[j])
            ++j;
        if (j == len)
            return 1;
    }
    return 0;
}
static int save_popup_array_length(void *array) {
    void *c = findclass(p_domain_get(), "System", "Array");
    void *b =
        array ? pf_invoke(browser_method(c, "get_Length", "System.Int32", 0, 0, 1), array, 0) : 0;
    int n = b && pf_class(p_object_get_class(b), "System.Int32") ? *(int *)p_object_unbox(b) : 0;
    return n >= 0 && n <= 65536 ? n : 0;
}
static void save_popup_resolve(void) {
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *m = browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0);
    void *t = browser_type("UnityEngine", "Sprite");
    void *a[] = {t};
    void *all = t ? pf_invoke(m, 0, a) : 0;
    if (all && pf_root(&roots, all))
        for (int i = 0, n = save_popup_array_length(all); i < n; ++i) {
            void *s = write_array_item(all, i);
            void *name = s ? browser_optional(s, "get_name", "System.String") : 0;
            for (unsigned k = 0; k < 15; ++k)
                if (!save_popup_sprite[k] && save_popup_name(name, save_popup_names[k]) &&
                    save_popup_hold(s))
                    save_popup_sprite[k] = s;
        }
    t = browser_type("UnityEngine.U2D", "SpriteAtlas");
    void *aa[] = {t};
    void *atlases = t ? pf_invoke(m, 0, aa) : 0;
    if (atlases && pf_root(&roots, atlases))
        for (int i = 0, n = save_popup_array_length(atlases); i < n; ++i) {
            void *atlas = write_array_item(atlases, i);
            if (!atlas)
                continue;
            const char *sp[] = {"System.String"};
            void *get = browser_method(p_object_get_class(atlas), "GetSprite", "UnityEngine.Sprite",
                                       1, sp, 1);
            for (unsigned k = 0; k < 15; ++k)
                if (!save_popup_sprite[k]) {
                    void *name = p_string_new(save_popup_names[k]);
                    void *args[] = {name};
                    void *s = pf_invoke(get, atlas, args);
                    if (s && save_popup_hold(s)) {
                        save_popup_sprite[k] = s;
                        save_popup_owned[k] = 1;
                    }
                }
        }
    for (unsigned k = 0; k < 15; ++k)
        if (!save_popup_sprite[k]) {
            logtext("SAVE TEMPLATE: popup artwork not loaded: ");
            logtext(save_popup_names[k]);
            nl();
        }
    pf_release(&roots);
}
static void save_popup_bank(void) {
    for (unsigned i = 0; i < 6; ++i)
        if (save_popup_roots[i].n <= 20) {
            browser_current_roots = &save_popup_roots[i];
            return;
        }
    browser_failed = 1;
}
static void *save_popup_image(void *parent, const char *name, float x, float y, float w, float h,
                              void *sprite, int sliced) {
    save_popup_bank();
    void *g = browser_make(name, parent, x, y, w, h, .5f, .5f);
    BrowserColor white = {1, 1, 1, 1};
    void *image = browser_image(g, white);
    unsigned char no = 0;
    browser_set(image, "set_sprite", "UnityEngine.Sprite", sprite);
    browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &sliced);
    browser_set(image, "set_raycastTarget", "System.Boolean", &no);
    browser_active(g, 1);
    return image;
}
/* Sample only images from the native construction window. Never copy scripts,
 * callbacks or a native confirm action into the save dialog. */
static void save_popup_sample_native(void) {
    PFRoots sourceRoots = {0};
    int count = 0;
    void *array = save_cached_panels[0];
    if (!array || !write_array_count(array, &count) || !count) {
        void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
        const char *p[] = {"System.Type"};
        void *t = browser_type("TPS.Game", "TrackRideSetupUI");
        void *a[] = {t};
        array = t ? pf_invoke(
                        browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0),
                        0, a)
                  : 0;
        if (!array || !pf_root(&sourceRoots, array) || !write_array_count(array, &count)) {
            pf_release(&sourceRoots);
            return;
        }
    }
    logtext("SAVE PARCHMENT: available construction panels=");
    lognum((UINT_PTR)count);
    nl();
    for (int i = 0; i < count; ++i) {
        void *panel = write_array_item(array, i);
        if (!panel || !browser_alive(panel))
            continue;
        void *button =
            pf_ref(panel, p_object_get_class(panel), "_confirmButton", "TPS.Core.UI.UIButton");
        void *br = button ? browser_optional(button, "get_transform", "UnityEngine.Transform") : 0;
        void *buttonBox = br ? browser_optional(br, "get_rect", "UnityEngine.Rect") : 0;
        if (buttonBox) {
            BrowserRect box = *(BrowserRect *)p_object_unbox(buttonBox);
            if (box.h >= 60 && box.h <= 150)
                save_popup_action_edge = box.h;
        }
        void *graphic = button ? pf_ref(button, p_object_get_class(button), "_childGraphic",
                                        "UnityEngine.UI.Graphic")
                               : 0;
        void *tick = graphic ? browser_optional(graphic, "get_sprite", "UnityEngine.Sprite") : 0;
        if (tick && !save_popup_sprite[10] && save_popup_hold(tick))
            save_popup_sprite[10] = tick;
        void *close =
            pf_ref(panel, p_object_get_class(panel), "_closeButton", "TPS.Core.UI.UIButton");
        void *closeData = close ? pf_ref(close, p_object_get_class(close), "_stateData",
                                         "TPS.Core.UI.UIButton.StateData")
                                : 0;
        void *normal = closeData ? pf_ref(closeData, p_object_get_class(closeData), "Normal",
                                          "TPS.Core.UI.UIButton.ImageData")
                                 : 0;
        void *idle =
            normal ? pf_ref(normal, p_object_get_class(normal), "Idle", "UnityEngine.Sprite") : 0;
        void *over =
            normal ? pf_ref(normal, p_object_get_class(normal), "Hover", "UnityEngine.Sprite") : 0;
        if (idle && save_popup_hold(idle)) {
            save_popup_close_idle = idle;
            save_popup_native_fold = 1;
        }
        void *closeRect =
            close ? browser_optional(close, "get_transform", "UnityEngine.Transform") : 0;
        void *closeBox =
            closeRect ? browser_optional(closeRect, "get_rect", "UnityEngine.Rect") : 0;
        if (closeBox) {
            BrowserRect box = *(BrowserRect *)p_object_unbox(closeBox);
            if (box.w >= 40 && box.w <= 150 && box.h >= 40 && box.h <= 150) {
                save_popup_close_width = box.w;
                save_popup_close_height = box.h;
            }
        }
        if (over && save_popup_hold(over))
            save_popup_close_hover = over;
        void *cf =
            normal ? pf_field(p_object_get_class(normal), "IdleTint", "UnityEngine.Color") : 0;
        if (cf)
            p_field_get_value(normal, cf, &save_popup_close_tint);
        /* Look beside the native close control for its separate folded header
     * artwork. Reject the rectangular child Background and the cross itself. */
        if (close && !save_popup_native_fold) {
            void *cr = browser_optional(close, "get_transform", "UnityEngine.Transform");
            void *parent = cr ? browser_optional(cr, "get_parent", "UnityEngine.Transform") : 0;
            void *cg =
                parent ? browser_optional(parent, "get_gameObject", "UnityEngine.GameObject") : 0;
            BrowserBounds closeBounds = {0};
            const char *ip[] = {"System.Type", "System.Boolean"};
            void *it = browser_type("UnityEngine.UI", "Image");
            unsigned char include = 1;
            void *ia[] = {it, &include};
            PFRoots roots = {0};
            void *children =
                cg ? pf_invoke(browser_method(p_object_get_class(cg), "GetComponentsInChildren",
                                              "UnityEngine.Component[]", 2, ip, 1),
                               cg, ia)
                   : 0;
            int n = 0;
            if (parent && browser_side_bounds(parent, cr, &closeBounds) && children &&
                pf_root(&roots, children) && write_array_count(children, &n))
                for (int j = 0; j < n; ++j) {
                    void *image = write_array_item(children, j);
                    void *rt =
                        browser_optional(image, "get_rectTransform", "UnityEngine.RectTransform");
                    void *sp = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
                    BrowserBounds bb = {0};
                    if (!sp || !browser_side_bounds(parent, rt, &bb) || bb.size.x < 60 ||
                        bb.size.x > 150 || bb.size.y < 60 || bb.size.y > 150)
                        continue;
                    float dx = bb.center.x - closeBounds.center.x,
                          dy = bb.center.y - closeBounds.center.y;
                    if (dx < -45 || dx > 45 || dy < -45 || dy > 45)
                        continue;
                    void *name = browser_optional(sp, "get_name", "System.String");
                    /* The native header backing is part of the ribbon artwork. */
                    if (!save_popup_name(name, "UI_T_Inspectors_HeaderBase") &&
                        !save_popup_name(name, "UI_T_Menus_HeaderBase") &&
                        !save_popup_name(name, "UI_T_Inspectors_HeaderIconBase"))
                        continue;
                    if (save_popup_hold(sp)) {
                        save_popup_native_fold = 1;
                        save_popup_close_idle = sp;
                        save_popup_close_hover = sp;
                        void *tint = browser_optional(image, "get_color", "UnityEngine.Color");
                        if (tint)
                            save_popup_close_tint = *(BrowserColor *)p_object_unbox(tint);
                    }
                }
            pf_release(&roots);
        }
        void *crossGraphic = close ? pf_ref(close, p_object_get_class(close), "_childGraphic",
                                            "UnityEngine.UI.Graphic")
                                   : 0;
        void *cross =
            crossGraphic ? browser_optional(crossGraphic, "get_sprite", "UnityEngine.Sprite") : 0;
        if (cross && save_popup_hold(cross))
            save_popup_cross_fallback = cross;
        /* Read the rendered close backing directly. Its Background child is the
     * native pale tab; StateData may be a value type and cannot use pf_ref. */
        void *closeGo =
            close ? browser_optional(close, "get_gameObject", "UnityEngine.GameObject") : 0;
        if (closeGo) {
            const char *ip[] = {"System.Type", "System.Boolean"};
            void *it = browser_type("UnityEngine.UI", "Image");
            unsigned char include = 1;
            void *ia[] = {it, &include};
            PFRoots crRoots = {0};
            void *images =
                pf_invoke(browser_method(p_object_get_class(closeGo), "GetComponentsInChildren",
                                         "UnityEngine.Component[]", 2, ip, 1),
                          closeGo, ia);
            float largest = 0;
            int countImages = 0;
            if (images && pf_root(&crRoots, images) && write_array_count(images, &countImages))
                for (int j = 0; j < countImages; ++j) {
                    void *im = write_array_item(images, j);
                    if (im == crossGraphic)
                        continue;
                    void *sp = browser_optional(im, "get_sprite", "UnityEngine.Sprite");
                    void *rt =
                        browser_optional(im, "get_rectTransform", "UnityEngine.RectTransform");
                    void *box = rt ? browser_optional(rt, "get_rect", "UnityEngine.Rect") : 0;
                    if (!sp || !box)
                        continue;
                    BrowserRect b = *(BrowserRect *)p_object_unbox(box);
                    float area = b.w * b.h;
                    if (b.w < 20 || b.h < 20 || b.w > 180 || b.h > 180 || area <= largest)
                        continue;
                    largest = area;
                    save_popup_read_surface(im, &save_popup_close_style);
                    save_popup_close_idle = save_popup_close_hover = sp;
                    save_popup_close_tint = save_popup_close_style.tint;
                    save_popup_native_fold = 1;
                    save_popup_close_width = b.w;
                    save_popup_close_height = b.h;
                }
            pf_release(&crRoots);
        }

        void *go = browser_optional(panel, "get_gameObject", "UnityEngine.GameObject");
        const char *p[] = {"System.Type", "System.Boolean"};
        unsigned char yes = 1;
        void *t = browser_type("UnityEngine.UI", "Image");
        void *a[] = {t, &yes};
        PFRoots roots = {0};
        void *mainPaper = pf_ref(panel, p_object_get_class(panel), "_mainPanelTransform",
                                 "UnityEngine.RectTransform");
        void *images =
            go ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                          "UnityEngine.Component[]", 2, p, 1),
                           go, a)
               : 0;
        int n = 0, paperPriority = 0;
        if (images && pf_root(&roots, images) && write_array_count(images, &n))
            for (int j = 0; j < n; ++j) {
                void *im = write_array_item(images, j);
                void *rt = browser_optional(im, "get_rectTransform", "UnityEngine.RectTransform");
                void *box = browser_optional(rt, "get_rect", "UnityEngine.Rect");
                void *s = browser_optional(im, "get_sprite", "UnityEngine.Sprite");
                if (!box || !s)
                    continue;
                BrowserRect r = *(BrowserRect *)p_object_unbox(box);
                void *name = browser_optional(s, "get_name", "System.String");
                /* Never select the largest image: that is the full-window black dimmer. */
                int isPaper = save_popup_contains(name, "UI_T_Inspectors_") &&
                              save_popup_contains(name, "PanelBase") &&
                              save_popup_contains(name, "Opaque");
                int priority = isPaper ? (rt == mainPaper                               ? 3
                                          : save_popup_contains(name, "CreateTemplate") ? 1
                                                                                        : 2)
                                       : 0;
                if (r.w >= 300 && r.h >= 150 && priority > paperPriority && save_popup_hold(s)) {
                    paperPriority = priority;
                    save_popup_read_surface(im, &save_popup_paper_style);
                    save_popup_sprite[0] = s;
                    char assetName[256];
                    int nameLen = name ? p_string_length(name) : 0;
                    const WCHAR *nameChars = name ? p_string_chars(name) : 0;
                    if (nameChars && nameLen > 0 && nameLen < 256)
                        for (int k = 0; k <= nameLen; ++k)
                            assetName[k] =
                                k < nameLen ? (nameChars[k] < 128 ? (char)nameChars[k] : '?') : 0;
                    if (nameChars && nameLen > 0 && nameLen < 256) {
                        logtext("SAVE PARCHMENT: native sprite=");
                        logtext(assetName);
                        logtext(" width=");
                        lognum((UINT_PTR)r.w);
                        logtext(" height=");
                        lognum((UINT_PTR)r.h);
                        logtext(" slice multiplier x1000=");
                        lognum((UINT_PTR)(save_popup_paper_style.ppu * 1000));
                        logtext(" base PPU x1000=");
                        lognum((UINT_PTR)(save_popup_paper_style.basePPU * 1000));
                        nl();
                    }
                    void *mat = browser_optional(im, "get_material", "UnityEngine.Material");
                    if (mat && save_popup_hold(mat))
                        save_popup_paper_material = mat;
                }
                if (save_popup_name(name, "UI_T_Inspectors_HeaderBase") && save_popup_hold(s)) {
                    save_popup_sprite[1] = s;
                    save_popup_read_surface(im, &save_popup_header_style);
                }
            }
        void *textType = browser_type("TMPro", "TMP_Text");
        void *ta[] = {textType, &yes};
        void *texts =
            go ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                          "UnityEngine.Component[]", 2, p, 1),
                           go, ta)
               : 0;
        int tn = 0;
        float largestFont = 0;
        if (texts && pf_root(&roots, texts) && write_array_count(texts, &tn))
            for (int j = 0; j < tn; ++j) {
                void *label = write_array_item(texts, j);
                void *size = browser_optional(label, "get_fontSize", "System.Single");
                float fs = size ? *(float *)p_object_unbox(size) : 0;
                if (fs < 38 || fs <= largestFont)
                    continue;
                void *font = browser_optional(label, "get_font", "TMPro.TMP_FontAsset");
                if (!font || !save_popup_hold(font))
                    continue;
                save_popup_header_font = font;
                save_popup_header_size = fs;
                largestFont = fs;
                void *style = browser_optional(label, "get_fontStyle", "TMPro.FontStyles");
                if (style)
                    save_popup_header_font_style = *(int *)p_object_unbox(style);
            }
        pf_release(&roots);
        if (save_popup_paper_style.sprite)
            break;
    }
    pf_release(&sourceRoots);
}
static void save_popup_sample_input(void) {
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("TMPro", "TMP_InputField");
    void *a[] = {t, &yes};
    PFRoots roots = {0};
    void *all =
        t ? pf_invoke(browser_method(c, "FindObjectsOfType", "UnityEngine.Object[]", 2, p, 0), 0, a)
          : 0;
    if (all && pf_root(&roots, all))
        for (int i = 0, n = save_popup_array_length(all); i < n && i < 512; ++i) {
            void *field = write_array_item(all, i);
            if (!field || field == save_input)
                continue;
            void *name = browser_optional(field, "get_name", "System.String");
            if (save_popup_name(name, "TPM template name"))
                continue;
            void *image = browser_optional(field, "get_targetGraphic", "UnityEngine.UI.Graphic");
            if (!image || !pf_class(p_object_get_class(image), "UnityEngine.UI.Image"))
                continue;
            void *sprite = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
            if (!sprite)
                continue;
            save_popup_read_surface(image, &save_popup_input_style);
            break;
        }
    pf_release(&roots);
}
/* Owned code-drawn UI mask, only when the native separate fold is unavailable. */
static void save_popup_make_fold(void) {
    if (save_popup_native_fold)
        return;
    void *tc = findclass(p_domain_get(), "UnityEngine", "Texture2D");
    void *sc = findclass(p_domain_get(), "UnityEngine", "Sprite");
    void *fc = findclass(p_domain_get(), "UnityEngine", "TextureFormat");
    void *f = fc ? p_class_get_field_from_name(fc, "RGBA32") : 0;
    if (!tc || !sc || !f || !(p_field_get_flags(f) & 0x10))
        return;
    int format = 0;
    p_field_static_get_value(f, &format);
    int side = 64, bytes = 64 * 64 * 4;
    unsigned char no = 0;
    void *texture = p_object_new(tc);
    if (!texture || !save_popup_hold(texture))
        return;
    save_popup_fold_texture = texture;
    const char *cp[] = {"System.Int32", "System.Int32", "UnityEngine.TextureFormat",
                        "System.Boolean"};
    void *ca[] = {&side, &side, &format, &no};
    if (!write_void(browser_method(tc, ".ctor", "System.Void", 4, cp, 1), texture, ca))
        return;
    static unsigned char pixels[64 * 64 * 4];
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x) {
            unsigned p = (y * 64 + x) * 4;
            int d = (int)y - 32;
            if (d < 0)
                d = -d;
            int left = d * 3 / 4, right = 63 - (int)y / 32;
            pixels[p] = pixels[p + 1] = pixels[p + 2] = 255;
            pixels[p + 3] = (unsigned char)((int)x >= left && (int)x <= right ? 255 : 0);
        }
    void *data = pixels;
    const char *lp[] = {"System.IntPtr", "System.Int32"};
    void *la[] = {&data, &bytes};
    if (!write_void(browser_method(tc, "LoadRawTextureData", "System.Void", 2, lp, 1), texture, la))
        return;
    const char *ap[] = {"System.Boolean", "System.Boolean"};
    void *aa[] = {&no, &no};
    if (!write_void(browser_method(tc, "Apply", "System.Void", 2, ap, 1), texture, aa))
        return;
    BrowserRect rect = {0, 0, 64, 64};
    BrowserV2 pivot = {.5f, .5f};
    float ppu = 100;
    const char *sp[] = {"UnityEngine.Texture2D", "UnityEngine.Rect", "UnityEngine.Vector2",
                        "System.Single"};
    void *sa[] = {texture, &rect, &pivot, &ppu};
    void *sprite = pf_invoke(browser_method(sc, "Create", "UnityEngine.Sprite", 4, sp, 0), 0, sa);
    if (!sprite || !save_popup_hold(sprite))
        return;
    save_popup_fold_sprite = sprite;
    save_popup_close_idle = save_popup_close_hover = sprite;
    save_popup_close_tint = (BrowserColor){.72f, .84f, .88f, .90f};
}
static void save_popup_build(void *parent, void *font) {
    save_popup_sample_native();
    save_popup_sample_input();
    save_popup_resolve();
    save_popup_make_fold();
    if (!save_popup_sprite[9])
        save_popup_sprite[9] = save_popup_cross_fallback;
    browser_current_roots = &save_panel_roots;
    save_panel = browser_make("TPM Save Template panel", parent, 0, 0, 1000, 480, .5f, .5f);
    unsigned char yes = 1;
    void *overlay = browser_add(save_panel, "UnityEngine", "Canvas");
    int sorting = 200;
    browser_set(overlay, "set_overrideSorting", "System.Boolean", &yes);
    browser_set(overlay, "set_sortingOrder", "System.Int32", &sorting);
    browser_add(save_panel, "UnityEngine.UI", "GraphicRaycaster");
    void *pt = browser_get(save_panel, "get_transform", "UnityEngine.Transform");
    /* Block clicks over the window only, leaving the surrounding scene
   * available for camera movement and screenshot framing. */
    void *shield =
        save_popup_image(pt, "Save Template window input shield", 0, 40, 1020, 460, 0, 0);
    BrowserColor clear = {1, 1, 1, 0};
    browser_set(shield, "set_color", "UnityEngine.Color", &clear);
    browser_set(shield, "set_raycastTarget", "System.Boolean", &yes);
    browser_current_roots = &save_panel_roots;
    void *group = browser_add(save_panel, "UnityEngine", "CanvasGroup");
    browser_set(group, "set_blocksRaycasts", "System.Boolean", &yes);
    browser_set(group, "set_interactable", "System.Boolean", &yes);
    /* Keep the native folded corner transparent: no cream rectangle beneath it.
   * The plain shadow supplies the separate window backing. */
    void *shadow = save_popup_image(pt, "Native parchment shadow", -4, 15, 1020, 410, 0, 0);
    BrowserColor shadowInk = {.17f, .19f, .09f, .60f};
    browser_set(shadow, "set_color", "UnityEngine.Color", &shadowInk);
    void *paper =
        save_popup_image(pt, "Native coaster parchment", 0, -5, 1000, 346,
                         save_popup_sprite[0] ? save_popup_sprite[0] : save_popup_sprite[12], 1);
    save_popup_paper_image = paper;
    if (save_popup_paper_material)
        browser_set(paper, "set_material", "UnityEngine.Material", save_popup_paper_material);
    save_popup_apply_surface(paper, &save_popup_paper_style);
    browser_set(paper, "set_raycastTarget", "System.Boolean", &yes);
    /* Image border size also depends on its Canvas referencePixelsPerUnit.
   * Match the native effective slice density on this separately owned canvas. */
    void *paperPPU = browser_optional(paper, "get_pixelsPerUnit", "System.Single");
    if (paperPPU && save_popup_paper_style.basePPU > 0) {
        float own = *(float *)p_object_unbox(paperPPU);
        if (own > 0) {
            float scale = save_popup_paper_style.ppu * save_popup_paper_style.basePPU / own;
            if (scale >= .01f && scale <= 100) {
                browser_set(paper, "set_pixelsPerUnitMultiplier", "System.Single", &scale);
                logtext("SAVE PARCHMENT: matched effective slice density; own base PPU x1000=");
                lognum((UINT_PTR)(own * 1000));
                logtext(" adjusted multiplier x1000=");
                lognum((UINT_PTR)(scale * 1000));
                nl();
            }
        }
    }
    /* Retain the construction panel's complete image settings together;
   * overriding only its sprite or slice scale distorts the folded corner. */
    /* Close backing is above the window shadow and below the ribbon. */
    void *closeBacking =
        save_popup_image(pt, "Native close fold backing", 452, 226, save_popup_close_width,
                         save_popup_close_height, save_popup_close_idle, 0);
    browser_set(closeBacking, "set_color", "UnityEngine.Color", &save_popup_close_tint);
    save_popup_apply_surface(closeBacking, &save_popup_close_style);
    void *ribbon =
        save_popup_image(pt, "Native coaster ribbon", -30, 220, 980, 120, save_popup_sprite[1], 1);
    save_popup_apply_surface(ribbon, &save_popup_header_style);
    save_popup_image(pt, "Native coaster icon backing", -445, 215, 100, 110, save_popup_sprite[2],
                     0);
    save_popup_image(pt, "Coaster header icon", -445, 215, 96, 96, save_popup_sprite[3], 0);
    save_popup_bank();
    void *heading =
        browser_label(pt, save_popup_header_font ? save_popup_header_font : font,
                      "Save Coaster Template", 0, 230, 800, 90, save_popup_header_size);
    BrowserColor white = {1, 1, 1, 1};
    int left = 513;
    browser_set(heading, "set_fontStyle", "TMPro.FontStyles", &save_popup_header_font_style);
    browser_set(heading, "set_color", "UnityEngine.Color", &white);
    browser_set(heading, "set_alignment", "TMPro.TextAlignmentOptions", &left);
    save_popup_bank();
    save_status = browser_label(pt, font, "Coaster Name", 0, 112, 880, 60, 32);
    int labelBold = 1;
    browser_set(save_status, "set_fontStyle", "TMPro.FontStyles", &labelBold);
    unsigned char autoSize = 1;
    float minFont = 24, maxFont = 32;
    browser_set(save_status, "set_enableAutoSizing", "System.Boolean", &autoSize);
    browser_set(save_status, "set_fontSizeMin", "System.Single", &minFont);
    browser_set(save_status, "set_fontSizeMax", "System.Single", &maxFont);
    for (unsigned i = 0; i < 3; ++i) {
        save_popup_bank();
        float x = i == 0   ? -62.f
                  : i == 1 ? 62.f
                           : 452.f,
              y = i == 2 ? 226.f : -95.f,
              edge = i == 2 ? save_popup_close_height : save_popup_action_edge;
        void *toggle = browser_toggle(pt, font,
                                      save_popup_sprite[i == 0 ? 10 : 9] ? ""
                                      : i == 0                           ? "Save"
                                                                         : "X",
                                      x, y, i == 2 ? save_popup_close_width : edge, edge, .5f, .5f);
        save_popup_toggle[i] = toggle;
        save_popup_go[i] = browser_get(toggle, "get_gameObject", "UnityEngine.GameObject");
        void *im = browser_get(toggle, "get_targetGraphic", "UnityEngine.UI.Graphic");
        int simple = 0;
        browser_set(im, "set_type", "UnityEngine.UI.Image.Type", &simple);
        BrowserColor invisible = {1, 1, 1, 0};
        browser_set(im, "set_color", "UnityEngine.Color", i == 2 ? &invisible : &white);
        browser_set(im, "set_sprite", "UnityEngine.Sprite",
                    i == 2 ? save_popup_close_idle : save_popup_sprite[i == 0 ? 5 : 7]);
        void *icon = save_popup_image(
            browser_get(save_popup_go[i], "get_transform", "UnityEngine.Transform"),
            i ? "Cancel cross" : "Save tick", i == 2 ? 8.f : 0.f, 0, i == 2 ? 40.f : edge * .60f,
            i == 2 ? 40.f : edge * .60f,
            i == 2 && save_popup_cross_fallback ? save_popup_cross_fallback
                                                : save_popup_sprite[i ? 9 : 10],
            0);
        BrowserColor ink = i == 2 ? (BrowserColor){.9f, .12f, .02f, 1} : white;
        browser_set(icon, "set_color", "UnityEngine.Color", &ink);
    }
    save_confirm = save_popup_toggle[0];
    save_cancel = save_popup_toggle[1];
    save_close = save_popup_toggle[2];
    save_popup_visual = -2;
    save_log("Save popup: native construction artwork; independent green tick and red cancel.");
}
static void save_popup_refresh_paper(void) {
    if (save_popup_paper_style.sprite || !save_popup_paper_image)
        return;
    save_popup_sample_native();
    if (!save_popup_paper_style.sprite) {
        save_log("Native construction parchment not available yet; will retry next open.");
        return;
    }
    save_popup_apply_surface(save_popup_paper_image, &save_popup_paper_style);
    void *b = browser_optional(save_popup_paper_image, "get_pixelsPerUnit", "System.Single");
    if (b && save_popup_paper_style.basePPU > 0) {
        float own = *(float *)p_object_unbox(b);
        if (own > 0) {
            float scale = save_popup_paper_style.ppu * save_popup_paper_style.basePPU / own;
            if (scale >= .01f && scale <= 100)
                browser_set(save_popup_paper_image, "set_pixelsPerUnitMultiplier", "System.Single",
                            &scale);
        }
    }
    save_log("Native construction parchment applied on dialog open.");
}
static void save_popup_hover(void) {
    int index = -1, pressed = 0;
    PFRoots roots = {0};
    if (save_open && save_panel && browser_alive(save_panel)) {
        void *app = findclass(p_domain_get(), "TPS.Core", "App");
        void *instance =
            pf_invoke(browser_method(app, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0);
        void *input =
            instance ? browser_optional(instance, "get_InputManager", "TPS.Core.Input.InputManager")
                     : 0;
        void *pos = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
        BrowserV2 mouse = {0};
        if (pos)
            mouse = *(BrowserV2 *)p_object_unbox(pos);
        if (input) {
            const char *mp[] = {"System.Int32"};
            int leftButton = 0;
            void *ma[] = {&leftButton};
            void *b = pf_invoke(browser_method(p_object_get_class(input), "WrappedGetMouseButton",
                                               "System.Boolean", 1, mp, 1),
                                input, ma);
            if (b)
                pressed = *(unsigned char *)p_object_unbox(b) != 0;
        }
        const char *cp[] = {"System.Type", "System.Boolean"};
        void *t = browser_type("UnityEngine", "Canvas");
        unsigned char yes = 1;
        void *ca[] = {t, &yes};
        void *canvas =
            pf_invoke(browser_method(p_object_get_class(save_panel), "GetComponentInParent",
                                     "UnityEngine.Component", 2, cp, 1),
                      save_panel, ca);
        void *root = canvas ? browser_optional(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
        void *camera = 0;
        void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
        if (mode && *(int *)p_object_unbox(mode) != 0)
            camera = browser_optional(root, "get_worldCamera", "UnityEngine.Camera");
        if (pos) {
            const char *p[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2",
                               "UnityEngine.Camera"};
            void *rc = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
            void *m = browser_method(rc, "RectangleContainsScreenPoint", "System.Boolean", 3, p, 0);
            for (unsigned i = 0; i < 3; ++i) {
                void *rt = browser_get(save_popup_go[i], "get_transform", "UnityEngine.Transform");
                void *a[] = {rt, &mouse, camera};
                void *b = pf_invoke(m, 0, a);
                if (b && *(unsigned char *)p_object_unbox(b)) {
                    index = (int)i;
                    break;
                }
            }
        }
    }
    int visual = index >= 0 ? index + (pressed ? 3 : 0) : -1;
    if (visual != save_popup_visual) {
        save_popup_visual = visual;
        for (unsigned i = 0; i < 3; ++i)
            if (save_popup_toggle[i] && browser_alive(save_popup_toggle[i])) {
                void *im = browser_get(save_popup_toggle[i], "get_targetGraphic",
                                       "UnityEngine.UI.Graphic");
                unsigned k = i == 0 ? (index == 0 ? 6 : 5) : i == 1 ? (index == 1 ? 8 : 7) : 4;
                if (pressed && index == (int)i && i < 2)
                    k = i == 0 ? 14 : 13;
                if (i < 2)
                    browser_set(im, "set_sprite", "UnityEngine.Sprite", save_popup_sprite[k]);
                float f = index == (int)i ? (pressed ? 1.05f : 1.14f) : 1.f;
                BrowserV3 scale = {f, f, 1};
                browser_set(browser_get(save_popup_go[i], "get_transform", "UnityEngine.Transform"),
                            "set_localScale", "UnityEngine.Vector3", &scale);
            }
    }
    pf_release(&roots);
}
static void save_popup_cleanup(void) {
    for (unsigned i = 0; i < 6; ++i)
        save_destroy_roots(&save_popup_roots[i]);
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = browser_method(c, "Destroy", "System.Void", 1, p, 0);
    for (unsigned i = 0; i < 15; ++i)
        if (save_popup_owned[i] && save_popup_sprite[i]) {
            void *a[] = {save_popup_sprite[i]};
            write_void(m, 0, a);
        }
    void *folds[] = {save_popup_fold_sprite, save_popup_fold_texture};
    for (unsigned i = 0; i < 2; ++i)
        if (folds[i]) {
            void *a[] = {folds[i]};
            write_void(m, 0, a);
        }
    save_popup_fold_sprite = save_popup_fold_texture = 0;
    save_popup_native_fold = 0;
    pf_release(&save_popup_assets);
    pf_release(&save_popup_extra_assets);
    memset(save_popup_sprite, 0, sizeof(save_popup_sprite));
    memset(save_popup_owned, 0, sizeof(save_popup_owned));
    memset(save_popup_toggle, 0, sizeof(save_popup_toggle));
    memset(save_popup_go, 0, sizeof(save_popup_go));
    save_close = 0;
    save_popup_cross_fallback = 0;
    save_popup_paper_material = save_popup_close_idle = save_popup_close_hover = 0;
    save_popup_close_tint = (BrowserColor){1, 1, 1, 1};
    save_popup_header_font = 0;
    save_popup_header_size = 48;
    save_popup_header_font_style = 1;
    memset(&save_popup_paper_style, 0, sizeof(save_popup_paper_style));
    memset(&save_popup_header_style, 0, sizeof(save_popup_header_style));
    memset(&save_popup_input_style, 0, sizeof(save_popup_input_style));
    memset(&save_popup_close_style, 0, sizeof(save_popup_close_style));
    save_popup_visual = -2;
    save_popup_action_edge = 108;
    save_popup_close_width = 84;
    save_popup_close_height = 76;
    save_popup_native_fold = 0;
    save_popup_paper_image = 0;
}
