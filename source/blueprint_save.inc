#include "save_ui_timing.h"
/* In-game Save Blueprint: frozen read-only snapshot + asynchronous Unity PNG,
 * published as one directory transaction. Never saves or changes the museum. */
DLLIMPORT int WINAPI WideCharToMultiByte(unsigned, DWORD, const WCHAR *, int, char *, int,
                                         const char *, BOOL *);
DLLIMPORT void WINAPI GetSystemTimeAsFileTime(void *);
DLLIMPORT BOOL WINAPI QueryPerformanceCounter(long long *);
DLLIMPORT BOOL WINAPI RemoveDirectoryW(const WCHAR *);
static PFRoots save_controls, save_panel_roots, save_input_roots, save_station_roots;
static void *save_modal_registration;
static void *save_owner, *save_button_go, *save_button, *save_panel, *save_status, *save_input,
    *save_cancel, *save_confirm, *save_station;
static unsigned char save_document[4U * 1024U * 1024U + 1];
static unsigned save_document_size;
static char save_name[401], save_coaster_name[401], save_id[33], save_hash[65];
static WCHAR save_root[768], save_stage[1024], save_final[1024], save_photo[1100];
static long long save_definition;
static unsigned long long save_source;
static unsigned save_entity[2];
static int save_phase, save_open, save_stage_owned;
static void save_scan_ui(void);
static UINT_PTR save_scan_due, save_dialog_opened;
static PFRoots save_discovery_roots[3];
static void *save_cached_panels[3];
static UINT_PTR save_discovery_due[3];
static unsigned save_discovery_cursor;
/* Diagnostic counts: no heap allocation, no per-frame disk writes. */
static UINT_PTR save_perf_calls, save_perf_total_us, save_perf_peak_us, save_perf_global_scans;
static PFRoots save_slot_roots[3];
static void *save_slot_donor[3], *save_slot_toggle[3];
static UINT_PTR save_seen, save_due, save_started;
static unsigned save_sequence;
static BlueprintDisplayStats save_display_stats;
static unsigned save_curve_count;
static float save_curve[CURVE_PREVIEW_MAX][3];
static void save_log(const char *s) {
    log_stamp();
    logtext("SAVE TEMPLATE: ");
    logtext(s);
    nl();
}
#include "blueprint_save_cache.inc"
#include "blueprint_save_button.inc"
#include "blueprint_save_popup.inc"
/* Reuse the verified save-window surface without sharing owned UI objects. */
static void library_price_surface(void *target) {
    if (!target || !library_price_source)
        return;
    SavePopupSurface style = {0};
    save_popup_read_surface(library_price_source, &style);
    if (style.sprite)
        library_theme_hold(style.sprite);
    if (style.material)
        library_theme_hold(style.material);
    save_popup_apply_surface(target, &style);
    void *b = browser_optional(target, "get_pixelsPerUnit", "System.Single");
    if (b && style.basePPU > 0) {
        float own = *(float *)p_object_unbox(b);
        if (own > 0) {
            float scale = style.ppu * style.basePPU / own;
            browser_set(target, "set_pixelsPerUnitMultiplier", "System.Single", &scale);
        }
    }
}
static void library_theme_close(void *back, void *cross) {
    save_popup_sample_native();
    if (save_popup_native_fold && save_popup_close_idle) {
        library_theme_hold(save_popup_close_idle);
        if (save_popup_close_style.material)
            library_theme_hold(save_popup_close_style.material);
        browser_set(back, "set_sprite", "UnityEngine.Sprite", save_popup_close_idle);
        save_popup_apply_surface(back, &save_popup_close_style);
        browser_set(back, "set_color", "UnityEngine.Color", &save_popup_close_tint);
        void *rt = browser_optional(back, "get_transform", "UnityEngine.Transform");
        BrowserV2 size = {save_popup_close_width, save_popup_close_height};
        browser_set(rt, "set_sizeDelta", "UnityEngine.Vector2", &size);
    }
    if (save_popup_cross_fallback && library_theme_hold(save_popup_cross_fallback))
        browser_set(cross, "set_sprite", "UnityEngine.Sprite", save_popup_cross_fallback);
}
static void library_theme_paper(void *target) {
    if (!save_popup_paper_style.sprite)
        save_popup_sample_native();
    if (!save_popup_paper_style.sprite)
        return;
    library_theme_hold(save_popup_paper_style.sprite);
    if (save_popup_paper_style.material)
        library_theme_hold(save_popup_paper_style.material);
    save_popup_apply_surface(target, &save_popup_paper_style);
    void *b = browser_optional(target, "get_pixelsPerUnit", "System.Single");
    if (b && save_popup_paper_style.basePPU > 0) {
        float own = *(float *)p_object_unbox(b);
        if (own > 0) {
            float scale = save_popup_paper_style.ppu * save_popup_paper_style.basePPU / own;
            browser_set(target, "set_pixelsPerUnitMultiplier", "System.Single", &scale);
        }
    }
}

static int save_utf8(void *string, char *out, unsigned cap) {
    if (!string || !pf_class(p_object_get_class(string), "System.String"))
        return 0;
    int n = p_string_length(string);
    const WCHAR *chars = p_string_chars(string);
    if (!chars || n < 1 || n > 400)
        return 0;
    int bytes = WideCharToMultiByte(65001, 0x80, chars, n, out, (int)cap - 1, 0, 0);
    if (bytes < 1)
        return 0;
    out[bytes] = 0;
    return 1;
}
static int save_normalize_name(char *name) {
    unsigned n = length(name), first = 0;
    while (first < n && (name[first] == ' ' || name[first] == '\t'))
        ++first;
    while (n > first && (name[n - 1] == ' ' || name[n - 1] == '\t'))
        --n;
    unsigned count = n - first;
    for (unsigned i = 0; i < count; ++i)
        name[i] = name[first + i];
    name[count] = 0;
    return library_name(name);
}
static void save_message(const char *s) {
    if (save_status && browser_alive(save_status))
        browser_set(save_status, "set_text", "System.String", p_string_new(s));
    save_log(s);
}
static int save_child_path(WCHAR *out, const WCHAR *base, const WCHAR *file) {
    unsigned pos = 0;
    return append(out, 1100, &pos, base) && append(out, 1100, &pos, L"\\") &&
           append(out, 1100, &pos, file);
}
static int save_write_new(const WCHAR *path, const unsigned char *b, unsigned n) {
    HANDLE file = CreateFileW(path, 0x40000000, 0, 0, 1, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    DWORD written = 0;
    int ok = WriteFile(file, b, n, &written, 0) && written == n;
    if (ok)
        ok = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file))
        ok = 0;
    return ok;
}
static __declspec(noinline) void save_discard_stage(void) {
    if (!save_stage_owned)
        return;
    WCHAR path[1100];
    const WCHAR *files[] = {L"entry.json", L"blueprint.json", L"preview.png"};
    for (unsigned i = 0; i < 3; ++i)
        if (save_child_path(path, save_stage, files[i]))
            DeleteFileW(path);
    RemoveDirectoryW(save_stage);
    save_stage_owned = 0; /* No recursive deletion, even for a failed capture. */
}
static __declspec(noinline) int save_make_stage(void) {
    if (!coaster_storage_path(save_root, 768, 0, 1))
        return 0;
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        unsigned char uuid[16];
        typedef int(WINAPI * RandomBytes)(void *, unsigned char *, unsigned, unsigned);
        HMODULE crypto = LoadLibraryW(L"bcrypt.dll");
        RandomBytes random = crypto ? (RandomBytes)GetProcAddress(crypto, "BCryptGenRandom") : 0;
        int secure = random && random(0, uuid, 16, 2) >= 0;
        if (crypto)
            FreeLibrary(crypto);
        if (!secure) {
            unsigned long long stamp = 0;
            long long counter = 0;
            GetSystemTimeAsFileTime(&stamp);
            QueryPerformanceCounter(&counter);
            unsigned long long entropy[4] = {stamp, (unsigned long long)counter,
                                             ((unsigned long long)GetCurrentThreadId() << 32) ^
                                                 ++save_sequence,
                                             (UINT_PTR)&uuid ^ save_source};
            char hash[65];
            library_sha((unsigned char *)entropy, sizeof(entropy), hash);
            for (unsigned i = 0; i < 16; ++i)
                uuid[i] = (unsigned char)((lj_hex(hash[i * 2]) << 4) | lj_hex(hash[i * 2 + 1]));
        }
        uuid[6] = (uuid[6] & 15) | 64;
        uuid[8] = (uuid[8] & 63) | 128;
        const char *hex = "0123456789abcdef";
        WCHAR wide[33];
        for (unsigned i = 0; i < 16; ++i) {
            save_id[i * 2] = hex[uuid[i] >> 4];
            save_id[i * 2 + 1] = hex[uuid[i] & 15];
        }
        save_id[32] = 0;
        for (unsigned i = 0; i < 33; ++i)
            wide[i] = (WCHAR)save_id[i];
        unsigned a = 0, b = 0;
        if (!append(save_stage, 1024, &a, save_root) ||
            !append(save_stage, 1024, &a, L"\\.saving-") || !append(save_stage, 1024, &a, wide) ||
            !append(save_final, 1024, &b, save_root) || !append(save_final, 1024, &b, L"\\") ||
            !append(save_final, 1024, &b, wide))
            return 0;
        if (GetFileAttributesW(save_final) != 0xffffffffU)
            continue;
        if (CreateDirectoryW(save_stage, 0)) {
            save_stage_owned = 1;
            return save_child_path(save_photo, save_stage, L"preview.png");
        }
    }
    return 0;
}
/* jq is the legacy ASCII exporter; preserve UTF-8 names verbatim here. */
static void save_json_string(const char *s) {
    js("\"");
    while (*s) {
        char b[3] = {0};
        unsigned char c = (unsigned char)*s++;
        if (c == '"' || c == '\\') {
            b[0] = '\\';
            b[1] = (char)c;
        } else
            b[0] = (char)c;
        js(b);
    }
    js("\"");
}
/* Sample the already-built native spline; never rebuild geometry or spawn track. */
static void save_capture_curve(void *station) {
    save_curve_count = 0;
    PFRoots roots = {0};
    void *bus = 0;
    void *context = write_context(station, save_entity, &roots, &bus);
    if (!context)
        goto done;
    void *visual = pf_ref(context, p_object_get_class(context), "trackRideVisualComponent",
                          "TPS.Game.TrackRideVisualComponent");
    void *container = visual ? pf_ref(visual, p_object_get_class(visual), "_splineContainer",
                                      "UnityEngine.Splines.SplineContainer")
                             : 0;
    if (!container || !pf_root(&roots, container))
        goto done;
    void *c = p_object_get_class(container);
    const char *sp[] = {"System.Single"};
    void *evaluate = pf_method(c, "EvaluatePosition", "Unity.Mathematics.float3", 1, sp, 1);
    void *length =
        pf_invoke(pf_method(c, "CalculateLength", "System.Single", 0, 0, 1), container, 0);
    if (!evaluate || !length || !pf_class(p_object_get_class(length), "System.Single"))
        goto done;
    float metres = *(float *)p_object_unbox(length);
    if (!(metres > 1 && metres < 30000))
        goto done;
    station_frame(station);
    if (!g_frame)
        goto done;
    for (unsigned i = 0; i < CURVE_PREVIEW_MAX; ++i) {
        float t = (float)i / CURVE_PREVIEW_MAX;
        void *a[] = {&t};
        void *point = pf_invoke(evaluate, container, a);
        if (!point || !pf_class(p_object_get_class(point), "Unity.Mathematics.float3") ||
            !write_value_layout(p_object_get_class(point), 12))
            goto done;
        float world[3], local[3];
        memcpy(world, p_object_unbox(point), 12);
        if (!finite_vector(world, 3))
            goto done;
        inverse_point(world, g_origin, g_rotation, local);
        for (unsigned k = 0; k < 3; ++k) {
            if (!(local[k] >= -4096 && local[k] <= 4096))
                goto done;
            save_curve[i][k] = local[k];
        }
    }
    save_curve_count = CURVE_PREVIEW_MAX;
done:
    pf_release(&roots);
    save_log(save_curve_count ? "Native 3D spline preview captured"
                              : "Native spline preview unavailable; footprint fallback retained");
}
static __declspec(noinline) int save_write_metadata(void) {
    jpos = 0;
    jok = 1;
    jbuf[0] = 0;
    js("{\"schemaVersion\":1,\"id\":");
    save_json_string(save_id);
    js(",\"name\":");
    save_json_string(save_name);
    js(",\"coasterName\":");
    save_json_string(save_coaster_name);
    js(",\"stationDefinitionId\":\"");
    ji(save_definition);
    js("\",\"screenshot\":\"preview.png\",\"planSha256\":");
    save_json_string(save_hash);
    js(",\"displayStats\":{");
    for (unsigned i = 0; i < 5; ++i) {
        if (i)
            js(",");
        save_json_string(blueprint_stat_keys[i]);
        js(":");
        if (save_display_stats.value[i][0])
            save_json_string(save_display_stats.value[i]);
        else
            js("null");
    }
    js("}");
    if (colour_saved.present) {
        js(",\"trackColour\":{\"custom\":");
        js(colour_saved.custom ? "true" : "false");
        js(",\"rgba\":");
        jvector(colour_saved.rgba, 4);
        js("}");
    }
    if (save_curve_count) {
        js(",\"previewCurve\":[");
        for (unsigned i = 0; i < save_curve_count; ++i) {
            if (i)
                js(",");
            jvector(save_curve[i], 3);
        }
        js("]");
    }
    js("}\n");
    if (jpos > 15000 && save_curve_count) {
        save_curve_count = 0;
        save_log("Spline preview omitted to keep metadata within its size limit");
        return save_write_metadata();
    }
    WCHAR path[1100];
    return jok && jpos <= LIBRARY_METADATA_LIMIT &&
           save_child_path(path, save_stage, L"entry.json") &&
           save_write_new(path, (const unsigned char *)jbuf, jpos);
}
static int save_validate_document(void) {
    unsigned root = lj_parse(save_document, save_document_size),
             n = root ? library_prepare(root, library_plan) : 0;
    if (!n || !write_decode_plan(library_plan, n, 0))
        return 0;
    long long definition = 0;
    unsigned long long source = 0;
    memcpy(&definition, library_plan + 24, 8);
    memcpy(&source, library_plan + 16, 8);
    if (definition != save_definition || source != save_source)
        return 0;
    library_sha(library_plan, n, save_hash);
    return 1;
}
static __declspec(noinline) void blueprint_save_exported(void) {
    if (!blueprint_save_export_request || save_phase != 1 || jpos > 4U * 1024U * 1024U)
        return;
    save_document_size = jpos;
    memcpy(save_document, jbuf, jpos);
    if (!save_validate_document()) {
        save_message(
            "This coaster cannot be saved yet.\n\nIts snapshot is missing or contains unsupported data.\n\nNo library entry was created.");
        return;
    }
    if (!save_make_stage()) {
        save_message(
            "Could not create the library folder.\n\nCheck the mod log and folder access.");
        return;
    }
    WCHAR path[1100];
    if (!save_child_path(path, save_stage, L"blueprint.json") ||
        !save_write_new(path, save_document, save_document_size) || !save_write_metadata()) {
        save_discard_stage();
        save_message("Could not write the template.\n\nNo completed entry was published.");
        return;
    }
    save_phase = 2;
    save_due = GetTickCount64() + 350;
    save_started = GetTickCount64();
    browser_active(save_panel, 0);
    browser_active(save_button_go, 0);
    save_open = 0;
    save_log("Snapshot validated and staged; waiting to capture current view.");
}
static unsigned save_png_crc(const unsigned char *b, unsigned n) {
    static unsigned table[256];
    static int ready;
    if (!ready) {
        for (unsigned i = 0; i < 256; ++i) {
            unsigned c = i;
            for (unsigned j = 0; j < 8; ++j)
                c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
            table[i] = c;
        }
        ready = 1;
    }
    unsigned crc = 0xffffffffU;
    for (unsigned i = 0; i < n; ++i)
        crc = (crc >> 8) ^ table[(crc ^ b[i]) & 255];
    return crc ^ 0xffffffffU;
}
static int save_complete_png(const unsigned char *b, unsigned n) {
    unsigned w = 0, h = 0;
    if (n < 57 || b[0] != 137 || !library_image_size(b, n, &w, &h))
        return 0;
    unsigned pos = 8;
    int image = 0;
    while (pos <= n - 12) {
        unsigned len = library_be32(b + pos);
        if (len > n - pos - 12)
            return 0;
        if (save_png_crc(b + pos + 4, len + 4) != library_be32(b + pos + 8 + len))
            return 0;
        const unsigned char *t = b + pos + 4;
        if (t[0] == 'I' && t[1] == 'D' && t[2] == 'A' && t[3] == 'T')
            image = 1;
        if (t[0] == 'I' && t[1] == 'E' && t[2] == 'N' && t[3] == 'D')
            return image && len == 0 && pos + 12 == n;
        pos += len + 12;
    }
    return 0;
}
#include "blueprint_preview.inc"
#include "isolated_preview.inc"
static __declspec(noinline) int save_finish_stage(void) {
    library_catalogue_invalidate();
    unsigned n = library_read(save_photo, library_image_bytes, 20U * 1024U * 1024U);
    if (!n || !save_complete_png(library_image_bytes, n))
        return 0;
    if (!preview_resize_capture(save_photo))
        return -1;
    HANDLE photo =
        CreateFileW(save_photo, 0x40000000, FILE_SHARE_READ, 0, 3, FILE_ATTRIBUTE_NORMAL, 0);
    if (photo == INVALID_HANDLE_VALUE)
        return 0;
    int flushed = FlushFileBuffers(photo) != 0;
    if (!CloseHandle(photo))
        flushed = 0;
    if (!flushed)
        return 0;
    if (!MoveFileExW(save_stage, save_final, 0))
        return -1;
    save_stage_owned = 0;
    return 1;
}
/* Canvas.enabled hides rendering without disabling GameObjects or callbacks.
 * Only previously enabled canvases are retained and restored. */
static PFRoots save_hidden_canvases[8];
static void *save_canvas_setter;
static int save_ui_hidden;
static void save_restore_ui(void) {
    unsigned char enabled = 1;
    void *a[] = {&enabled};
    for (unsigned g = 0; g < 8; ++g) {
        PFRoots *r = &save_hidden_canvases[g];
        for (unsigned i = 0; i < r->n; ++i) {
            void *canvas = p_gchandle_get_target(r->h[i]);
            if (canvas)
                write_void(save_canvas_setter, canvas, a);
        }
        pf_release(r);
    }
    save_ui_hidden = 0;
    save_canvas_setter = 0;
}
static int save_hide_ui(void) {
    void *c = findclass(p_domain_get(), "UnityEngine", "Canvas");
    const char *bp[] = {"System.Boolean"};
    void *objectClass = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *tp[] = {"System.Type"};
    void *all = objectClass
                    ? pf_method(objectClass, "FindObjectsOfType", "UnityEngine.Object[]", 1, tp, 0)
                    : 0;
    void *canvasType = c ? p_type_get_object(p_class_get_type(c)) : 0;
    void *getter = c ? browser_method(c, "get_enabled", "System.Boolean", 0, 0, 1) : 0;
    save_canvas_setter = c ? browser_method(c, "set_enabled", "System.Void", 1, bp, 1) : 0;
    PFRoots roots = {0};
    int count = 0, ok = 0;
    void *array = 0;
    if (!canvasType || !pf_root(&roots, canvasType))
        goto done;
    void *typeArgs[] = {canvasType};
    array = pf_invoke(all, 0, typeArgs);
    if (!getter || !save_canvas_setter || !array || !pf_root(&roots, array) ||
        !write_array_count(array, &count) || count < 1 || count > 256)
        goto done;
    unsigned n = 0;
    unsigned char disabled = 0;
    void *a[] = {&disabled};
    for (int i = 0; i < count; ++i) {
        void *canvas = write_array_item(array, i);
        if (!canvas)
            goto done;
        void *box = pf_invoke(getter, canvas, 0);
        if (!box || !pf_class(p_object_get_class(box), "System.Boolean"))
            goto done;
        unsigned char *enabled = p_object_unbox(box);
        if (!enabled)
            goto done;
        if (!*enabled)
            continue;
        if (!pf_root(&save_hidden_canvases[n / 32], canvas))
            goto done;
        ++n;
        if (!write_void(save_canvas_setter, canvas, a))
            goto done;
    }
    save_ui_hidden = 1;
    ok = 1;
done:
    pf_release(&roots);
    if (!ok)
        save_restore_ui();
    return ok;
}
static int preview_capture_start(void);
static void preview_capture_stop(void);
static int preview_capture_choice(void);
static void preview_capture_hide_box(void);
static void save_result(const char *text) {
    preview_capture_stop();
    save_restore_ui();
    save_phase = 0;
    if (save_panel && browser_alive(save_panel) && save_station) {
        browser_active(save_panel, 1);
        save_open = 1;
    } else {
        pf_release(&save_station_roots);
        save_station = 0;
        save_open = 0;
    }
    save_message(text);
}
static void blueprint_save_tick(void) {
    if (!p_thread_current())
        return;
    UINT_PTR now = GetTickCount64();
    if (save_phase < 2 && now >= save_scan_due) {
        save_scan_due = now + 100;
        save_scan_ui();
        if (!save_open && save_owner &&
            save_ui_owner_expired(now, save_seen, browser_alive(save_owner))) {
            if (save_button_go && browser_alive(save_button_go))
                browser_active(save_button_go, 0);
            if (save_panel && browser_alive(save_panel))
                browser_active(save_panel, 0);
            save_open = 0;
            save_owner = 0;
        }
    }
    if (save_phase == 4) {
        int choice = preview_capture_choice();
        if (choice < 0) {
            preview_capture_stop();
            save_discard_stage();
            save_phase = 0;
            save_open = 1;
            browser_active(save_panel, 1);
            save_log("Preview capture cancelled; no template published.");
            return;
        }
        if (!choice)
            return;
        preview_capture_hide_box();
        save_phase = 2;
        save_due = now + 150;
    }
    if (save_phase == 2 && !save_ui_hidden) {
        if (!preview_capture_start()) {
            save_discard_stage();
            save_result(
                "Could not hide the game UI safely.\n\nNo library entry was published. Please try again.");
            return;
        }
        save_phase = 4;
        save_log("Game UI hidden; waiting for Preview Capture Save.");
        return;
    }
    if (save_phase == 2 && now >= save_due) {
        if (!isolated_capture(save_station, save_entity, save_photo)) {
            preview_capture_stop();
            save_restore_ui();
            save_discard_stage();
            save_result(
                "Isolated coaster preview could not be captured.\n\nNo library entry was published. Please check the mod log.");
            return;
        }
        save_phase = 3;
        save_due = now + 500;
        save_started = now;
        save_log("Isolated coaster image requested; waiting for a complete PNG.");
    }
    if (save_phase == 3 && now >= save_due) {
        save_restore_ui();
        save_due = now + 250;
        int result = save_finish_stage();
        if (result == 1) {
            preview_capture_stop();
            save_restore_ui();
            save_phase = 0;
            pf_release(&save_station_roots);
            save_station = 0;
            save_open = 0;
            browser_active(save_panel, 0);
            save_log("Template saved; save dialog closed.");
            save_log("COMPLETE: layout, name and screenshot published together.");
        } else if (result < 0 || now - save_started > 15000) {
            save_discard_stage();
            save_result(
                "Could not finish saving the screenshot.\n\nNo completed library entry was published.\n\nPlease try again.");
        }
    }
}
static void save_destroy_roots(PFRoots *r) {
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "Destroy", "System.Void", 1, p, 0) : 0;
    for (unsigned i = 0; i < r->n; ++i) {
        void *o = p_gchandle_get_target(r->h[i]);
        if (o && pf_class(p_object_get_class(o), "UnityEngine.GameObject") && browser_alive(o)) {
            browser_active(o, 0);
            void *a[] = {o};
            write_void(m, 0, a);
        }
    }
    pf_release(r);
}
#include "preview_capture.inc"
static void save_cleanup_ui(void) {
    if (!save_phase && !management_busy())
        preview_capture_cleanup();
    if (save_modal_registration) {
        write_void(browser_method(p_object_get_class(save_modal_registration), "Unregister",
                                  "System.Void", 0, 0, 1),
                   save_modal_registration, 0);
        save_modal_registration = 0;
    }
    save_popup_cleanup();
    save_ui_button_cleanup();
    save_destroy_roots(&save_input_roots);
    save_destroy_roots(&save_panel_roots);
    save_destroy_roots(&save_controls);
    save_owner = save_button_go = save_button = save_panel = save_status = save_input =
        save_cancel = save_confirm = 0;
    save_open = 0;
    if (!save_phase) {
        pf_release(&save_station_roots);
        save_station = 0;
    }
}
static __declspec(noinline) int save_construct(void *self, void *add) {
    int previousFailure = browser_failed;
    browser_failed = 0;
    void *label = pf_ref(add, p_object_get_class(add), "_childGraphic", "UnityEngine.UI.Graphic");
    void *font = label ? browser_optional(label, "get_font", "TMPro.TMP_FontAsset") : 0;
    if (!font)
        font = save_ui_font(self);
    const char *params[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *type = browser_type("UnityEngine", "Canvas");
    void *args[] = {type, &yes};
    void *m = browser_method(p_object_get_class(self), "GetComponentInParent",
                             "UnityEngine.Component", 2, params, 1);
    void *canvas = type ? pf_invoke(m, self, args) : 0;
    void *root = canvas ? browser_get(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *parent = root ? browser_get(root, "get_transform", "UnityEngine.Transform") : 0;
    if (!font || !parent) {
        browser_failed = previousFailure;
        return 0;
    }
    browser_current_roots = &save_controls;
    save_owner = self;
    if (!browser_hold(self) || !browser_hold(root) || !browser_hold(font)) {
        browser_current_roots = &browser_roots;
        browser_failed = previousFailure;
        save_cleanup_ui();
        return 0;
    }
    void *rc = findclass(p_domain_get(), "TPS.Core.Input", "GraphicRaycasters");
    save_modal_registration = rc ? p_object_new(rc) : 0;
    if (!save_modal_registration || !browser_hold(save_modal_registration) ||
        !write_void(browser_method(rc, ".ctor", "System.Void", 0, 0, 1), save_modal_registration,
                    0)) {
        browser_current_roots = &browser_roots;
        browser_failed = previousFailure;
        save_cleanup_ui();
        return 0;
    }
    save_popup_build(parent, font);
    preview_capture_ensure(parent, font);
    void *panelTransform = browser_get(save_panel, "get_transform", "UnityEngine.Transform");
    browser_current_roots = &save_input_roots;
    void *field = browser_make("TPM template name", panelTransform, 0, 24, 820, 68, .5f, .5f);
    BrowserColor inputColor = {1, .98f, .89f, 1};
    void *background = browser_image(field, inputColor);
    save_popup_apply_surface(background, &save_popup_input_style);
    void *viewport = browser_make("Coaster name text viewport",
                                  browser_get(field, "get_transform", "UnityEngine.Transform"), 0,
                                  0, 772, 60, .5f, .5f);
    browser_add(viewport, "UnityEngine.UI", "RectMask2D");
    browser_active(viewport, 1);
    void *viewportTransform = browser_get(viewport, "get_transform", "UnityEngine.Transform");
    void *text = browser_label(viewportTransform, font, "", 0, 0, 772, 60, 32);
    save_input = browser_add(field, "TMPro", "TMP_InputField");
    browser_set(save_input, "set_textComponent", "TMPro.TMP_Text", text);
    browser_set(save_input, "set_textViewport", "UnityEngine.RectTransform", viewportTransform);
    browser_set(save_input, "set_targetGraphic", "UnityEngine.UI.Graphic", background);
    int limit = 100, lineType = 0;
    browser_set(save_input, "set_characterLimit", "System.Int32", &limit);
    browser_set(save_input, "set_lineType", "TMPro.TMP_InputField.LineType", &lineType);
    browser_active(field, 1);
    browser_active(save_panel, 0);
    browser_current_roots = &browser_roots;
    int failed = browser_failed;
    browser_failed = previousFailure;
    if (failed) {
        save_cleanup_ui();
        return 0;
    }
    save_log("READY: native yellow Save Template beside the active coaster action.");
    return 1;
}
static int save_station_identity(void *station, unsigned long long *id, unsigned *entity,
                                 long long *definition) {
    if (!station || !g_managed_fields_verified || !g_gameitem_class ||
        !p_class_is_assignable_from(g_gameitem_class, p_object_get_class(station)))
        return 0;
    p_field_get_value(station, g_id_field, id);
    p_field_get_value(station, g_entity_field, entity);
    *definition = export_station_definition_id(station);
    return *id && entity[0] && entity[1] && *definition;
}
/* Inspector ratings are read from the native rating-to-widget mapping, never
 * inferred from screen order or a different coaster's inactive setup popup. */
static void save_capture_overview_stats(void *station) {
    /* On-demand enumeration avoids stale UI discovery caches without adding idle work. */
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *tp[] = {"System.Type"};
    void *t = browser_type("TPS.Game.UI", "InspectorPageExhibitRideOverview");
    void *ta[] = {t};
    void *array =
        t ? pf_invoke(browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, tp, 0),
                      0, ta)
          : 0;
    int count = 0;
    if (!array || !pf_root(&roots, array) || !write_array_count(array, &count)) {
        pf_release(&roots);
        return;
    }
    for (int i = 0; i < count; ++i) {
        void *panel = write_array_item(array, i);
        if (!panel || !browser_alive(panel) ||
            !funfair_property_true(panel, "get_isActiveAndEnabled"))
            continue;
        if (pf_ref(panel, p_object_get_class(panel), "_item", "TPS.Game.GameItemInstance") !=
            station)
            continue;
        void *stats = pf_ref(panel, p_object_get_class(panel), "_widgetExhibitStats",
                             "TPS.Game.WidgetExhibitStats");
        if (!stats)
            continue;
        void *rc = findclass(p_domain_get(), "TPS.Game", "RatingType");
        const char *params[] = {"TPS.Game.RatingType"};
        void *method = browser_method(p_object_get_class(stats), "GetWidget",
                                      "TPS.Game.UI.WidgetTextWithIcon", 1, params, 1);
        const char *names[] = {"", "Buzz", "Thrill", "Ferocity", "Nausea"};
        for (unsigned k = 1; k < 5; ++k) {
            if (save_display_stats.value[k][0])
                continue;
            void *f = rc ? p_class_get_field_from_name(rc, names[k]) : 0;
            if (!f || !(p_field_get_flags(f) & 0x10) ||
                !pf_type(p_field_get_type(f), "TPS.Game.RatingType"))
                continue;
            int rating = 0;
            p_field_static_get_value(f, &rating);
            void *a[] = {&rating};
            void *widget = pf_invoke(method, stats, a);
            void *text =
                widget ? pf_ref(widget, p_object_get_class(widget), "_text", "TMPro.TMP_Text") : 0;
            void *value = text ? browser_optional(text, "get_text", "System.String") : 0;
            char utf8[512];
            if (value && save_utf8(value, utf8, sizeof(utf8)))
                blueprint_stat_text(utf8, save_display_stats.value[k]);
        }
        save_log("Inspector display ratings read for exact saved coaster.");
        break;
    }
    pf_release(&roots);
}
/* Use the game's own localised thresholds for both save entry points and old display metadata. */
static void blueprint_label_stats(BlueprintDisplayStats *stats) {
    float rating[2];
    char numbers[2][32];
    int raw[2] = {blueprint_rating_number(stats->value[1], &rating[0], numbers[0]),
                  blueprint_rating_number(stats->value[2], &rating[1], numbers[1])};
    if (!raw[0] && !raw[1])
        return;
    int old = browser_failed;
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Resources");
    const char *tp[] = {"System.Type"};
    void *t = browser_type("TPS.Game", "TrackRidesConfig");
    void *a[] = {t};
    void *all =
        t ? pf_invoke(browser_method(c, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, tp, 0),
                      0, a)
          : 0;
    int count = 0;
    if (all && pf_root(&roots, all) && write_array_count(all, &count) && count == 1) {
        void *config = write_array_item(all, 0);
        const char *methods[] = {"GetBuzzRatingString", "GetThrillRatingString"};
        const char *fp[] = {"System.Single"};
        for (unsigned k = 0; k < 2; ++k)
            if (raw[k]) {
                void *fa[] = {&rating[k]};
                void *label = pf_invoke(browser_method(p_object_get_class(config), methods[k],
                                                       "System.String", 1, fp, 1),
                                        config, fa);
                char text[256], combined[300];
                if (!label || !save_utf8(label, text, sizeof(text)))
                    continue;
                unsigned n = 0;
                for (unsigned i = 0; text[i] && n < 250; ++i)
                    combined[n++] = text[i];
                combined[n++] = ' ';
                combined[n++] = '(';
                for (unsigned i = 0; numbers[k][i]; ++i)
                    combined[n++] = numbers[k][i];
                combined[n++] = ')';
                combined[n] = 0;
                char normalized[48];
                if (blueprint_stat_text(combined, normalized))
                    memcpy(stats->value[k + 1], normalized, 48);
            }
    }
    pf_release(&roots);
    browser_failed = old;
}
static void save_capture_display_stats(void *station) {
    memset(&save_display_stats, 0, sizeof(save_display_stats));
    int previous = browser_failed;
    PFRoots roots = {0};
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"System.Type"};
    void *t = browser_type("TPS.Game", "TrackRideSetupUI");
    void *a[] = {t};
    void *array =
        t ? pf_invoke(c ? pf_method(c, "FindObjectsOfType", "UnityEngine.Object[]", 1, p, 0) : 0, 0,
                      a)
          : 0;
    int count = 0, matched = 0;
    if (array && pf_root(&roots, array) && write_array_count(array, &count))
        for (int i = 0; i < count; ++i) {
            void *panel = write_array_item(array, i);
            if (!panel || !browser_alive(panel) ||
                !funfair_property_true(panel, "get_isActiveAndEnabled"))
                continue;
            void *source =
                pf_ref(panel, p_object_get_class(panel), "_station", "TPS.Game.GameItemInstance");
            if (source != station)
                continue;
            if (++matched > 1) {
                memset(&save_display_stats, 0, sizeof(save_display_stats));
                break;
            }
            const char *fields[] = {"_widgetCost", "_buzzRating", "_thrillRating",
                                    "_ferocityRating", "_nauseaRating"};
            for (unsigned k = 1; k < 5; ++k) {
                void *widget =
                    pf_ref(panel, p_object_get_class(panel), fields[k],
                           k ? "TPS.Game.UI.WidgetStarRating" : "TPS.Game.UI.WidgetTextAndMoney");
                void *label = widget ? pf_ref(widget, p_object_get_class(widget),
                                              k ? "_textRating" : "_textMoney", "TMPro.TMP_Text")
                                     : 0;
                void *text = label ? browser_optional(label, "get_text", "System.String") : 0;
                char value[512];
                if (text && save_utf8(text, value, sizeof(value)))
                    blueprint_stat_text(value, save_display_stats.value[k]);
            }
        }
    save_capture_overview_stats(station);
    blueprint_label_stats(&save_display_stats);
    /* Setup's cost widget displays only the edit balance. Capture the total before
   * the game's Refresh subtracts the already-paid amount. This is a read-only query. */
    void *utils = findclass(p_domain_get(), "TPS.Game", "TrackRideUtils");
    const char *costParams[] = {"Unity.Entities.Entity"};
    void *costMethod =
        utils ? pf_method(utils, "CalculateTrackRideCost", "System.Int32", 1, costParams, 0) : 0;
    unsigned entity[2] = {save_entity[0], save_entity[1]};
    void *costArgs[] = {entity};
    void *cost = entity[0] && entity[1] ? pf_invoke(costMethod, 0, costArgs) : 0;
    if (cost && pf_class(p_object_get_class(cost), "System.Int32"))
        blueprint_cost_text(*(int *)p_object_unbox(cost), save_display_stats.value[0]);
    pf_release(&roots);
    browser_failed = previous;
    save_log(matched == 1
                 ? "DISPLAY STATS: captured from matching native coaster setup panel"
                 : "DISPLAY STATS: exact coaster panel unavailable; unknown fields kept null");
    for (unsigned i = 0; i < 5; ++i) {
        logtext("SAVE TEMPLATE: ");
        logtext(blueprint_stat_keys[i]);
        logtext("=");
        logtext(save_display_stats.value[i][0] ? save_display_stats.value[i] : "?");
        nl();
    }
}
/* On-demand capture for the Info Menu: the native editing cache can be empty.
 * Bind the game's existing context to this exact station/entity, call only the
 * reviewed snapshot collector, preserve the result in rooted managed objects,
 * then restore the previous editing cache BEFORE exporting. No revert event,
 * geometry rebuild or edit-mode transition is dispatched. */
static __declspec(noinline) void save_capture_live_track(void *station, const unsigned *entity) {
    PFRoots roots = {0};
    void *bus = 0;
    void *snapshot = 0;
    void *field = 0;
    void *old = 0;
    void *data = 0;
    unsigned char flag = 0;
    int captured = 0;
    if (write_pending || !g_snapshot_reader_verified || !g_snapshot_flag_verified)
        return;
    void *args[] = {(void *)entity};
    void *exists = pf_invoke(g_exists_method, 0, args);
    void *value = exists ? p_object_unbox(exists) : 0;
    if (!value || *(unsigned char *)value != 1 || !pf_root(&roots, station))
        goto done;
    void *context = write_context(station, entity, &roots, &bus);
    if (!context)
        goto done;
    void *method = pf_method(p_object_get_class(context), "<SetComponents>g__Snapshot|30",
                             "System.Void", 0, 0, 1);
    if (!write_code_guard(method, 0x2ea8180, 0x778, WRITE_SNAPSHOT_HASH))
        goto done;
    snapshot = pf_invoke(g_snapshot_reader, 0, args);
    if (!snapshot || p_object_get_class(snapshot) != g_snapshot_class || !pf_root(&roots, snapshot))
        goto done;
    field = pf_field(g_snapshot_class, "SnapshotData", "TPS.Game.TrackRideSnapshotData");
    if (!field || !write_value_layout(p_class_from_type(p_field_get_type(field)), 16))
        goto done;
    old = write_snapshot_data(snapshot, field, &roots);
    p_field_get_value(snapshot, g_snapshot_flag_field, &flag);
    if (!old || flag > 1)
        goto done;
    save_log("Capturing selected coaster from live track; preserving editing cache.");
    captured = write_void(method, context, 0);
    if (captured)
        data = write_snapshot_data(snapshot, field, &roots);
    /* Restore even when the collector throws or rooting its result fails. */
    if (!write_restore_snapshot(snapshot, field, old, flag)) {
        save_log("Live capture stopped: editing cache restoration could not be verified.");
        goto done;
    }
    unsigned now[2] = {0};
    p_field_get_value(station, g_entity_field, now);
    int supports = 0, pieces = 0;
    void *dc = p_class_from_type(p_field_get_type(field));
    if (!captured || !data || !pf_same_entity(now, entity) ||
        !write_counts(data, dc, &supports, &pieces) || supports <= 0 || pieces <= 0)
        goto done;
    void *list = pf_ref(data, dc, "TrackPieceData",
                        "System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>");
    unsigned decorated = 0;
    const char *ip[] = {"System.Int32"};
    void *get = list ? pf_method(p_object_get_class(list), "get_Item",
                                 "TPS.Game.TrackPieceSnapshotData", 1, ip, 1)
                     : 0;
    for (int index = 0; get && index < pieces; ++index) {
        PFRoots recordRoots = {0};
        void *args[] = {&index};
        void *piece = pf_invoke(get, list, args);
        if (piece && pf_root(&recordRoots, piece)) {
            long long decoration = 0;
            void *f = pf_field(p_object_get_class(piece), "TrackPieceDecorationDefinition",
                               "TPS.Game.GameItemDefinitionID");
            if (f && write_value_layout(p_class_from_type(p_field_get_type(f)), 8)) {
                p_field_get_value(piece, f, &decoration);
                if (decoration)
                    ++decorated;
            }
        }
        pf_release(&recordRoots);
    }
    logtext("SAVE TEMPLATE: live snapshot decorated track sections=");
    lognum(decorated);
    nl();
    save_log("Live track captured and editing cache restored; exporting selected coaster.");
    export_blueprint(data, dc, station);
done:
    pf_release(&roots);
}
static __declspec(noinline) void save_begin(void *current) {
    save_log("Save confirmation received; validating selected coaster.");
    if (library_busy() || save_phase || !save_station || current != save_station) {
        save_message("Select the same completed coaster and wait for any build to finish.");
        return;
    }
    unsigned long long id = 0;
    unsigned entity[2] = {0};
    long long definition = 0;
    if (!save_station_identity(current, &id, entity, &definition) || id != save_source ||
        !pf_same_entity(entity, save_entity) || definition != save_definition) {
        save_message("The selected coaster changed. Close and reopen Save Template.");
        return;
    }
    void *name = pf_invoke(
        browser_method(p_object_get_class(save_input), "get_text", "System.String", 0, 0, 1),
        save_input, 0);
    if (!save_utf8(name, save_name, sizeof(save_name)) || !save_normalize_name(save_name)) {
        save_message("Enter a name containing 1 to 100 printable characters.");
        return;
    }
    /* Preserve only missing fields from this dialog's exact frozen station.
   * Fresh values take priority; a new dialog always replaces this snapshot. */
    if (!coaster_colour_capture(current, save_entity, &colour_saved)) {
        save_message(
            "Could not read this coaster colour. Close and reopen Save Template, then try again.");
        return;
    }
    save_capture_curve(current);
    BlueprintDisplayStats openedStats = save_display_stats;
    save_capture_display_stats(current);
    for (unsigned k = 1; k < 5; ++k)
        if (!save_display_stats.value[k][0] && openedStats.value[k][0])
            memcpy(save_display_stats.value[k], openedStats.value[k], 48);
    save_phase = 1;
    save_document_size = 0;
    blueprint_save_export_request = 1;
    verify_support_orientation();
    /* Editing snapshots can predate decoration changes. Always collect current
     * live track, preserving/restoring the original native cancellation cache. */
    save_capture_live_track(current, entity);
    blueprint_save_export_request = 0;
    if (save_phase == 1) {
        save_phase = 0;
        if (!save_document_size)
            save_message(
                "No usable coaster snapshot was found.\n\nComplete the track and try again.");
    }
}
static int save_modal_on_active_canvas(void *owner) {
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1, no = 0;
    void *t = browser_type("UnityEngine", "Canvas");
    void *a[] = {t, &yes};
    void *canvas = pf_invoke(browser_method(p_object_get_class(owner), "GetComponentInParent",
                                            "UnityEngine.Component", 2, p, 1),
                             owner, a);
    void *root = canvas ? browser_get(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *parent = root ? browser_get(root, "get_transform", "UnityEngine.Transform") : 0;
    void *go = parent ? browser_get(parent, "get_gameObject", "UnityEngine.GameObject") : 0;
    if (!go || !funfair_property_true(go, "get_activeInHierarchy"))
        return 0;
    void *rect = browser_get(save_panel, "get_transform", "UnityEngine.Transform");
    const char *sp[] = {"UnityEngine.Transform", "System.Boolean"};
    void *sa[] = {parent, &no};
    if (!write_void(browser_method(p_object_get_class(rect), "SetParent", "System.Void", 2, sp, 1),
                    rect, sa))
        return 0;
    BrowserV2 center = {.5f, .5f}, zero = {0, 0};
    BrowserV3 scale = {1, 1, 1};
    browser_set(rect, "set_anchorMin", "UnityEngine.Vector2", &center);
    browser_set(rect, "set_anchorMax", "UnityEngine.Vector2", &center);
    browser_set(rect, "set_anchoredPosition", "UnityEngine.Vector2", &zero);
    browser_set(rect, "set_localScale", "UnityEngine.Vector3", &scale);
    const char *gp[] = {"System.Type"};
    void *ga[] = {t};
    void *overlay = pf_invoke(browser_method(p_object_get_class(save_panel), "GetComponent",
                                             "UnityEngine.Component", 1, gp, 1),
                              save_panel, ga);
    void *layer = browser_get(root, "get_sortingLayerID", "System.Int32");
    void *order = browser_get(root, "get_sortingOrder", "System.Int32");
    int above = order ? *(int *)p_object_unbox(order) + 200 : 200;
    if (layer)
        browser_set(overlay, "set_sortingLayerID", "System.Int32", p_object_unbox(layer));
    browser_set(overlay, "set_sortingOrder", "System.Int32", &above);
    /* Native input keeps its own raycaster collection. Rebind after reparenting
   * so the inspector's modal controls participate in the same input route. */
    if (!overlay || !save_modal_registration)
        return 0;
    write_void(browser_method(p_object_get_class(save_modal_registration), "Unregister",
                              "System.Void", 0, 0, 1),
               save_modal_registration, 0);
    const char *rp[] = {"UnityEngine.GameObject"};
    void *ra[] = {save_panel};
    if (!write_void(browser_method(p_object_get_class(save_modal_registration), "Register",
                                   "System.Void", 1, rp, 1),
                    save_modal_registration, ra))
        return 0;
    return 1;
}
static void save_open_for_station(void *station, void *owner) {
    unsigned long long id = 0;
    unsigned entity[2] = {0};
    long long definition = 0;
    if (save_phase || library_busy() || !save_station_identity(station, &id, entity, &definition))
        return;
    if (!save_modal_on_active_canvas(owner)) {
        save_log("Save dialog could not attach to the visible canvas.");
        return;
    }
    save_popup_refresh_paper();
    pf_release(&save_station_roots);
    if (!pf_root(&save_station_roots, station))
        return;
    save_station = station;
    save_source = id;
    save_definition = definition;
    memcpy(save_entity, entity, 8);
    save_capture_display_stats(station);
    void *name = pf_invoke(
        browser_method(p_object_get_class(station), "get_DisplayName", "System.String", 0, 0, 1),
        station, 0);
    if (!save_utf8(name, save_coaster_name, sizeof(save_coaster_name)) ||
        !save_normalize_name(save_coaster_name))
        library_copy(save_coaster_name, sizeof(save_coaster_name), "Coaster");
    browser_set(save_input, "set_text", "System.String", p_string_new(save_coaster_name));
    save_message("Coaster Name");
    browser_active(save_panel, 1);
    save_open = 1;
    save_dialog_opened = GetTickCount64();
    unsigned char off = 0;
    browser_set(save_confirm, "SetIsOnWithoutNotify", "System.Boolean", &off);
    browser_set(save_cancel, "SetIsOnWithoutNotify", "System.Boolean", &off);
    browser_set(save_close, "SetIsOnWithoutNotify", "System.Boolean", &off);
}

/* Poll native panels independently of editing callbacks. Only exact station/item
 * references from the currently visible panel are accepted. */
static void save_scan_ui(void) {
    long long perf_started = perf_now();
    int previous = browser_failed;
    PFRoots roots = {0};
    void *chosen = 0;
    const char *ns[] = {"TPS.Game", "TPS.Game", "TPS.Game.UI"};
    const char *names[] = {"TrackRideSetupUI", "WidgetTrackRide",
                           "InspectorPageExhibitRideOverview"};
    const char *fields[] = {"_confirmButton", "_completeButton", "_editTrackButton"};
    const char *items[] = {"_station", "_station", "_item"};
    /* Do not retain a five-second negative discovery result: inspectors are
     * instantiated on demand. One type per 100ms service tick bounds scanning;
     * each absent/destroyed type is retried within three ticks. Live arrays are
     * still reused, and controls are prepared only for a visible valid station. */
    UINT_PTR now = GetTickCount64();
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        unsigned k = (save_discovery_cursor + attempt) % 3;
        if (now < save_discovery_due[k])
            continue;
        save_discovery_cursor = (k + 1) % 3;
        save_discovery_due[k] = now + 250;
        int n = 0, complete = save_cached_panels[k] &&
                              write_array_count(save_cached_panels[k], &n) && n > 0;
        for (int i = 0; complete && i < n; ++i)
            if (!browser_alive(write_array_item(save_cached_panels[k], i)))
                complete = 0;
        if (!complete) {
            /* These are global Unity object searches. The original 250ms negative
             * retry interval ran them ~10 times/s while playing normally.
             * A missing panel can be created later, so retry every 2 seconds,
             * without pinning an indefinite negative cache. Once a panel is
             * present, the cheap cached-panel path remains responsive. */
            save_discovery_due[k] = now + 2000;
            pf_release(&save_discovery_roots[k]);
            save_cached_panels[k] = 0;
            void *c = findclass(p_domain_get(), "UnityEngine", "Object");
            const char *p[] = {"System.Type", "System.Boolean"};
            unsigned char yes = 1;
            void *m = c ? pf_method(c, "FindObjectsOfType", "UnityEngine.Object[]", 2, p, 0) : 0;
            void *t = browser_type(ns[k], names[k]);
            void *a[] = {t, &yes};
            if (t && m)
                ++save_perf_global_scans;
            void *array = t ? pf_invoke(m, 0, a) : 0;
            if (array && pf_root(&save_discovery_roots[k], array))
                save_cached_panels[k] = array;
        }
        break;
    }
    for (unsigned k = 0; k < 3; ++k) {
        void *array = save_cached_panels[k];
        int count = 0;
        void *panelChoice = 0, *station = 0, *donor = 0;
        if (array && pf_root(&roots, array) && write_array_count(array, &count))
            for (int i = 0; i < count; ++i) {
                void *panel = write_array_item(array, i);
                if (!panel || !browser_alive(panel) ||
                    (k != 1 && !funfair_property_true(panel, "get_isActiveAndEnabled")))
                    continue;
                void *b =
                    pf_ref(panel, p_object_get_class(panel), fields[k], "TPS.Core.UI.UIButton");
                void *g = b ? browser_optional(b, "get_gameObject", "UnityEngine.GameObject") : 0;
                /* Construction refreshes may temporarily hide its confirm donor while the
       * panel remains active. Save owns its visibility and validates snapshots
       * independently; the donor supplies placement geometry, not availability. */
                if (!g || (k != 0 && !funfair_property_true(g, "get_activeInHierarchy")))
                    continue;
                void *item =
                    pf_ref(panel, p_object_get_class(panel), items[k], "TPS.Game.GameItemInstance");
                if (k == 2) {
                    unsigned char coaster = 0;
                    void *f =
                        pf_field(p_object_get_class(panel), "_isRollercoaster", "System.Boolean");
                    if (f)
                        p_field_get_value(panel, f, &coaster);
                    if (!coaster)
                        continue;
                }
                if (!item)
                    continue;
                panelChoice = panel;
                station = item;
                donor = b;
                break;
            }
        if (panelChoice) {
            if (!chosen)
                chosen = panelChoice;
            if (!save_panel || !browser_alive(save_panel)) {
                save_cleanup_ui();
                if (!save_construct(panelChoice, donor))
                    continue;
            }
            if (save_ui_slot(k, panelChoice, donor) && browser_pressed(save_slot_toggle[k])) {
                /* Log only a real toggle activation. No per-frame log overhead.
                 * Distinguishes an input/raycaster failure from a station/save failure. */
                save_log("BUTTON PRESS received by selected panel");
                logtext("SAVE TEMPLATE: pressed_slot=");
                lognum(k);
                nl();
                if (!save_open)
                    save_open_for_station(station, donor);
                else
                    save_log("BUTTON PRESS ignored: save dialog already open");
            }
        } else if (save_ui_go[k] && browser_alive(save_ui_go[k]))
            browser_active(save_ui_go[k], 0);
    }
    if (chosen) {
        save_owner = chosen;
        save_seen = GetTickCount64();
        save_ui_hover_update();
    } else
        save_ui_hover_reset();
    /* The dialog owns a rooted station; inspector visibility must not dismiss it. */
    save_popup_hover();
    if (save_open && (browser_pressed(save_cancel) || browser_pressed(save_close))) {
        browser_active(save_panel, 0);
        save_open = 0;
        pf_release(&save_station_roots);
        save_station = 0;
    }
    if (save_open && GetTickCount64() - save_dialog_opened >= 250 && browser_pressed(save_confirm))
        save_begin(save_station);
    pf_release(&roots);
    browser_failed = previous;
    {
        UINT_PTR us = perf_us(perf_started);
        ++save_perf_calls;
        save_perf_total_us += us;
        if (us > save_perf_peak_us)
            save_perf_peak_us = us;
    }
}

#undef findclass
#undef browser_method
#undef browser_get
#undef browser_optional
#undef browser_set
#undef browser_alive
#undef funfair_property_true
#undef browser_pressed

#undef browser_type
#undef write_array_count
#undef write_array_item
