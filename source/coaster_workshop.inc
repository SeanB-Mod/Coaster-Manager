/* Steam owns these files. Read only fixed .coaster packages directly inside
 * numeric installed-item folders; no SDK calls, downloads or extraction. */
static WCHAR coaster_workshop_root[1024], coaster_workshop_files[LIBRARY_SOURCE_LIMIT][1024];
static WCHAR coaster_workshop_ids[LIBRARY_SOURCE_LIMIT][33];
static unsigned coaster_workshop_scanned_folders, coaster_workshop_scanned_packages;
static int coaster_directory(const WCHAR *path) {
    DWORD a = GetFileAttributesW(path);
    return a != 0xffffffffU && (a & 0x10) && !(a & 0x400);
}
static unsigned coaster_parent(WCHAR *path) {
    unsigned n = 0;
    while (path[n])
        ++n;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/')
        --n;
    if (n < 4)
        return 0;
    path[n - 1] = 0;
    return n - 1;
}
static int coaster_component(const WCHAR *path, const WCHAR *word) {
    unsigned n = 0;
    while (path[n])
        ++n;
    unsigned at = n;
    while (at && path[at - 1] != '\\' && path[at - 1] != '/')
        --at;
    unsigned j = 0;
    while (word[j] && at + j < n) {
        unsigned a = path[at + j], b = word[j];
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (b >= 'A' && b <= 'Z')
            b += 32;
        if (a != b)
            return 0;
        ++j;
    }
    return !word[j] && at + j == n;
}
static int coaster_workshop_resolve(const WCHAR *executable, WCHAR *out) {
    unsigned pos = 0;
    if (!append(out, 1024, &pos, executable) || !coaster_parent(out) || !coaster_parent(out) ||
        !coaster_component(out, L"common") || !coaster_parent(out) ||
        !coaster_component(out, L"steamapps"))
        return 0;
    /* Check each directory before entering the Workshop content tree. */
    if (!coaster_directory(out))
        return 0;
    pos = 0;
    while (out[pos])
        ++pos;
    if (!append(out, 1024, &pos, L"\\workshop") || !coaster_directory(out) ||
        !append(out, 1024, &pos, L"\\content") || !coaster_directory(out) ||
        !append(out, 1024, &pos, L"\\2185060") || !coaster_directory(out))
        return 0;
    return 1;
}
static int coaster_workshop_item(const WCHAR *name) {
    unsigned n = 0;
    while (name[n]) {
        if (name[n] < '0' || name[n] > '9' || n >= 20)
            return 0;
        ++n;
    }
    return n && !(n == 1 && name[0] == '0');
}
static int coaster_workshop_path_valid(const WCHAR *path) {
    if (!coaster_workshop_root[0] || !coaster_directory(coaster_workshop_root))
        return 0;
    static WCHAR ancestor[1024];
    unsigned n = 0;
    if (!append(ancestor, 1024, &n, coaster_workshop_root))
        return 0;
    for (unsigned j = 0; j < 3; ++j)
        if (!coaster_parent(ancestor) || !coaster_directory(ancestor))
            return 0;
    unsigned at = 0;
    while (coaster_workshop_root[at]) {
        if (path[at] != coaster_workshop_root[at])
            return 0;
        ++at;
    }
    if (path[at++] != '\\')
        return 0;
    WCHAR item[21];
    unsigned k = 0;
    while (path[at] && path[at] != '\\') {
        if (k >= 20)
            return 0;
        item[k++] = path[at++];
    }
    item[k] = 0;
    if (!coaster_workshop_item(item) || path[at++] != '\\' || !path[at])
        return 0;
    for (unsigned i = at; path[i]; ++i)
        if (path[i] == '\\' || path[i] == '/' || path[i] == ':')
            return 0;
    n = 0;
    if (!append(ancestor, 1024, &n, coaster_workshop_root) || !append(ancestor, 1024, &n, L"\\") ||
        !append(ancestor, 1024, &n, item) || !coaster_directory(ancestor))
        return 0;
    return 1;
}
static __declspec(noinline) void coaster_workshop_scan_at(const WCHAR *root, long long definition) {
    coaster_workshop_scanned_folders = coaster_workshop_scanned_packages = 0;
    unsigned pos = 0;
    if (!append(coaster_workshop_root, 1024, &pos, root) ||
        !coaster_directory(coaster_workshop_root))
        return;
    static WCHAR pattern[1024], itemdir[1024], glob[1024];
    pos = 0;
    if (!append(pattern, 1024, &pos, root) || !append(pattern, 1024, &pos, L"\\*"))
        return;
    LibraryFindData item;
    HANDLE items = FindFirstFileW(pattern, &item);
    if (items == INVALID_HANDLE_VALUE)
        return;
    unsigned folders = 0, count = 0, budget = 0;
    do {
        if (!(item.attributes & 0x10) || (item.attributes & 0x400) ||
            !coaster_workshop_item(item.name))
            continue;
        if (++folders > 16384) {
            browser_log("WORKSHOP: item-folder safety limit reached (16384)");
            break;
        }
        pos = 0;
        if (!append(itemdir, 1024, &pos, root) || !append(itemdir, 1024, &pos, L"\\") ||
            !append(itemdir, 1024, &pos, item.name))
            continue;
        unsigned n = 0;
        if (!append(glob, 1024, &n, itemdir) || !append(glob, 1024, &n, L"\\*.coaster"))
            continue;
        LibraryFindData file;
        HANDLE files = FindFirstFileW(glob, &file);
        if (files == INVALID_HANDLE_VALUE)
            continue;
        do {
            if (file.attributes & (0x10 | 0x400))
                continue;
            if (count == LIBRARY_SOURCE_LIMIT) {
                ++library_skipped;
                break;
            }
            if (file.sizeHigh || !library_scan_bytes_fit(budget, file.sizeLow)) {
                ++library_skipped;
                browser_log("WORKSHOP REJECT: package or scan byte limit exceeded");
                continue;
            }
            n = 0;
            if (append(coaster_workshop_files[count], 1024, &n, itemdir) &&
                append(coaster_workshop_files[count], 1024, &n, L"\\") &&
                append(coaster_workshop_files[count], 1024, &n, file.name)) {
                budget += file.sizeLow;
                ++count;
            }
        } while (FindNextFileW(files, &file));
        FindClose(files);
    } while (count < LIBRARY_SOURCE_LIMIT && FindNextFileW(items, &item));
    FindClose(items);
    coaster_workshop_scanned_folders = folders;
    coaster_workshop_scanned_packages = count;
    for (unsigned i = 1; i < count; ++i) {
        static WCHAR value[1024];
        memcpy(value, coaster_workshop_files[i], sizeof(value));
        unsigned j = i;
        while (j && coaster_path_compare(coaster_workshop_files[j - 1], value) > 0) {
            memcpy(coaster_workshop_files[j], coaster_workshop_files[j - 1], sizeof(value));
            --j;
        }
        memcpy(coaster_workshop_files[j], value, sizeof(value));
    }
    unsigned seen = 0, accepted = 0;
    for (unsigned i = 0; i < count; ++i) {
        static LibraryEntry e;
        if (!coaster_workshop_path_valid(coaster_workshop_files[i]) ||
            !coaster_package_entry(coaster_workshop_files[i], &e)) {
            ++library_skipped;
            static char filename[4096];
            int n = WideCharToMultiByte(65001, 0, coaster_workshop_files[i], -1, filename,
                                        sizeof(filename), 0, 0);
            logtext("WORKSHOP REJECT: ");
            if (n > 0)
                logtext(filename);
            logtext(" reason=");
            logtext(coaster_import_error ? coaster_import_error : "invalid item path");
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
        for (unsigned j = 0; j < seen; ++j)
            if (coaster_same_id(e.folder, coaster_workshop_ids[j]))
                duplicate = 1;
        memcpy(coaster_workshop_ids[seen++], e.folder, 66);
        if (duplicate) {
            browser_log("WORKSHOP: duplicate ID hidden; higher-priority source retained");
            continue;
        }
        if (e.definition != definition)
            continue;
        if (library_count >= LIBRARY_DISPLAY_LIMIT) {
            ++library_skipped;
            continue;
        }
        e.source = 2;
        library_entries[library_count++] = e;
        ++accepted;
    }
    logtext("WORKSHOP: scanned item folders=");
    lognum(folders);
    logtext(" candidate packages=");
    lognum(count);
    nl();
    logtext("WORKSHOP: matching packages added=");
    lognum(accepted);
    nl();
}
static void coaster_workshop_scan(long long definition) {
    static WCHAR executable[1024], root[1024];
    coaster_workshop_root[0] = 0;
    DWORD n = GetModuleFileNameW(0, executable, 1024);
    if (!n || n >= 1024 || !coaster_workshop_resolve(executable, root)) {
        browser_log("WORKSHOP: active Steam library content folder unavailable");
        return;
    }
    coaster_workshop_scan_at(root, definition);
}
