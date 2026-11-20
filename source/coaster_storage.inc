/* One canonical per-user root. Never derive coaster storage from the DLL/game directory. */
DLLIMPORT DWORD WINAPI GetEnvironmentVariableW(const WCHAR *, WCHAR *, DWORD);
DLLIMPORT DWORD WINAPI GetFileAttributesW(const WCHAR *);
static int coaster_storage_path(WCHAR *out, unsigned capacity, const WCHAR *child, int create) {
    DWORD size = GetEnvironmentVariableW(L"USERPROFILE", out, capacity);
    if (!size || size >= capacity)
        return 0;
    unsigned n = size;
    if (!append(out, capacity, &n, L"\\AppData\\LocalLow\\Two Point Studios"))
        return 0;
    if (create) {
        DWORD attr = GetFileAttributesW(out);
        if (attr == 0xffffffffU || !(attr & 0x10) || (attr & 0x400))
            return 0;
    }
    if (!append(out, capacity, &n, L"\\TPM Coaster Library"))
        return 0;
    if (create) {
        DWORD attr = GetFileAttributesW(out);
        if (attr == 0xffffffffU) {
            if (!CreateDirectoryW(out, 0))
                return 0;
            attr = GetFileAttributesW(out);
        }
        if (!(attr & 0x10) || (attr & 0x400))
            return 0;
    }
    if (child && *child) {
        if (!append(out, capacity, &n, L"\\") || !append(out, capacity, &n, child))
            return 0;
        if (create) {
            DWORD attr = GetFileAttributesW(out);
            if (attr == 0xffffffffU) {
                if (!CreateDirectoryW(out, 0))
                    return 0;
                attr = GetFileAttributesW(out);
            }
            if (!(attr & 0x10) || (attr & 0x400))
                return 0;
        }
    }
    return 1;
}
