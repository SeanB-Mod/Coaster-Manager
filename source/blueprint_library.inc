/* Fixed discovery/display capacities; image residency is independently bounded. */
#define LIBRARY_DISPLAY_LIMIT 512U
#define LIBRARY_SOURCE_LIMIT 2048U
#define LIBRARY_METADATA_LIMIT (64U * 1024U)
#define LIBRARY_SCAN_BYTE_LIMIT (512U * 1024U * 1024U)
static int library_scan_bytes_fit(unsigned used, unsigned file) {
    return used <= LIBRARY_SCAN_BYTE_LIMIT && file <= 25U * 1024U * 1024U &&
           file <= LIBRARY_SCAN_BYTE_LIMIT - used;
}
/* LocalLow discovery only reads fixed files in non-reparse UUID directories. */
typedef struct {
    DWORD attributes, times[6], sizeHigh, sizeLow, reserved[2];
    WCHAR name[260], alternate[14];
} LibraryFindData;
DLLIMPORT DWORD WINAPI GetEnvironmentVariableW(const WCHAR *, WCHAR *, DWORD);
DLLIMPORT DWORD WINAPI GetFileAttributesW(const WCHAR *);
DLLIMPORT HANDLE WINAPI FindFirstFileW(const WCHAR *, LibraryFindData *);
DLLIMPORT BOOL WINAPI FindNextFileW(HANDLE, LibraryFindData *);
DLLIMPORT BOOL WINAPI FindClose(HANDLE);
#include "blueprint_display_stats.inc"
typedef struct {
    WCHAR folder[33];
    char name[401], screenshot[16], sha[65];
    long long definition;
    int readOnly, source;
    WCHAR package[1024];
    char packageSha[65];
    BlueprintDisplayStats stats;
    int componentsPrice;          /* Display-only cached supports + decorations */
    unsigned char componentsPriceChecked;
    unsigned char componentsPriceValid;
} LibraryEntry;
static void blueprint_label_stats(BlueprintDisplayStats *stats);
static LibraryEntry library_entries[LIBRARY_DISPLAY_LIMIT];
static unsigned library_count, library_index, library_skipped;
static UINT_PTR library_catalogue_until;
static long long library_catalogue_definition;
static LibraryEntry library_catalogue_entries[LIBRARY_DISPLAY_LIMIT];
static unsigned library_catalogue_count, library_catalogue_skipped;
static WCHAR library_catalogue_root[768];
static void library_catalogue_invalidate(void) {
    library_catalogue_until = 0;
}
static WCHAR library_root[768];
static WCHAR library_local_ids[LIBRARY_SOURCE_LIMIT][33];
static unsigned library_local_count;
static unsigned coaster_manual_seen;
static void coaster_workshop_scan(long long definition);
static void coaster_manual_scan(long long definition);
static unsigned library_entry_read(const LibraryEntry *e, const WCHAR *file, unsigned char *out,
                                   unsigned capacity);
static unsigned char library_bytes[4U * 1024U * 1024U + 1];
static unsigned char library_plan[64 + WRITE_MAX_SUPPORTS * 72];
static PFRoots library_controls, library_assets;
static void *library_slot_image[6], *library_slot_backdrop[6], *library_slot_camera[6];
static void *library_previous, *library_next;
static int library_busy(void) {
    return management_busy() || !placement_prepare_retry(0);
}
static int library_path(WCHAR *out, const WCHAR *folder, const WCHAR *file) {
    unsigned n = 0;
    return append(out, 1024, &n, library_root) && append(out, 1024, &n, L"\\") &&
           append(out, 1024, &n, folder) && append(out, 1024, &n, L"\\") &&
           append(out, 1024, &n, file);
}
static unsigned library_read(const WCHAR *path, unsigned char *out, unsigned capacity) {
    DWORD attr = GetFileAttributesW(path);
    if (attr == 0xffffffffU || (attr & (0x400 | 0x10)))
        return 0;
    HANDLE h = CreateFileW(path, 0x80000000, FILE_SHARE_READ, 0, 3, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    DWORD n = 0;
    BOOL ok = ReadFile(h, out, capacity + 1, &n, 0);
    CloseHandle(h);
    return ok && n <= capacity ? n : 0;
}
/* Read the saved blueprint, not the preview picture or entry.json statistics.
 * This is a lazy, per-entry, read-only price estimate. The native game remains
 * responsible for all transactions. Never change or rewrite template files. */
static int library_component_price(LibraryEntry *e, int *price) {
    if (!e || !price)
        return 0;
    if (e->componentsPriceChecked) {
        if (!e->componentsPriceValid)
            return 0;
        *price = e->componentsPrice;
        return 1;
    }
    e->componentsPriceChecked = 1;
    unsigned n = library_entry_read(e, L"blueprint.json", library_bytes,
                                    4U * 1024U * 1024U);
    unsigned root = n ? lj_parse(library_bytes, n) : 0;
    long long version = 0, definition = 0;
    unsigned support = root ? lj_member(root, "TrackSupportData") : 0;
    unsigned pieces = root ? lj_member(root, "TrackPieceData") : 0;
    if (!root || !lj_is(lj_member(root, "schema"), "tpm-coaster-snapshot") ||
        !lj_integer(root, "schemaVersion", &version) || version != 2 ||
        !lj_int(lj_member(root, "sourceStationDefinitionId"), &definition) ||
        definition != e->definition || !support || !pieces ||
        lj_tokens[support].kind != 2 || lj_tokens[pieces].kind != 2 ||
        !lj_tokens[support].count || lj_tokens[support].count > WRITE_MAX_SUPPORTS ||
        lj_tokens[support].count != lj_tokens[pieces].count)
        return 0;
    unsigned supports = lj_tokens[support].count, decorations = 0;
    unsigned item = lj_tokens[pieces].child;
    for (unsigned i = 0; i < supports; ++i, item = lj_tokens[item].next) {
        long long decorationId = 0;
        if (!item || !lj_int(lj_member(item, "TrackPieceDecorationDefinition"),
                              &decorationId))
            return 0;
        if (decorationId)
            ++decorations;
    }
    long long savedSupports = 0, savedPieces = 0;
    unsigned sc = lj_member(root, "supportCount"), pc = lj_member(root, "pieceCount");
    if ((sc && (!lj_int(sc, &savedSupports) || savedSupports != (long long)supports)) ||
        (pc && (!lj_int(pc, &savedPieces) || savedPieces != (long long)supports)))
        return 0;
    if (!blueprint_component_display_cost(supports, decorations, &e->componentsPrice))
        return 0;
    e->componentsPriceValid = 1;
    *price = e->componentsPrice;
    return 1;
}
/* Single source of truth for the displayed total. The source entry.json cost
 * stays track-only so installed Workshop packages cannot double-add the extras. */
static int library_total_price(LibraryEntry *entry, int stationCost,
                               char *out, unsigned capacity) {
    int components = 0;
    return library_component_price(entry, &components) &&
           blueprint_template_total_cost(entry->stats.value[0], stationCost,
                                         components, out, capacity);
}
static int library_copy(char *out, unsigned cap, const char *s) {
    if (!s)
        return 0;
    unsigned n = length(s);
    if (!n || n >= cap)
        return 0;
    memcpy(out, s, n + 1);
    return 1;
}
static int library_name(const char *s) {
    unsigned count = 0;
    for (unsigned i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c == 127)
            return 0;
        if ((c & 0xc0) != 0x80)
            ++count;
    }
    return count && count <= 100;
}
/* Small SHA-256 implementation: entry identity is the exact validated plan bytes. */
static unsigned library_rotr(unsigned x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}
static void library_sha(const unsigned char *data, unsigned n, char *out) {
    static const unsigned constants[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    unsigned state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    unsigned blocks = (n + 9 + 63) / 64;
    for (unsigned block = 0; block < blocks; ++block) {
        unsigned w[64] = {0};
        for (unsigned j = 0; j < 64; ++j) {
            unsigned at = block * 64 + j, c = at < n ? data[at] : at == n ? 128 : 0;
            if (at >= blocks * 64 - 8) {
                unsigned shift = (blocks * 64 - 1 - at) * 8;
                c = (unsigned)(((unsigned long long)n * 8) >> shift) & 255;
            }
            w[j / 4] |= c << ((3 - j % 4) * 8);
        }
        for (unsigned j = 16; j < 64; ++j) {
            unsigned a = w[j - 15], b = w[j - 2];
            w[j] = w[j - 16] + (library_rotr(a, 7) ^ library_rotr(a, 18) ^ (a >> 3)) + w[j - 7] +
                   (library_rotr(b, 17) ^ library_rotr(b, 19) ^ (b >> 10));
        }
        unsigned a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5],
                 g = state[6], h = state[7];
        for (unsigned j = 0; j < 64; ++j) {
            unsigned t = h + (library_rotr(e, 6) ^ library_rotr(e, 11) ^ library_rotr(e, 25)) +
                         ((e & f) ^ (~e & g)) + constants[j] + w[j];
            unsigned u = (library_rotr(a, 2) ^ library_rotr(a, 13) ^ library_rotr(a, 22)) +
                         ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t;
            d = c;
            c = b;
            b = a;
            a = t + u;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    const char *hex = "0123456789abcdef";
    for (unsigned i = 0; i < 64; ++i)
        out[i] = hex[(state[i / 8] >> ((7 - i % 8) * 4)) & 15];
    out[64] = 0;
}
static __declspec(noinline) void library_scan(long long definition) {
    library_catalogue_invalidate();
    library_count = library_index = library_skipped = library_local_count = coaster_manual_seen = 0;
    if (!coaster_storage_path(library_root, 768, 0, 1))
        return;
    unsigned pos = 0;
    if (GetFileAttributesW(library_root) & 0x400)
        return;
    WCHAR path[1024];
    pos = 0;
    if (!append(path, 1024, &pos, library_root) || !append(path, 1024, &pos, L"\\*"))
        return;
    LibraryFindData fd;
    HANDLE find = FindFirstFileW(path, &fd);
    if (find == INVALID_HANDLE_VALUE)
        return;
    unsigned scanned = 0;
    do {
        if (!(fd.attributes & 0x10) || (fd.attributes & 0x400))
            continue;
        unsigned chars = 0;
        while (chars < 33 && fd.name[chars])
            ++chars;
        if (chars != 32)
            continue;
        int valid = 1;
        char id[33];
        for (unsigned i = 0; i < 32; ++i) {
            unsigned c = fd.name[i];
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f'))
                valid = 0;
            id[i] = (char)c;
        }
        id[32] = 0;
        if (!valid)
            continue;
        if (++scanned > LIBRARY_SOURCE_LIMIT) {
            ++library_skipped;
            break;
        }
        if (!library_path(path, fd.name, L"entry.json")) {
            ++library_skipped;
            continue;
        }
        unsigned n = library_read(path, library_bytes, LIBRARY_METADATA_LIMIT),
                 root = n ? lj_parse(library_bytes, n) : 0;
        long long version = 0, type = 0;
        static LibraryEntry entry;
        memset(&entry, 0, sizeof(entry));
        if (!root || !lj_integer(root, "schemaVersion", &version) || version != 1 ||
            !lj_is(lj_member(root, "id"), id) ||
            !lj_int(lj_member(root, "stationDefinitionId"), &type) || !type ||
            !library_copy(entry.name, sizeof(entry.name),
                          lj_string_value(lj_member(root, "name"))) ||
            !library_name(entry.name) ||
            !library_copy(entry.screenshot, sizeof(entry.screenshot),
                          lj_string_value(lj_member(root, "screenshot"))) ||
            (!equals(entry.screenshot, "preview.png") &&
             !equals(entry.screenshot, "preview.jpg")) ||
            !library_copy(entry.sha, sizeof(entry.sha),
                          lj_string_value(lj_member(root, "planSha256"))) ||
            length(entry.sha) != 64) {
            ++library_skipped;
            continue;
        }
        for (unsigned i = 0; i < 64; ++i)
            if (lj_hex((unsigned char)entry.sha[i]) < 0)
                valid = 0;
        if (!valid) {
            ++library_skipped;
            continue;
        }
        if (library_local_count < LIBRARY_SOURCE_LIMIT)
            memcpy(library_local_ids[library_local_count++], fd.name, 66);
        if (type != definition)
            continue;
        if (library_count >= LIBRARY_DISPLAY_LIMIT) {
            ++library_skipped;
            continue;
        }
        memcpy(entry.folder, fd.name, 66);
        entry.definition = type;
        entry.readOnly = library_metadata_readonly(root);
        blueprint_stats_read(root, &entry.stats);
        library_entries[library_count++] = entry;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    coaster_manual_scan(definition);
    coaster_workshop_scan(definition);
    /* Stable name order, with UUID tie break; discovery order is unspecified. */
    for (unsigned i = 1; i < library_count; ++i) {
        static LibraryEntry value;
        value = library_entries[i];
        unsigned j = i;
        while (j) {
            const char *a = library_entries[j - 1].name, *b = value.name;
            while (*a && *a == *b) {
                ++a;
                ++b;
            }
            if ((unsigned char)*a <= (unsigned char)*b)
                break;
            library_entries[j] = library_entries[j - 1];
            --j;
        }
        library_entries[j] = value;
    }
    browser_log("LIBRARY: scan complete");
    logtext("TEMPLATE UI: matching=");
    lognum(library_count);
    logtext(" skipped=");
    lognum(library_skipped);
    nl();
}
/* Short-lived display index only. Build/upload retain their full validation. */
static void library_open_scan(long long definition) {
    WCHAR currentRoot[768];
    int rootMatches = coaster_storage_path(currentRoot, 768, 0, 1);
    for (unsigned i = 0; rootMatches && i < 768; ++i) {
        if (currentRoot[i] != library_catalogue_root[i])
            rootMatches = 0;
        if (!currentRoot[i])
            break;
    }
    if (rootMatches && definition == library_catalogue_definition &&
        GetTickCount64() < library_catalogue_until) {
        memcpy(library_entries, library_catalogue_entries,
               library_catalogue_count * sizeof(library_entries[0]));
        memcpy(library_root, library_catalogue_root, sizeof(library_root));
        library_count = library_catalogue_count;
        library_skipped = library_catalogue_skipped;
        library_index = 0;
        browser_log("CATALOGUE: reused recent display index (independent of UI rebuild)");
        return;
    }
    browser_log(!rootMatches ? "CATALOGUE: fresh scan; source root changed or uncached"
                : definition != library_catalogue_definition
                    ? "CATALOGUE: fresh scan; coaster type changed"
                : !library_catalogue_until ? "CATALOGUE: fresh scan; explicitly invalidated"
                                           : "CATALOGUE: fresh scan; five-second index expired");
    library_scan(definition);
    library_catalogue_definition = definition;
    memcpy(library_catalogue_entries, library_entries, library_count * sizeof(library_entries[0]));
    memcpy(library_catalogue_root, library_root, sizeof(library_root));
    library_catalogue_count = library_count;
    library_catalogue_skipped = library_skipped;
    library_catalogue_until = GetTickCount64() + 5000;
}
static void library_read_preview_curve(unsigned metadataRoot) {
    footprint_curve_count = 0;
    unsigned curve = lj_member(metadataRoot, "previewCurve");
    if (curve && lj_tokens[curve].kind == 2 && lj_tokens[curve].count >= 2 &&
        lj_tokens[curve].count <= CURVE_PREVIEW_MAX) {
        unsigned count = 0;
        int valid = 1;
        for (unsigned point = lj_tokens[curve].child; point && valid;
             point = lj_tokens[point].next) {
            if (lj_tokens[point].kind != 2 || lj_tokens[point].count != 3) {
                valid = 0;
                break;
            }
            unsigned v = lj_tokens[point].child;
            for (unsigned k = 0; k < 3; ++k) {
                double n = lj_tokens[v].number;
                if (lj_tokens[v].kind != 5 || !(n >= -4096 && n <= 4096)) {
                    valid = 0;
                    break;
                }
                footprint_curve[count][k] = (float)n;
                v = lj_tokens[v].next;
            }
            ++count;
        }
        if (valid)
            footprint_curve_count = count;
    }
}
static int library_read_colour(unsigned root) {
    memset(&colour_selected, 0, sizeof(colour_selected));
    unsigned node = lj_member(root, "trackColour");
    if (!node)
        return 1;
    unsigned custom = lj_member(node, "custom"), rgba = lj_member(node, "rgba");
    if (lj_tokens[node].kind != 1 || !custom || !rgba || lj_tokens[rgba].kind != 2 ||
        lj_tokens[rgba].count != 4)
        return 0;
    if (lj_tokens[custom].kind != 4)
        return 0;
    colour_selected.custom = (unsigned char)(lj_tokens[custom].number == 1);
    unsigned v = lj_tokens[rgba].child;
    for (unsigned i = 0; i < 4; ++i) {
        if (!v || lj_tokens[v].kind != 5 || !(lj_tokens[v].number >= 0 && lj_tokens[v].number <= 1))
            return 0;
        colour_selected.rgba[i] = (float)lj_tokens[v].number;
        v = lj_tokens[v].next;
    }
    colour_selected.present = 1;
    return colour_valid(&colour_selected);
}
static __declspec(noinline) int library_load_plan(void) {
    if (library_index >= library_count || management_busy() || !placement_prepare_retry(1))
        return 0;
    LibraryEntry *e = &library_entries[library_index];
    WCHAR path[1024];
    unsigned metadataSize =
        library_entry_read(e, L"entry.json", library_bytes, LIBRARY_METADATA_LIMIT);
    unsigned metadataRoot = metadataSize ? lj_parse(library_bytes, metadataSize) : 0;
    library_read_preview_curve(metadataRoot);
    if (!library_read_colour(metadataRoot)) {
        browser_log("LIBRARY REJECT: malformed track colour metadata");
        return 0;
    }
    if (!library_path(path, e->folder, L"blueprint.json"))
        return 0;
    unsigned n = library_entry_read(e, L"blueprint.json", library_bytes, 4U * 1024U * 1024U);
    unsigned root = n ? lj_parse(library_bytes, n) : 0,
             bytes = root ? library_prepare(root, library_plan) : 0;
    char hash[65];
    if (!bytes) {
        browser_log("LIBRARY REJECT: source JSON/frame/native fields invalid or unavailable");
        return 0;
    }
    library_sha(library_plan, bytes, hash);
    long long type = 0;
    memcpy(&type, library_plan + 24, 8);
    if (type != browser_definition || type != e->definition) {
        browser_log("LIBRARY REJECT: station definition changed");
        return 0;
    }
    if (!equals(hash, e->sha)) {
        browser_log("LIBRARY REJECT: validated plan SHA-256 differs from saved entry");
        return 0;
    }
    return write_decode_selection(library_plan, bytes);
}
static void preview_destroy(void *o);
#define LIBRARY_TEXTURE_CACHE 12
static struct {
    char sha[65];
    void *texture;
    PFRoots roots;
    UINT_PTR used;
} library_texture_cache[LIBRARY_TEXTURE_CACHE];
static UINT_PTR library_texture_clock;
static unsigned library_texture_hits, library_texture_decodes;
static void library_decode_release(void) {
    for (unsigned i = 0; i < library_assets.n; ++i) {
        void *o = p_gchandle_get_target(library_assets.h[i]);
        if (o && pf_class(p_object_get_class(o), "UnityEngine.Texture2D"))
            preview_destroy(o);
    }
    pf_release(&library_assets);
}
static void library_clear_assets(void) {
    for (unsigned k = 0; k < 6; ++k)
        if (library_slot_image[k])
            browser_set(library_slot_image[k], "set_texture", "UnityEngine.Texture", 0);
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "Destroy", "System.Void", 1, p, 0) : 0;
    for (unsigned i = 0; i < library_assets.n; ++i) {
        void *o = p_gchandle_get_target(library_assets.h[i]);
        if (o && (pf_class(p_object_get_class(o), "UnityEngine.Texture2D") ||
                  pf_class(p_object_get_class(o), "UnityEngine.Sprite"))) {
            void *a[] = {o};
            write_void(m, 0, a);
        }
    }
    pf_release(&library_assets);
}
static void library_texture_cleanup(void) {
    for (unsigned i = 0; i < LIBRARY_TEXTURE_CACHE; ++i) {
        if (library_texture_cache[i].texture)
            preview_destroy(library_texture_cache[i].texture);
        pf_release(&library_texture_cache[i].roots);
    }
    memset(library_texture_cache, 0, sizeof(library_texture_cache));
    library_texture_clock = 0;
}
static unsigned char library_image_bytes[20U * 1024U * 1024U + 1];
static unsigned library_be32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}
static int library_image_size(const unsigned char *b, unsigned n, unsigned *w, unsigned *h) {
    if (n >= 24 && b[0] == 137 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G' && b[4] == 13 &&
        b[5] == 10 && b[6] == 26 && b[7] == 10 && library_be32(b + 8) == 13 && b[12] == 'I' &&
        b[13] == 'H' && b[14] == 'D' && b[15] == 'R') {
        *w = library_be32(b + 16);
        *h = library_be32(b + 20);
    } else if (n >= 4 && b[0] == 255 && b[1] == 216) {
        unsigned at = 2;
        int found = 0;
        while (at + 4 <= n) {
            if (b[at++] != 255)
                return 0;
            while (at < n && b[at] == 255)
                ++at;
            if (at >= n)
                return 0;
            unsigned marker = b[at++];
            if (marker == 217 || marker == 218)
                break;
            if (marker == 1 || (marker >= 208 && marker <= 215))
                continue;
            if (at + 2 > n)
                return 0;
            unsigned len = ((unsigned)b[at] << 8) | b[at + 1];
            if (len < 2 || len > n - at)
                return 0;
            if ((marker >= 192 && marker <= 195) || (marker >= 197 && marker <= 199) ||
                (marker >= 201 && marker <= 203) || (marker >= 205 && marker <= 207)) {
                if (len < 8)
                    return 0;
                *h = ((unsigned)b[at + 3] << 8) | b[at + 4];
                *w = ((unsigned)b[at + 5] << 8) | b[at + 6];
                found = 1;
                break;
            }
            at += len;
        }
        if (!found)
            return 0;
    } else
        return 0;
    return *w && *h && *w <= 4096 && *h <= 4096;
}
static __declspec(noinline) int library_show_image(void *target) {
    /* Preview destinations are owned row RawImages, never the header Image. */
    int owned = 0;
    for (unsigned k = 0; k < 6; ++k)
        if (target && target == library_slot_image[k])
            owned = 1;
    if (!owned || target == library_type_image ||
        !pf_class(p_object_get_class(target), "UnityEngine.UI.RawImage")) {
        browser_log("PREVIEW: rejected non-row image target");
        return 0;
    }
    LibraryEntry *e = &library_entries[library_index];
    WCHAR path[1024];
    const WCHAR *name = equals(e->screenshot, "preview.png") ? L"preview.png" : L"preview.jpg";
    if (!library_path(path, e->folder, name))
        return 0;
    unsigned n = library_entry_read(e, name, library_image_bytes, 20U * 1024U * 1024U), w = 0,
             h = 0;
    if (!n || !library_image_size(library_image_bytes, n, &w, &h))
        return 0;
    char imageSha[65];
    library_sha(library_image_bytes, n, imageSha);
    unsigned cacheSlot = 0;
    for (unsigned i = 0; i < LIBRARY_TEXTURE_CACHE; ++i) {
        if (equals(imageSha, library_texture_cache[i].sha) &&
            browser_alive(library_texture_cache[i].texture)) {
            ++library_texture_hits;
            library_texture_cache[i].used = ++library_texture_clock;
            BrowserColor white = {1, 1, 1, 1};
            browser_set(target, "set_color", "UnityEngine.Color", &white);
            return browser_set(target, "set_texture", "UnityEngine.Texture",
                               library_texture_cache[i].texture);
        }
        if (!library_texture_cache[i].texture ||
            library_texture_cache[i].used < library_texture_cache[cacheSlot].used)
            cacheSlot = i;
    }
    library_decode_release();
    void *c = findclass(p_domain_get(), "UnityEngine", "Texture2D"),
         *byte = findclass(p_domain_get(), "System", "Byte");
    void *texture = c ? p_object_new(c) : 0;
    if (!texture || !byte || !pf_root(&library_assets, texture))
        return 0;
    const char *cp[] = {"System.Int32", "System.Int32"};
    int two = 2;
    void *ca[] = {&two, &two};
    if (!write_void(pf_method(c, ".ctor", "System.Void", 2, cp, 1), texture, ca))
        return 0;
    void *array = p_array_new(byte, n);
    if (!array || !pf_root(&library_assets, array))
        return 0;
    /* IL2CPP x64 SZARRAY header: object(16), bounds(8), max_length(8). Same layout used by checked array reader. */
    unsigned long long count = 0;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (const unsigned char *)array + 24, &count, 8,
                           &got) ||
        got != 8 || count != n)
        return 0;
    memcpy((unsigned char *)array + 32, library_image_bytes, n);
    void *convert = findclass(p_domain_get(), "UnityEngine", "ImageConversion");
    const char *lp[] = {"UnityEngine.Texture2D", "System.Byte[]", "System.Boolean"};
    unsigned char yes = 0;
    void *la[] = {texture, array, &yes};
    void *result =
        pf_invoke(convert ? pf_method(convert, "LoadImage", "System.Boolean", 3, lp, 0) : 0, 0, la);
    if (!result || !pf_class(p_object_get_class(result), "System.Boolean") ||
        *(unsigned char *)p_object_unbox(result) != 1)
        return 0;
    /* Compressed source bytes are temporary, not part of the visible cache. */
    for (unsigned i = 0; i < library_assets.n; ++i)
        if (p_gchandle_get_target(library_assets.h[i]) == array) {
            p_gchandle_free(library_assets.h[i]);
            for (unsigned j = i + 1; j < library_assets.n; ++j)
                library_assets.h[j - 1] = library_assets.h[j];
            --library_assets.n;
            break;
        }

    void *width =
        pf_invoke(browser_method(p_object_get_class(texture), "get_width", "System.Int32", 0, 0, 1),
                  texture, 0);
    if (!width || !pf_class(p_object_get_class(width), "System.Int32") ||
        *(int *)p_object_unbox(width) != (int)w)
        return 0;
    void *height = pf_invoke(
        browser_method(p_object_get_class(texture), "get_height", "System.Int32", 0, 0, 1), texture,
        0);
    if (!height || !pf_class(p_object_get_class(height), "System.Int32") ||
        *(int *)p_object_unbox(height) != (int)h)
        return 0;
    if (w != 960 || h != 540) {
        void *small = preview_small_texture(texture, w, h, &library_assets);
        if (!small)
            return 0;
        /* Resampler has already scheduled the source texture for destruction. */
        for (unsigned i = 0; i < library_assets.n; ++i)
            if (p_gchandle_get_target(library_assets.h[i]) == texture) {
                p_gchandle_free(library_assets.h[i]);
                library_assets.h[i] = library_assets.h[--library_assets.n];
                break;
            }
        texture = small;
    }
    if (!pf_root(&library_texture_cache[cacheSlot].roots, texture))
        return 0;
    /* Eviction happens only during a row rebind after old RawImages detach. */
    void *oldTexture = library_texture_cache[cacheSlot].texture;
    if (oldTexture) {
        preview_destroy(oldTexture);
        /* The new root was just appended; retain it while releasing the old. */
        GCHandle keep =
            library_texture_cache[cacheSlot].roots.h[library_texture_cache[cacheSlot].roots.n - 1];
        --library_texture_cache[cacheSlot].roots.n;
        pf_release(&library_texture_cache[cacheSlot].roots);
        library_texture_cache[cacheSlot].roots.h[0] = keep;
        library_texture_cache[cacheSlot].roots.n = 1;
    }
    ++library_texture_decodes;
    library_texture_cache[cacheSlot].texture = texture;
    library_copy(library_texture_cache[cacheSlot].sha, 65, imageSha);
    library_texture_cache[cacheSlot].used = ++library_texture_clock;
    for (unsigned i = 0; i < library_assets.n; ++i)
        if (p_gchandle_get_target(library_assets.h[i]) == texture) {
            p_gchandle_free(library_assets.h[i]);
            library_assets.h[i] = library_assets.h[--library_assets.n];
            break;
        }
    library_decode_release();
    /* RawImage displays the decoded texture directly, with no sprite mesh or atlas UV path. */
    BrowserColor white = {1, 1, 1, 1};
    browser_set(target, "set_color", "UnityEngine.Color", &white);
    if (!browser_set(target, "set_texture", "UnityEngine.Texture", texture))
        return 0;
    logtext("TEMPLATE UI: PREVIEW bound row=");
    lognum(library_index);
    logtext(" texture=960x540 name=");
    logtext(e->name);
    nl();
    return 1;
}
/* Six visible slots keep image memory bounded, regardless of library length.
 * ScrollRect owns motion/clipping; slots are rebound as the viewport moves. */
static PFRoots library_slot_roots[6][5];
static void *library_hover_sprite[6][3];
static void *library_scroll, *library_content, *library_slot_go[6], *library_slot_name[6],
    *library_slot_build[6], *library_slot_delete[6], *library_slot_export[6],
    *library_slot_photo[6], *library_slot_input[6], *library_slot_origin[6],
    *library_slot_values[6][4], *library_slot_price[6];
static unsigned library_first = 0xffffffffU;
static float library_last_normalized = 1;
static float library_view_height, library_row_height;
static void library_row_roots(unsigned k) {
    for (unsigned b = 0; b < 5; ++b)
        if (library_slot_roots[k][b].n < 22) {
            browser_current_roots = &library_slot_roots[k][b];
            return;
        }
    browser_failed = 1;
}
static int library_name_focused(void) {
    for (unsigned k = 0; k < 6; ++k)
        if (library_slot_input[k] && funfair_property_true(library_slot_input[k], "get_isFocused"))
            return 1;
    return 0;
}
static unsigned library_visible_first(unsigned count, float normalized) {
    if (count <= 4)
        return 0;
    if (!(normalized >= 0))
        normalized = 1;
    if (normalized > 1)
        normalized = 1;
    unsigned first = (unsigned)((1 - normalized) * (float)(count - 4));
    if (first)
        --first; /* One buffer row above the viewport. */
    unsigned last = count > 6 ? count - 6 : 0;
    return first > last ? last : first;
}
static void library_list_cleanup(void) {
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "Destroy", "System.Void", 1, p, 0) : 0;
    for (unsigned k = 0; k < 6; ++k) {
        if (library_slot_go[k] && browser_alive(library_slot_go[k])) {
            browser_active(library_slot_go[k], 0);
            void *a[] = {library_slot_go[k]};
            write_void(m, 0, a);
        }
        for (unsigned b = 0; b < 5; ++b)
            pf_release(&library_slot_roots[k][b]);
    }
    memset(library_slot_go, 0, sizeof(library_slot_go));
    memset(library_slot_image, 0, sizeof(library_slot_image));
    memset(library_slot_backdrop, 0, sizeof(library_slot_backdrop));
    memset(library_slot_camera, 0, sizeof(library_slot_camera));
    library_scroll = library_content = 0;
    library_first = 0xffffffffU;
}
static void *library_control_label(void *control) {
    void *g = browser_get(control, "get_gameObject", "UnityEngine.GameObject");
    const char *p[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("TMPro", "TMP_Text");
    void *a[] = {t, &yes};
    return pf_invoke(browser_method(p_object_get_class(g), "GetComponentInChildren",
                                    "UnityEngine.Component", 2, p, 1),
                     g, a);
}
static int library_list_construct(void *parent, void *font) {
    memset(library_hover_sprite, 0, sizeof(library_hover_sprite));
    browser_current_roots = &library_controls;
    library_view_height = browser_height - 190;
    library_row_height = library_view_height / 4;
    void *viewport = browser_make("Template list viewport", parent, 0, -30, browser_width - 90,
                                  library_view_height, .5f, .5f);
    void *viewRect = browser_get(viewport, "get_transform", "UnityEngine.Transform");
    browser_add(viewport, "UnityEngine.UI", "RectMask2D");
    library_scroll = browser_add(viewport, "UnityEngine.UI", "ScrollRect");
    void *content = browser_make("Template rows", viewRect, 0, 0, browser_width - 90,
                                 library_view_height, .5f, 1);
    library_content = browser_get(content, "get_transform", "UnityEngine.Transform");
    BrowserV2 pivot = {.5f, 1};
    browser_set(library_content, "set_pivot", "UnityEngine.Vector2", &pivot);
    browser_set(library_scroll, "set_viewport", "UnityEngine.RectTransform", viewRect);
    browser_set(library_scroll, "set_content", "UnityEngine.RectTransform", library_content);
    unsigned char no = 0, yes = 1;
    int clamped = 2;
    browser_set(library_scroll, "set_horizontal", "System.Boolean", &no);
    browser_set(library_scroll, "set_vertical", "System.Boolean", &yes);
    browser_set(library_scroll, "set_inertia", "System.Boolean", &no);
    browser_set(library_scroll, "set_movementType", "UnityEngine.UI.ScrollRect.MovementType",
                &clamped);
    float sensitivity = 90;
    browser_set(library_scroll, "set_scrollSensitivity", "System.Single", &sensitivity);
    BrowserColor transparent = {1, 1, 1, 0};
    browser_image(viewport, transparent);
    browser_active(content, 1);
    browser_active(viewport, 1);
    for (unsigned k = 0; k < 6; ++k) {
        library_row_roots(k);
        void *g = browser_make("Template row", library_content, 0,
                               -library_row_height * ((float)k + .5f), browser_width - 110,
                               library_row_height, .5f, 1);
        library_slot_go[k] = g;
        void *rt = browser_get(g, "get_transform", "UnityEngine.Transform");
        library_row_roots(k);
        void *photo = browser_make("Framed coaster photo", rt, -722, 0, 456, 267, .5f, .5f);
        void *pr = browser_get(photo, "get_transform", "UnityEngine.Transform");
        BrowserV3 angle = {0, 0, k % 2 ? 2.f : -2.f};
        browser_set(pr, "set_localEulerAngles", "UnityEngine.Vector3", &angle);
        BrowserColor white = {1, 1, 1, 1};
        void *frame = browser_image(photo, white);
        browser_skin_image(frame, library_theme_sprite[1], 0);
        browser_set(frame, "set_raycastTarget", "System.Boolean", &no);
        browser_active(photo, 1);
        library_row_roots(k);
        void *clip = browser_make("Photo inset clip", pr, 0, 0, 408, 229.5f, .5f, .5f);
        void *cr = browser_get(clip, "get_transform", "UnityEngine.Transform");
        /* Stencil clipping follows the tilted photo. RectMask2D uses an
         * axis-aligned clip and previously culled these rotated insets. */
        void *maskImage = browser_image(clip, white);
        browser_set(maskImage, "set_raycastTarget", "System.Boolean", &no);
        void *mask = browser_add(clip, "UnityEngine.UI", "Mask");
        browser_set(mask, "set_showMaskGraphic", "System.Boolean", &no);
        browser_active(clip, 1);
        library_row_roots(k);
        void *back = browser_make("Coaster native backdrop", cr, 0, 0, 456, 256.5f, .5f, .5f);
        library_slot_backdrop[k] = browser_image(back, white);
        browser_set(library_slot_backdrop[k], "set_raycastTarget", "System.Boolean", &no);
        browser_active(back, 1);
        library_row_roots(k);
        void *shot = browser_make("Coaster screenshot", cr, 0, 0, 408, 229.5f, .5f, .5f);
        library_slot_image[k] = browser_add(shot, "UnityEngine.UI", "RawImage");
        browser_set(library_slot_image[k], "set_color", "UnityEngine.Color", &white);
        browser_set(library_slot_image[k], "set_raycastTarget", "System.Boolean", &no);
        browser_active(shot, 1);
        write_void(
            browser_method(p_object_get_class(cr), "SetAsLastSibling", "System.Void", 0, 0, 1), cr,
            0);
        library_row_roots(k);
        library_slot_photo[k] = browser_toggle(rt, font, "", -532, -104, 64.4f, 64.4f, .5f, .5f);
        void *im =
            browser_get(library_slot_photo[k], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(im, library_theme_sprite[18], 0);
        library_row_roots(k);
        library_slot_camera[k] =
            library_art(browser_get(browser_get(library_slot_photo[k], "get_gameObject",
                                                "UnityEngine.GameObject"),
                                    "get_transform", "UnityEngine.Transform"),
                        "Retake photo", 0, 0, 43.7f, 43.7f, library_theme_sprite[2], 0);
        library_row_roots(k);
        void *nameGo = browser_make("Editable template name", rt, -330, 0, 300, 210, .5f, .5f);
        void *nameRect = browser_get(nameGo, "get_transform", "UnityEngine.Transform");
        void *background = browser_image(nameGo, transparent);
        browser_add(nameGo, "UnityEngine.UI", "RectMask2D");
        library_slot_name[k] = browser_label(nameRect, font, "", 0, 0, 284, 180, 34);
        library_row_roots(k);
        library_slot_input[k] = browser_add(nameGo, "TMPro", "TMP_InputField");
        browser_set(library_slot_input[k], "set_textComponent", "TMPro.TMP_Text",
                    library_slot_name[k]);
        browser_set(library_slot_input[k], "set_textViewport", "UnityEngine.RectTransform",
                    nameRect);
        browser_set(library_slot_input[k], "set_targetGraphic", "UnityEngine.UI.Graphic",
                    background);
        int limit = 100, multiLineSubmit = 1;
        browser_set(library_slot_input[k], "set_characterLimit", "System.Int32", &limit);
        browser_set(library_slot_input[k], "set_lineType", "TMPro.TMP_InputField.LineType",
                    &multiLineSubmit);
        browser_set(library_slot_name[k], "set_enableWordWrapping", "System.Boolean", &yes);
        browser_set(library_slot_name[k], "set_enableAutoSizing", "System.Boolean", &yes);
        float nameMin = 26, nameMax = 34;
        browser_set(library_slot_name[k], "set_fontSizeMin", "System.Single", &nameMin);
        browser_set(library_slot_name[k], "set_fontSizeMax", "System.Single", &nameMax);
        browser_active(nameGo, 1);
        library_row_roots(k);
        library_slot_origin[k] =
            browser_label(rt, font, "Manual download", -330, -100, 284, 32, 24);
        for (unsigned j = 0; j < 4; ++j) {
            const float statX[] = {-70, 125, 300, 435};
            float x = statX[j];
            float paperWidth = j < 2 ? 190.f : 130.f;
            library_row_roots(k);
            library_art(rt, "Coaster rating", x, 32, 68, 68, library_theme_sprite[11 + j], 0);
            library_row_roots(k);
            library_art(rt, "Native rating value paper", x, -25, paperWidth, 48,
                        library_theme_sprite[15], 1);
            library_row_roots(k);
            library_slot_values[k][j] =
                browser_label(rt, font, "?", x, -25, paperWidth - 16, 44, 26);
            int bold = 1;
            browser_set(library_slot_values[k][j], "set_fontStyle", "TMPro.FontStyles", &bold);
            browser_set(library_slot_values[k][j], "set_enableWordWrapping", "System.Boolean", &no);
            float min = 18, max = 26;
            browser_set(library_slot_values[k][j], "set_enableAutoSizing", "System.Boolean", &yes);
            browser_set(library_slot_values[k][j], "set_fontSizeMin", "System.Single", &min);
            browser_set(library_slot_values[k][j], "set_fontSizeMax", "System.Single", &max);
        }
        library_row_roots(k);
        library_slot_build[k] = browser_toggle(rt, font, "?", 625, 0, 220, 82, .5f, .5f);
        im = browser_get(library_slot_build[k], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(
            im, browser_purchase_sprite ? browser_purchase_sprite : library_theme_sprite[4], 0);
        library_price_surface(im);
        library_slot_price[k] = library_control_label(library_slot_build[k]);
        int bold = 1;
        float priceSize = 34;
        browser_set(library_slot_price[k], "set_fontStyle", "TMPro.FontStyles", &bold);
        browser_set(library_slot_price[k], "set_fontSize", "System.Single", &priceSize);
        BrowserV2 pricePos = {0, 8};
        browser_set(browser_get(library_slot_price[k], "get_transform", "UnityEngine.Transform"),
                    "set_anchoredPosition", "UnityEngine.Vector2", &pricePos);
        int centered = 514;
        browser_set(library_slot_price[k], "set_alignment", "TMPro.TextAlignmentOptions",
                    &centered);
        library_row_roots(k);
        library_slot_export[k] = browser_toggle(rt, font, "", 790, 0, 104, 82, .5f, .5f);
        im = browser_get(library_slot_export[k], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(im, library_theme_sprite[24], 0);
        library_row_roots(k);
        library_art(browser_get(library_slot_export[k], "get_transform", "UnityEngine.Transform"),
                    "Upload to Steam Workshop", 0, 6, 46, 46, library_theme_sprite[27], 0);
        library_row_roots(k);
        library_slot_delete[k] = browser_toggle(rt, font, "", 900, 0, 104, 82, .5f, .5f);
        im = browser_get(library_slot_delete[k], "get_targetGraphic", "UnityEngine.UI.Graphic");
        browser_skin_image(im, library_theme_sprite[5], 0);
        library_row_roots(k);
        library_art(browser_get(browser_get(library_slot_delete[k], "get_gameObject",
                                            "UnityEngine.GameObject"),
                                "get_transform", "UnityEngine.Transform"),
                    "Delete template", 0, 6, 46, 46, library_theme_sprite[3], 0);
        library_row_roots(k);
        void *line = browser_make("Row divider", rt, 0, -library_row_height / 2 + 1,
                                  browser_width - 150, 1.5f, .5f, .5f);
        BrowserColor rule = {.63f, .42f, .15f, .35f};
        void *li = browser_image(line, rule);
        browser_set(li, "set_raycastTarget", "System.Boolean", &no);
        browser_active(line, 1);
        browser_active(g, 0);
    }
    browser_current_roots = &browser_roots;
    return !browser_failed;
}
static void library_list_render(unsigned first) {
    long long renderStart = perf_now();
    unsigned hitsBefore = library_texture_hits, decodesBefore = library_texture_decodes;
    memset(library_hover_sprite, 0, sizeof(library_hover_sprite));
    library_clear_assets();
    library_first = first;
    unsigned selected = library_index;
    for (unsigned k = 0; k < 6; ++k) {
        unsigned index = first + k;
        int visible = index < library_count;
        browser_active(library_slot_go[k], visible);
        if (!visible)
            continue;
        void *rt = browser_get(library_slot_go[k], "get_transform", "UnityEngine.Transform");
        BrowserV2 position = {0, -library_row_height * ((float)index + .5f)};
        browser_set(rt, "set_anchoredPosition", "UnityEngine.Vector2", &position);
        browser_set(library_slot_input[k], "set_text", "System.String",
                    p_string_new(library_entries[index].name));
        int manual = library_entries[index].source == 1,
            download = library_entries[index].source != 0;
        browser_set(library_slot_origin[k], "set_text", "System.String",
                    p_string_new(manual ? "Manual download" : "Steam Workshop"));
        browser_active(
            browser_get(library_slot_origin[k], "get_gameObject", "UnityEngine.GameObject"),
            download);
        BrowserV2 namePos = {0, 0}, nameSize = {284, download ? 150.f : 180.f};
        void *nameTransform =
            browser_get(library_slot_name[k], "get_transform", "UnityEngine.Transform");
        browser_set(nameTransform, "set_anchoredPosition", "UnityEngine.Vector2", &namePos);
        browser_set(nameTransform, "set_sizeDelta", "UnityEngine.Vector2", &nameSize);
        /* Centre the complete visible glyph block, rather than font line metrics.
         * Download source labels stay below it, without shifting the name up. */
        void *alignClass = findclass(p_domain_get(), "TMPro", "TextAlignmentOptions");
        void *midlineField = alignClass ? p_class_get_field_from_name(alignClass, "Midline") : 0;
        int nameAlignment = 514;
        if (midlineField)
            p_field_static_get_value(midlineField, &nameAlignment);
        browser_set(library_slot_name[k], "set_alignment", "TMPro.TextAlignmentOptions",
                    &nameAlignment);
        unsigned char readOnly = (unsigned char)library_entries[index].readOnly;
        browser_set(library_slot_input[k], "set_readOnly", "System.Boolean", &readOnly);
        BlueprintDisplayStats display = library_entries[index].stats;
        blueprint_label_stats(&display);
        for (unsigned j = 0; j < 4; ++j)
            browser_set(library_slot_values[k][j], "set_text", "System.String",
                        p_string_new(display.value[j + 1][0] ? display.value[j + 1] : "?"));
        /* Display the true up-front total: saved track + station purchase +
         * per-support and per-decoration native charges. Never affect billing,
         * construction, confirmation or the stored template metadata. */
        char totalPrice[48] = {0};
        if (!library_total_price(&library_entries[index], library_station_build_cost,
                                 totalPrice, sizeof(totalPrice)))
            totalPrice[0] = '?', totalPrice[1] = 0;
        browser_set(library_slot_price[k], "set_text", "System.String",
                    p_string_new(totalPrice));
        browser_active(
            browser_get(library_slot_delete[k], "get_gameObject", "UnityEngine.GameObject"),
            !readOnly || manual);
        browser_active(
            browser_get(library_slot_photo[k], "get_gameObject", "UnityEngine.GameObject"),
            !readOnly);
        browser_active(
            browser_get(library_slot_export[k], "get_gameObject", "UnityEngine.GameObject"),
            !readOnly);
        library_index = index;
        unsigned art = 28 + preview_background_index(library_entries[index].folder);
        void *backgroundSprite = library_theme_sprite[art];
        /* A theme can be absent from the currently loaded native atlases.
         * Choose the next available theme rather than a black empty layer. */
        for (unsigned offset = 1; !backgroundSprite && offset < 8; ++offset)
            backgroundSprite = library_theme_sprite[28 + ((art - 28 + offset) % 8)];
        browser_set(library_slot_backdrop[k], "set_sprite", "UnityEngine.Sprite", backgroundSprite);
        int simpleImage = 0;
        browser_set(library_slot_backdrop[k], "set_type", "UnityEngine.UI.Image.Type",
                    &simpleImage);
        BrowserColor backgroundColour =
            backgroundSprite ? (BrowserColor){1, 1, 1, 1} : (BrowserColor){0, 0, 0, 1};
        browser_set(library_slot_backdrop[k], "set_color", "UnityEngine.Color", &backgroundColour);
        if (!library_show_image(library_slot_image[k]))
            browser_log("PREVIEW: unavailable thumbnail; row remains usable");
        unsigned char enabled = (unsigned char)!library_busy();
        browser_set(library_slot_build[k], "set_interactable", "System.Boolean", &enabled);
    }
    library_index = selected;
    logtext("TEMPLATE UI: PREVIEW CACHE reused=");
    lognum(library_texture_hits - hitsBefore);
    logtext(" decoded=");
    lognum(library_texture_decodes - decodesBefore);
    nl();
    library_restore_type_icon();
    logtext("TEMPLATE UI: PERF ROWS render_us=");
    lognum(perf_us(renderStart));
    nl();
}
/* Rebind critical chrome after late asset loading or native resource unload.
 * Normal checks are cheap and throttled; no inventory scan while art is valid. */
static void library_repair_artwork(int opening) {
    UINT_PTR now = GetTickCount64();
    if (!browser_open && !opening)
        return;
    if (!opening && now < library_art_due)
        return;
    library_art_due = now + 2000;
    int ribbon = library_theme_usable(library_theme_sprite[0]);
    int camera = library_theme_usable(library_theme_sprite[2]);
    if (!ribbon || !camera) {
        browser_log("ART: repairing missing/destroyed library ribbon or camera artwork");
        if (!ribbon)
            library_theme_forget(0);
        if (!camera)
            library_theme_forget(2);
        library_theme_resolve();
        ribbon = library_theme_usable(library_theme_sprite[0]);
        camera = library_theme_usable(library_theme_sprite[2]);
    }
    if (library_ribbon_image) {
        browser_set(library_ribbon_image, "set_overrideSprite", "UnityEngine.Sprite", 0);
        browser_set(library_ribbon_image, "set_sprite", "UnityEngine.Sprite",
                    ribbon ? library_theme_sprite[0] : 0);
        BrowserColor ink = ribbon ? (BrowserColor){1, 1, 1, 1} : (BrowserColor){0, .65f, .80f, 1};
        browser_set(library_ribbon_image, "set_color", "UnityEngine.Color", &ink);
    }
    for (unsigned k = 0; k < 6; ++k)
        if (library_slot_camera[k]) {
            browser_set(library_slot_camera[k], "set_overrideSprite", "UnityEngine.Sprite", 0);
            browser_set(library_slot_camera[k], "set_sprite", "UnityEngine.Sprite",
                        camera ? library_theme_sprite[2] : 0);
            BrowserColor ink = {1, 1, 1, camera ? 1.f : 0.f};
            browser_set(library_slot_camera[k], "set_color", "UnityEngine.Color", &ink);
        }
}
static __declspec(noinline) void library_display(void) {
    library_repair_artwork(1);
    unsigned char enabled = (unsigned char)(library_count && !library_busy());
    browser_set(browser_build, "set_interactable", "System.Boolean", &enabled);
    const char *title = library_count ? "" : "No saved templates for this coaster";
    browser_set(browser_status, "set_text", "System.String", p_string_new(title));
    if (library_content) {
        BrowserV2 size = {browser_width - 100,
                          library_row_height * (float)(library_count > 4 ? library_count : 4)};
        browser_set(library_content, "set_sizeDelta", "UnityEngine.Vector2", &size);
        float normalized =
            library_count > 4 ? 1 - (float)(library_index < library_count - 4 ? library_index
                                                                              : library_count - 4) /
                                        (float)(library_count - 4)
                              : 1;
        write_void(browser_method(p_object_get_class(library_scroll), "StopMovement", "System.Void",
                                  0, 0, 1),
                   library_scroll, 0);
        BrowserV2 initial = {0, (1 - normalized) * library_row_height *
                                    (float)(library_count > 4 ? library_count - 4 : 0)};
        browser_set(library_content, "set_anchoredPosition", "UnityEngine.Vector2", &initial);
        library_last_normalized = normalized;
        browser_set(library_scroll, "set_verticalNormalizedPosition", "System.Single", &normalized);
        library_list_render(library_visible_first(library_count, normalized));
    }
}
static void library_row_hover(void) {
    void *app = findclass(p_domain_get(), "TPS.Core", "App");
    void *instance = pf_invoke(browser_method(app, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0);
    void *input =
        instance ? browser_optional(instance, "get_InputManager", "TPS.Core.Input.InputManager")
                 : 0;
    void *pos = input ? browser_optional(input, "GetMousePosition", "UnityEngine.Vector2") : 0;
    if (!pos)
        return;
    BrowserV2 mouse = *(BrowserV2 *)p_object_unbox(pos);
    const char *mp[] = {"System.Int32"};
    int left = 0;
    void *ma[] = {&left};
    void *pressedBox = pf_invoke(browser_method(p_object_get_class(input), "WrappedGetMouseButton",
                                                "System.Boolean", 1, mp, 1),
                                 input, ma);
    int pressed = pressedBox && *(unsigned char *)p_object_unbox(pressedBox);
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *t = browser_type("UnityEngine", "Canvas");
    void *ca[] = {t, &yes};
    void *canvas =
        pf_invoke(browser_method(p_object_get_class(browser_panel), "GetComponentInParent",
                                 "UnityEngine.Component", 2, cp, 1),
                  browser_panel, ca);
    void *root = canvas ? browser_optional(canvas, "get_rootCanvas", "UnityEngine.Canvas") : 0;
    void *camera = 0;
    void *mode = root ? browser_optional(root, "get_renderMode", "UnityEngine.RenderMode") : 0;
    if (mode && *(int *)p_object_unbox(mode) != 0)
        camera = browser_optional(root, "get_worldCamera", "UnityEngine.Camera");
    const char *rp[] = {"UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera"};
    void *rc = findclass(p_domain_get(), "UnityEngine", "RectTransformUtility");
    void *m = browser_method(rc, "RectangleContainsScreenPoint", "System.Boolean", 3, rp, 0);
    for (unsigned k = 0; k < 6 && library_first + k < library_count; ++k)
        for (unsigned j = 0; j < 3; ++j) {
            void *control = j == 2 ? library_slot_export[k]
                            : j    ? library_slot_delete[k]
                                   : library_slot_build[k];
            void *rt = browser_get(control, "get_transform", "UnityEngine.Transform");
            void *a[] = {rt, &mouse, camera};
            void *b = pf_invoke(m, 0, a);
            int over = b && *(unsigned char *)p_object_unbox(b);
            unsigned art = j == 2 ? (over ? (pressed ? 26 : 25) : 24)
                           : j    ? (over ? (pressed ? 21 : 20) : 5)
                                  : (over ? (pressed ? 23 : 22) : 4);
            void *sp = !j && !over && browser_purchase_sprite ? browser_purchase_sprite
                                                              : library_theme_sprite[art];
            if (sp && library_hover_sprite[k][j] != sp) {
                void *im = browser_get(control, "get_targetGraphic", "UnityEngine.UI.Graphic");
                if (im) {
                    browser_set(im, "set_sprite", "UnityEngine.Sprite", sp);
                    library_hover_sprite[k][j] = sp;
                }
            }
        }
}
/* Poll while the library is open, including between throttled list refreshes.
 * Focusing an input can move its ScrollRect; that is not a request to recycle
 * the row being edited. Wheel/scrollbar input explicitly ends the edit. */
static void library_scroll_input_tick(void) {
    if (!browser_open)
        return;
    void *c = findclass(p_domain_get(), "TPS.Core", "App");
    void *app = c ? pf_invoke(browser_method(c, "get_Instance", "TPS.Core.App", 0, 0, 0), 0, 0) : 0;
    void *input =
        app ? browser_optional(app, "get_InputManager", "TPS.Core.Input.InputManager") : 0;
    void *wheel = input ? browser_optional(input, "GetMouseWheel", "System.Single") : 0;
    void *drag =
        input ? browser_optional(input, "IsCurrentlyDraggingScrollbar", "System.Boolean") : 0;
    if ((wheel && *(float *)p_object_unbox(wheel) != 0) ||
        (drag && *(unsigned char *)p_object_unbox(drag))) {
        if (library_name_focused())
            library_end_inline_edit();
    }
}
static void library_scroll_rows_tick(void) {
    if (!browser_open || !library_scroll || !library_content)
        return;
    void *box = browser_get(library_content, "get_anchoredPosition", "UnityEngine.Vector2");
    if (!box || !p_object_unbox(box) || !(library_row_height > 0))
        return;
    BrowserV2 position = *(BrowserV2 *)p_object_unbox(box);
    float range = library_row_height * (float)(library_count > 4 ? library_count - 4 : 0);
    float offset = position.y;
    if (!(offset >= 0))
        offset = 0;
    if (offset > range)
        offset = range;
    if (position.y != offset) {
        position.y = offset;
        browser_set(library_content, "set_anchoredPosition", "UnityEngine.Vector2", &position);
        write_void(browser_method(p_object_get_class(library_scroll), "StopMovement", "System.Void",
                                  0, 0, 1),
                   library_scroll, 0);
    }
    float normalized = range > 0 ? 1 - offset / range : 1;
    unsigned first = library_visible_first(library_count, normalized);
    if (first != library_first && library_name_focused()) {
        write_void(browser_method(p_object_get_class(library_scroll), "StopMovement", "System.Void",
                                  0, 0, 1),
                   library_scroll, 0);
        browser_set(library_scroll, "set_verticalNormalizedPosition", "System.Single",
                    &library_last_normalized);
        return;
    }
    library_last_normalized = normalized;
    if (first != library_first) {
        library_end_inline_edit();
        library_list_render(first);
        if (library_name_focused())
            return;
    }
}
static void library_list_tick(void) {
    library_row_hover();
    if (!library_scroll)
        return;
    for (unsigned k = 0; k < 6 && library_first + k < library_count; ++k) {
        if (browser_pressed(library_slot_export[k])) {
            library_index = library_first + k;
            library_row_action(library_index, 6);
            return;
        }
        if (browser_pressed(library_slot_delete[k])) {
            library_index = library_first + k;
            library_row_action(library_index, 2);
            return;
        }
        if (browser_pressed(library_slot_photo[k])) {
            library_index = library_first + k;
            library_row_action(library_index, 3);
            return;
        }
        if (browser_pressed(library_slot_build[k])) {
            library_index = library_first + k;
            unsigned char yes = 1;
            browser_set(browser_build, "SetIsOnWithoutNotify", "System.Boolean", &yes);
            return;
        }
    }
}

static void library_cleanup(void) {
    /* Catalogue owns file data only; retain its short TTL across UI rebuilds. */
    library_clear_assets();
    library_texture_cleanup();
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *m = c ? pf_method(c, "Destroy", "System.Void", 1, p, 0) : 0;
    for (unsigned i = 0; i < library_controls.n; ++i) {
        void *o = p_gchandle_get_target(library_controls.h[i]);
        if (o && pf_class(p_object_get_class(o), "UnityEngine.GameObject") && browser_alive(o)) {
            browser_active(o, 0);
            void *a[] = {o};
            write_void(m, 0, a);
        }
    }
    pf_release(&library_controls);
    library_previous = library_next = 0;
    library_count = library_index = 0;
}
