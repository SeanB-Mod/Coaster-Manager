/* Steam SDK flat API through the game's existing Steam session. Never Init,
 * Shutdown, RunCallbacks or install a second callback dispatcher. */
typedef unsigned long long WorkshopId;
typedef unsigned char WorkshopBool;
typedef struct {
    int result;
    WorkshopId id;
    WorkshopBool legal;
} WorkshopCreated;
typedef struct {
    int result;
    WorkshopBool legal;
    WorkshopId id;
} WorkshopSubmitted;
typedef struct {
    int result;
    WorkshopId parent, child;
} WorkshopDependency;
typedef struct {
    int result;
    WorkshopId item;
    unsigned app;
} WorkshopAppDependency;
typedef char WorkshopAppDependencySize[sizeof(WorkshopAppDependency) == 24 ? 1 : -1];
#define WORKSHOP_RIDES_RELICS_APP 4511680U
#define WORKSHOP_REQUIRED_ITEM_COUNT 2U
static const WorkshopId workshop_required_items[WORKSHOP_REQUIRED_ITEM_COUNT] = {
    3830269659ULL, 3830269581ULL
};
static unsigned workshop_dependency_index;
static WorkshopId (*workshop_add_app_dependency)(void *, WorkshopId, unsigned);
static WorkshopId (*workshop_add_dependency)(void *, WorkshopId, WorkshopId);
typedef char WorkshopCreatedSize[sizeof(WorkshopCreated) == 24 ? 1 : -1];
typedef char WorkshopSubmittedSize[sizeof(WorkshopSubmitted) == 16 ? 1 : -1];
typedef char WorkshopDependencySize[sizeof(WorkshopDependency) == 24 ? 1 : -1];
typedef struct {
    const char **strings;
    int count;
} WorkshopTags;
static void *workshop_ugc, *workshop_utils, *workshop_user;
static void *(*workshop_get_ugc)(void);
static void *(*workshop_get_utils)(void);
static void *(*workshop_get_user)(void);
static unsigned (*workshop_app)(void *);
static WorkshopId (*workshop_owner)(void *);
static WorkshopBool (*workshop_logged_on)(void *);
static WorkshopId (*workshop_create)(void *, unsigned, int);
static WorkshopId (*workshop_update)(void *, unsigned, WorkshopId);
static WorkshopBool (*workshop_title)(void *, WorkshopId, const char *);
static WorkshopBool (*workshop_description)(void *, WorkshopId, const char *);
static WorkshopBool (*workshop_content)(void *, WorkshopId, const char *);
static WorkshopBool (*workshop_preview)(void *, WorkshopId, const char *);
static WorkshopBool (*workshop_tags)(void *, WorkshopId, const WorkshopTags *);
static WorkshopBool (*workshop_visibility)(void *, WorkshopId, int);
static WorkshopId (*workshop_submit)(void *, WorkshopId, const char *);
static WorkshopBool (*workshop_completed)(void *, WorkshopId, WorkshopBool *);
static WorkshopBool (*workshop_result)(void *, WorkshopId, void *, int, int, WorkshopBool *);
static int workshop_relaxed, workshop_persist_attempts;
static int workshop_dlc_warning;
static WorkshopBool workshop_created_legal;
static const char *workshop_bind_error;
static int
    workshop_phase; /* 1 create; 2 submit; 3 persist retry; 4 item check; 5 required item; 6 required DLC */
static WorkshopId workshop_call, workshop_item, workshop_account;
static UINT_PTR workshop_due, workshop_started;
static LibraryEntry workshop_entry;
static char workshop_type[401], workshop_title_text[804], workshop_text[2048],
    workshop_content_utf8[4096], workshop_preview_utf8[4096];
static WCHAR workshop_stage[1024], workshop_content_dir[1024], workshop_package[1024],
    workshop_jpeg[1024];
static unsigned char workshop_original_metadata[LIBRARY_METADATA_LIMIT + 1];

#include "workshop_connection.inc"
static int workshop_busy(void) {
    return workshop_phase != 0 && !workshop_relaxed;
}
static const char *workshop_fixed_type(int index) {
    static const char *types[] = {"Steeper Chase", "Gearbox Glider", "Revolver Coaster",
                                  "Cross-Country Cart", "Monobeast Metro"};
    return index >= 0 && index < 5 ? types[index] : 0;
}
static char workshop_tag_type[401];
static int workshop_same_folder(const WCHAR *a, const WCHAR *b) {
    for (unsigned i = 0; i < 33; ++i)
        if (a[i] != b[i])
            return 0;
    return 1;
}
static void workshop_message(const char *message) {
    browser_log(message);
    if (browser_status)
        browser_set(browser_status, "set_text", "System.String", p_string_new(message));
}
static int workshop_bind(void) {
    workshop_bind_error = "Steam Workshop is unavailable. Start through Steam and sign in.";
    HMODULE steam = GetModuleHandleW(L"steam_api64.dll");
    if (!steam)
        return 0;
#define WS_BIND(name, symbol)                                                                      \
    *(void **)(&name) = (void *)GetProcAddress(steam, symbol);                                     \
    if (!name) {                                                                                   \
        logtext("WORKSHOP missing SDK export: ");                                                  \
        logtext(symbol);                                                                           \
        nl();                                                                                      \
        workshop_bind_error =                                                                      \
            "Steam Workshop SDK is incompatible with this game version. Check the mod log.";       \
        return 0;                                                                                  \
    }
    WS_BIND(workshop_get_ugc, "SteamAPI_SteamUGC_v020");
    WS_BIND(workshop_get_utils, "SteamAPI_SteamUtils_v010");
    WS_BIND(workshop_get_user, "SteamAPI_SteamUser_v023");
    WS_BIND(workshop_app, "SteamAPI_ISteamUtils_GetAppID");
    WS_BIND(workshop_owner, "SteamAPI_ISteamUser_GetSteamID");
    WS_BIND(workshop_logged_on, "SteamAPI_ISteamUser_BLoggedOn");
    WS_BIND(workshop_create, "SteamAPI_ISteamUGC_CreateItem");
    WS_BIND(workshop_update, "SteamAPI_ISteamUGC_StartItemUpdate");
    WS_BIND(workshop_title, "SteamAPI_ISteamUGC_SetItemTitle");
    WS_BIND(workshop_description, "SteamAPI_ISteamUGC_SetItemDescription");
    WS_BIND(workshop_content, "SteamAPI_ISteamUGC_SetItemContent");
    WS_BIND(workshop_preview, "SteamAPI_ISteamUGC_SetItemPreview");
    WS_BIND(workshop_tags, "SteamAPI_ISteamUGC_SetItemTags");
    WS_BIND(workshop_visibility, "SteamAPI_ISteamUGC_SetItemVisibility");
    WS_BIND(workshop_add_dependency, "SteamAPI_ISteamUGC_AddDependency");
    *(void **)(&workshop_add_app_dependency) =
        (void *)GetProcAddress(steam, "SteamAPI_ISteamUGC_AddAppDependency");
    WS_BIND(workshop_submit, "SteamAPI_ISteamUGC_SubmitItemUpdate");
    workshop_completed = workshop_owned_completed;
    workshop_result = workshop_owned_result;
#undef WS_BIND
    workshop_ugc = workshop_get_ugc();
    workshop_utils = workshop_get_utils();
    workshop_user = workshop_get_user();
    if (!(workshop_ugc && workshop_utils && workshop_user &&
          workshop_app(workshop_utils) == 2185060 && workshop_logged_on(workshop_user) &&
          workshop_owner(workshop_user)))
        return 0;
    return workshop_open_connection(steam);
}
static void workshop_decimal(WorkshopId value, char *out) {
    char reverse[24];
    unsigned n = 0, k = 0;
    do {
        reverse[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n)
        out[k++] = reverse[--n];
    out[k] = 0;
}
static int workshop_id(unsigned token, WorkshopId *out) {
    if (!token || lj_tokens[token].kind != 3)
        return 0;
    const char *s = lj_text + (unsigned)lj_tokens[token].number;
    WorkshopId n = 0;
    unsigned i = 0;
    for (; s[i]; ++i) {
        unsigned c = (unsigned char)s[i];
        if (i >= 20 || c < '0' || c > '9' || n > (~(WorkshopId)0 - (c - '0')) / 10)
            return 0;
        n = n * 10 + c - '0';
    }
    if (!i || !n)
        return 0;
    *out = n;
    return 1;
}
static int workshop_link(const LibraryEntry *e, WorkshopId *item, WorkshopId *owner) {
    unsigned root = management_validate(e);
    if (!root)
        return 0;
    unsigned a = lj_member(root, "workshopItemId"), b = lj_member(root, "workshopOwnerId");
    *item = *owner = 0;
    if (!a && !b)
        return 1;
    return workshop_id(a, item) && workshop_id(b, owner);
}
/* One atomic metadata replacement, preserving all unrelated keys and values. */
static int workshop_save_link(const LibraryEntry *e, WorkshopId item, WorkshopId owner) {
    unsigned root = management_validate(e);
    if (!root || ((item == 0) != (owner == 0)))
        return 0;
    unsigned n = management_metadata_size;
    memcpy(workshop_original_metadata, management_metadata, n);
    jpos = 0;
    jok = 1;
    js("{");
    int comma = 0;
    for (unsigned key = lj_tokens[root].child; key;) {
        unsigned value = key;
        const char *name = lj_text + lj_tokens[key].text;
        if (!equals(name, "workshopItemId") && !equals(name, "workshopOwnerId")) {
            if (comma)
                js(",");
            comma = 1;
            save_json_string(name);
            js(":");
            for (unsigned i = lj_tokens[value].rawStart; i < lj_tokens[value].rawEnd; ++i) {
                char ch[2] = {(char)workshop_original_metadata[i], 0};
                js(ch);
            }
        }
        key = lj_tokens[key].next;
    }
    char id[24], account[24];
    if (item) {
        workshop_decimal(item, id);
        workshop_decimal(owner, account);
        if (comma)
            js(",");
        js("\"workshopItemId\":");
        save_json_string(id);
        js(",\"workshopOwnerId\":");
        save_json_string(account);
    }
    js("}");
    if (!jok || jpos > LIBRARY_METADATA_LIMIT)
        return 0;
    unsigned size = jpos;
    memcpy(management_replacement, jbuf, size);
    static WCHAR temp[1024], target[1024];
    if (!library_path(temp, e->folder, L".entry-update.tmp") ||
        !library_path(target, e->folder, L"entry.json"))
        return 0;
    if (!management_write_temp(temp, management_replacement, size))
        return 0;
    int ok = management_validate(e) != 0 && management_metadata_size == n;
    for (unsigned i = 0; ok && i < n; ++i)
        if (management_metadata[i] != workshop_original_metadata[i])
            ok = 0;
    if (ok)
        ok = MoveFileExW(temp, target, 1 | 8) != 0;
    if (!ok)
        DeleteFileW(temp);
    return ok;
}
static int workshop_append(char *out, unsigned cap, unsigned *pos, const char *text, int literal) {
    for (unsigned i = 0; text[i]; ++i) {
        if (*pos >= cap - 1)
            return 0;
        char c = text[i];
        out[(*pos)++] = literal ? (c == '[' ? '(' : c == ']' ? ')' : c) : c;
    }
    out[*pos] = 0;
    return 1;
}
static int workshop_format_title(const LibraryEntry *e, const char *type, char *out, unsigned cap) {
    unsigned n = 0;
    return workshop_append(out, cap, &n, e->name, 0) && workshop_append(out, cap, &n, " - ", 0) &&
           workshop_append(out, cap, &n, type, 0);
}
/* Cost argument is the read-only full template estimate at upload time,
 * not the saved entry.json track-only cost. */
static int workshop_format(const LibraryEntry *e, const char *type, const char *totalPrice,
                           char *out, unsigned cap) {
    if (!totalPrice || !totalPrice[0])
        return 0;
    BlueprintDisplayStats stats = e->stats;
    blueprint_label_stats(&stats);
    unsigned n = 0;
    if (!workshop_append(
            out, cap, &n,
            "This coaster template requires Coaster Manager to work. Please subscribe to Coaster Manager and enable that mod first.\n\n[H1]",
            0) ||
        !workshop_append(out, cap, &n, e->name, 1) || !workshop_append(out, cap, &n, " - ", 0) ||
        !workshop_append(out, cap, &n, type, 1) || !workshop_append(out, cap, &n, "[/H1]\n", 0))
        return 0;
    const char *labels[] = {"[b]Cost:[/b] = ", "[b]Buzz[/b] = ", "[b]Thrill[/b] = ",
                            "[b]Ferocity[/b] = ", "[b]Nausea[/b] = "};
    for (unsigned i = 0; i < 5; ++i)
        if (!workshop_append(out, cap, &n, labels[i], 0) ||
            !workshop_append(out, cap, &n,
                             i == 0 ? totalPrice : (stats.value[i][0] ? stats.value[i] : "?"),
                             1) ||
            (i < 4 && !workshop_append(out, cap, &n, "\n", 0)))
            return 0;
    return 1;
}
static int workshop_directory(WCHAR *out, const WCHAR *parent, const WCHAR *child) {
    unsigned n = 0;
    if (!append(out, 1024, &n, parent) || !append(out, 1024, &n, L"\\") ||
        !append(out, 1024, &n, child))
        return 0;
    DWORD attr = GetFileAttributesW(out);
    if (attr == 0xffffffffU) {
        if (!CreateDirectoryW(out, 0))
            return 0;
        attr = GetFileAttributesW(out);
    }
    return (attr & 0x10) && !(attr & 0x400);
}
static int workshop_utf8_path(const WCHAR *path, char *out, unsigned cap) {
    unsigned n = 0;
    while (path[n])
        ++n;
    int size = WideCharToMultiByte(65001, 0, path, (int)n, out, (int)cap - 1, 0, 0);
    if (size <= 0 || (unsigned)size >= cap)
        return 0;
    out[size] = 0;
    return 1;
}
/* Decode the saved preview and make an upload-only JPEG. The library original
 * remains unchanged. Encoding quality is reduced until strictly below 1 MB. */
static int workshop_jpeg_valid(const unsigned char *b, unsigned long long size) {
    unsigned w = 0, h = 0;
    return size >= 16 && size < 1000000 && b[0] == 0xff && b[1] == 0xd8 && b[size - 2] == 0xff &&
           b[size - 1] == 0xd9 && library_image_size(b, (unsigned)size, &w, &h) && w == 960 &&
           h == 540;
}
static __declspec(noinline) int workshop_make_jpeg(const LibraryEntry *e,
                                                   const WCHAR *destination) {
    static WCHAR path[1024], temp[1100];
    const WCHAR *photo = equals(e->screenshot, "preview.png") ? L"preview.png" : L"preview.jpg";
    if (!library_path(path, e->folder, photo))
        return 0;
    unsigned n = library_read(path, library_image_bytes, 20U * 1024U * 1024U), w = 0, h = 0;
    if (!n || !library_image_size(library_image_bytes, n, &w, &h) || w > 4096 || h > 4096 ||
        (equals(e->screenshot, "preview.png") && !save_complete_png(library_image_bytes, n)))
        return 0;
    PFRoots roots = {0};
    int ok = 0;
    void *texture = 0, *small = 0;
    void *c = findclass(p_domain_get(), "UnityEngine", "Texture2D"),
         *byteClass = findclass(p_domain_get(), "System", "Byte");
    texture = c ? p_object_new(c) : 0;
    if (!texture || !pf_root(&roots, texture))
        goto done;
    const char *cp[] = {"System.Int32", "System.Int32"};
    int two = 2;
    void *ca[] = {&two, &two};
    if (!write_void(browser_method(c, ".ctor", "System.Void", 2, cp, 1), texture, ca))
        goto done;
    void *bytes = p_array_new(byteClass, n);
    if (!bytes || !pf_root(&roots, bytes) || preview_array_count(bytes) != n)
        goto done;
    memcpy((unsigned char *)bytes + 32, library_image_bytes, n);
    void *conversion = findclass(p_domain_get(), "UnityEngine", "ImageConversion");
    const char *lp[] = {"UnityEngine.Texture2D", "System.Byte[]", "System.Boolean"};
    unsigned char no = 0;
    void *la[] = {texture, bytes, &no};
    void *loaded =
        pf_invoke(browser_method(conversion, "LoadImage", "System.Boolean", 3, lp, 0), 0, la);
    if (!loaded || !*(unsigned char *)p_object_unbox(loaded))
        goto done;
    small = preview_small_texture(texture, w, h, &roots);
    if (!small)
        goto done;
    texture = 0;
    if (!preview_texture_black(small))
        goto done;
    const char *ep[] = {"UnityEngine.Texture2D", "System.Int32"};
    void *encode = browser_method(conversion, "EncodeToJPG", "System.Byte[]", 2, ep, 0);
    for (int quality = 90; quality >= 30; quality -= 15) {
        PFRoots encodedRoot = {0};
        void *ea[] = {small, &quality};
        void *jpeg = pf_invoke(encode, 0, ea);
        if (!jpeg || !pf_root(&encodedRoot, jpeg))
            break;
        unsigned long long size = preview_array_count(jpeg);
        const unsigned char *b = (unsigned char *)jpeg + 32;
        if (workshop_jpeg_valid(b, size)) {
            unsigned pos = 0;
            if (append(temp, 1100, &pos, destination) && append(temp, 1100, &pos, L".tmp")) {
                DeleteFileW(temp);
                if (save_write_new(temp, b, (unsigned)size)) {
                    ok = MoveFileExW(temp, destination, 1 | 8) != 0;
                    if (!ok)
                        DeleteFileW(temp);
                }
            }
            pf_release(&encodedRoot);
            break;
        }
        pf_release(&encodedRoot);
    }
done:
    preview_destroy(small);
    preview_destroy(texture);
    pf_release(&roots);
    return ok;
}
static void workshop_cleanup_stage(void) {
    workshop_release_connection();
    workshop_relaxed = 0;
    workshop_persist_attempts = 0;
    if (workshop_package[0])
        DeleteFileW(workshop_package);
    if (workshop_jpeg[0])
        DeleteFileW(workshop_jpeg);
    if (workshop_content_dir[0])
        RemoveDirectoryW(workshop_content_dir);
    if (workshop_stage[0])
        RemoveDirectoryW(workshop_stage);
    workshop_package[0] = workshop_jpeg[0] = workshop_content_dir[0] = workshop_stage[0] = 0;
}
static __declspec(noinline) int workshop_prepare(const LibraryEntry *e) {
    WCHAR root[1024];
    if (!coaster_storage_path(root, 1024, L"WorkshopUploads", 1) ||
        !workshop_directory(workshop_stage, root, e->folder) ||
        !workshop_directory(workshop_content_dir, workshop_stage, L"content"))
        return 0;
    unsigned n = 0;
    if (!append(workshop_package, 1024, &n, workshop_content_dir) ||
        !append(workshop_package, 1024, &n, L"\\") ||
        !append(workshop_package, 1024, &n, e->folder) ||
        !append(workshop_package, 1024, &n, L".coaster"))
        return 0;
    n = 0;
    if (!append(workshop_jpeg, 1024, &n, workshop_stage) ||
        !append(workshop_jpeg, 1024, &n, L"\\preview.jpg"))
        return 0;
    /* Recover our fixed previous staging files, never touch any source library file. */
    DeleteFileW(workshop_package);
    DeleteFileW(workshop_jpeg);
    if (!coaster_export_to(e, workshop_content_dir))
        return 0;
    if (!MoveFileExW(coaster_export_result, workshop_package, 8)) {
        DeleteFileW(coaster_export_result);
        return 0;
    }
    if (!workshop_make_jpeg(e, workshop_jpeg))
        return 0;
    return workshop_utf8_path(workshop_content_dir, workshop_content_utf8,
                              sizeof(workshop_content_utf8)) &&
           workshop_utf8_path(workshop_jpeg, workshop_preview_utf8, sizeof(workshop_preview_utf8));
}
static int workshop_setting(WorkshopBool ok, const char *name) {
    if (!ok) {
        logtext("WORKSHOP SETTING REJECTED: ");
        logtext(name);
        nl();
    }
    return ok != 0;
}
static int workshop_submit_current(void) {
    if(workshop_dependency_index != WORKSHOP_REQUIRED_ITEM_COUNT) return 0;
    if (!workshop_logged_on(workshop_user) || workshop_owner(workshop_user) != workshop_account)
        return 0;
    WorkshopId update = workshop_update(workshop_ugc, 2185060, workshop_item);
    if (!update || update == ~(WorkshopId)0)
        return 0;
    const char *tagNames[] = {"Templates", workshop_tag_type};
    WorkshopTags tags = {tagNames, 2};
    /* Public = 0; requested upload policy. Applies to explicit uploads/updates. */
    if (!workshop_setting(workshop_visibility(workshop_ugc, update, 0), "Public visibility") ||
        !workshop_setting(workshop_title(workshop_ugc, update, workshop_title_text), "Title") ||
        !workshop_setting(workshop_description(workshop_ugc, update, workshop_text),
                          "Description") ||
        !workshop_setting(workshop_tags(workshop_ugc, update, &tags),
                          "Template and coaster type tags") ||
        !workshop_setting(workshop_content(workshop_ugc, update, workshop_content_utf8),
                          "Content folder") ||
        !workshop_setting(workshop_preview(workshop_ugc, update, workshop_preview_utf8),
                          "JPEG preview"))
        return 0;
    workshop_completion_ready = 0;
    workshop_call = workshop_submit(workshop_ugc, update, "Coaster template export");
    if (!workshop_call)
        return 0;
    workshop_relaxed = 0;
    workshop_phase = 2;
    workshop_started = GetTickCount64();
    workshop_message("Uploading coaster to Steam Workshop (Public)...");
    return 1;
}
static int workshop_requires_dlc(void) {
    /* Use the frozen English type, never the currently selected library tab. */
    for (int i = 0; i < 4; ++i)
        if (equals(workshop_tag_type, workshop_fixed_type(i)))
            return 1;
    return 0;
}
static int workshop_start_dlc(void) {
    if (!workshop_add_app_dependency || !workshop_logged_on(workshop_user) ||
        workshop_owner(workshop_user) != workshop_account)
        return 0;
    workshop_completion_ready = 0;
    workshop_call =
        workshop_add_app_dependency(workshop_ugc, workshop_item, WORKSHOP_RIDES_RELICS_APP);
    if (!workshop_call)
        return 0;
    workshop_relaxed = 0;
    workshop_phase = 6;
    workshop_started = GetTickCount64();
    workshop_message("Adding Rides & Relics as required DLC...");
    return 1;
}
static int workshop_start_dependency(void) {
    if(workshop_dependency_index >= WORKSHOP_REQUIRED_ITEM_COUNT) return 0;
    if (!workshop_logged_on(workshop_user) || workshop_owner(workshop_user) != workshop_account)
        return 0;
    workshop_completion_ready = 0;
    workshop_call = workshop_add_dependency(workshop_ugc, workshop_item, workshop_required_items[workshop_dependency_index]);
    if (!workshop_call)
        return 0;
    workshop_relaxed = 0;
    workshop_phase = 5;
    workshop_started = GetTickCount64();
    workshop_message("Adding required Workshop items...");
    return 1;
}
/* 0 = fail, 1 = waiting for the next owned dependency call, 2 = both verified.
 * Never advance on a different parent/child or on a failed Steam response. */
static int workshop_accept_dependency(const WorkshopDependency *result, int failed) {
    if (failed || !result || workshop_dependency_index >= WORKSHOP_REQUIRED_ITEM_COUNT ||
        (result->result != 1 && result->result != 29) ||
        result->parent != workshop_item ||
        result->child != workshop_required_items[workshop_dependency_index]) return 0;
    ++workshop_dependency_index;
    if(workshop_dependency_index == WORKSHOP_REQUIRED_ITEM_COUNT) return 2;
    return workshop_start_dependency() ? 1 : 0;
}
/* DLC is optional metadata on a newly created item. Failure never blocks
 * content submission; existing item updates never call AddAppDependency. */
static void workshop_continue_created(void) {
    workshop_phase = 0;
    if (workshop_created_legal) {
        workshop_cleanup_stage();
        workshop_message(
            "Accept the Steam Workshop legal agreement on Steam, then upload again. The item ID is saved.");
        return;
    }
    if (!workshop_start_dependency()) {
        workshop_cleanup_stage();
        workshop_message(
            "Steam could not start adding the required Workshop items. Upload again to update the same saved item.");
    }
}
static int workshop_begin_prepared(void) {
    if (workshop_phase)
        return 0;
    workshop_relaxed = 0;
    workshop_dependency_index = 0;
    workshop_dlc_warning = 0;
    workshop_created_legal = 0;
    if (workshop_item) {
        workshop_completion_ready = 0;
        workshop_call = workshop_details(workshop_ugc, workshop_item, 0);
        if (!workshop_call) {
            workshop_cleanup_stage();
            workshop_message("Could not check the existing Workshop item. Its ID is unchanged.");
            return 0;
        }
        workshop_phase = 4;
        workshop_started = GetTickCount64();
        workshop_message("Checking the existing Steam Workshop item...");
        return 1;
    }
    workshop_completion_ready = 0;
    workshop_call = workshop_create(workshop_ugc, 2185060, 0);
    logtext("WORKSHOP CREATE call handle: ");
    lognum((UINT_PTR)workshop_call);
    nl();
    if (!workshop_call) {
        workshop_cleanup_stage();
        workshop_message("Steam could not start creating the Workshop item.");
        return 0;
    }
    workshop_phase = 1;
    workshop_started = GetTickCount64();
    workshop_message("Creating private Steam Workshop item...");
    return 1;
}
static __declspec(noinline) void workshop_start(const LibraryEntry *e) {
    if (workshop_phase == 3 && workshop_relaxed) {
        if (!workshop_same_folder(e->folder, workshop_entry.folder)) {
            workshop_message(
                "The previous upload ID needs saving. Retry Upload on that template first.");
            return;
        }
        if (!workshop_save_link(&workshop_entry, workshop_item, workshop_account)) {
            workshop_message(
                "Could not save the Workshop ID. Check library access, then retry Upload on this template.");
            return;
        }
        workshop_phase = 0;
        workshop_cleanup_stage();
    }
    if (workshop_phase) {
        workshop_message(
            "An earlier Steam request is still pending. No additional upload was started.");
        return;
    }
    if (e->readOnly) {
        workshop_message("Only your own templates can be uploaded.");
        return;
    }
    if (save_phase || !management_validate(e)) {
        workshop_message(
            "This template is busy, changed or unavailable. Reopen the library and try again.");
        return;
    }
    if (!workshop_bind()) {
        workshop_message(workshop_bind_error);
        return;
    }
    WorkshopId owner = 0;
    if (!workshop_link(e, &workshop_item, &owner)) {
        workshop_release_connection();
        workshop_message("The Workshop link is invalid. No new item was created.");
        return;
    }
    workshop_account = workshop_owner(workshop_user);
    if (workshop_item && owner != workshop_account) {
        workshop_release_connection();
        workshop_message("This template is linked to another Steam account. No upload was made.");
        return;
    }
    const char *tag = workshop_fixed_type(library_type_index);
    if (!tag) {
        workshop_release_connection();
        workshop_message("Could not identify the coaster type. Reopen its template library.");
        return;
    }
    library_copy(workshop_tag_type, sizeof(workshop_tag_type), tag);
    workshop_entry = *e;
    void *heading = browser_optional(library_heading, "get_text", "System.String");
    if (!save_utf8(heading, workshop_type, sizeof(workshop_type))) {
        workshop_release_connection();
        workshop_message("Could not read the coaster type.");
        return;
    }
    unsigned n = length(workshop_type);
    const char *suffix = " Templates";
    unsigned suffixSize = length(suffix);
    if (n > suffixSize && equals(workshop_type + n - suffixSize, suffix))
        workshop_type[n - suffixSize] = 0;
    if (!workshop_format_title(e, workshop_type, workshop_title_text,
                               sizeof(workshop_title_text)) ||
        length(workshop_title_text) > 127) {
        workshop_release_connection();
        workshop_message(
            "The coaster name and type must fit within Steam's 127 UTF-8 byte title limit. Shorten the name first.");
        return;
    }
    /* Freeze the full displayed price into the Steam Workshop description for
     * this upload. Keep entry.json untouched: subscribers recompute their live
     * station+support+decoration totals when browsing their template library. */
    char uploadTotal[48] = {0};
    if (!library_total_price(&workshop_entry, library_station_build_cost,
                             uploadTotal, sizeof(uploadTotal))) {
        workshop_release_connection();
        workshop_message("Could not calculate the complete template cost. Reopen the coaster templates window and check the blueprint. Nothing uploaded.");
        return;
    }
    if (!workshop_format(&workshop_entry, workshop_type, uploadTotal,
                         workshop_text, sizeof(workshop_text)) ||
        !workshop_prepare(e)) {
        workshop_cleanup_stage();
        workshop_message(
            "Could not prepare the coaster package or JPEG preview. Nothing uploaded.");
        return;
    }
    workshop_begin_prepared();
}
static __declspec(noinline) void workshop_tick(void) {
    if (!workshop_phase || GetTickCount64() < workshop_due)
        return;
    workshop_due = GetTickCount64() + 100;
    if (workshop_phase == 3) {
        if (workshop_relaxed)
            return;
        workshop_due = GetTickCount64() + 5000;
        if (workshop_save_link(&workshop_entry, workshop_item, workshop_account)) {
            workshop_phase = 0;
            workshop_cleanup_stage();
            workshop_message("Workshop ID saved. Upload again to finish uploading.");
        } else if (++workshop_persist_attempts >= 6) {
            workshop_relaxed = 1;
            workshop_message(
                "Workshop ID could not be saved. Library unlocked; fix folder access and retry Upload on this template.");
        }
        return;
    }
    WorkshopBool failed = 0;
    if (!workshop_completed(workshop_utils, workshop_call, &failed)) {
        if (workshop_phase == 6 && GetTickCount64() - workshop_started >= 600000) {
            workshop_dlc_warning = 1;
            browser_log(
                "WORKSHOP: required DLC metadata timed out; continuing upload, add Rides & Relics manually if absent.");
            workshop_continue_created();
            return;
        }
        if (GetTickCount64() - workshop_started >= 600000 && !workshop_relaxed) {
            if (workshop_phase == 1) {
                workshop_relaxed = 1;
                workshop_message(
                    "Steam creation is delayed. Library unlocked; still waiting for its ID to avoid creating duplicates. Check the mod log.");
            } else {
                /* A deadline is not cancellation: Steam may still read the staged
                 * files or finish this call. Retain ownership until completion,
                 * while allowing the library to be used. workshop_start refuses
                 * another upload while this phase remains pending. */
                workshop_relaxed = 1;
                workshop_message(
                    "Steam is delayed. Library unlocked; the upload files and connection are retained while waiting. No additional upload can start yet.");
            }
        }
        return;
    }
    int phase = workshop_phase;
    workshop_phase = 0;
    if (failed && phase != 6) {
        workshop_cleanup_stage();
        workshop_message(
            "Steam completion could not be verified. Check Steam and the mod log before retrying.");
        return;
    }
    if (phase == 6) {
        WorkshopAppDependency result = {0};
        if (failed ||
            !workshop_result(workshop_utils, workshop_call, &result, sizeof(result), 3414,
                             &failed) ||
            failed || result.result != 1 || result.item != workshop_item ||
            result.app != WORKSHOP_RIDES_RELICS_APP) {
            workshop_dlc_warning = 1;
            browser_log(
                "WORKSHOP: required DLC metadata not confirmed; continuing upload, add Rides & Relics manually if absent.");
        } else {
            browser_log("WORKSHOP: Rides & Relics required DLC verified.");
        }
        workshop_continue_created();
        return;
    }
    if (phase == 5) {
        WorkshopDependency result = {0};
        int progression = workshop_result(workshop_utils, workshop_call, &result, sizeof(result),
                                         3412, &failed)
                              ? workshop_accept_dependency(&result, failed) : 0;
        if (!progression) {
            logtext("WORKSHOP REQUIRED ITEMS: completion or next request failed at index ");
            lognum(workshop_dependency_index);
            nl();
            workshop_cleanup_stage();
            workshop_message("Both required Workshop items could not be confirmed. The item ID is saved; upload again to retry.");
            return;
        }
        if(progression == 1) return; /* Wait for the second owned completion. */
        if (!workshop_submit_current()) {
            workshop_cleanup_stage();
            workshop_message("Steam rejected the upload settings. The Workshop ID is saved; upload again to retry.");
        }
        return;
    }
    if (phase == 4) {
        /* RequestUGCDetails starts with SteamUGCDetails_t: ID at 0, EResult at 8,
   * creator/consumer App IDs at 16/20. Read only its verified prefix. */
        static unsigned char details[65536];
        WorkshopId id = 0;
        int result = 0;
        unsigned app = 0;
        unsigned size = workshop_completion_size;
        if (size < 24 || size > sizeof(details) ||
            !workshop_result(workshop_utils, workshop_call, details, (int)size, 3402, &failed) ||
            failed) {
            workshop_cleanup_stage();
            workshop_message("Could not verify the existing Workshop item. Its ID is unchanged.");
            return;
        }
        memcpy(&id, details, 8);
        memcpy(&result, details + 8, 4);
        memcpy(&app, details + 20, 4);
        logtext("WORKSHOP ITEM CHECK: result=");
        lognum((UINT_PTR)(result > 0 ? result : 0));
        nl();
        if (id != workshop_item && !(result == 9 && id == 0)) {
            workshop_cleanup_stage();
            workshop_message(
                "Workshop returned a different item. No update or replacement was made.");
            return;
        }
        if (result == 9) {
            if (!workshop_save_link(&workshop_entry, 0, 0)) {
                workshop_cleanup_stage();
                workshop_message(
                    "The Workshop item was deleted, but its old ID could not be cleared. Try again.");
                return;
            }
            workshop_item = 0;
            workshop_message(
                "Previous Workshop item was deleted. Creating a replacement...");
            workshop_begin_prepared();
            return;
        }
        if (result != 1 || app != 2185060) {
            workshop_cleanup_stage();
            workshop_message(
                "The Workshop item could not be accessed. Its ID is unchanged; no duplicate was created.");
            return;
        }
        if (!workshop_start_dependency()) {
            workshop_cleanup_stage();
            workshop_message(
                "Steam could not start adding the required Workshop items. The saved Workshop ID is unchanged.");
        }
        return;
    }
    if (phase == 1) {
        WorkshopCreated result = {0};
        if (!workshop_result(workshop_utils, workshop_call, &result, sizeof(result), 3403,
                             &failed) ||
            failed || result.result != 1 || !result.id) {
            logtext("WORKSHOP CREATE RESULT: ");
            lognum((UINT_PTR)(result.result > 0 ? result.result : 0));
            nl();
            workshop_cleanup_stage();
            workshop_message("Steam could not create the Workshop item. Check the mod log.");
            return;
        }
        workshop_item = result.id;
        logtext("WORKSHOP CREATED ID: ");
        lognum((UINT_PTR)workshop_item);
        nl();
        if (!workshop_save_link(&workshop_entry, workshop_item, workshop_account)) {
            workshop_phase = 3;
            workshop_relaxed = 0;
            workshop_persist_attempts = 0;
            workshop_message(
                "Steam created the item, but its ID could not be saved. Retrying locally; do not create another item.");
            return;
        }
        workshop_created_legal = result.legal;
        if (workshop_requires_dlc()) {
            if (workshop_start_dlc())
                return;
            workshop_dlc_warning = 1;
            browser_log(
                "WORKSHOP: required DLC call unavailable; continuing upload, add Rides & Relics manually if absent.");
        }
        workshop_continue_created();
        return;
    }
    WorkshopSubmitted result = {0};
    if (!workshop_result(workshop_utils, workshop_call, &result, sizeof(result), 3404, &failed) ||
        failed || result.result != 1 || (result.id && result.id != workshop_item)) {
        logtext("WORKSHOP SUBMIT RESULT: ");
        lognum((UINT_PTR)(result.result > 0 ? result.result : 0));
        nl();
        workshop_cleanup_stage();
        workshop_message(
            "Steam upload failed. The Workshop ID is saved; uploading again updates the same item.");
        return;
    }
    workshop_cleanup_stage();
    workshop_message(
        result.legal
            ? "Accept the Steam Workshop legal agreement on Steam, then upload again. The item ID is saved."
        : workshop_dlc_warning
            ? "Coaster uploaded (Public). Add Rides & Relics under Required DLC on Steam if it is missing."
            : "Coaster uploaded to Steam Workshop (Public).");
}
