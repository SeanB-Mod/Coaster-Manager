/* Portable ZIP_STORED container. Fixed members only, bounded inputs, no overwrite.
 * Used internally to stage the Steam Workshop content package. */
typedef struct {
    unsigned crc, size, offset;
    const char *name;
} CoasterZipEntry;
static unsigned char coaster_export_bytes[20U * 1024U * 1024U + 1],
    coaster_export_metadata[LIBRARY_METADATA_LIMIT + 1];
static WCHAR coaster_export_temp[1024], coaster_export_result[1024];
static void coaster_u16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}
static void coaster_u32(unsigned char *p, unsigned v) {
    coaster_u16(p, v);
    coaster_u16(p + 2, v >> 16);
}
static int coaster_write(HANDLE h, const void *p, unsigned size) {
    DWORD written = 0;
    return WriteFile(h, p, size, &written, 0) && written == size;
}
static unsigned coaster_crc(const unsigned char *p, unsigned n) {
    unsigned crc = 0xffffffffU;
    for (unsigned i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
    }
    return ~crc;
}
static int coaster_zip_member(HANDLE h, CoasterZipEntry *e, const char *name,
                              const unsigned char *bytes, unsigned size, unsigned *offset) {
    unsigned char header[30] = {0};
    unsigned len = length(name);
    e->crc = coaster_crc(bytes, size);
    e->size = size;
    e->offset = *offset;
    e->name = name;
    coaster_u32(header, 0x04034b50);
    coaster_u16(header + 4, 20);
    coaster_u16(header + 6, 0x800);
    coaster_u16(header + 12, 33);
    coaster_u32(header + 14, e->crc);
    coaster_u32(header + 18, size);
    coaster_u32(header + 22, size);
    coaster_u16(header + 26, len);
    if (!coaster_write(h, header, 30) || !coaster_write(h, name, len) ||
        !coaster_write(h, bytes, size))
        return 0;
    *offset += 30 + len + size;
    return 1;
}
static int coaster_zip_finish(HANDLE h, CoasterZipEntry *entries, unsigned count, unsigned offset) {
    unsigned start = offset;
    for (unsigned i = 0; i < count; ++i) {
        CoasterZipEntry *e = &entries[i];
        unsigned char header[46] = {0};
        unsigned len = length(e->name);
        coaster_u32(header, 0x02014b50);
        coaster_u16(header + 4, 20);
        coaster_u16(header + 6, 20);
        coaster_u16(header + 8, 0x800);
        coaster_u16(header + 14, 33);
        coaster_u32(header + 16, e->crc);
        coaster_u32(header + 20, e->size);
        coaster_u32(header + 24, e->size);
        coaster_u16(header + 28, len);
        coaster_u32(header + 42, e->offset);
        if (!coaster_write(h, header, 46) || !coaster_write(h, e->name, len))
            return 0;
        offset += 46 + len;
    }
    unsigned char end[22] = {0};
    coaster_u32(end, 0x06054b50);
    coaster_u16(end + 8, count);
    coaster_u16(end + 10, count);
    coaster_u32(end + 12, offset - start);
    coaster_u32(end + 16, start);
    return coaster_write(h, end, 22);
}
static int coaster_filename(const LibraryEntry *entry, const WCHAR *directory, unsigned copy,
                            WCHAR *out) {
    WCHAR name[256];
    int count = MultiByteToWideChar(65001, 8, entry->name, -1, name, 256);
    if (count <= 1)
        return 0;
    unsigned pos = 0, n = 0;
    if (!append(out, 1024, &pos, directory) || !append(out, 1024, &pos, L"\\"))
        return 0;
    /* Keep the complete filename below MAX_PATH even on redirected Desktops. */
    unsigned max = pos < 160 ? 80 : pos < 210 ? 230 - pos : 0;
    if (!max)
        return 0;
    for (unsigned i = 0; name[i] && n < max; ++i) {
        WCHAR ch = name[i];
        if (ch < 32 || ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' ||
            ch == '\\' || ch == '|' || ch == '?' || ch == '*')
            ch = '_';
        out[pos + n++] = ch;
    }
    if (n && out[pos + n - 1] >= 0xd800 && out[pos + n - 1] <= 0xdbff)
        --n;
    while (n && (out[pos + n - 1] == ' ' || out[pos + n - 1] == '.'))
        --n;
    pos += n;
    if (!n && !append(out, 1024, &pos, L"Coaster"))
        return 0;
    if (!append(out, 1024, &pos, L"_"))
        return 0;
    for (unsigned i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20)
            out[pos++] = '-';
        out[pos++] = entry->folder[i];
    }
    out[pos] = 0;
    if (copy > 1) {
        WCHAR number[16], reverse[10];
        unsigned k = 0, j = 0;
        do {
            reverse[k++] = (WCHAR)('0' + copy % 10);
            copy /= 10;
        } while (copy);
        number[j++] = ' ';
        number[j++] = '(';
        while (k)
            number[j++] = reverse[--k];
        number[j++] = ')';
        number[j] = 0;
        if (!append(out, 1024, &pos, number))
            return 0;
    }
    return append(out, 1024, &pos, L".coaster");
}
static __declspec(noinline) int coaster_export_to(const LibraryEntry *entry,
                                                  const WCHAR *directory) {
    if (entry->readOnly || !management_validate(entry))
        return 0;
    unsigned metadataSize = management_metadata_size;
    memcpy(coaster_export_metadata, management_metadata, metadataSize);
    unsigned n = 0;
    if (!append(coaster_export_temp, 1024, &n, directory) ||
        !append(coaster_export_temp, 1024, &n, L"\\.coaster-export-") ||
        !append(coaster_export_temp, 1024, &n, entry->folder) ||
        !append(coaster_export_temp, 1024, &n, L".tmp"))
        return 0;
    HANDLE h = CreateFileW(coaster_export_temp, 0x40000000, 0, 0, 1, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    int ok = 0;
    CoasterZipEntry entries[4];
    unsigned offset = 0;
    WCHAR path[1024];
    const char *manifest = "{\"format\":\"TPM.Coaster\",\"version\":1}\n";
    if (!coaster_zip_member(h, &entries[0], "package.json", (const unsigned char *)manifest,
                            length(manifest), &offset) ||
        !coaster_zip_member(h, &entries[1], "entry.json", coaster_export_metadata, metadataSize,
                            &offset))
        goto done;
    if (!library_path(path, entry->folder, L"blueprint.json"))
        goto done;
    n = library_read(path, coaster_export_bytes, 4U * 1024U * 1024U);
    unsigned root = n ? lj_parse(coaster_export_bytes, n) : 0,
             planSize = root ? library_prepare(root, library_plan) : 0;
    if (!planSize)
        goto done;
    char sha[65];
    library_sha(library_plan, planSize, sha);
    long long definition = 0;
    memcpy(&definition, library_plan + 24, 8);
    if (!equals(sha, entry->sha) || definition != entry->definition)
        goto done;
    if (!coaster_zip_member(h, &entries[2], "blueprint.json", coaster_export_bytes, n, &offset))
        goto done;
    const WCHAR *photo = equals(entry->screenshot, "preview.png") ? L"preview.png" : L"preview.jpg";
    if (!library_path(path, entry->folder, photo))
        goto done;
    n = library_read(path, coaster_export_bytes, 20U * 1024U * 1024U);
    unsigned width = 0, height = 0;
    if (!n || !library_image_size(coaster_export_bytes, n, &width, &height) || width > 4096 ||
        height > 4096 ||
        (equals(entry->screenshot, "preview.png") && !save_complete_png(coaster_export_bytes, n)))
        goto done;
    if (!coaster_zip_member(h, &entries[3], entry->screenshot, coaster_export_bytes, n, &offset) ||
        !management_validate(entry) || management_metadata_size != metadataSize)
        goto done;
    for (unsigned i = 0; i < metadataSize; ++i)
        if (management_metadata[i] != coaster_export_metadata[i])
            goto done;
    if (!coaster_zip_finish(h, entries, 4, offset) || !FlushFileBuffers(h))
        goto done;
    ok = 1;
done:
    if (!CloseHandle(h))
        ok = 0;
    if (ok) {
        ok = 0;
        for (unsigned copy = 1; copy <= 999; ++copy) {
            if (!coaster_filename(entry, directory, copy, coaster_export_result))
                break;
            if (GetFileAttributesW(coaster_export_result) != 0xffffffffU)
                continue;
            if (MoveFileExW(coaster_export_temp, coaster_export_result, 8)) {
                ok = 1;
                break;
            }
            if (GetFileAttributesW(coaster_export_result) == 0xffffffffU)
                break;
        }
    }
    if (!ok)
        DeleteFileW(coaster_export_temp);
    return ok;
}
