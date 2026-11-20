/* Local library management. Downloaded Workshop entries are read-only.
 * Fixed filenames only; no recursive removal and no backup copies. */
static PFRoots management_tools, management_modal, management_input_roots;
static void *management_rename, *management_retake, *management_delete, *management_panel,
    *management_status, *management_input, *management_input_go, *management_cancel,
    *management_confirm;
static int management_mode, management_photo_owned;
static int management_writable_state = -1;
static UINT_PTR management_due, management_started;
static LibraryEntry management_entry;
static WCHAR management_photo[1024];
static unsigned char management_metadata[LIBRARY_METADATA_LIMIT + 1],
    management_replacement[LIBRARY_METADATA_LIMIT + 4096];
static unsigned management_metadata_size;
#include "delete_confirmation.inc"
static int workshop_busy(void);
static void workshop_tick(void);
static int management_dialog_open(void) {
    return management_mode != 0;
}
static int management_busy(void) {
    return management_mode != 0 || workshop_busy();
}
static int management_directory(const LibraryEntry *e, WCHAR *out) {
    for (unsigned i = 0; i < 32; ++i) {
        unsigned c = e->folder[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
    }
    if (e->folder[32] || e->readOnly)
        return 0;
    DWORD attr = GetFileAttributesW(library_root);
    if (attr == 0xffffffffU || !(attr & 0x10) || (attr & 0x400))
        return 0;
    unsigned n = 0;
    if (!append(out, 1024, &n, library_root) || !append(out, 1024, &n, L"\\") ||
        !append(out, 1024, &n, e->folder))
        return 0;
    attr = GetFileAttributesW(out);
    return attr != 0xffffffffU && (attr & 0x10) && !(attr & 0x400);
}
static unsigned management_validate(const LibraryEntry *e) {
    static WCHAR directory[1024], path[1024];
    if (!management_directory(e, directory) || !library_path(path, e->folder, L"entry.json"))
        return 0;
    unsigned n = library_read(path, management_metadata, LIBRARY_METADATA_LIMIT),
             root = n ? lj_parse(management_metadata, n) : 0;
    long long schema = 0, type = 0;
    char id[33];
    for (unsigned i = 0; i < 32; ++i)
        id[i] = (char)e->folder[i];
    id[32] = 0;
    if (!root || !lj_integer(root, "schemaVersion", &schema) || schema != 1 ||
        !lj_is(lj_member(root, "id"), id) || library_metadata_readonly(root) ||
        !lj_int(lj_member(root, "stationDefinitionId"), &type) || type != e->definition ||
        !lj_is(lj_member(root, "planSha256"), e->sha) || !lj_is(lj_member(root, "name"), e->name) ||
        !lj_is(lj_member(root, "screenshot"), e->screenshot))
        return 0;
    management_metadata_size = n;
    return root;
}
#include "coaster_package_export.inc"
#include "coaster_package_import.inc"
static int management_write_temp(const WCHAR *path, const unsigned char *data, unsigned size) {
    HANDLE file = CreateFileW(path, 0x40000000, 0, 0, 1, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    DWORD written = 0;
    int ok = WriteFile(file, data, size, &written, 0) && written == size;
    if (ok)
        ok = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file))
        ok = 0;
    if (!ok)
        DeleteFileW(path);
    return ok;
}
static int management_replace_field(const LibraryEntry *e, const char *field, const char *value) {
    unsigned root = management_validate(e), token = root ? lj_member(root, field) : 0;
    if (!token || lj_tokens[token].kind != 3)
        return 0;
    unsigned start = lj_tokens[token].rawStart, end = lj_tokens[token].rawEnd,
             n = management_metadata_size;
    if (end <= start || end > n)
        return 0;
    jpos = 0;
    jok = 1;
    save_json_string(value);
    if (!jok || start + jpos + n - end >= sizeof(management_replacement))
        return 0;
    memcpy(management_replacement, management_metadata, start);
    memcpy(management_replacement + start, jbuf, jpos);
    memcpy(management_replacement + start + jpos, management_metadata + end, n - end);
    unsigned bytes = start + jpos + n - end;
    if (bytes > LIBRARY_METADATA_LIMIT)
        return 0;
    static WCHAR temp[1024], target[1024];
    if (!library_path(temp, e->folder, L".entry-update.tmp") ||
        !library_path(target, e->folder, L"entry.json"))
        return 0;
    if (!management_write_temp(temp, management_replacement, bytes))
        return 0;
    /* The current metadata must still match the entry immediately before replace. */
    int ok = management_validate(e) != 0 && MoveFileExW(temp, target, 1 | 8);
    if (!ok)
        DeleteFileW(temp);
    return ok;
}
#include "workshop_upload.inc"

static int management_rename_entry(const LibraryEntry *e, const char *name) {
    /* Validate UTF-8 using the same strict JSON reader as library names. */
    char normalized[401];
    if (!library_copy(normalized, sizeof(normalized), name) || !save_normalize_name(normalized))
        return 0;
    jpos = 0;
    jok = 1;
    save_json_string(normalized);
    if (!jok || !lj_parse((unsigned char *)jbuf, jpos))
        return 0;
    return management_replace_field(e, "name", normalized);
}
static int management_known_file(const WCHAR *name) {
    const WCHAR *allowed[] = {L"entry.json", L"blueprint.json", L"preview.png", L"preview.jpg"};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned j = 0;
        while (name[j] && name[j] == allowed[i][j])
            ++j;
        if (!name[j] && !allowed[i][j])
            return 1;
    }
    return 0;
}
static int management_delete_entry(const LibraryEntry *e) {
    if (e->source == 2)
        return 0;
    if (e->source == 1)
        return coaster_manual_delete(e);
    if (!management_validate(e))
        return 0;
    static WCHAR directory[1024], pattern[1024], path[1024];
    if (!management_directory(e, directory))
        return 0;
    unsigned n = 0;
    if (!append(pattern, 1024, &n, directory) || !append(pattern, 1024, &n, L"\\*"))
        return 0;
    LibraryFindData fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    int safe = 1;
    do {
        if (fd.name[0] == '.' && (!fd.name[1] || (fd.name[1] == '.' && !fd.name[2])))
            continue;
        if ((fd.attributes & (0x10 | 0x400)) || !management_known_file(fd.name)) {
            safe = 0;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!safe || !management_validate(e))
        return 0;
    const WCHAR *files[] = {L"preview.png", L"preview.jpg", L"blueprint.json", L"entry.json"};
    for (unsigned i = 0; i < 4; ++i) {
        if (!library_path(path, e->folder, files[i]))
            return 0;
        DWORD attr = GetFileAttributesW(path);
        if (attr == 0xffffffffU)
            continue;
        if (attr & (0x10 | 0x400) || !DeleteFileW(path))
            return 0;
    }
    return RemoveDirectoryW(directory) != 0;
}
static int management_start_photo(const LibraryEntry *e) {
    if (!management_validate(e) ||
        !library_path(management_photo, e->folder, L".preview-update.png"))
        return 0;
    /* Reserve a new capture file, never overwrite another operation's file. */
    if (!management_write_temp(management_photo, (const unsigned char *)"", 0))
        return 0;
    management_photo_owned = 1;
    return 1;
}
static PFRoots management_target_roots;
static void *management_target;
static unsigned management_target_entity[2];
static int management_probe_only, management_pick_mouse;
static void management_target_clear(void) {
    pf_release(&management_target_roots);
    management_target = 0;
    memset(management_target_entity, 0, sizeof(management_target_entity));
}
static int management_target_set(void *station) {
    unsigned long long id = 0;
    long long definition = 0;
    unsigned entity[2] = {0};
    if (!save_station_identity(station, &id, entity, &definition) ||
        definition != management_entry.definition)
        return 0;
    management_target_clear();
    if (!pf_root(&management_target_roots, station))
        return 0;
    management_target = station;
    memcpy(management_target_entity, entity, sizeof(entity));
    return 1;
}
static void management_target_prompt(void) {
    browser_set(preview_capture_title, "set_text", "System.String",
                p_string_new(management_target ? "Preview Capture" : "Click coaster station"));
    float size = management_target ? 48.f : 32.f;
    browser_set(preview_capture_title, "set_fontSize", "System.Single", &size);
    unsigned char enabled = (unsigned char)(management_target != 0);
    browser_set(preview_capture_capture, "set_interactable", "System.Boolean", &enabled);
}
#include "retake_ray.h"
/* Visual fallback: station render bounds remain available when a menu or
 * quick-load leaves native selection colliders disabled/unregistered. */
static float management_station_ray(void *visual, void *ray) {
    PFRoots roots = {0};
    float nearest = 10001.f;
    void *go = browser_optional(visual, "get_GameObject", "UnityEngine.GameObject");
    if (!go || !pf_root(&roots, go))
        goto done;
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *type = browser_type("UnityEngine", "Renderer");
    void *ca[] = {type, &yes};
    void *array = type ? pf_invoke(browser_method(p_object_get_class(go), "GetComponentsInChildren",
                                                  "UnityEngine.Component[]", 2, cp, 1),
                                   go, ca)
                       : 0;
    int count = 0;
    if (!array || !pf_root(&roots, array) || !write_array_count_limit(array, &count, 4096))
        goto done;
    for (int i = 0; i < count; ++i) {
        void *renderer = write_array_item(array, i);
        if (!renderer || !browser_alive(renderer) ||
            !funfair_property_true(renderer, "get_enabled"))
            continue;
        void *object = browser_optional(renderer, "get_gameObject", "UnityEngine.GameObject");
        if (!object || !funfair_property_true(object, "get_activeInHierarchy"))
            continue;
        void *bounds = browser_optional(renderer, "get_bounds", "UnityEngine.Bounds");
        PFRoots temporary = {0};
        if (!bounds || !pf_root(&temporary, bounds))
            continue;
        if (write_value_layout(p_object_get_class(bounds), 24)) {
            float distance = management_ray_bounds((const float *)p_object_unbox(ray),
                                                   (const float *)p_object_unbox(bounds));
            if (distance < nearest)
                nearest = distance;
        }
        pf_release(&temporary);
    }
done:
    pf_release(&roots);
    return nearest;
}
/* Match the game's registered selection colliders, including trigger volumes.
 * A track/terrain collider in front must not hide a station hit farther along
 * the same ray. No native selection or gameplay state is changed. */
static void management_pick_station(void) {
    int mouse = (GetAsyncKeyState(1) & 0x8000) != 0;
    int clicked = mouse && !management_pick_mouse;
    management_pick_mouse = mouse;
    if (management_target || !clicked)
        return;
    const char *result = "RETAKE PICK: mouse position unavailable";
    PFRoots roots = {0};
    void *app = findclass(p_domain_get(), "TPS.Core", "App");
    void *instance = pf_invoke(browser_method(app, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0);
    void *input =
        instance ? browser_optional(instance, "get_InputManager", "TPS.Core.Input.InputManager")
                 : 0;
    void *point = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (!point || !p_object_unbox(point))
        goto done;
    BrowserV2 screen = *(BrowserV2 *)p_object_unbox(point);
    /* The independent overlay canvas may be camera-space. Use its actual
     * camera, rather than treating its world-space rect as screen pixels. */
    void *canvasRoot =
        browser_optional(preview_capture_canvas, "get_rootCanvas", "UnityEngine.Canvas");
    void *mode =
        canvasRoot ? browser_optional(canvasRoot, "get_renderMode", "UnityEngine.RenderMode") : 0;
    result = "RETAKE PICK: overlay canvas unavailable";
    if (!mode || !p_object_unbox(mode))
        goto done;
    void *uiCamera = *(int *)p_object_unbox(mode)
                         ? browser_optional(canvasRoot, "get_worldCamera", "UnityEngine.Camera")
                         : 0;
    if (*(int *)p_object_unbox(mode) && !uiCamera)
        goto done;
    void *utility = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
    const char *up[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera"};
    void *ua[] = {preview_capture_box, &screen, uiCamera};
    void *over = pf_invoke(
        browser_method(utility, "RectangleContainsScreenPoint", "System.Boolean", 3, up, 0), 0, ua);
    result = "RETAKE PICK: overlay hit test unavailable";
    if (!over || !p_object_unbox(over))
        goto done;
    if (*(unsigned char *)p_object_unbox(over)) {
        result = "RETAKE PICK: click on capture controls; choose station in world";
        goto done;
    }
    void *cc = findclass(p_domain_get(), "UnityEngine", "Camera");
    void *camera = pf_invoke(browser_method(cc, "get_main", "UnityEngine.Camera", 0, 0, 0), 0, 0);
    result = "RETAKE PICK: world camera unavailable";
    if (!camera || !pf_root(&roots, camera))
        goto done;
    BrowserV3 xyz = {screen.x, screen.y, 0};
    const char *sp[] = {"UnityEngine.Vector3"};
    void *sa[] = {&xyz};
    void *ray =
        pf_invoke(browser_method(cc, "ScreenPointToRay", "UnityEngine.Ray", 1, sp, 1), camera, sa);
    result = "RETAKE PICK: screen ray unavailable";
    if (!ray || !pf_root(&roots, ray) || !write_value_layout(p_object_get_class(ray), 24))
        goto done;
    float distance = 10000.f;
    int layers = -1, triggers = 2; /* Include selection triggers. */
    const char *rp[] = {"UnityEngine.Ray", "System.Single", "System.Int32",
                        "UnityEngine.QueryTriggerInteraction"};
    void *ra[] = {p_object_unbox(ray), &distance, &layers, &triggers};
    void *physics = findclass(p_domain_get(), "UnityEngine", "Physics");
    void *hits = pf_invoke(
        browser_method(physics, "RaycastAll", "UnityEngine.RaycastHit[]", 4, rp, 0), 0, ra);
    int count = 0;
    result = "RETAKE PICK: raycast unavailable or hit limit exceeded";
    if (!hits || !pf_root(&roots, hits) || !write_array_count_limit(hits, &count, 256))
        goto done;
    int ids[256];
    float distances[256];
    unsigned used = 0;
    const char *ip[] = {"System.Int32"};
    void *get = browser_method(p_object_get_class(hits), "GetValue", "System.Object", 1, ip, 1);
    void *hc = findclass(p_domain_get(), "UnityEngine", "RaycastHit");
    void *getCollider = browser_method(hc, "get_collider", "UnityEngine.Collider", 0, 0, 1);
    void *getDistance = browser_method(hc, "get_distance", "System.Single", 0, 0, 1);
    result = "RETAKE PICK: hit accessors unavailable";
    if (!get || !getCollider || !getDistance)
        goto done;
    for (int i = 0; i < count; ++i) {
        void *ia[] = {&i};
        void *hit = pf_invoke(get, hits, ia);
        PFRoots temporary = {0};
        if (!hit || !pf_root(&temporary, hit))
            continue;
        void *collider = pf_invoke(getCollider, p_object_unbox(hit), 0);
        void *db = pf_invoke(getDistance, p_object_unbox(hit), 0);
        float d = db && p_object_unbox(db) ? *(float *)p_object_unbox(db) : -1.f;
        void *id = collider ? browser_optional(collider, "GetInstanceID", "System.Int32") : 0;
        if (id && p_object_unbox(id) && d >= 0 && d <= distance) {
            ids[used] = *(int *)p_object_unbox(id);
            distances[used++] = d;
        }
        pf_release(&temporary);
    }
    result = "RETAKE PICK: no station collider hit; click the station platform";
    void *all = isolated_resources("TPS.Game", "GameItemVisual");
    int visuals = 0;
    if (!all || !pf_root(&roots, all) || !browser_asset_array_count(all, &visuals)) {
        result = "RETAKE PICK: item visuals unavailable";
        goto done;
    }
    void *station = 0, *visualStation = 0;
    float nearest = distance + 1.f, visualNearest = distance + 1.f;
    for (int i = 0; i < visuals; ++i) {
        void *visual = write_array_item(all, i);
        if (!visual || !browser_alive(visual) ||
            !funfair_property_true(visual, "get_isActiveAndEnabled"))
            continue;
        void *vc = p_object_get_class(visual);
        void *item = pf_ref(visual, vc, "Item", "TPS.Game.GameItemInstance");
        unsigned long long source = 0;
        long long definition = 0;
        unsigned entity[2] = {0};
        if (!save_station_identity(item, &source, entity, &definition) ||
            definition != management_entry.definition)
            continue;
        float visualDistance = management_station_ray(visual, ray);
        if (visualDistance < visualNearest) {
            visualNearest = visualDistance;
            visualStation = item;
        }
        void *primary = pf_ref(visual, vc, "Collider", "UnityEngine.Collider");
        void *extra = pf_ref(visual, vc, "ExtraColliders", "UnityEngine.Collider[]");
        int extras = 0;
        if (extra && !write_array_count_limit(extra, &extras, 256))
            continue;
        for (int j = -1; j < extras; ++j) {
            void *collider = j < 0 ? primary : write_array_item(extra, j);
            void *id = collider && browser_alive(collider)
                           ? browser_optional(collider, "GetInstanceID", "System.Int32")
                           : 0;
            if (!id || !p_object_unbox(id))
                continue;
            int value = *(int *)p_object_unbox(id);
            for (unsigned k = 0; k < used; ++k)
                if (value == ids[k] && distances[k] < nearest) {
                    nearest = distances[k];
                    station = item;
                }
        }
    }
    if (!station)
        station = visualStation;
    if (station && management_target_set(station)) {
        result = nearest <= distance ? "RETAKE PICK: collider station accepted; Save enabled"
                                     : "RETAKE PICK: visible station accepted; Save enabled";
        management_target_prompt();
    }
done:
    browser_log(result);
    pf_release(&roots);
}
/* Retake matches the persistent item ID and definition, then uses the current
 * runtime entity. A saved entity index/generation is invalid after reload. */
#include "retake_identity.h"
static int management_capture_isolated(const LibraryEntry *e) {
    if ((!management_photo_owned && !management_probe_only) || !g_managed_fields_verified ||
        !management_validate(e))
        return 0;
    if (management_target && !management_probe_only) {
        if (!colour_identity(management_target, management_target_entity) ||
            !DeleteFileW(management_photo))
            return 0;
        return isolated_capture(management_target, management_target_entity, management_photo);
    }
    unsigned n = library_entry_read(e, L"blueprint.json", library_bytes, 4U * 1024U * 1024U);
    unsigned root = n ? lj_parse(library_bytes, n) : 0;
    long long source = 0, definition = 0;
    if (!root || !lj_int(lj_member(root, "sourceStationId"), &source) || source <= 0 ||
        !lj_int(lj_member(root, "sourceStationDefinitionId"), &definition) ||
        definition != e->definition)
        return 0;
    unsigned entity[2] = {0};
    unsigned bytes = library_prepare(root, library_plan);
    char hash[65];
    if (!bytes)
        return 0;
    library_sha(library_plan, bytes, hash);
    if (!equals(hash, e->sha))
        return 0;
    PFRoots roots = {0};
    void *all = isolated_resources("TPS.Game", "GameItemVisual");
    int count = 0, ok = 0;
    void *station = 0;
    if (!all || !pf_root(&roots, all) || !browser_asset_array_count(all, &count))
        goto done;
    for (int i = 0; i < count; ++i) {
        void *visual = write_array_item(all, i);
        if (!visual || !browser_alive(visual) ||
            !funfair_property_true(visual, "get_isActiveAndEnabled"))
            continue;
        void *item =
            pf_ref(visual, p_object_get_class(visual), "Item", "TPS.Game.GameItemInstance");
        unsigned long long id = 0;
        long long currentDefinition = 0;
        unsigned currentEntity[2] = {0};
        if (!save_station_identity(item, &id, currentEntity, &currentDefinition) ||
            !retake_identity_matches((unsigned long long)source, definition, id, currentDefinition,
                                     currentEntity))
            continue;
        if (station && station != item)
            goto done;
        if (!station && !pf_root(&roots, item))
            goto done;
        station = item;
        memcpy(entity, currentEntity, sizeof(entity));
    }
    if (!station) {
        browser_log(
            "RETAKE: persistent source station ID/definition not found; old preview retained");
        goto done;
    }
    if (management_probe_only) {
        ok = management_target_set(station);
        goto done;
    }
    /* Release only our zero-length reservation; isolated_capture creates a new
     * file and the existing validated atomic replacement publishes it later. */
    if (!DeleteFileW(management_photo))
        goto done;
    ok = isolated_capture(station, entity, management_photo);
    if (ok)
        browser_log("RETAKE: isolated source coaster captured; template UUID/background retained");
done:
    pf_release(&roots);
    return ok;
}
static int management_finish_photo(const LibraryEntry *e) {
    unsigned n = library_read(management_photo, library_image_bytes, 20U * 1024U * 1024U);
    if (!n || !save_complete_png(library_image_bytes, n))
        return 0;
    if (!management_validate(e) || !preview_resize_capture(management_photo))
        return -1;
    HANDLE capture =
        CreateFileW(management_photo, 0x40000000, FILE_SHARE_READ, 0, 3, FILE_ATTRIBUTE_NORMAL, 0);
    if (capture == INVALID_HANDLE_VALUE)
        return -1;
    int flushed = FlushFileBuffers(capture) != 0;
    if (!CloseHandle(capture))
        flushed = 0;
    if (!flushed)
        return -1;
    static WCHAR target[1024];
    if (!library_path(target, e->folder, L"preview.png"))
        return -1;
    DWORD attr = GetFileAttributesW(target);
    if (attr != 0xffffffffU && (attr & (0x10 | 0x400)))
        return -1;
    /* JPG entries keep their old image until metadata points to the new PNG. */
    if (!equals(e->screenshot, "preview.png") && attr != 0xffffffffU)
        return -1;
    int wasPng = equals(e->screenshot, "preview.png");
    if (!MoveFileExW(management_photo, target, wasPng ? (1 | 8) : 8))
        return -1;
    management_photo_owned = 0;
    if (!wasPng) {
        if (!management_replace_field(e, "screenshot", "preview.png")) {
            DeleteFileW(target);
            return -1;
        }
        static WCHAR oldPhoto[1024];
        if (library_path(oldPhoto, e->folder, L"preview.jpg"))
            DeleteFileW(oldPhoto);
    }
    return 1;
}
static void management_discard_photo(void) {
    if (management_photo_owned)
        DeleteFileW(management_photo);
    management_photo_owned = 0;
}
static void management_message(const char *text) {
    if (management_status)
        browser_set(management_status, "set_text", "System.String", p_string_new(text));
    browser_log(text);
}
static void management_close(void) {
    management_target_clear();
    if (management_mode == 2 || management_mode == 6)
        delete_stop();
    if (management_mode)
        preview_capture_stop();
    browser_active(management_panel, 0);
    management_mode = 0;
}
static void management_refresh(void) {
    WCHAR selected[33];
    memcpy(selected, management_entry.folder, 66);
    long long type = browser_definition;
    library_scan(type);
    for (unsigned i = 0; i < library_count; ++i) {
        unsigned j = 0;
        while (j < 33 && selected[j] == library_entries[i].folder[j])
            ++j;
        if (j == 33) {
            library_index = i;
            break;
        }
    }
    library_display();
}
static void management_cleanup(void) {
    management_target_clear();
    if (management_mode)
        preview_capture_stop();
    if (management_mode >= 4)
        save_restore_ui();
    management_discard_photo();
    management_mode = 0;
    delete_cleanup();
    save_destroy_roots(&management_input_roots);
    save_destroy_roots(&management_modal);
    save_destroy_roots(&management_tools);
    management_rename = management_retake = management_delete = management_panel =
        management_status = management_input = management_input_go = management_cancel =
            management_confirm = 0;
}
static int management_construct(void *parent, void *font) {
    management_writable_state = -1;
    browser_current_roots = &management_tools;
    management_rename = browser_toggle(parent, font, "Rename", -browser_width / 2 + 210,
                                       -browser_height / 2 + 65, 270, 64, .5f, .5f);
    management_retake = browser_toggle(parent, font, "Retake Screenshot", -browser_width / 2 + 590,
                                       -browser_height / 2 + 65, 430, 64, .5f, .5f);
    management_delete = browser_toggle(parent, font, "Delete", -browser_width / 2 + 950,
                                       -browser_height / 2 + 65, 230, 64, .5f, .5f);
    browser_danger(management_delete);
    browser_active(browser_get(management_rename, "get_gameObject", "UnityEngine.GameObject"), 0);
    browser_active(browser_get(management_retake, "get_gameObject", "UnityEngine.GameObject"), 0);
    browser_active(browser_get(management_delete, "get_gameObject", "UnityEngine.GameObject"), 0);
    browser_current_roots = &management_modal;
    delete_construct(parent, font);
    void *transform = browser_get(management_panel, "get_transform", "UnityEngine.Transform");
    browser_current_roots = &management_input_roots;
    management_input_go = browser_make("Template name", transform, 0, -50, 1000, 70, .5f, .5f);
    BrowserColor ivory = {1, .98f, .89f, 1};
    void *image = browser_image(management_input_go, ivory);
    void *rect = browser_get(management_input_go, "get_transform", "UnityEngine.Transform");
    void *text = browser_label(rect, font, "", 0, 0, 950, 64, 34);
    management_input = browser_add(management_input_go, "TMPro", "TMP_InputField");
    browser_set(management_input, "set_textComponent", "TMPro.TMP_Text", text);
    browser_set(management_input, "set_textViewport", "UnityEngine.RectTransform", rect);
    browser_set(management_input, "set_targetGraphic", "UnityEngine.UI.Graphic", image);
    int limit = 100, single = 0;
    browser_set(management_input, "set_characterLimit", "System.Int32", &limit);
    browser_set(management_input, "set_lineType", "TMPro.TMP_InputField.LineType", &single);
    browser_active(management_panel, 0);
    preview_capture_ensure(parent, font);
    browser_current_roots = &browser_roots;
    return !browser_failed;
}
static void management_open(int mode) {
    if (save_phase || management_mode || library_index >= library_count || library_busy())
        return;
    management_entry = library_entries[library_index];
    if (!(mode == 2 && management_entry.source == 1 ? coaster_manual_valid(&management_entry)
                                                    : management_validate(&management_entry))) {
        browser_log("MANAGE BLOCKED: entry changed, unavailable or Workshop read-only");
        return;
    }
    management_mode = mode;
    unsigned char off = 0;
    browser_set(management_confirm, "SetIsOnWithoutNotify", "System.Boolean", &off);
    browser_set(management_cancel, "SetIsOnWithoutNotify", "System.Boolean", &off);
    browser_set(delete_close, "SetIsOnWithoutNotify", "System.Boolean", &off);
    delete_mouse_down = (GetAsyncKeyState(1) & 0x8000) != 0;
    delete_escape_down = (GetAsyncKeyState(27) & 0x8000) != 0;
    browser_active(browser_get(delete_name, "get_gameObject", "UnityEngine.GameObject"), mode == 2);
    browser_active(management_input_go, mode == 1);
    browser_active(management_panel, 1);
    if ((mode == 2 || mode == 6) && !delete_start()) {
        management_mode = 0;
        browser_log("DELETE: could not hide UI; confirmation cancelled");
        return;
    }
    if (mode == 2 || mode == 6) {
        int exporting = mode == 6;
        browser_set(delete_title, "set_text", "System.String",
                    p_string_new(exporting ? "Upload Template" : "Delete Template"));
        browser_set(library_control_label(management_confirm), "set_text", "System.String",
                    p_string_new(exporting ? "" : "Yes"));
        browser_set(library_control_label(management_cancel), "set_text", "System.String",
                    p_string_new(exporting ? "" : "No"));
        delete_rect(management_confirm, exporting ? -62.f : -112.f, exporting ? -130.f : -230.f,
                    exporting ? save_popup_action_edge : 220.f,
                    exporting ? save_popup_action_edge : 104.f);
        delete_rect(management_cancel, exporting ? 62.f : 112.f, exporting ? -130.f : -230.f,
                    exporting ? save_popup_action_edge : 220.f,
                    exporting ? save_popup_action_edge : 104.f);
    }
    if (mode == 6) {
        browser_active(browser_get(delete_name, "get_gameObject", "UnityEngine.GameObject"), 0);
        static char question[800];
        unsigned n = 0;
        question[0] = 0;
        workshop_append(question, sizeof(question), &n, "Do you want to upload ", 0);
        workshop_append(question, sizeof(question), &n, management_entry.name, 0);
        workshop_append(question, sizeof(question), &n, " to the Steam Workshop as a Public item?", 0);
        management_message(question);
        return;
    }

    if (mode == 1) {
        browser_set(management_input, "set_text", "System.String",
                    p_string_new(management_entry.name));
        management_message("Rename Template\n\nEnter a new name for this local template.");
    } else if (mode == 2) {
        static char message[800];
        const char *prefix = "Deleting the ";
        const char *suffix =
            management_entry.source == 1
                ? " template will permanently delete its downloaded .coaster file from your Manual folder.\n\nThis action cannot be undone.\n\nAre you sure you want to proceed?"
                : " template will completely remove the template from your game.\n\nThis action cannot be undone.\n\nAre you sure you want to proceed?";
        unsigned n = 0;
        for (unsigned i = 0; prefix[i]; ++i)
            message[n++] = prefix[i];
        for (unsigned i = 0; management_entry.name[i] && n < sizeof(message) - 180; ++i)
            message[n++] = management_entry.name[i];
        for (unsigned i = 0; suffix[i] && n < sizeof(message) - 1; ++i)
            message[n++] = suffix[i];
        message[n] = 0;
        browser_active(browser_get(delete_name, "get_gameObject", "UnityEngine.GameObject"), 0);
        management_message(message);
    } else {
        management_target_clear();
        management_probe_only = 1;
        management_capture_isolated(&management_entry);
        management_probe_only = 0;
        management_pick_mouse = (GetAsyncKeyState(1) & 0x8000) != 0;
        if (!preview_capture_start()) {
            management_target_clear();
            management_mode = 0;
            browser_log("MANAGE: could not enter framing mode; UI restored");
        } else {
            management_target_prompt();
            if (!management_target)
                browser_log(
                    "RETAKE: automatic target unavailable; click the coaster station, then Save");
        }
    }
}
static void management_tick(void) {
    workshop_tick();
    if (!browser_open && !management_mode)
        return;
    if (!p_thread_current())
        return;
    library_scroll_input_tick();
    library_inline_tick();
    if (!management_panel)
        return;
    if (management_mode == 6) {
        int choice = delete_choice_raw();
        if (!choice)
            return;
        static LibraryEntry selected;
        selected = management_entry;
        management_close();
        if (choice < 0)
            return;
        if (choice == 1)
            workshop_start(&selected);
        return;
    }
    if (management_mode < 4) {
        unsigned char writable =
            (unsigned char)(!save_phase && library_count && library_index < library_count &&
                            !library_entries[library_index].readOnly && !management_mode);
        if (management_writable_state != (int)writable) {
            browser_set(management_rename, "set_interactable", "System.Boolean", &writable);
            browser_set(management_retake, "set_interactable", "System.Boolean", &writable);
            browser_set(management_delete, "set_interactable", "System.Boolean", &writable);
            management_writable_state = (int)writable;
        }
        if (!browser_open && management_mode != 3)
            return;
        if (!management_mode) {
            if (browser_pressed(management_rename))
                management_open(1);
            else if (browser_pressed(management_delete))
                management_open(2);
            else if (browser_pressed(management_retake))
                management_open(3);
            return;
        }
        if (management_mode == 3) {
            management_pick_station();
            int choice = preview_capture_choice();
            if (choice < 0) {
                management_close();
                return;
            }
            if (!choice || !management_target)
                return;
        } else if (management_mode == 2) {
            int choice = delete_choice();
            if (choice < 0) {
                management_close();
                return;
            }
            if (!choice)
                return;
        } else {
            if (browser_pressed(management_cancel)) {
                management_close();
                return;
            }
            if (!browser_pressed(management_confirm))
                return;
        }
        if (management_mode == 1) {
            char name[401];
            void *str = pf_invoke(browser_method(p_object_get_class(management_input), "get_text",
                                                 "System.String", 0, 0, 1),
                                  management_input, 0);
            if (!save_utf8(str, name, sizeof(name)) ||
                !management_rename_entry(&management_entry, name)) {
                management_message(
                    "Could not rename this template.\nUse 1 to 100 printable characters and check that the entry is still available.");
                return;
            }
            management_close();
            management_refresh();
            return;
        }
        if (management_mode == 2) {
            if (!management_delete_entry(&management_entry)) {
                management_message(
                    "Could not complete deletion.\nThe folder may contain extra files or be unavailable.\nCheck the library before trying again.");
                return;
            }
            management_close();
            management_refresh();
            return;
        }
        if (!management_start_photo(&management_entry)) {
            management_close();
            browser_set(browser_status, "set_text", "System.String",
                        p_string_new("Could not start capture. The entry may have changed."));
            return;
        }
        management_mode = 4;
        management_started = GetTickCount64();
        management_due = management_started + 150;
        /* Framing already hid the game canvases. Remove the last box before capture. */
        browser_active(preview_capture_root, 0);
        return;
    }
    UINT_PTR now = GetTickCount64();
    if (management_mode == 4 && now >= management_due) {
        int ok = management_capture_isolated(&management_entry);
        if (!ok) {
            management_discard_photo();
            management_target_clear();
            preview_capture_stop();
            management_mode = 3;
            management_pick_mouse = (GetAsyncKeyState(1) & 0x8000) != 0;
            if (preview_capture_start()) {
                management_target_prompt();
                browser_set(preview_capture_title, "set_text", "System.String",
                            p_string_new("Click station to retry"));
                browser_log(
                    "RETAKE: capture failed; choose a station in the current view, then Save. Old preview retained.");
            } else {
                management_close();
                browser_set(
                    browser_status, "set_text", "System.String",
                    p_string_new(
                        "Capture failed and framing could not reopen. The old preview is unchanged."));
            }
            return;
        }
        management_mode = 5;
        management_started = now;
        management_due = now + 500;
    }
    if (management_mode == 5 && now >= management_due) {
        save_restore_ui();
        management_due = now + 250;
        int result = management_finish_photo(&management_entry);
        if (result == 1) {
            management_close();
            management_refresh();
            browser_log("MANAGE: preview replaced; original snapshot unchanged");
        } else if (result < 0 || now - management_started > 15000) {
            management_discard_photo();
            management_close();
            browser_set(browser_status, "set_text", "System.String",
                        p_string_new("Could not replace the preview. Please try again."));
        }
    }
}

static void library_row_action(unsigned index, int mode) {
    if (index >= library_count ||
        (library_entries[index].readOnly && !(mode == 2 && library_entries[index].source == 1)))
        return;
    library_index = index;
    management_open(mode);
}
/* Enter commits only the frozen entry that owned the focused name field. */
static int library_edit_slot = -1, library_enter_down;
static LibraryEntry library_edit_entry;
/* Scrolling recycles row slots: discard an uncommitted edit while its old
 * identity still owns the field, then release native focus before rebinding. */
static void library_end_inline_edit(void) {
    if (library_edit_slot >= 0 && library_edit_slot < 6) {
        browser_set(library_slot_input[library_edit_slot], "set_text", "System.String",
                    p_string_new(library_edit_entry.name));
    }
    library_edit_slot = -1;
    const char *params[] = {"System.Boolean"};
    unsigned char yes = 1;
    void *args[] = {&yes};
    for (unsigned k = 0; k < 6; ++k)
        if (library_slot_input[k] &&
            funfair_property_true(library_slot_input[k], "get_isFocused")) {
            write_void(browser_method(p_object_get_class(library_slot_input[k]),
                                      "DeactivateInputField", "System.Void", 1, params, 1),
                       library_slot_input[k], args);
        }
}
static void library_inline_tick(void) {
    int enter = (GetAsyncKeyState(13) & 0x8000) != 0;
    if (!browser_open || management_busy()) {
        library_edit_slot = -1;
        library_enter_down = enter;
        return;
    }
    int focused = -1;
    for (unsigned k = 0; k < 6; ++k)
        if (library_slot_input[k] &&
            funfair_property_true(library_slot_input[k], "get_isFocused")) {
            focused = (int)k;
            break;
        }
    if (focused >= 0 && library_edit_slot < 0 &&
        library_first + (unsigned)focused < library_count) {
        library_edit_slot = focused;
        library_edit_entry = library_entries[library_first + (unsigned)focused];
    }
    if (library_edit_slot >= 0 && enter && !library_enter_down) {
        void *text =
            browser_optional(library_slot_input[library_edit_slot], "get_text", "System.String");
        char name[401];
        int ok = !library_edit_entry.readOnly && save_utf8(text, name, sizeof(name)) &&
                 management_rename_entry(&library_edit_entry, name);
        library_edit_slot = -1;
        if (ok) {
            management_entry = library_edit_entry;
            management_refresh();
        } else
            browser_set(
                browser_status, "set_text", "System.String",
                p_string_new("Could not rename this template. Check the name and try again."));
    } else if (focused < 0 && library_edit_slot >= 0) {
        browser_set(library_slot_input[library_edit_slot], "set_text", "System.String",
                    p_string_new(library_edit_entry.name));
        library_edit_slot = -1;
    }
    library_enter_down = enter;
}
