/* Read packages in place: never extract archive paths or modify imported files.
 * Version 1 uses four fixed ZIP_STORED members, as produced by our exporter. */
typedef struct {
    const unsigned char *data;
    unsigned size, crc, offset;
} CoasterMember;
static unsigned char coaster_import_bytes[25U * 1024U * 1024U + 1];
static CoasterMember coaster_members[4];
static unsigned coaster_import_size;
static const char *coaster_import_error;
static unsigned coaster_le16(const unsigned char *p) {
    return p[0] | ((unsigned)p[1] << 8);
}
static unsigned coaster_le32(const unsigned char *p) {
    return coaster_le16(p) | (coaster_le16(p + 2) << 16);
}
static int coaster_member_index(const unsigned char *p, unsigned n) {
    const char *names[] = {"package.json", "entry.json", "blueprint.json", "preview.png",
                           "preview.jpg"};
    for (unsigned i = 0; i < 5; ++i) {
        if (n != length(names[i]))
            continue;
        unsigned j = 0;
        while (j < n && p[j] == (unsigned char)names[i][j])
            ++j;
        if (j == n)
            return i == 4 ? 3 : (int)i;
    }
    return -1;
}
static int coaster_zip_open(const WCHAR *path) {
    coaster_import_error = "invalid ZIP structure, member or CRC";
    memset(coaster_members, 0, sizeof(coaster_members));
    unsigned n = library_read(path, coaster_import_bytes, 25U * 1024U * 1024U);
    coaster_import_size = n;
    if (n < 22)
        return 0;
    const unsigned char *b = coaster_import_bytes, *end = b + n - 22;
    if (coaster_le32(end) != 0x06054b50 || coaster_le16(end + 4) || coaster_le16(end + 6) ||
        coaster_le16(end + 8) != 4 || coaster_le16(end + 10) != 4 || coaster_le16(end + 20))
        return 0;
    unsigned at = coaster_le32(end + 16), size = coaster_le32(end + 12);
    if (at > n - 22 || size != n - 22 - at)
        return 0;
    unsigned local = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (n - 22 - at < 46)
            return 0;
        const unsigned char *c = b + at;
        unsigned name = coaster_le16(c + 28), extra = coaster_le16(c + 30),
                 comment = coaster_le16(c + 32);
        if (coaster_le32(c) != 0x02014b50 || coaster_le16(c + 6) > 20 ||
            (coaster_le16(c + 8) & ~0x800U) || coaster_le16(c + 10) || extra || comment ||
            coaster_le16(c + 34) || name > 32 || 46 + name > n - 22 - at)
            return 0;
        int slot = coaster_member_index(c + 46, name);
        if (slot < 0 || coaster_members[slot].data)
            return 0;
        unsigned bytes = coaster_le32(c + 24), offset = coaster_le32(c + 42),
                 crc = coaster_le32(c + 16),
                 limit = slot == 0   ? 1024
                         : slot == 1 ? LIBRARY_METADATA_LIMIT
                         : slot == 2 ? 4U * 1024U * 1024U
                                     : 20U * 1024U * 1024U;
        if (!bytes || bytes > limit || coaster_le32(c + 20) != bytes || offset != local ||
            offset > coaster_le32(end + 16) || coaster_le32(end + 16) - offset < 30 + name)
            return 0;
        const unsigned char *l = b + offset;
        if (coaster_le32(l) != 0x04034b50 || coaster_le16(l + 4) > 20 ||
            coaster_le16(l + 6) != coaster_le16(c + 8) || coaster_le16(l + 8) ||
            coaster_le32(l + 14) != crc || coaster_le32(l + 18) != bytes ||
            coaster_le32(l + 22) != bytes || coaster_le16(l + 26) != name || coaster_le16(l + 28))
            return 0;
        for (unsigned j = 0; j < name; ++j)
            if (l[30 + j] != c[46 + j])
                return 0;
        unsigned data = offset + 30 + name;
        if (bytes > coaster_le32(end + 16) - data || coaster_crc(b + data, bytes) != crc)
            return 0;
        coaster_members[slot].data = b + data;
        coaster_members[slot].size = bytes;
        coaster_members[slot].crc = crc;
        coaster_members[slot].offset = offset;
        local = data + bytes;
        at += 46 + name;
    }
    return at == n - 22 && local == coaster_le32(end + 16);
}
static int coaster_package_entry(const WCHAR *path, LibraryEntry *e) {
    if (!coaster_zip_open(path))
        return 0;
    coaster_import_error = "unsupported package format/version";
    unsigned root = lj_parse(coaster_members[0].data, coaster_members[0].size);
    long long version = 0, type = 0;
    if (!root || !lj_is(lj_member(root, "format"), "TPM.Coaster") ||
        !lj_integer(root, "version", &version) || version != 1)
        return 0;
    coaster_import_error = "invalid template metadata";
    root = lj_parse(coaster_members[1].data, coaster_members[1].size);
    memset(e, 0, sizeof(*e));
    const char *id = root ? lj_string_value(lj_member(root, "id")) : 0;
    if (!id || length(id) != 32)
        return 0;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned c = (unsigned char)id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return 0;
        e->folder[i] = (WCHAR)c;
    }
    if (!lj_integer(root, "schemaVersion", &version) || version != 1 ||
        !lj_int(lj_member(root, "stationDefinitionId"), &type) || !type ||
        !library_copy(e->name, sizeof(e->name), lj_string_value(lj_member(root, "name"))) ||
        !library_name(e->name) ||
        !library_copy(e->sha, sizeof(e->sha), lj_string_value(lj_member(root, "planSha256"))) ||
        length(e->sha) != 64 ||
        !library_copy(e->screenshot, sizeof(e->screenshot),
                      lj_string_value(lj_member(root, "screenshot"))) ||
        (!equals(e->screenshot, "preview.png") && !equals(e->screenshot, "preview.jpg")))
        return 0;
    for (unsigned i = 0; i < 64; ++i)
        if (lj_hex((unsigned char)e->sha[i]) < 0)
            return 0;
    /* The preview extension in metadata must match the ZIP member. */
    const unsigned char *l = coaster_import_bytes + coaster_members[3].offset;
    unsigned len = coaster_le16(l + 26);
    if (len != length(e->screenshot))
        return 0;
    for (unsigned i = 0; i < len; ++i)
        if (l[30 + i] != (unsigned char)e->screenshot[i])
            return 0;
    blueprint_stats_read(root, &e->stats);
    e->definition = type;
    e->readOnly = 1;
    e->source = 1;
    coaster_import_error = "invalid layout or plan fingerprint";
    root = lj_parse(coaster_members[2].data, coaster_members[2].size);
    unsigned bytes = root ? library_prepare(root, library_plan) : 0;
    if (!bytes)
        return 0;
    char hash[65];
    long long planType = 0;
    library_sha(library_plan, bytes, hash);
    memcpy(&planType, library_plan + 24, 8);
    if (planType != type || !equals(hash, e->sha))
        return 0;
    coaster_import_error = "invalid or incomplete preview";
    unsigned w = 0, h = 0;
    if (!library_image_size(coaster_members[3].data, coaster_members[3].size, &w, &h))
        return 0;
    if (equals(e->screenshot, "preview.png")) {
        if (!save_complete_png(coaster_members[3].data, coaster_members[3].size))
            return 0;
    } else {
        unsigned n = coaster_members[3].size;
        const unsigned char *p = coaster_members[3].data;
        if (n < 4 || p[n - 2] != 255 || p[n - 1] != 217)
            return 0;
    }
    library_sha(coaster_import_bytes, coaster_import_size, e->packageSha);
    unsigned pos = 0;
    return append(e->package, 1024, &pos, path);
}
static int coaster_same_id(const WCHAR *a, const WCHAR *b) {
    for (unsigned i = 0; i < 33; ++i)
        if (a[i] != b[i])
            return 0;
    return 1;
}
static int coaster_path_compare(const WCHAR *a, const WCHAR *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}
static WCHAR coaster_manual_files[LIBRARY_SOURCE_LIMIT][1024];
static WCHAR coaster_manual_ids[LIBRARY_SOURCE_LIMIT][33];
static __declspec(noinline) void coaster_manual_scan(long long definition) {
    static WCHAR directory[768], pattern[1024];
    unsigned pos = 0;
    if (!coaster_storage_path(directory, 768, L"Manual", 1))
        return;
    if (!append(pattern, 1024, &pos, directory) || !append(pattern, 1024, &pos, L"\\*.coaster"))
        return;
    LibraryFindData fd;
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE)
        return;
    unsigned count = 0, budget = 0;
    do {
        if (fd.attributes & (0x10 | 0x400))
            continue;
        if (count == LIBRARY_SOURCE_LIMIT) {
            ++library_skipped;
            browser_log("MANUAL: file limit reached (2048)");
            break;
        }
        if (fd.sizeHigh || !library_scan_bytes_fit(budget, fd.sizeLow)) {
            ++library_skipped;
            browser_log("MANUAL REJECT: package or scan byte limit exceeded");
            continue;
        }
        budget += fd.sizeLow;
        pos = 0;
        if (append(coaster_manual_files[count], 1024, &pos, directory) &&
            append(coaster_manual_files[count], 1024, &pos, L"\\") &&
            append(coaster_manual_files[count], 1024, &pos, fd.name))
            ++count;
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    /* Stable winner for duplicate manual files; do not depend on filesystem order. */
    for (unsigned i = 1; i < count; ++i) {
        static WCHAR value[1024];
        memcpy(value, coaster_manual_files[i], sizeof(value));
        unsigned j = i;
        while (j && coaster_path_compare(coaster_manual_files[j - 1], value) > 0) {
            memcpy(coaster_manual_files[j], coaster_manual_files[j - 1], sizeof(value));
            --j;
        }
        memcpy(coaster_manual_files[j], value, sizeof(value));
    }
    coaster_manual_seen = 0;
    for (unsigned i = 0; i < count; ++i) {
        static LibraryEntry e;
        if (!coaster_package_entry(coaster_manual_files[i], &e)) {
            static char filename[4096];
            ++library_skipped;
            int len = WideCharToMultiByte(65001, 0, coaster_manual_files[i], -1, filename,
                                          sizeof(filename), 0, 0);
            logtext("MANUAL REJECT: ");
            if (len > 0)
                logtext(filename);
            logtext(" reason=");
            logtext(coaster_import_error);
            nl();
            continue;
        }
        int duplicate = 0;
        for (unsigned j = 0; j < library_local_count; ++j)
            if (coaster_same_id(e.folder, library_local_ids[j]))
                duplicate = 1;
        for (unsigned j = 0; j < coaster_manual_seen; ++j)
            if (coaster_same_id(e.folder, coaster_manual_ids[j]))
                duplicate = 1;
        memcpy(coaster_manual_ids[coaster_manual_seen++], e.folder, 66);
        if (duplicate) {
            browser_log("MANUAL: duplicate ID hidden; existing source retained");
            continue;
        }
        if (e.definition != definition)
            continue;
        if (library_count >= LIBRARY_DISPLAY_LIMIT) {
            ++library_skipped;
            continue;
        }
        library_entries[library_count++] = e;
    }
}
#include "coaster_workshop.inc"
static __declspec(noinline) unsigned library_entry_read(const LibraryEntry *e, const WCHAR *file,
                                                        unsigned char *out, unsigned capacity) {
    if (!e->package[0]) {
        static WCHAR path[1024];
        return library_path(path, e->folder, file) ? library_read(path, out, capacity) : 0;
    }
    if (e->source == 2) {
        if (!coaster_workshop_path_valid(e->package))
            return 0;
    } else if (e->source == 1) {
        static WCHAR directory[768];
        if (!coaster_storage_path(directory, 768, L"Manual", 0))
            return 0;
        DWORD attr = GetFileAttributesW(library_root);
        if (attr == 0xffffffffU || !(attr & 0x10) || (attr & 0x400))
            return 0;
        attr = GetFileAttributesW(directory);
        if (attr == 0xffffffffU || !(attr & 0x10) || (attr & 0x400))
            return 0;
        unsigned prefix = 0;
        while (directory[prefix]) {
            if (directory[prefix] != e->package[prefix])
                return 0;
            ++prefix;
        }
        if (e->package[prefix] != '\\')
            return 0;
        for (unsigned i = prefix + 1; e->package[i]; ++i)
            if (e->package[i] == '\\' || e->package[i] == '/' || e->package[i] == ':')
                return 0;
    } else
        return 0;
    /* Revalidate on use: a changed/replaced package cannot silently change a selected entry. */
    static LibraryEntry current;
    if (!coaster_package_entry(e->package, &current) ||
        !coaster_same_id(e->folder, current.folder) || e->definition != current.definition ||
        !equals(e->sha, current.sha) || !equals(e->name, current.name) ||
        !equals(e->screenshot, current.screenshot) || !equals(e->packageSha, current.packageSha))
        return 0;
    unsigned slot;
    if (coaster_path_compare(file, L"entry.json") == 0)
        slot = 1;
    else if (coaster_path_compare(file, L"blueprint.json") == 0)
        slot = 2;
    else if (coaster_path_compare(file, L"preview.png") == 0 ||
             coaster_path_compare(file, L"preview.jpg") == 0)
        slot = 3;
    else
        return 0;
    unsigned n = coaster_members[slot].size;
    if (n > capacity)
        return 0;
    memcpy(out, coaster_members[slot].data, n);
    return n;
}

static int coaster_manual_valid(const LibraryEntry *e) {
    return e->source == 1 && e->package[0] &&
           library_entry_read(e, L"blueprint.json", library_bytes, 4U * 1024U * 1024U) != 0;
}
static int coaster_manual_delete(const LibraryEntry *e) {
    if (!coaster_manual_valid(e))
        return 0;
    /* Only the selected, unchanged package directly inside our Manual folder. */
    return DeleteFileW(e->package) != 0;
}
