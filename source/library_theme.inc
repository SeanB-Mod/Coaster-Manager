#include "preview_background.h"
#define LIBRARY_THEME_COUNT 36
/* Library-only native artwork; accepted save popup and side buttons are untouched. */
static PFRoots library_theme_roots[4], library_chrome_roots[5];
static void *library_registration;
static void library_chrome_bank(void) {
    for (unsigned b = 0; b < 5; ++b)
        if (library_chrome_roots[b].n < 22) {
            browser_current_roots = &library_chrome_roots[b];
            return;
        }
    browser_failed = 1;
}
static void *library_theme_sprite[LIBRARY_THEME_COUNT];
static unsigned char library_theme_owned[LIBRARY_THEME_COUNT];
static void *library_heading, *library_type_image, *library_paper_image, *library_type_font;
static int library_type_index = -1;
static PFRoots library_header_roots;
static void *library_header_sprite;
static void *library_ribbon_image;
static UINT_PTR library_art_due;
static int library_theme_usable(void *sprite) {
    if (!sprite || !browser_alive(sprite))
        return 0;
    void *texture = browser_optional(sprite, "get_texture", "UnityEngine.Texture2D");
    return texture && browser_alive(texture);
}
static const char *library_theme_names[] = {
    "UI_T_Inspectors_HeaderBase",
    "UI_T_Shared_TooltipBaseFrame",
    "UI_Icons_PauseMenu_PhotoMode",
    "UI_Atlas_Icons_Inspectors_ButtonDeleteSmall_Idle",
    "UI_T_Inspectors_GreenRectanglePrimaryButtonLargeBase_Idle",
    "UI_T_Inspectors_RedRectanglePrimaryButtonLargeBase_Idle",
    "UI_InGame_T_Icon_Item_Rollercoaster_SteepleChase_Cart",
    "UI_InGame_T_Icon_Item_Rollercoaster_Steel_FrontCart",
    "UI_InGame_T_Icon_Item_Rollercoaster_Spinning_Cart",
    "UI_InGame_T_Icon_Item_Rollercoaster_Amusement_Cart",
    "UI_InGame_T_Icon_Item_Rollercoaster_BabyCaterpillar_FrontCart",
    "UI_Atlas_Icons_Inspectors_BuzzRating",
    "UI_Atlas_Icons_RightInspector_Thrill",
    "UI_Atlas_Icons_TrillDLC_FerocityAttribute",
    "UI_Atlas_Icons_RightInspector_Nausea",
    "UI_T_Inspector_RatingTextBase",
    "UI_T_Inspectors_CloseButton",
    "UI_T_Inspectors_CreateTemplatePanelBase_Opaque",
    "UI_T_Inspectors_OrangeSquarePrimaryButtonLargeBase_Idle",
    "UI_T_Inspectors_HeaderIconBase",
    "UI_T_Inspectors_RedRectanglePrimaryButtonLargeBase_Hovered",
    "UI_T_Inspectors_RedRectanglePrimaryButtonLargeBase_Selected",
    "UI_T_Inspectors_GreenRectanglePrimaryButtonLargeBase_Hovered",
    "UI_T_Inspectors_GreenRectanglePrimaryButtonLargeBase_Selected",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonLargeBase_Idle",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonLargeBase_Hovered",
    "UI_T_Inspectors_OrangeRectanglePrimaryButtonLargeBase_Selected",
    "UI_Atlas_Pause_QuickLoad",
    "UI_T_Container_Workshop_BG_Prehistory",
    "UI_T_Container_Workshop_BG_Atlantis",
    "UI_T_Container_Workshop_BG_Science",
    "UI_T_Container_Workshop_BG_Space",
    "UI_T_Container_Workshop_BG_Ghostology",
    "UI_T_Container_Workshop_BG_Botany",
    "UI_T_Container_Workshop_BG_Wildlife",
    "UI_T_Container_Workshop_BG_Cult"};
static int library_theme_hold(void *o) {
    if (!o)
        return 0;
    for (unsigned b = 0; b < 4; ++b) {
        for (unsigned i = 0; i < library_theme_roots[b].n; ++i)
            if (p_gchandle_get_target(library_theme_roots[b].h[i]) == o)
                return 1;
    }
    for (unsigned b = 0; b < 4; ++b)
        if (library_theme_roots[b].n < 32)
            return pf_root(&library_theme_roots[b], o);
    return 0;
}
static int library_theme_match(void *s, const char *n) {
    if (!s)
        return 0;
    int len = (int)length(n);
    if (p_string_length(s) < len)
        return 0;
    const WCHAR *w = p_string_chars(s);
    if (!w)
        return 0;
    for (int i = 0; i < len; ++i)
        if (w[i] != (WCHAR)(unsigned char)n[i])
            return 0;
    return p_string_length(s) == len || w[len] == '(' || w[len] == ' ';
}
/* Resource inventories also contain our temporary dialog sprite clones. Never
 * borrow their lifetime: retain a distinct native Sprite owned by this cache. */
static int library_theme_copy(unsigned slot, void *source) {
    if (slot >= LIBRARY_THEME_COUNT || !library_theme_usable(source))
        return 0;
    PFRoots roots = {0};
    if (!pf_root(&roots, source))
        return 0;
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *a[] = {source};
    void *copy = pf_invoke(browser_method(c, "Instantiate", "UnityEngine.Object", 1, p, 0), 0, a);
    int ok = copy && copy != source && library_theme_usable(copy) && library_theme_hold(copy);
    if (ok) {
        browser_set(copy, "set_name", "System.String",
                    p_string_new("TPM_CoasterTools_Library_Art"));
        library_theme_sprite[slot] = copy;
        library_theme_owned[slot] = 1;
    } else if (copy && copy != source) {
        void *da[] = {copy};
        write_void(browser_method(c, "Destroy", "System.Void", 1, p, 0), 0, da);
    }
    pf_release(&roots);
    return ok;
}
static void library_theme_forget(unsigned slot) {
    void *old = library_theme_sprite[slot];
    if (old && library_theme_owned[slot] && browser_alive(old)) {
        void *c = findclass(p_domain_get(), "UnityEngine", "Object");
        const char *p[] = {"UnityEngine.Object"};
        void *a[] = {old};
        write_void(browser_method(c, "Destroy", "System.Void", 1, p, 0), 0, a);
    }
    for (unsigned b = 0; b < 4; ++b)
        for (unsigned i = 0; i < library_theme_roots[b].n; ++i)
            if (p_gchandle_get_target(library_theme_roots[b].h[i]) == old) {
                p_gchandle_free(library_theme_roots[b].h[i]);
                for (unsigned j = i + 1; j < library_theme_roots[b].n; ++j)
                    library_theme_roots[b].h[j - 1] = library_theme_roots[b].h[j];
                --library_theme_roots[b].n;
                break;
            }
    library_theme_sprite[slot] = 0;
    library_theme_owned[slot] = 0;
}
static void library_theme_resolve(void) {
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *p[] = {"System.Type"};
    void *m = browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, p, 0);
    void *t = browser_type("UnityEngine", "Sprite");
    void *a[] = {t};
    void *all = pf_invoke(m, 0, a);
    int n = 0;
    if (all && pf_root(&roots, all) && browser_asset_array_count(all, &n))
        for (int i = 0; i < n; ++i) {
            void *sp = write_array_item(all, i);
            void *name = browser_optional(sp, "get_name", "System.String");
            for (unsigned k = 0; k < LIBRARY_THEME_COUNT; ++k)
                if (!library_theme_sprite[k] && library_theme_match(name, library_theme_names[k]))
                    library_theme_copy(k, sp);
        }
    t = browser_type("UnityEngine.U2D", "SpriteAtlas");
    void *aa[] = {t};
    void *atlases = pf_invoke(m, 0, aa);
    if (atlases && pf_root(&roots, atlases) && browser_asset_array_count(atlases, &n))
        for (int i = 0; i < n; ++i) {
            void *atlas = write_array_item(atlases, i);
            const char *sp[] = {"System.String"};
            void *get = browser_method(p_object_get_class(atlas), "GetSprite", "UnityEngine.Sprite",
                                       1, sp, 1);
            for (unsigned k = 0; k < LIBRARY_THEME_COUNT; ++k)
                if (!library_theme_sprite[k]) {
                    void *arg[] = {p_string_new(library_theme_names[k])};
                    void *image = pf_invoke(get, atlas, arg);
                    if (image) {
                        library_theme_copy(k, image);
                        /* Destroy is deferred: hide its native-looking name
                         * before another UI resolver can borrow this temporary. */
                        browser_set(image, "set_name", "System.String",
                                    p_string_new("TPM_CoasterTools_Temporary_Art"));
                        void *oc = findclass(p_domain_get(), "UnityEngine", "Object");
                        const char *dp[] = {"UnityEngine.Object"};
                        void *da[] = {image};
                        write_void(browser_method(oc, "Destroy", "System.Void", 1, dp, 0), 0, da);
                    }
                }
        }
    logtext("TEMPLATE UI: artwork inventory last scan count=");
    lognum(n);
    nl();
    pf_release(&roots);
    for (unsigned k = 0; k < LIBRARY_THEME_COUNT; ++k)
        if (!library_theme_sprite[k]) {
            browser_log("LIBRARY ART missing:");
            logtext(library_theme_names[k]);
            nl();
        }
}
static void *library_art(void *parent, const char *name, float x, float y, float w, float h,
                         void *sprite, int sliced) {
    void *g = browser_make(name, parent, x, y, w, h, .5f, .5f);
    BrowserColor white = {1, 1, 1, 1};
    void *im = browser_image(g, white);
    unsigned char no = 0;
    browser_set(im, "set_sprite", "UnityEngine.Sprite", sprite);
    browser_set(im, "set_type", "UnityEngine.UI.Image.Type", &sliced);
    browser_set(im, "set_raycastTarget", "System.Boolean", &no);
    browser_active(g, 1);
    return im;
}
/* Sample the same actual construction parchment as the accepted save screen. */
static void library_theme_paper(void *target);
static void library_price_surface(void *target);
static void library_theme_close(void *back, void *cross);
static void library_theme_cleanup(void) {
    library_ribbon_image = 0;
    library_art_due = 0;
    pf_release(&library_header_roots);
    library_header_sprite = 0;
    if (library_registration) {
        write_void(browser_method(p_object_get_class(library_registration), "Unregister",
                                  "System.Void", 0, 0, 1),
                   library_registration, 0);
        library_registration = 0;
    }
    library_price_source = 0;
    for (unsigned b = 0; b < 5; ++b)
        pf_release(&library_chrome_roots[b]);
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = browser_method(c, "Destroy", "System.Void", 1, p, 0);
    for (unsigned k = 0; k < LIBRARY_THEME_COUNT; ++k)
        if (library_theme_owned[k] && library_theme_sprite[k]) {
            void *a[] = {library_theme_sprite[k]};
            write_void(m, 0, a);
        }
    for (unsigned b = 0; b < 4; ++b)
        pf_release(&library_theme_roots[b]);
    memset(library_theme_sprite, 0, sizeof(library_theme_sprite));
    memset(library_theme_owned, 0, sizeof(library_theme_owned));
    library_heading = library_type_image = library_paper_image = library_type_font = 0;
    library_type_index = -1;
}
static void library_row_action(unsigned index, int mode);
static void library_inline_tick(void);
static void library_end_inline_edit(void);
static void *preview_small_texture(void *source, unsigned w, unsigned h, PFRoots *roots);

/* Borrow the selected native row's resolved artwork, independently of previews.
 * Keep it rooted across row recycling; never destroy a borrowed sprite. */
static void library_restore_type_icon(void) {
    if (!library_type_image)
        return;
    void *sprite = library_header_sprite;
    if (!sprite && library_type_index >= 0 && library_type_index < 5)
        sprite = library_theme_sprite[6 + (unsigned)library_type_index];
    browser_set(library_type_image, "set_overrideSprite", "UnityEngine.Sprite", 0);
    browser_set(library_type_image, "set_material", "UnityEngine.Material", 0);
    browser_set(library_type_image, "set_sprite", "UnityEngine.Sprite", sprite);
    int simple = 0;
    browser_set(library_type_image, "set_type", "UnityEngine.UI.Image.Type", &simple);
    BrowserColor ink = {1, 1, 1, sprite ? 1.f : 0.f};
    browser_set(library_type_image, "set_color", "UnityEngine.Color", &ink);
}
static void library_type_header(void *row) {
    pf_release(&library_header_roots);
    library_header_sprite = 0;
    void *icon = pf_ref(row, p_object_get_class(row), "_icon", "UnityEngine.UI.Image");
    void *sprite = icon ? browser_optional(icon, "get_overrideSprite", "UnityEngine.Sprite") : 0;
    if (!sprite && icon)
        sprite = browser_optional(icon, "get_sprite", "UnityEngine.Sprite");
    if (sprite && browser_alive(sprite) && pf_root(&library_header_roots, sprite))
        library_header_sprite = sprite;
    browser_log(library_header_sprite ? "HEADER: selected native row sprite retained"
                                      : "HEADER: native row sprite unavailable; type fallback");
    void *donor = pf_ref(row, p_object_get_class(row), "_nameLabel", "TMPro.TMP_Text");
    void *name = donor ? browser_optional(donor, "get_text", "System.String") : 0;
    WCHAR title[256];
    unsigned n = 0;
    const WCHAR *w = name ? p_string_chars(name) : 0;
    int len = name ? p_string_length(name) : 0;
    if (w && len > 0 && len < 200) {
        for (int i = 0; i < len; ++i)
            title[n++] = w[i];
    } else {
        const WCHAR *unknown = L"Coaster";
        while (*unknown)
            title[n++] = *unknown++;
    }
    const WCHAR *suffix = L" Templates";
    while (*suffix)
        title[n++] = *suffix++;
    title[n] = 0;
    browser_set(library_heading, "set_text", "System.String", p_string_new_utf16(title, (int)n));
    PFRoots roots = {0};
    void *d = browser_row_definition(row, &roots);
    void *base = findclass(p_domain_get(), "TPS.Game", "EntityDefinition");
    void *array = d ? pf_ref(d, base, "Components", "TPS.Game.IComponentProxy[]") : 0;
    int count = 0, index = -1;
    if (array && pf_root(&roots, array) && write_array_count(array, &count))
        for (int i = 0; i < count; ++i) {
            void *proxy = write_array_item(array, i);
            if (!proxy || !pf_class(p_object_get_class(proxy), "TPS.Game.ECPTrackRide"))
                continue;
            void *f =
                pf_field(p_object_get_class(proxy), "trackRideType", "TPS.Game.TrackRideType");
            int value = 0;
            if (!f)
                break;
            p_field_get_value(proxy, f, &value);
            void *ec = findclass(p_domain_get(), "TPS.Game", "TrackRideType");
            const char *names[] = {"Steeplechase", "Steel", "Spinning", "Amusement", "Baby"};
            for (unsigned k = 0; k < 5; ++k) {
                void *ef = p_class_get_field_from_name(ec, names[k]);
                int v = 0;
                if (ef) {
                    p_field_static_get_value(ef, &v);
                    if (v == value)
                        index = (int)k;
                }
            }
            break;
        }
    pf_release(&roots);
    library_type_index = index;
    library_restore_type_icon();
    library_theme_paper(library_paper_image);
}
