/* Museum Demolition style; copy artwork only, never native actions/listeners. */
static PFRoots delete_chrome[4], delete_assets;
static void *delete_art[6], *delete_name, *delete_title, *delete_close;
static unsigned char delete_owned[6];
static int delete_mouse_down, delete_escape_down, delete_ui_hidden;
static void *delete_box, *delete_canvas, *delete_registration;
static SavePopupSurface delete_button_surface, delete_header_surface;
static void *delete_header_material;
static void *delete_body_font, *delete_ribbon, *delete_fold, *delete_paper, *delete_backing;
static float delete_line_spacing = -28;
static const char *delete_art_names[] = {
    "UI_T_Menus_PopUpBase",
    "UI_T_Menus_HeaderBase",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonMediumBase_Idle",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonMediumBase_Hovered",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonMediumBase_Selected",
    "Icons_Menu_TitleHeaders_Popout_Mods"};
static void *export_header_icon, *export_icon_back, *export_icon_frame;
static void *export_prefab_sprite;
static void *upload_action_icon[2];
static SavePopupSurface export_original_paper, export_original_header, export_original_button;
static void delete_bank(unsigned i) {
    browser_current_roots = &delete_chrome[i];
}
static void delete_resolve(void) {
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *m = browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0);
    void *t = browser_type("UnityEngine", "Sprite");
    void *a[] = {t};
    void *all = pf_invoke(m, 0, a);
    int n = 0;
    if (all && pf_root(&roots, all) && write_array_count(all, &n))
        for (int i = 0; i < n; ++i) {
            void *s = write_array_item(all, i);
            void *name = browser_optional(s, "get_name", "System.String");
            for (unsigned j = 0; j < 6; ++j)
                if (!delete_art[j] && library_theme_match(name, delete_art_names[j]) &&
                    pf_root(&delete_assets, s))
                    delete_art[j] = s;
        }
    t = browser_type("UnityEngine.U2D", "SpriteAtlas");
    void *aa[] = {t};
    void *atlases = pf_invoke(m, 0, aa);
    if (atlases && pf_root(&roots, atlases) && write_array_count(atlases, &n))
        for (int i = 0; i < n; ++i) {
            void *atlas = write_array_item(atlases, i);
            const char *sp[] = {"System.String"};
            void *get = browser_method(p_object_get_class(atlas), "GetSprite", "UnityEngine.Sprite",
                                       1, sp, 1);
            for (unsigned j = 0; j < 6; ++j)
                if (!delete_art[j]) {
                    void *sa[] = {p_string_new(delete_art_names[j])};
                    void *s = pf_invoke(get, atlas, sa);
                    if (s && pf_root(&delete_assets, s)) {
                        delete_art[j] = s;
                        delete_owned[j] = 1;
                    }
                }
        }
    /* This build ships the Mods tab artwork under this verified atlas name.
  * Prefer the requested popout icon when present; use native Mods artwork otherwise. */
    if (!delete_art[5] && all && write_array_count(all, &n))
        for (int i = 0; i < n && !delete_art[5]; ++i) {
            void *sp = write_array_item(all, i);
            void *name = browser_optional(sp, "get_name", "System.String");
            if (library_theme_match(name, "Borelli_Icons_Menu_Tabs_Mods") &&
                pf_root(&delete_assets, sp))
                delete_art[5] = sp;
        }
    if (!delete_art[5] && atlases && write_array_count(atlases, &n))
        for (int i = 0; i < n && !delete_art[5]; ++i) {
            void *atlas = write_array_item(atlases, i);
            const char *sp[] = {"System.String"};
            void *sa[] = {p_string_new("Borelli_Icons_Menu_Tabs_Mods")};
            void *icon = pf_invoke(browser_method(p_object_get_class(atlas), "GetSprite",
                                                  "UnityEngine.Sprite", 1, sp, 1),
                                   atlas, sa);
            if (icon && pf_root(&delete_assets, icon)) {
                delete_art[5] = icon;
                delete_owned[5] = 1;
            }
        }
    browser_log(delete_art[5] ? "EXPORT: native Mods icon resolved"
                              : "EXPORT: Mods icon unavailable in loaded atlases");
    pf_release(&roots);
}
/* The supplied name may identify an Image child rather than the Sprite asset.
 * Sample its artwork only; never clone the prefab's controls or listeners. */
static void export_find_prefab_icon(void) {
    if (export_prefab_sprite)
        return;
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *t = browser_type("UnityEngine.UI", "Image");
    void *a[] = {t};
    void *all =
        t ? pf_invoke(browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0), 0,
                      a)
          : 0;
    int n = 0;
    if (all && pf_root(&roots, all) && write_array_count(all, &n))
        for (int i = 0; i < n; ++i) {
            void *image = write_array_item(all, i);
            void *go = browser_optional(image, "get_gameObject", "UnityEngine.GameObject");
            void *name = go ? browser_optional(go, "get_name", "System.String") : 0;
            if (!library_theme_match(name, "Icons_Menu_TitleHeaders_Popout_Mods"))
                continue;
            void *sp = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
            if (sp && pf_root(&delete_assets, sp)) {
                export_prefab_sprite = sp;
                browser_log("EXPORT: exact Mods artwork sampled from prefab Image child");
                break;
            }
        }
    pf_release(&roots);
}
static void delete_sample_button(void) {
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *m = browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0);
    void *t = browser_type("TPS.Game.UI", "MessagePopup2ButtonUI");
    void *a[] = {t};
    void *all = t ? pf_invoke(m, 0, a) : 0;
    int n = 0;
    if (all && pf_root(&roots, all) && write_array_count(all, &n))
        for (int i = 0; i < n && !delete_button_surface.sprite; ++i) {
            void *popup = write_array_item(all, i);
            void *base = findclass(p_domain_get(), "TPS.Game.UI", "MessagePopupUI");
            void *body = popup ? pf_ref(popup, base, "_text", "TMPro.TMP_Text") : 0;
            if (body) {
                void *font = browser_optional(body, "get_font", "TMPro.TMP_FontAsset");
                if (font && pf_root(&delete_assets, font))
                    delete_body_font = font;
                void *spacing = browser_optional(body, "get_lineSpacing", "System.Single");
                if (spacing)
                    delete_line_spacing = *(float *)p_object_unbox(spacing);
            }
            void *button =
                popup ? pf_ref(popup, p_object_get_class(popup), "_button1", "TPS.Core.UI.UIButton")
                      : 0;
            void *go =
                button ? browser_optional(button, "get_gameObject", "UnityEngine.GameObject") : 0;
            if (!go)
                continue;
            const char *ip[] = {"System.Type", "System.Boolean"};
            unsigned char yes = 1;
            void *it = browser_type("UnityEngine.UI", "Image");
            void *ia[] = {it, &yes};
            void *images =
                pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                         "UnityEngine.Component[]", 2, ip, 1),
                          go, ia);
            int count = 0;
            if (images && pf_root(&roots, images) && write_array_count(images, &count))
                for (int j = 0; j < count; ++j) {
                    void *image = write_array_item(images, j);
                    void *sp = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
                    void *name = sp ? browser_optional(sp, "get_name", "System.String") : 0;
                    void *rt =
                        browser_optional(image, "get_rectTransform", "UnityEngine.RectTransform");
                    void *box = rt ? browser_optional(rt, "get_rect", "UnityEngine.Rect") : 0;
                    if (sp && box) {
                        BrowserRect bounds = *(BrowserRect *)p_object_unbox(box);
                        if (bounds.w >= 100 && bounds.h >= 40) {
                            save_popup_read_surface(image, &delete_button_surface);
                            if (delete_button_surface.sprite)
                                pf_root(&delete_assets, delete_button_surface.sprite);
                            if (delete_button_surface.material)
                                pf_root(&delete_assets, delete_button_surface.material);
                            browser_log("DELETE native button sprite:");
                            char asset[256];
                            if (name && save_utf8(name, asset, sizeof(asset)))
                                browser_log(asset);
                            break;
                        }
                    }
                }
            void *popupGo = browser_optional(popup, "get_gameObject", "UnityEngine.GameObject");
            void *popupImages = popupGo
                                    ? pf_invoke(browser_method(p_object_get_class(popupGo),
                                                               "GetComponentsInChildren",
                                                               "UnityEngine.Component[]", 2, ip, 1),
                                                popupGo, ia)
                                    : 0;
            int imageCount = 0;
            if (popupImages && pf_root(&roots, popupImages) &&
                write_array_count(popupImages, &imageCount))
                for (int j = 0; j < imageCount; ++j) {
                    void *image = write_array_item(popupImages, j);
                    void *sp = browser_optional(image, "get_sprite", "UnityEngine.Sprite");
                    void *name = sp ? browser_optional(sp, "get_name", "System.String") : 0;
                    void *rt =
                        browser_optional(image, "get_rectTransform", "UnityEngine.RectTransform");
                    void *box = rt ? browser_optional(rt, "get_rect", "UnityEngine.Rect") : 0;
                    if (!box)
                        continue;
                    BrowserRect bb = *(BrowserRect *)p_object_unbox(box);
                    if (bb.w > 600 && (library_theme_match(name, "UI_T_Menus_HeaderBase") ||
                                       library_theme_match(name, "UI_T_Inspectors_HeaderBase"))) {
                        save_popup_read_surface(image, &delete_header_surface);
                        if (delete_header_surface.sprite)
                            pf_root(&delete_assets, delete_header_surface.sprite);
                        if (delete_header_surface.material)
                            pf_root(&delete_assets, delete_header_surface.material);
                        browser_log("DELETE: sampled native popup header surface");
                        break;
                    }
                }
        }
    pf_release(&roots);
    browser_log(delete_button_surface.sprite
                    ? "DELETE: sampled native confirmation button surface"
                    : "DELETE: native confirmation button unavailable; named artwork fallback");
}
/* Sliced Image padding is in sprite pixels, independent of the stretched centre. */
static void delete_rect(void *component, float x, float y, float w, float h) {
    void *rt = browser_get(component, "get_transform", "UnityEngine.Transform");
    BrowserV2 pos = {x, y}, size = {w, h};
    browser_set(rt, "set_anchoredPosition", "UnityEngine.Vector2", &pos);
    browser_set(rt, "set_sizeDelta", "UnityEngine.Vector2", &size);
}
static void delete_align(void) {
    if (!delete_ribbon)
        return;
    float scale = preview_capture_price_width() / 240;
    if (scale < .1f || scale > 4)
        scale = .5f;
    /* The rendered atlas quad contains alpha inside its mesh: Sprite.GetPadding returns zero.
  * Supplied captures measure a 36px blue face in a 48px quad (75 percent opaque height).
  * Size the blue face, not the quad; keep its lower edge flush to the white body. */
    float visibleHeight = 48 / scale, height = visibleHeight / .75f, bottom = height * .20f,
          top = height * .05f, left = 6, centre = 340 - visibleHeight / 2 - bottom + height / 2,
          bodyLeft = -550 + left, bodyRight = 500;
    float canvasScale = 1;
    void *cs = browser_optional(delete_canvas, "get_scaleFactor", "System.Single");
    if (cs && *(float *)p_object_unbox(cs) > .01f)
        canvasScale = *(float *)p_object_unbox(cs);
    BrowserV3 boxScale = {scale / canvasScale, scale / canvasScale, 1};
    browser_set(delete_box, "set_localScale", "UnityEngine.Vector3", &boxScale);
    delete_rect(delete_ribbon, -45, centre, 1010, height);
    delete_rect(delete_backing, (bodyLeft + bodyRight) / 2, 0, bodyRight - bodyLeft, 680);
    delete_rect(delete_paper, (bodyLeft + bodyRight) / 2, 0, bodyRight - bodyLeft, 680);
    float visualCentre = 340;
    delete_rect(delete_title, -30, visualCentre, 800, 90);
    delete_rect(delete_fold, 448, visualCentre, save_popup_close_width * 1.2f,
                save_popup_close_height * 1.2f);
    delete_rect(delete_close, 448, visualCentre, save_popup_close_width * 1.2f,
                save_popup_close_height * 1.2f);
    /* Restore the accepted delete appearance before applying export-only styling. */
    int exporting = management_mode == 6;
    BrowserColor white = {1, 1, 1, 1};
    for (unsigned i = 0; i < 2; ++i)
        browser_active(
            browser_get(upload_action_icon[i], "get_gameObject", "UnityEngine.GameObject"),
            exporting);
    if (exporting) {
        save_popup_resolve();
        save_popup_sample_native();
        export_find_prefab_icon();
        for (unsigned i = 0; i < 2; ++i) {
            void *sp = save_popup_sprite[i ? 9 : 10];
            browser_set(upload_action_icon[i], "set_sprite", "UnityEngine.Sprite", sp);
            BrowserColor ink = {1, 1, 1, sp ? 1.f : 0.f};
            browser_set(upload_action_icon[i], "set_color", "UnityEngine.Color", &ink);
        }
    }
    if (exporting && !delete_art[5])
        delete_resolve();
    void *mods = export_prefab_sprite ? export_prefab_sprite : delete_art[5];
    if (exporting && mods) {
        browser_set(export_header_icon, "set_sprite", "UnityEngine.Sprite", mods);
        browser_set(export_header_icon, "set_color", "UnityEngine.Color", &white);
    }
    browser_set(delete_paper, "set_sprite", "UnityEngine.Sprite", export_original_paper.sprite);
    browser_set(delete_paper, "set_material", "UnityEngine.Material",
                export_original_paper.material);
    browser_set(delete_paper, "set_type", "UnityEngine.UI.Image.Type", &export_original_paper.type);
    browser_set(delete_paper, "set_color", "UnityEngine.Color", &export_original_paper.tint);
    browser_set(delete_ribbon, "set_material", "UnityEngine.Material",
                export_original_header.material);
    save_popup_apply_surface(delete_ribbon, &export_original_header);
    browser_active(browser_get(export_header_icon, "get_gameObject", "UnityEngine.GameObject"),
                   exporting);
    browser_active(browser_get(export_icon_back, "get_gameObject", "UnityEngine.GameObject"),
                   exporting);
    browser_active(browser_get(export_icon_frame, "get_gameObject", "UnityEngine.GameObject"),
                   exporting);
    browser_set(delete_backing, "set_color", "UnityEngine.Color", &white);
    delete_rect(management_status, 0, 35, 930, 390);
    float min = 38, max = 42;
    browser_set(management_status, "set_fontSizeMin", "System.Single", &min);
    browser_set(management_status, "set_fontSizeMax", "System.Single", &max);
    void *controls[] = {management_confirm, management_cancel};
    for (unsigned i = 0; i < 2; ++i) {
        void *image = browser_get(controls[i], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_set(image, "set_material", "UnityEngine.Material", export_original_button.material);
        save_popup_apply_surface(image, &export_original_button);
    }
    if (exporting) {
        float iconY = 220 + 5 / scale;
        delete_rect(export_icon_frame, -445, iconY, 165, 165);
        delete_rect(export_icon_back, -445, iconY, 150, 150);
        delete_rect(export_header_icon, -445, iconY, 135, 135);
        SavePopupSurface paper = save_popup_paper_style;
        if (!paper.sprite)
            paper.sprite = library_theme_sprite[17];
        save_popup_apply_surface(delete_paper, &paper);
        void *b = browser_optional(delete_paper, "get_pixelsPerUnit", "System.Single");
        if (b && paper.basePPU > 0) {
            float own = *(float *)p_object_unbox(b);
            if (own > 0) {
                float density = paper.ppu * paper.basePPU / own;
                browser_set(delete_paper, "set_pixelsPerUnitMultiplier", "System.Single", &density);
            }
        }
        save_popup_apply_surface(delete_ribbon, &save_popup_header_style);
        delete_rect(delete_paper, 0, 0, 1000, 440);
        delete_rect(delete_backing, 0, -5, 1010, 450);
        BrowserColor shadow = {.17f, .19f, .09f, .60f};
        browser_set(delete_backing, "set_color", "UnityEngine.Color", &shadow);
        delete_rect(delete_ribbon, -30, 220, 980, height);
        delete_rect(delete_title, 55, 230, 730, 90);
        delete_rect(delete_fold, 452, 226, save_popup_close_width, save_popup_close_height);
        delete_rect(delete_close, 452, 226, save_popup_close_width, save_popup_close_height);
        delete_rect(management_status, 0, 50, 900, 250);
        min = 34;
        max = 36;
        browser_set(management_status, "set_fontSizeMin", "System.Single", &min);
        browser_set(management_status, "set_fontSizeMax", "System.Single", &max);
        for (unsigned i = 0; i < 2; ++i) {
            void *image = browser_get(controls[i], "get_targetGraphic", "UnityEngine.UI.Graphic");
            browser_skin_image(image, save_popup_sprite[i ? 7 : 5], 0);
            browser_set(image, "set_material", "UnityEngine.Material", 0);
            browser_set(image, "set_color", "UnityEngine.Color", &white);
            int simple = 0;
            browser_set(image, "set_type", "UnityEngine.UI.Image.Type", &simple);
        }
    }
    logtext("TEMPLATE UI: DELETE visible header target px=48; sprite padding L/B/T units=");
    lognum((UINT_PTR)left);
    logtext("/");
    lognum((UINT_PTR)bottom);
    logtext("/");
    lognum((UINT_PTR)top);
    logtext("; screen scale x1000=");
    lognum((UINT_PTR)(scale * 1000));
    nl();
}
static void delete_stop(void) {
    browser_active(management_panel, 0);
    if (delete_ui_hidden) {
        save_restore_ui();
        delete_ui_hidden = 0;
    }
}
static int delete_start(void) {
    delete_align();
    browser_active(management_panel, 0);
    if (management_mode != 6) {
        if (!save_hide_ui())
            return 0;
        delete_ui_hidden = 1;
    }
    unsigned char on = 1;
    browser_set(delete_canvas, "set_enabled", "System.Boolean", &on);
    browser_active(management_panel, 1);
    return 1;
}
static void delete_cleanup(void) {
    delete_stop();
    if (delete_registration) {
        write_void(browser_method(p_object_get_class(delete_registration), "Unregister",
                                  "System.Void", 0, 0, 1),
                   delete_registration, 0);
        delete_registration = 0;
    }
    delete_box = delete_canvas = 0;
    export_header_icon = export_icon_back = export_icon_frame = export_prefab_sprite = 0;
    memset(upload_action_icon, 0, sizeof(upload_action_icon));
    if (delete_header_material)
        preview_destroy(delete_header_material);
    delete_header_material = 0;
    for (unsigned i = 0; i < 4; ++i)
        save_destroy_roots(&delete_chrome[i]);
    for (unsigned i = 0; i < 6; ++i)
        if (delete_owned[i])
            preview_destroy(delete_art[i]);
    pf_release(&delete_assets);
    memset(delete_art, 0, sizeof(delete_art));
    memset(delete_owned, 0, sizeof(delete_owned));
    delete_name = delete_title = delete_close = delete_body_font = 0;
    delete_ribbon = delete_fold = delete_paper = delete_backing = 0;
    delete_line_spacing = -28;
    memset(&delete_button_surface, 0, sizeof(delete_button_surface));
    memset(&delete_header_surface, 0, sizeof(delete_header_surface));
}
static void delete_construct(void *parent, void *font) {
    save_popup_resolve();
    save_popup_sample_native();
    delete_resolve();
    delete_sample_button();
    delete_bank(0);
    management_panel =
        browser_make("Delete Template confirmation", parent, 0, 0, 1920, 1080, .5f, .5f);
    void *canvas = delete_canvas = browser_add(management_panel, "UnityEngine", "Canvas");
    unsigned char yes = 1, no = 0;
    int order = 550, overlay = 0;
    browser_set(canvas, "set_renderMode", "UnityEngine.RenderMode", &overlay);
    browser_set(canvas, "set_overrideSorting", "System.Boolean", &yes);
    browser_set(canvas, "set_sortingOrder", "System.Int32", &order);
    browser_add(management_panel, "UnityEngine.UI", "GraphicRaycaster");
    void *pt = browser_get(management_panel, "get_transform", "UnityEngine.Transform");
    const char *pp[] = {"UnityEngine.Transform", "System.Boolean"};
    void *pa[] = {0, &no};
    write_void(browser_method(p_object_get_class(pt), "SetParent", "System.Void", 2, pp, 1), pt,
               pa);
    void *scaler = browser_add(management_panel, "UnityEngine.UI", "CanvasScaler");
    int scalerMode = 1;
    BrowserV2 reference = {1920, 1080};
    browser_set(scaler, "set_uiScaleMode", "UnityEngine.UI.CanvasScaler.ScaleMode", &scalerMode);
    browser_set(scaler, "set_referenceResolution", "UnityEngine.Vector2", &reference);
    void *screenShield = library_art(pt, "Full screen delete input shield", 0, 0, 1920, 1080, 0, 0);
    BrowserColor shieldColour = {1, 1, 1, 0};
    browser_set(screenShield, "set_color", "UnityEngine.Color", &shieldColour);
    browser_set(screenShield, "set_raycastTarget", "System.Boolean", &yes);
    delete_box = browser_make("Delete dialog layout", pt, 0, 0, 1000, 800, .5f, .5f);
    browser_active(delete_box, 1);
    delete_box = browser_get(delete_box, "get_transform", "UnityEngine.Transform");
    pt = delete_box;

    void *shield = library_art(pt, "Delete modal input shield", 0, 0, browser_width + 40,
                               browser_height + 160, 0, 0);
    BrowserColor clear = {1, 1, 1, 0};
    browser_set(shield, "set_color", "UnityEngine.Color", &clear);
    browser_set(shield, "set_raycastTarget", "System.Boolean", &yes);
    delete_bank(1);
    delete_backing = library_art(pt, "Opaque white confirmation backing", 0, -20, 1000, 720, 0, 0);
    void *paper = delete_paper =
        library_art(pt, "Solid white confirmation body", 0, -20, 1000, 720, 0, 0);
    BrowserColor white = {1, 1, 1, 1};
    browser_set(paper, "set_color", "UnityEngine.Color", &white);
    browser_set(paper, "set_raycastTarget", "System.Boolean", &yes);
    void *fold = delete_fold =
        library_art(pt, "Delete close fold", 440, 365, save_popup_close_width,
                    save_popup_close_height, save_popup_close_idle, 0);
    save_popup_apply_surface(fold, &save_popup_close_style);
    browser_set(fold, "set_color", "UnityEngine.Color", &save_popup_close_tint);
    delete_bank(2);
    void *ribbon = delete_ribbon = library_art(pt, "Delete confirmation ribbon", -30, 365, 1040,
                                               100, library_theme_sprite[0], 1);
    if (delete_header_surface.sprite)
        save_popup_apply_surface(ribbon, &delete_header_surface);
    else
        save_popup_apply_surface(ribbon, &save_popup_header_style);
    SavePopupSurface *headerStyle =
        delete_header_surface.sprite ? &delete_header_surface : &save_popup_header_style;
    void *headerPPU = browser_optional(ribbon, "get_pixelsPerUnit", "System.Single");
    if (headerPPU && headerStyle->basePPU > 0) {
        float own = *(float *)p_object_unbox(headerPPU);
        if (own > 0) {
            float factor = headerStyle->ppu * headerStyle->basePPU / own;
            browser_set(ribbon, "set_pixelsPerUnitMultiplier", "System.Single", &factor);
        }
    }
    if (!delete_header_surface.sprite) {
        void *source = browser_optional(ribbon, "get_material", "UnityEngine.Material");
        void *oc = findclass(p_domain_get(), "UnityEngine", "Object");
        const char *op[] = {"UnityEngine.Object"};
        void *oa[] = {source};
        void *copy =
            source ? pf_invoke(browser_method(oc, "Instantiate", "UnityEngine.Object", 1, op, 0), 0,
                               oa)
                   : 0;
        if (copy && pf_root(&delete_assets, copy)) {
            delete_header_material = copy;
            const char *cp[] = {"System.String", "UnityEngine.Color"};
            BrowserColor cyan = {1, 1.15f, 1.04f, 1};
            void *ca[] = {p_string_new("_Color"), &cyan};
            if (write_void(
                    browser_method(p_object_get_class(copy), "SetColor", "System.Void", 2, cp, 1),
                    copy, ca))
                browser_set(ribbon, "set_material", "UnityEngine.Material", copy);
        }
    }
    delete_bank(2);
    export_icon_frame =
        library_art(pt, "Export Mods white square frame", -445, 220, 165, 165, 0, 0);
    export_icon_back = library_art(pt, "Export Mods grey inset", -445, 220, 150, 150, 0, 0);
    BrowserColor iconInk = {.22f, .29f, .27f, 1};
    browser_set(export_icon_back, "set_color", "UnityEngine.Color", &iconInk);
    export_header_icon =
        library_art(pt, "Export Mods header icon", -445, 220, 135, 135, delete_art[5], 0);
    browser_set(export_header_icon, "set_preserveAspect", "System.Boolean", &yes);
    if (!delete_art[5])
        browser_set(export_header_icon, "set_color", "UnityEngine.Color", &clear);

    delete_title = browser_label(pt, font, "Delete Template", 0, 365, 800, 90, 48);
    int bold = 1, left = 513;
    browser_set(delete_title, "set_color", "UnityEngine.Color", &white);
    browser_set(delete_title, "set_fontStyle", "TMPro.FontStyles", &bold);
    browser_set(delete_title, "set_alignment", "TMPro.TextAlignmentOptions", &left);
    delete_close = browser_toggle(pt, font, "", 440, 365, save_popup_close_width,
                                  save_popup_close_height, .5f, .5f);
    void *im = browser_get(delete_close, "get_targetGraphic", "UnityEngine.UI.Graphic");
    browser_set(im, "set_sprite", "UnityEngine.Sprite", 0);
    browser_set(im, "set_color", "UnityEngine.Color", &clear);
    void *ct = browser_get(delete_close, "get_transform", "UnityEngine.Transform");
    library_art(ct, "Delete close cross", 8, 0, 40, 40,
                save_popup_cross_fallback ? save_popup_cross_fallback : library_theme_sprite[16],
                0);
    delete_bank(3);
    delete_name = browser_label(pt, font, "", 0, 230, 900, 105, 40);
    browser_set(delete_name, "set_fontStyle", "TMPro.FontStyles", &bold);
    browser_set(delete_name, "set_enableAutoSizing", "System.Boolean", &yes);
    float min = 24, max = 40;
    browser_set(delete_name, "set_fontSizeMin", "System.Single", &min);
    browser_set(delete_name, "set_fontSizeMax", "System.Single", &max);
    management_status = browser_label(pt, font, "", 0, 35, 930, 390, 42);
    BrowserColor bodyInk = {.27f, .56f, .70f, 1};
    browser_set(management_status, "set_color", "UnityEngine.Color", &bodyInk);
    float spacing = 0;
    if (delete_body_font)
        browser_set(management_status, "set_font", "TMPro.TMP_FontAsset", delete_body_font);
    browser_set(management_status, "set_lineSpacing", "System.Single", &delete_line_spacing);
    browser_set(management_status, "set_paragraphSpacing", "System.Single", &spacing);
    browser_set(management_status, "set_enableWordWrapping", "System.Boolean", &yes);
    int overflow = 0;
    browser_set(management_status, "set_overflowMode", "TMPro.TextOverflowModes", &overflow);
    browser_set(management_status, "set_enableAutoSizing", "System.Boolean", &yes);
    min = 38;
    max = 42;
    browser_set(management_status, "set_fontSizeMin", "System.Single", &min);
    browser_set(management_status, "set_fontSizeMax", "System.Single", &max);
    browser_current_roots = &management_modal;
    management_confirm = browser_toggle(pt, font, "Yes", -112, -230, 220, 104, .5f, .5f);
    management_cancel = browser_toggle(pt, font, "No", 112, -230, 220, 104, .5f, .5f);
    void *controls[] = {management_confirm, management_cancel};
    for (unsigned i = 0; i < 2; ++i) {
        void *image = browser_get(controls[i], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(image, delete_art[2], 0);
        if (delete_button_surface.sprite) {
            save_popup_apply_surface(image, &delete_button_surface);
            void *b = browser_optional(image, "get_pixelsPerUnit", "System.Single");
            if (b && delete_button_surface.basePPU > 0) {
                float own = *(float *)p_object_unbox(b);
                if (own > 0) {
                    float scale = delete_button_surface.ppu * delete_button_surface.basePPU / own;
                    browser_set(image, "set_pixelsPerUnitMultiplier", "System.Single", &scale);
                }
            }
        }
        void *label = library_control_label(controls[i]);
        float size = 38;
        browser_set(label, "set_fontSize", "System.Single", &size);
        BrowserV2 pos = {0, 8};
        browser_set(browser_get(label, "get_transform", "UnityEngine.Transform"),
                    "set_anchoredPosition", "UnityEngine.Vector2", &pos);
    }
    browser_set(paper, "set_maskable", "System.Boolean", &no);
    browser_current_roots = &management_modal;
    for (unsigned i = 0; i < 2; ++i) {
        void *control = i ? management_cancel : management_confirm;
        upload_action_icon[i] = library_art(
            browser_get(control, "get_transform", "UnityEngine.Transform"),
            i ? "Cancel upload cross" : "Confirm upload tick", 0, 0, save_popup_action_edge * .60f,
            save_popup_action_edge * .60f, save_popup_sprite[i ? 9 : 10], 0);
    }
    save_popup_read_surface(delete_paper, &export_original_paper);
    save_popup_read_surface(delete_ribbon, &export_original_header);
    save_popup_read_surface(
        browser_get(management_confirm, "get_targetGraphic", "UnityEngine.UI.Graphic"),
        &export_original_button);
    browser_current_roots = &management_modal;
    void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
    delete_registration = rc ? p_object_new(rc) : 0;
    if (!delete_registration || !browser_hold(delete_registration))
        browser_failed = 1;
    else {
        write_void(browser_method(rc, ".ctor", "System.Void", 0, 0, 1), delete_registration, 0);
        const char *rp[] = {"UnityEngine.GameObject"};
        void *ra[] = {management_panel};
        if (!write_void(browser_method(rc, "Register", "System.Void", 1, rp, 1),
                        delete_registration, ra))
            browser_failed = 1;
    }
}
static int delete_choice_raw(void) {
    int mouse = (GetAsyncKeyState(1) & 0x8000) != 0, escape = (GetAsyncKeyState(27) & 0x8000) != 0,
        click = mouse && !delete_mouse_down, back = escape && !delete_escape_down;
    delete_mouse_down = mouse;
    delete_escape_down = escape;
    if (back || browser_pressed(delete_close))
        return -1;
    if (browser_pressed(management_cancel))
        return 2;
    if (browser_pressed(management_confirm))
        return 1;
    void *app = findclass(p_domain_get(), "TPS.Core", "App");
    void *instance = pf_invoke(browser_method(app, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0);
    void *input =
        instance ? browser_optional(instance, "get_InputManager", "TPS.Core.Input.InputManager")
                 : 0;
    void *position = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (!position)
        return 0;
    BrowserV2 point = *(BrowserV2 *)p_object_unbox(position);
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *canvas =
        pf_invoke(browser_method(p_object_get_class(management_panel), "GetComponentInParent",
                                 "UnityEngine.Component", 2, cp, 1),
                  management_panel, ca);
    void *root = canvas ? browser_optional(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
    void *camera = mode && *(int *)p_object_unbox(mode) != 0
                       ? browser_optional(root, "get_worldCamera", "UnityEngine.Camera")
                       : 0;
    const char *p[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera"};
    void *c = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
    void *m = browser_method(c, "RectangleContainsScreenPoint", "System.Boolean", 3, p, 0);
    void *controls[] = {delete_close, management_cancel, management_confirm};
    int result = 0;
    for (unsigned i = 0; i < 3; ++i) {
        void *rt = browser_get(controls[i], "get_transform", "UnityEngine.Transform");
        void *a[] = {rt, &point, camera};
        void *b = pf_invoke(m, 0, a);
        int over = b && *(unsigned char *)p_object_unbox(b);
        if (i) {
            void *im = browser_get(controls[i], "get_targetGraphic", "UnityEngine.UI.Graphic");
            void *sp = delete_art[over ? (mouse ? 4 : 3) : 2];
            if (management_mode == 6) {
                unsigned index =
                    i == 2 ? (over ? (mouse ? 14 : 6) : 5) : (over ? (mouse ? 13 : 8) : 7);
                void *art = save_popup_sprite[index];
                if (art)
                    browser_set(im, "set_sprite", "UnityEngine.Sprite", art);
            } else if (sp && !delete_button_surface.sprite)
                browser_set(im, "set_sprite", "UnityEngine.Sprite", sp);
        }
        if (click && over)
            result = i == 2 ? 1 : (i == 1 ? 2 : -1);
    }
    return result;
}

static int delete_choice(void) {
    int choice = delete_choice_raw();
    return choice == 2 ? -1 : choice;
}
