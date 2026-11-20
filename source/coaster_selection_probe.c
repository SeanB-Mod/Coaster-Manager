/* Coaster Manager v1.1.5 cached-shared-assets candidate - coaster blueprint library.
 * Native save/build workflows; experimental developer hotkeys are off by default.
 * Hooks MethodInfo.methodPointer only AFTER strict sanity checks; does not patch code.
 * Developed with assistance from ChatGPT by OpenAI.
 */
#if !defined(_MSC_VER) || !defined(_M_X64)
#error Compile for Windows x64 with clang-cl / MSVC.
#endif
#define WINAPI __stdcall
#define DLLIMPORT __declspec(dllimport)
#define DLL_PROCESS_ATTACH 1
#ifndef TPM_ENABLE_DIAGNOSTICS
#define TPM_ENABLE_DIAGNOSTICS 0
#endif
#define VK_F8 0x77
#define VK_F9 0x78
#define FILE_APPEND_DATA 0x00000004
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x80
#define PAGE_READWRITE 0x04
#define INVALID_HANDLE_VALUE ((void *)(long long)-1)
typedef unsigned short WCHAR;
typedef unsigned long DWORD;
typedef unsigned long long SIZE_T;
typedef unsigned long long UINT_PTR;
typedef int BOOL;
typedef void *HMODULE;
typedef void *HANDLE;
typedef void *HINSTANCE;
typedef void *FARPROC;
/* LLVM may lower fixed copies to memcpy. Keep production DLL CRT-free. */
int _fltused = 0;
void *memcpy(void *dst, const void *src, SIZE_T n) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const volatile unsigned char *s = (const volatile unsigned char *)src;
    for (SIZE_T i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}
SIZE_T strlen(const char *s) {
    const volatile char *p = s;
    SIZE_T n = 0;
    while (p[n])
        ++n;
    return n;
}
DLLIMPORT HMODULE WINAPI GetModuleHandleW(const WCHAR *);
DLLIMPORT HMODULE WINAPI LoadLibraryW(const WCHAR *);
DLLIMPORT BOOL WINAPI FreeLibrary(HMODULE);
DLLIMPORT int WINAPI MultiByteToWideChar(unsigned, DWORD, const char *, int, WCHAR *, int);
void *memset(void *dst, int value, SIZE_T n) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    for (SIZE_T i = 0; i < n; ++i)
        d[i] = (unsigned char)value;
    return dst;
}
DLLIMPORT FARPROC WINAPI GetProcAddress(HMODULE, const char *);
DLLIMPORT HANDLE WINAPI CreateThread(void *, SIZE_T, DWORD(WINAPI *)(void *), void *, DWORD,
                                     DWORD *);
DLLIMPORT BOOL WINAPI CloseHandle(HANDLE);
DLLIMPORT DWORD WINAPI GetModuleFileNameW(HMODULE, WCHAR *, DWORD);
DLLIMPORT DWORD WINAPI GetCurrentThreadId(void);
DLLIMPORT BOOL WINAPI CreateDirectoryW(const WCHAR *, void *);
DLLIMPORT HANDLE WINAPI CreateFileW(const WCHAR *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
DLLIMPORT BOOL WINAPI ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
DLLIMPORT BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
DLLIMPORT BOOL WINAPI FlushFileBuffers(HANDLE);
DLLIMPORT BOOL WINAPI VirtualProtect(void *, SIZE_T, DWORD, DWORD *);
DLLIMPORT short WINAPI GetAsyncKeyState(int);
DLLIMPORT void WINAPI Sleep(DWORD);
DLLIMPORT HANDLE WINAPI GetCurrentProcess(void);
DLLIMPORT BOOL WINAPI ReadProcessMemory(HANDLE, const void *, void *, SIZE_T, SIZE_T *);
DLLIMPORT UINT_PTR WINAPI GetTickCount64(void);
DLLIMPORT BOOL WINAPI MoveFileExW(const WCHAR *, const WCHAR *, DWORD);
DLLIMPORT BOOL WINAPI DeleteFileW(const WCHAR *);

static HMODULE g_module;
static HANDLE g_log = INVALID_HANDLE_VALUE;
static volatile long g_pending = 0;
static volatile long g_hook_count = 0;
static volatile long g_export_count = 0;
static DWORD g_pending_tick = 0; /* worker thread only */
static unsigned length(const char *s) {
    unsigned i = 0;
    if (!s)
        return 0;
    while (s[i] && i < 16384)
        ++i;
    return i;
}
DLLIMPORT BOOL WINAPI GetFileSizeEx(HANDLE, long long *);
DLLIMPORT BOOL WINAPI MoveFileExW(const WCHAR *, const WCHAR *, DWORD);
#include "log_budget.h"
static void logtext(const char *s) {
    if (!s || g_log == INVALID_HANDLE_VALUE)
        return;
    unsigned n = length(s);
    int claim = log_budget_reserve(n);
    if (!claim)
        return;
    DWORD w = 0;
    if (claim < 0) {
        static const char marker[] =
            "\r\n[LOG LIMIT REACHED: further messages suppressed until next game start]\r\n";
        WriteFile(g_log, marker, sizeof(marker) - 1, &w, 0);
        return;
    }
    WriteFile(g_log, s, n, &w, 0);
}
static void nl(void) {
    logtext("\r\n");
}
static void loghex(UINT_PTR x) {
    char s[19];
    s[0] = '0';
    s[1] = 'x';
    for (int j = 0; j < 16; ++j) {
        unsigned v = (unsigned)((x >> ((15 - j) * 4)) & 15);
        s[2 + j] = (char)(v < 10 ? '0' + v : 'A' + v - 10);
    }
    s[18] = 0;
    logtext(s);
}
static void lognum(UINT_PTR x) {
    char b[32], s[32];
    unsigned n = 0, i = 0;
    do {
        b[n++] = (char)('0' + x % 10);
        x /= 10;
    } while (x && n < 30);
    while (n)
        s[i++] = b[--n];
    s[i] = 0;
    logtext(s);
}
static int equals(const char *a, const char *b) {
    if (!a || !b)
        return 0;
    while (*a && *b) {
        if (*a++ != *b++)
            return 0;
    }
    return *a == *b;
}
static int append(WCHAR *d, unsigned cap, unsigned *pos, const WCHAR *s) {
    while (*s) {
        if (*pos >= cap - 1)
            return 0;
        d[(*pos)++] = *s++;
    }
    d[*pos] = 0;
    return 1;
}
static void log_stamp(void) {
    logtext("[uptime-ms=");
    lognum(GetTickCount64());
    logtext(" thread=");
    lognum(GetCurrentThreadId());
    logtext("] ");
}
DLLIMPORT BOOL WINAPI QueryPerformanceCounter(long long *);
DLLIMPORT BOOL WINAPI QueryPerformanceFrequency(long long *);
static long long perf_now(void) {
    long long n = 0;
    QueryPerformanceCounter(&n);
    return n;
}
static UINT_PTR perf_us(long long start) {
    static long long frequency;
    if (!frequency && (!QueryPerformanceFrequency(&frequency) || frequency <= 0))
        return 0;
    long long end = perf_now();
    return start > 0 && end >= start ? (UINT_PTR)((end - start) * 1000000 / frequency) : 0;
}
static void perf_memory(void) {
    typedef BOOL(WINAPI * MemoryQuery)(HANDLE, void *, DWORD);
    struct {
        DWORD size, faults;
        SIZE_T values[9];
    } counters = {0};
    MemoryQuery query =
        (MemoryQuery)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo");
    counters.size = sizeof(counters);
    if (query && query(GetCurrentProcess(), &counters, sizeof(counters))) {
        logtext(" process_working_set_MiB=");
        lognum(counters.values[1] / (1024 * 1024));
        logtext(" process_private_MiB=");
        lognum(counters.values[8] / (1024 * 1024));
    }
}
static int initlog(void) {
    WCHAR path[512];
    DWORD n = GetModuleFileNameW(0, path, 512);
    if (!n || n >= 400)
        return 0;
    while (n && path[n - 1] != '\\' && path[n - 1] != '/')
        --n;
    if (!n)
        return 0;
    path[n] = 0;
    unsigned i = n;
    if (!append(path, 512, &i, L"Mods"))
        return 0;
    CreateDirectoryW(path, 0);
    if (!append(path, 512, &i, L"\\Logs"))
        return 0;
    CreateDirectoryW(path, 0);
    i = n;
    path[n] = 0;
    if (!append(path, 512, &i, L"Mods\\Logs\\TPM-Coaster-Tools.log"))
        return 0;
    g_log = CreateFileW(path, 0x80000000U | FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (g_log == INVALID_HANDLE_VALUE)
        return 0;
    long long size = 0;
    if (!GetFileSizeEx(g_log, &size) || size < 0) {
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
        return 0;
    }
    if (size >= LOG_DATA_LIMIT) {
        WCHAR previous[512];
        memcpy(previous, path, (i + 1) * sizeof(WCHAR));
        unsigned used = i;
        if (append(previous, 512, &used, L".previous")) {
            CloseHandle(g_log);
            g_log = INVALID_HANDLE_VALUE;
            if (MoveFileExW(path, previous, 1 | 8))
                size = 0;
            g_log = CreateFileW(path, 0x80000000U | FILE_APPEND_DATA,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, 0);
            if (g_log == INVALID_HANDLE_VALUE || !GetFileSizeEx(g_log, &size)) {
                if (g_log != INVALID_HANDLE_VALUE)
                    CloseHandle(g_log);
                g_log = INVALID_HANDLE_VALUE;
                return 0;
            }
        }
    }
    log_budget_bytes = size >= LOG_DATA_LIMIT ? LOG_BYTE_LIMIT : (long)size;
    log_stamp();
    logtext("log session started; 8 MiB cap, one previous log retained\r\n");
    return 1;
}

typedef void *(*fn_string_new)(const char *);
typedef void *(*fn_string_new_utf16)(const WCHAR *, int);
typedef const WCHAR *(*fn_string_chars)(void *);
typedef int (*fn_string_length)(void *);
typedef void *(*fn_domain_get)(void);
typedef void *(*fn_thread_attach)(void *);
typedef void (*fn_thread_detach)(void *);
typedef void *(*fn_thread_current)(void);
typedef void **(*fn_domain_get_assemblies)(void *, SIZE_T *);
typedef void *(*fn_assembly_get_image)(void *);
typedef void *(*fn_class_from_name)(void *, const char *, const char *);
typedef void *(*fn_class_get_method_from_name)(void *, const char *, int);
typedef void *(*fn_class_get_field_from_name)(void *, const char *);
typedef void *(*fn_class_from_type)(void *);
typedef void *(*fn_class_get_parent)(void *);
typedef const char *(*fn_class_get_name)(void *);
typedef const char *(*fn_class_get_namespace)(void *);
typedef SIZE_T (*fn_image_get_class_count)(void *);
typedef void *(*fn_image_get_class)(void *, SIZE_T);
typedef void *(*fn_class_get_methods)(void *, void **);
typedef void *(*fn_class_get_fields)(void *, void **);
typedef const char *(*fn_field_get_name)(void *);
typedef SIZE_T (*fn_field_get_offset)(void *);
typedef const char *(*fn_method_get_name)(void *);
typedef unsigned char (*fn_method_is_generic)(void *);
typedef void *(*fn_class_get_type)(void *);
typedef void *(*fn_type_get_object)(void *);
typedef void *(*fn_object_get_class)(void *);
typedef unsigned char (*fn_class_is_assignable_from)(void *, void *);
typedef int (*fn_class_value_size)(void *, unsigned *);
typedef unsigned char (*fn_class_is_valuetype)(void *);
typedef void *(*fn_field_get_type)(void *);
typedef void (*fn_field_get_value)(void *, void *, void *);
typedef void (*fn_field_static_get_value)(void *, void *);
typedef void (*fn_field_set_value)(void *, void *, void *);
typedef void (*fn_field_set_value_object)(void *, void *, void *);
typedef void *(*fn_object_new)(void *);
typedef void *(*fn_value_box)(void *, void *);
typedef void *(*fn_method_get_return_type)(void *);
typedef void *(*fn_method_get_param)(void *, unsigned);
typedef unsigned (*fn_method_get_flags)(void *, unsigned *);
typedef void *(*fn_runtime_invoke)(void *, void *, void **, void **);
typedef void *(*fn_object_unbox)(void *);
typedef void *(*fn_array_new)(void *, SIZE_T);
typedef void *(*fn_method_get_object)(void *, void *);
typedef void *(*fn_method_get_from_reflection)(void *);
typedef void *(*fn_object_get_virtual_method)(void *, void *);
typedef unsigned char (*fn_method_is_inflated)(void *);
typedef unsigned (*fn_method_get_param_count)(void *);
typedef unsigned char (*fn_method_is_instance)(void *);
typedef char *(*fn_type_get_name)(void *);
typedef void (*fn_free)(void *);
typedef void *(*fn_field_get_parent)(void *);
typedef int (*fn_field_get_flags)(void *);
typedef void *(*fn_method_get_class)(void *);
typedef void *(*fn_field_get_value_object)(void *, void *);
/* Unity 6 Il2CppGCHandle is void*, not the legacy uint32_t. */
typedef void *GCHandle;
typedef char gc_handle_must_be_64_bit[(sizeof(GCHandle) == 8) ? 1 : -1];
typedef GCHandle (*fn_gchandle_new)(void *, unsigned char);
typedef void *(*fn_gchandle_get_target)(GCHandle);
typedef void (*fn_gchandle_free)(GCHandle);
#define APIS(X)                                                                                    \
    X(string_new_utf16)                                                                            \
    X(string_chars)                                                                                \
    X(string_length) X(string_new) X(field_set_value) X(field_set_value_object) X(object_new) X(   \
        value_box) X(field_static_get_value) X(class_get_name) X(class_get_namespace)              \
        X(image_get_class_count) X(image_get_class) X(class_get_parent) X(field_get_parent) X(     \
            field_get_flags) X(method_get_class) X(field_get_value_object) X(gchandle_new)         \
            X(gchandle_get_target) X(gchandle_free) X(domain_get) X(thread_attach) X(              \
                thread_detach) X(thread_current) X(domain_get_assemblies) X(assembly_get_image)    \
                X(class_from_name) X(class_get_method_from_name) X(class_get_field_from_name)      \
                    X(class_from_type) X(class_get_methods) X(class_get_fields) X(field_get_name)  \
                        X(field_get_offset) X(method_get_name) X(method_is_generic)                \
                            X(class_get_type) X(type_get_object) X(object_get_class)               \
                                X(class_is_assignable_from) X(class_value_size)                    \
                                    X(class_is_valuetype) X(field_get_type) X(field_get_value)     \
                                        X(method_get_return_type) X(method_get_param)              \
                                            X(method_get_flags) X(runtime_invoke) X(object_unbox)  \
                                                X(array_new) X(method_get_object)                  \
                                                    X(method_get_from_reflection)                  \
                                                        X(object_get_virtual_method)               \
                                                            X(method_is_inflated)                  \
                                                                X(method_get_param_count)          \
                                                                    X(method_is_instance)          \
                                                                        X(type_get_name) X(free)
#define DECL(N) static fn_##N p_##N;
APIS(DECL)
#undef DECL
static int resolve(HMODULE game) {
#define RES(N)                                                                                     \
    p_##N = (fn_##N)GetProcAddress(game, "il2cpp_" #N);                                            \
    if (!p_##N) {                                                                                  \
        logtext("Missing il2cpp_" #N "\r\n");                                                      \
        return 0;                                                                                  \
    }
    APIS(RES)
#undef RES
    return 1;
}
static void *findclass(void *domain, const char *ns, const char *name) {
    SIZE_T count = 0;
    void **list = p_domain_get_assemblies(domain, &count);
    if (!list || count > 10000)
        return 0;
    for (SIZE_T i = 0; i < count; ++i) {
        void *image = p_assembly_get_image(list[i]);
        if (!image)
            continue;
        void *c = p_class_from_name(image, ns, name);
        if (c)
            return c;
    }
    return 0;
}

typedef void (*native_ui_lateupdate)(void *object, void *methodInfo);
static native_ui_lateupdate g_original = 0;
static void *g_ui_method = 0;
static void *g_station_field = 0;
static void *g_support_field = 0;
static DWORD g_hook_first_thread = 0;
static void *g_gameitem_class = 0;
static void *g_managed_class = 0;
static void *g_id_field = 0;
static void *g_entity_field = 0;
static int g_id_size = 0;
static int g_entity_size = 0;
static int g_managed_fields_verified = 0;
static void *g_exists_method = 0;
static void *g_hascomponent_method = 0;
static void *g_ride_class = 0;
static void *g_snapshot_class = 0;
static int g_hascomponent_verified = 0;
static void *g_component_type_factory = 0;
static void *g_component_type_class = 0;
static int g_component_type_size = 0;
static int g_component_factory_verified = 0;
static int g_exists_verified = 0;
static void *g_snapshot_reader = 0;
static void *g_snapshot_flag_field = 0;
static int g_snapshot_reader_verified = 0;
static int g_snapshot_flag_verified = 0;

/* Validate the non-generic, static TPS.Core.Entities.EntityManager.Exists(Entity)
 * before invoking it. No generic ECS component APIs are called in v0.3.7.
 */
static int setup_entity_exists(void) {
    void *cls = findclass(p_domain_get(), "TPS.Core.Entities", "EntityManager");
    if (!cls) {
        logtext("SKIP: TPS.Core.Entities.EntityManager unavailable.\r\n");
        return 0;
    }
    void *m = p_class_get_method_from_name(cls, "Exists", 1);
    if (!m || p_method_get_param_count(m) != 1) {
        logtext("SKIP: EntityManager.Exists(Entity) signature unavailable.\r\n");
        return 0;
    }
    unsigned impl = 0;
    unsigned flags = p_method_get_flags(m, &impl);
    if (!(flags & 0x0010)) {
        logtext("SKIP: EntityManager.Exists must be static.\r\n");
        return 0;
    }
    void *rt = p_method_get_return_type(m);
    void *pt = p_method_get_param(m, 0);
    char *ret = rt ? p_type_get_name(rt) : 0;
    char *arg = pt ? p_type_get_name(pt) : 0;
    int names_ok = equals(ret, "System.Boolean") && equals(arg, "Unity.Entities.Entity");
    logtext("EntityManager.Exists return type: ");
    logtext(ret ? ret : "?");
    nl();
    logtext("EntityManager.Exists parameter type: ");
    logtext(arg ? arg : "?");
    nl();
    if (ret)
        p_free(ret);
    if (arg)
        p_free(arg);
    if (!names_ok) {
        logtext("SKIP: Exists return/parameter mismatch.\r\n");
        return 0;
    }
    void *argClass = p_class_from_type(pt);
    unsigned align = 0;
    if (!argClass || !p_class_is_valuetype(argClass) || p_class_value_size(argClass, &align) != 8) {
        logtext("SKIP: ECS Entity arg is not an eight-byte value type.\r\n");
        return 0;
    }
    g_exists_method = m;
    g_exists_verified = 1;
    logtext("Verified static EntityManager.Exists(Entity) ABI (ECS query not yet invoked).\r\n");
    return 1;
}

/* Unity ECS exposes non-generic HasComponent(Entity, ComponentType), NOT
 * HasComponent(Entity, System.Type). Discover the exact method and the exact
 * ComponentType.ReadOnly(System.Type) factory. NEVER manufacture TypeIndex /
 * ComponentType internals from guessed bit layouts. */
static void setup_hascomponent(void) {
    void *manager = findclass(p_domain_get(), "TPS.Core.Entities", "EntityManager");
    g_ride_class = findclass(p_domain_get(), "TPS.Game", "ECTrackRide");
    g_snapshot_class = findclass(p_domain_get(), "TPS.Game", "ECTrackRideSnapshot");
    g_component_type_class = findclass(p_domain_get(), "Unity.Entities", "ComponentType");
    if (!manager || !g_ride_class || !g_snapshot_class || !g_component_type_class) {
        logtext("SKIP: required ECS manager, ComponentType or ride types unavailable.\r\n");
        return;
    }
    unsigned align = 0;
    g_component_type_size = p_class_value_size(g_component_type_class, &align);
    logtext("Unity.Entities.ComponentType runtime size=");
    lognum(g_component_type_size > 0 ? (UINT_PTR)g_component_type_size : 0);
    nl();
    if (!p_class_is_valuetype(g_component_type_class) || g_component_type_size < 4 ||
        g_component_type_size > 64) {
        logtext("SKIP: ComponentType is not a supported small value type.\r\n");
        return;
    }
    logtext("--- TPS.Core.Entities.EntityManager.HasComponent overloads ---\r\n");
    void *iter = 0;
    void *m = 0;
    while ((m = p_class_get_methods(manager, &iter)) != 0) {
        if (!equals(p_method_get_name(m), "HasComponent"))
            continue;
        unsigned impl = 0, flags = p_method_get_flags(m, &impl);
        unsigned n = p_method_get_param_count(m);
        int generic = p_method_is_generic(m) != 0;
        void *ret = p_method_get_return_type(m);
        char *retname = ret ? p_type_get_name(ret) : 0;
        logtext(" HasComponent ");
        logtext(flags & 0x0010 ? "static " : "instance ");
        logtext(generic ? "generic " : "non-generic ");
        logtext("returns ");
        logtext(retname ? retname : "?");
        logtext(" params=");
        lognum(n);
        nl();
        int candidate = (flags & 0x0010) && !generic && equals(retname, "System.Boolean") && n == 2;
        for (unsigned i = 0; i < n && i < 8; ++i) {
            void *pt = p_method_get_param(m, i);
            char *name = pt ? p_type_get_name(pt) : 0;
            logtext("  arg");
            lognum(i);
            logtext(" = ");
            logtext(name ? name : "?");
            nl();
            if (i == 0 && !equals(name, "Unity.Entities.Entity"))
                candidate = 0;
            if (i == 1 && !equals(name, "Unity.Entities.ComponentType"))
                candidate = 0;
            if (name)
                p_free(name);
        }
        if (retname)
            p_free(retname);
        if (candidate) {
            g_hascomponent_method = m;
            g_hascomponent_verified = 1;
            logtext(" Verified HasComponent(Entity,ComponentType) signature.\r\n");
        }
    }
    if (!g_hascomponent_verified) {
        logtext("SKIP: no exact non-generic HasComponent(Entity,ComponentType).\r\n");
        return;
    }

    logtext("--- Unity.Entities.ComponentType factories ---\r\n");
    iter = 0;
    m = 0;
    while ((m = p_class_get_methods(g_component_type_class, &iter)) != 0) {
        const char *methodName = p_method_get_name(m);
        if (!equals(methodName, "ReadOnly") && !equals(methodName, "ReadWrite"))
            continue;
        unsigned impl = 0, flags = p_method_get_flags(m, &impl);
        int generic = p_method_is_generic(m) != 0;
        unsigned n = p_method_get_param_count(m);
        void *rt = p_method_get_return_type(m);
        char *rn = rt ? p_type_get_name(rt) : 0;
        logtext(" ");
        logtext(methodName);
        logtext(flags & 0x0010 ? " static " : " instance ");
        logtext(generic ? "generic " : "non-generic ");
        logtext("return ");
        logtext(rn ? rn : "?");
        logtext(" params=");
        lognum(n);
        nl();
        int match = equals(methodName, "ReadOnly") && (flags & 0x0010) && !generic &&
                    equals(rn, "Unity.Entities.ComponentType") && n == 1;
        for (unsigned i = 0; i < n && i < 4; ++i) {
            void *pt = p_method_get_param(m, i);
            char *pn = pt ? p_type_get_name(pt) : 0;
            logtext("  arg");
            lognum(i);
            logtext(" = ");
            logtext(pn ? pn : "?");
            nl();
            if (i == 0 && !equals(pn, "System.Type"))
                match = 0;
            if (pn)
                p_free(pn);
        }
        if (rn)
            p_free(rn);
        if (match) {
            g_component_type_factory = m;
            g_component_factory_verified = 1;
            logtext(" Verified ComponentType.ReadOnly(System.Type) factory.\r\n");
        }
    }
    if (!g_component_factory_verified) {
        logtext(
            "SKIP: no verified ComponentType.ReadOnly(System.Type) factory; F8 will NOT query components.\r\n");
    } else {
        logtext("Guarded component-presence test enabled for F8 (no component contents read).\r\n");
    }
}

/* Run once on the IL2CPP-attached worker. Metadata introspection only.
 * Never invoke a component getter, never compute an ECS pointer, never touch
 * save state. Used to choose a subsequent guarded accessor implementation. */
static int begins(const char *s, const char *prefix) {
    if (!s || !prefix)
        return 0;
    while (*prefix) {
        if (*s++ != *prefix++)
            return 0;
    }
    return 1;
}
/* Check runtime-constructed generic method pointers against the loaded PE
 * rather than invoking an unverified native entrypoint. */
static int is_executable_game_pointer(void *ptr) {
    HMODULE module = GetModuleHandleW(L"GameAssembly.dll");
    if (!module || !ptr)
        return 0;
    unsigned char *base = (unsigned char *)module;
    if (base[0] != 'M' || base[1] != 'Z')
        return 0;
    unsigned peoff = *(unsigned *)(base + 0x3c);
    if (peoff < 64 || peoff > 4096 || base[peoff] != 'P' || base[peoff + 1] != 'E')
        return 0;
    unsigned imageSize = *(unsigned *)(base + peoff + 24 + 0x38);
    if (imageSize < 65536 || imageSize > 1000000000U || (UINT_PTR)ptr < (UINT_PTR)base ||
        (UINT_PTR)ptr >= (UINT_PTR)(base + imageSize))
        return 0;
    unsigned nSections = *(unsigned short *)(base + peoff + 6);
    unsigned optSize = *(unsigned short *)(base + peoff + 20);
    if (nSections == 0 || nSections > 96 || optSize < 0x60 || optSize > 512)
        return 0;
    unsigned char *sections = base + peoff + 24 + optSize;
    unsigned rel = (unsigned)((UINT_PTR)ptr - (UINT_PTR)base);
    for (unsigned i = 0; i < nSections; ++i) {
        unsigned char *sect = sections + i * 40;
        unsigned vsize = *(unsigned *)(sect + 8), va = *(unsigned *)(sect + 12);
        unsigned flags = *(unsigned *)(sect + 36);
        if (rel >= va && rel - va < vsize && (flags & 0x20000000U))
            return 1;
    }
    return 0;
}

/* v1.1.0 guarded reflection binding for the *reference-type* snapshot.
 * Unity ECS component getter is generic. Reflection creates a CLOSED method,
 * and the IL2CPP MethodInfo returned by reflection is checked before use.
 * This runs on the attached worker; no scene or ECS getter is called here.
 */
static void setup_snapshot_reader(void) {
    void *manager = findclass(p_domain_get(), "TPS.Core.Entities", "EntityManager");
    void *systemArray = findclass(p_domain_get(), "System", "Array");
    void *systemType = findclass(p_domain_get(), "System", "Type");
    void *reflectionMethod = findclass(p_domain_get(), "System.Reflection", "MethodInfo");
    g_snapshot_flag_field =
        g_snapshot_class ? p_class_get_field_from_name(g_snapshot_class, "HasSnapshotData") : 0;
    if (!manager || !systemArray || !systemType || !reflectionMethod || !g_snapshot_flag_field ||
        p_class_is_valuetype(g_snapshot_class)) {
        logtext(
            "SNAPSHOT READ DISABLED: classes/field unavailable or snapshot not reference type.\r\n");
        return;
    }
    void *flagType = p_field_get_type(g_snapshot_flag_field);
    void *boolClass = flagType ? p_class_from_type(flagType) : 0;
    unsigned align = 0;
    char *flagName = flagType ? p_type_get_name(flagType) : 0;
    int flagOK = equals(flagName, "System.Boolean") && boolClass &&
                 p_class_is_valuetype(boolClass) && p_class_value_size(boolClass, &align) == 1;
    logtext("HasSnapshotData managed field: ");
    logtext(flagName ? flagName : "?");
    logtext(flagOK ? " (1-byte Boolean; VERIFIED)\r\n" : " (UNVERIFIED)\r\n");
    if (flagName)
        p_free(flagName);
    if (!flagOK)
        return;
    g_snapshot_flag_verified = 1;

    void *generic = 0;
    void *iter = 0;
    void *m = 0;
    while ((m = p_class_get_methods(manager, &iter))) {
        if (!equals(p_method_get_name(m), "GetComponentClass") || !p_method_is_generic(m) ||
            p_method_get_param_count(m) != 1)
            continue;
        unsigned impl = 0, flags = p_method_get_flags(m, &impl);
        void *param = p_method_get_param(m, 0);
        char *pn = param ? p_type_get_name(param) : 0;
        int match = (flags & 0x0010) != 0 && equals(pn, "Unity.Entities.Entity");
        if (pn)
            p_free(pn);
        if (match) {
            generic = m;
            break;
        }
    }
    if (!generic) {
        logtext("SNAPSHOT READ DISABLED: generic GetComponentClass<T>(Entity) not found.\r\n");
        return;
    }

    /* Obtain array of System.Type. Avoid hardcoding IL2CPP array offsets:
   * initialize through managed System.Array.SetValue(Object,Int32). */
    void *setValue = 0;
    iter = 0;
    while ((m = p_class_get_methods(systemArray, &iter))) {
        if (!equals(p_method_get_name(m), "SetValue") || p_method_get_param_count(m) != 2)
            continue;
        unsigned impl = 0, flags = p_method_get_flags(m, &impl);
        void *pt0 = p_method_get_param(m, 0);
        void *pt1 = p_method_get_param(m, 1);
        char *n0 = pt0 ? p_type_get_name(pt0) : 0;
        char *n1 = pt1 ? p_type_get_name(pt1) : 0;
        int match = !(flags & 0x0010) && equals(n0, "System.Object") && equals(n1, "System.Int32");
        if (n0)
            p_free(n0);
        if (n1)
            p_free(n1);
        if (match) {
            setValue = m;
            break;
        }
    }
    if (!setValue) {
        logtext("SNAPSHOT READ DISABLED: System.Array.SetValue(Object,Int32) absent.\r\n");
        return;
    }
    void *methodType = p_class_get_type(g_snapshot_class);
    void *managedType = methodType ? p_type_get_object(methodType) : 0;
    void *typesArray = p_array_new(systemType, 1);
    if (!managedType || !typesArray) {
        logtext("SNAPSHOT READ DISABLED: managed System.Type array unavailable.\r\n");
        return;
    }
    int index = 0;
    void *argSet[2] = {managedType, &index};
    void *exception = 0;
    logtext("Constructing generic method Type[] using Array.SetValue.\r\n");
    p_runtime_invoke(setValue, typesArray, argSet, &exception);
    if (exception) {
        logtext("SNAPSHOT READ DISABLED: Type[] assignment exception.\r\n");
        return;
    }

    void *methodObj = p_method_get_object(generic, manager);
    if (!methodObj ||
        !p_class_is_assignable_from(reflectionMethod, p_object_get_class(methodObj))) {
        logtext("SNAPSHOT READ DISABLED: GetComponentClass reflection MethodInfo unavailable.\r\n");
        return;
    }
    /* Resolve virtual MakeGenericMethod(Type[]) on the actual runtime MethodInfo. */
    void *make = 0;
    iter = 0;
    while ((m = p_class_get_methods(reflectionMethod, &iter))) {
        if (!equals(p_method_get_name(m), "MakeGenericMethod") || p_method_get_param_count(m) != 1)
            continue;
        void *pt = p_method_get_param(m, 0);
        char *pn = pt ? p_type_get_name(pt) : 0;
        unsigned impl = 0, flags = p_method_get_flags(m, &impl);
        int match = !(flags & 0x0010) && equals(pn, "System.Type[]");
        if (pn)
            p_free(pn);
        if (match) {
            make = m;
            break;
        }
    }
    if (!make) {
        logtext("SNAPSHOT READ DISABLED: MakeGenericMethod(Type[]) unavailable.\r\n");
        return;
    }
    void *actualMake = p_object_get_virtual_method(methodObj, make);
    if (!actualMake) {
        logtext("SNAPSHOT READ DISABLED: cannot resolve virtual MakeGenericMethod.\r\n");
        return;
    }
    void *args[1] = {typesArray};
    exception = 0;
    logtext("About to close GetComponentClass<T> for ECTrackRideSnapshot via reflection.\r\n");
    void *closedObject = p_runtime_invoke(actualMake, methodObj, args, &exception);
    if (exception || !closedObject) {
        logtext("SNAPSHOT READ DISABLED: generic method reflection threw exception/null.\r\n");
        return;
    }
    if (!p_class_is_assignable_from(reflectionMethod, p_object_get_class(closedObject))) {
        logtext("SNAPSHOT READ DISABLED: generic result not Reflection.MethodInfo.\r\n");
        return;
    }
    void *closed = p_method_get_from_reflection(closedObject);
    if (!closed || !p_method_is_inflated(closed) || p_method_get_param_count(closed) != 1) {
        logtext("SNAPSHOT READ DISABLED: method was not inflated with one argument.\r\n");
        return;
    }
    void *param = p_method_get_param(closed, 0);
    void *ret = p_method_get_return_type(closed);
    char *pn = param ? p_type_get_name(param) : 0;
    char *rn = ret ? p_type_get_name(ret) : 0;
    unsigned impl = 0, flags = p_method_get_flags(closed, &impl);
    int valid = equals(p_method_get_name(closed), "GetComponentClass") && (flags & 0x0010) &&
                p_method_get_class(closed) == manager && equals(pn, "Unity.Entities.Entity") &&
                equals(rn, "TPS.Game.ECTrackRideSnapshot");
    logtext("Closed method: GetComponentClass(Entity) -> ");
    logtext(rn ? rn : "?");
    nl();
    if (pn)
        p_free(pn);
    if (rn)
        p_free(rn);
    if (!valid) {
        logtext("SNAPSHOT READ DISABLED: closed method signature mismatch.\r\n");
        return;
    }
    if (!is_executable_game_pointer(*(void **)closed)) {
        logtext(
            "SNAPSHOT READ DISABLED: closed getter native pointer is not executable GameAssembly code.\r\n");
        return;
    }
    g_snapshot_reader = closed;
    g_snapshot_reader_verified = 1;
    logtext(
        "VERIFIED: closed GetComponentClass<ECTrackRideSnapshot>(Entity); F8 may read snapshot list counts after validation.\r\n");
}

/* Field access on a runtime-boxed value deliberately avoids object-header
 * subtraction and raw offsets into an unboxed value containing GC references. */
static GCHandle pin_reference(void *obj) {
    GCHandle handle = p_gchandle_new(obj, 1);
    if (!handle) {
        logtext("SKIP: GC handle creation failed.\r\n");
        return 0;
    }
    if (p_gchandle_get_target(handle) != obj) {
        /* Do not call Free on a handle whose target could not be validated.
     * A retained handle is preferable to retrying an incompatible ABI. */
        logtext("SKIP: GC handle target differs; handle retained, probe stopped.\r\n");
        return 0;
    }
    return handle;
}
static void release_reference(GCHandle handle) {
    /* Flush at operation boundaries rather than for every copied record. */
    p_gchandle_free(handle);
}
static int exact_type(void *t, const char *expected) {
    char *n = t ? p_type_get_name(t) : 0;
    int ok = equals(n, expected);
    if (!ok) {
        logtext("SKIP: expected ");
        logtext(expected);
        logtext("; actual ");
        logtext(n ? n : "NULL");
        nl();
    }
    if (n)
        p_free(n);
    return ok;
}
static int instance_field(void *f, void *owner) {
    if (!f || p_field_get_parent(f) != owner || (p_field_get_flags(f) & 0x0010)) {
        logtext("SKIP: missing, inherited or static snapshot field.\r\n");
        return 0;
    }
    return 1;
}
static int read_list_count(void *boxedData, void *dataClass, const char *fieldName,
                           const char *expected) {
    logtext("Validating ");
    logtext(fieldName);
    nl();
    void *f = p_class_get_field_from_name(dataClass, fieldName);
    if (!instance_field(f, dataClass) || !exact_type(p_field_get_type(f), expected))
        return 0;
    void *listClass = p_class_from_type(p_field_get_type(f));
    if (!listClass || p_class_is_valuetype(listClass)) {
        logtext("SKIP: list is not a reference type.\r\n");
        return 0;
    }
    void *list = 0;
    logtext("About to read boxed snapshot list reference.\r\n");
    p_field_get_value(boxedData, f, &list);
    if (!list) {
        logtext("SKIP: ");
        logtext(fieldName);
        logtext(" is NULL.\r\n");
        return 0;
    }
    if (p_object_get_class(list) != listClass) {
        logtext("SKIP: concrete list runtime class differs.\r\n");
        return 0;
    }
    GCHandle root = pin_reference(list);
    if (!root) {
        logtext("SKIP: cannot root list.\r\n");
        return 0;
    }
    int ok = 0;
    void *m = p_class_get_method_from_name(listClass, "get_Count", 0);
    if (!m || p_method_get_class(m) != listClass || !p_method_is_instance(m) ||
        p_method_is_generic(m) || p_method_get_param_count(m) != 0 ||
        !exact_type(p_method_get_return_type(m), "System.Int32")) {
        logtext("SKIP: instance List.get_Count() signature differs.\r\n");
    } else {
        void *exception = 0;
        logtext("About to invoke ");
        logtext(fieldName);
        logtext(".get_Count().\r\n");
        void *result = p_runtime_invoke(m, list, 0, &exception);
        if (exception || !result) {
            logtext("SKIP: get_Count managed exception or NULL result.\r\n");
        } else {
            void *cls = p_object_get_class(result);
            unsigned align = 0;
            if (!cls || !p_class_is_valuetype(cls) || p_class_value_size(cls, &align) != 4 ||
                !exact_type(p_class_get_type(cls), "System.Int32")) {
                logtext("SKIP: get_Count result is not boxed Int32.\r\n");
            } else {
                void *value = p_object_unbox(result);
                if (!value) {
                    logtext("SKIP: Int32 unbox failed.\r\n");
                } else {
                    int count = 0;
                    for (unsigned i = 0; i < 4; ++i)
                        ((unsigned char *)&count)[i] = ((const unsigned char *)value)[i];
                    if (count < 0) {
                        logtext("SKIP: negative list count.\r\n");
                    } else {
                        logtext(fieldName);
                        logtext(".Count = ");
                        lognum((UINT_PTR)count);
                        nl();
                        ok = 1;
                    }
                }
            }
        }
    }
    release_reference(root);
    return ok;
}
static int g_support_world_verified;
static int blueprint_save_export_request;
static void blueprint_save_exported(void);
#include "coaster_storage.inc"
#include "blueprint_export.inc"

static void read_track_counts(void *obj, void *station) {
    GCHandle snapshotRoot = pin_reference(obj);
    if (!snapshotRoot) {
        logtext("SKIP: cannot root snapshot.\r\n");
        return;
    }
    void *f = p_class_get_field_from_name(g_snapshot_class, "SnapshotData");
    if (!instance_field(f, g_snapshot_class) ||
        !exact_type(p_field_get_type(f), "TPS.Game.TrackRideSnapshotData")) {
        release_reference(snapshotRoot);
        return;
    }
    void *cls = p_class_from_type(p_field_get_type(f));
    unsigned align = 0;
    if (!cls || !p_class_is_valuetype(cls) || p_class_value_size(cls, &align) != 16) {
        logtext("SKIP: TrackRideSnapshotData must be a 16-byte value type.\r\n");
        release_reference(snapshotRoot);
        return;
    }
    /* il2cpp_field_get_value_object boxes the field through the runtime and
   * preserves GC-visible references. field_get_value then reads its fields
   * using their boxed representation; no guessed offsets or header size. */
    logtext("About to box SnapshotData using il2cpp_field_get_value_object.\r\n");
    void *data = p_field_get_value_object(f, obj);
    if (!data || p_object_get_class(data) != cls) {
        logtext("SKIP: SnapshotData boxing returned NULL or unexpected class.\r\n");
        release_reference(snapshotRoot);
        return;
    }
    GCHandle dataRoot = pin_reference(data);
    if (!dataRoot) {
        logtext("SKIP: cannot root boxed SnapshotData.\r\n");
        release_reference(snapshotRoot);
        return;
    }
    int supports =
        read_list_count(data, cls, "TrackSupportData",
                        "System.Collections.Generic.List<TPS.Game.TrackSupportSnapshotData>");
    int pieces =
        read_list_count(data, cls, "TrackPieceData",
                        "System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>");
    logtext(
        supports && pieces
            ? "Read-only track-count probe completed.\r\n"
            : "Read-only track-count probe completed with SKIP(s); both counts not confirmed.\r\n");
    if (supports && pieces)
        export_blueprint(data, cls, station);
    release_reference(dataRoot);
    release_reference(snapshotRoot);
}

static void read_snapshot_flag(const unsigned char *entity, void *station) {
    if (!g_snapshot_reader_verified || !g_snapshot_reader || !g_snapshot_flag_verified) {
        logtext("SNAPSHOT READ SKIP: closed getter or Boolean layout not verified.\r\n");
        return;
    }
    void *args[1] = {(void *)entity};
    void *exception = 0;
    logtext("About to invoke read-only GetComponentClass<ECTrackRideSnapshot>(Entity).\r\n");
    void *obj = p_runtime_invoke(g_snapshot_reader, 0, args, &exception);
    if (exception) {
        logtext("SNAPSHOT READ SKIP: getter raised a managed exception.\r\n");
        return;
    }
    if (!obj) {
        logtext("SNAPSHOT READ SKIP: getter returned NULL reference.\r\n");
        return;
    }
    void *actual = p_object_get_class(obj);
    if (!actual || !p_class_is_assignable_from(g_snapshot_class, actual)) {
        logtext("SNAPSHOT READ SKIP: returned reference is not ECTrackRideSnapshot.\r\n");
        return;
    }
    unsigned char flag = 0;
    p_field_get_value(obj, g_snapshot_flag_field, &flag);
    logtext("ECTrackRideSnapshot.HasSnapshotData = ");
    logtext(flag ? "TRUE" : "FALSE");
    nl();
    if (flag == 1)
        read_track_counts(obj, station);
    else
        logtext("SKIP: HasSnapshotData is not TRUE; no list reads.\r\n");
}

static int probe_component_presence(const unsigned char *entity) {
    if (!g_hascomponent_verified || !g_hascomponent_method || !g_component_factory_verified ||
        !g_component_type_factory) {
        logtext("SKIP: ComponentType factory or HasComponent signature not verified.\r\n");
        return 0;
    }
    void *components[2] = {g_ride_class, g_snapshot_class};
    const char *names[2] = {"ECTrackRide", "ECTrackRideSnapshot"};
    unsigned matched = 0;
    for (unsigned i = 0; i < 2; ++i) {
        void *t = p_class_get_type(components[i]);
        void *managedType = t ? p_type_get_object(t) : 0;
        if (!managedType) {
            logtext("SKIP: component System.Type unavailable.\r\n");
            return 0;
        }
        void *typeClass = p_object_get_class(managedType);
        void *systemType = findclass(p_domain_get(), "System", "Type");
        if (!typeClass || !systemType || !p_class_is_assignable_from(systemType, typeClass)) {
            logtext("SKIP: component runtime System.Type verification failed.\r\n");
            return 0;
        }
        /* il2cpp_runtime_invoke takes reference args directly, value-type args
     * by address. This is deliberate and prevents v0.3's bad marshalling. */
        void *factoryArgs[1] = {managedType};
        void *exception = 0;
        logtext("About to construct ComponentType.ReadOnly for ");
        logtext(names[i]);
        nl();
        void *boxedType = p_runtime_invoke(g_component_type_factory, 0, factoryArgs, &exception);
        if (exception || !boxedType) {
            logtext("SKIP: ComponentType factory exception/null.\r\n");
            return 0;
        }
        void *typeValue = p_object_unbox(boxedType);
        if (!typeValue) {
            logtext("SKIP: failed to unbox ComponentType.\r\n");
            return 0;
        }
        unsigned char componentTypeBytes[64] = {0};
        for (int k = 0; k < g_component_type_size; ++k)
            componentTypeBytes[k] = ((unsigned char *)typeValue)[k];
        void *args[2] = {(void *)entity, componentTypeBytes};
        exception = 0;
        logtext("About to invoke HasComponent(Entity,ComponentType) for ");
        logtext(names[i]);
        nl();
        void *boxedResult = p_runtime_invoke(g_hascomponent_method, 0, args, &exception);
        if (exception || !boxedResult) {
            logtext("SKIP: HasComponent exception/null.\r\n");
            return 0;
        }
        void *unboxed = p_object_unbox(boxedResult);
        if (!unboxed) {
            logtext("SKIP: HasComponent result unbox failed.\r\n");
            return 0;
        }
        logtext("HasComponent ");
        logtext(names[i]);
        logtext(" = ");
        int present = (*(const unsigned char *)unboxed) != 0;
        logtext(present ? "TRUE" : "FALSE");
        nl();
        if (present)
            ++matched;
    }
    return matched == 2;
}

/* ManagedEntityID and Unity.Entities.Entity are value types. Confirm their
 * runtime size before copying. NEVER assume that object references and ECS
 * component structs have interchangeable layouts. */
static int inspect_value_field(void *owner, const char *label, void *field, int expectedSize) {
    if (!field) {
        logtext("SKIP: missing field: ");
        logtext(label);
        nl();
        return 0;
    }
    void *type = p_field_get_type(field);
    void *cls = type ? p_class_from_type(type) : 0;
    if (!cls || !p_class_is_valuetype(cls)) {
        logtext("SKIP: field is not a value type: ");
        logtext(label);
        nl();
        return 0;
    }
    unsigned align = 0;
    int bytes = p_class_value_size(cls, &align);
    char *name = p_type_get_name(type);
    logtext("Verified inherited field ");
    logtext(label);
    logtext(" type=");
    logtext(name ? name : "?");
    logtext(" bytes=");
    lognum((UINT_PTR)(bytes > 0 ? bytes : 0));
    nl();
    if (name)
        p_free(name);
    (void)owner;
    return bytes == expectedSize;
}
static void log_bytes(const unsigned char *buffer, unsigned count) {
    static const char digits[] = "0123456789ABCDEF";
    if (count > 32)
        count = 32;
    for (unsigned i = 0; i < count; ++i) {
        char c[3];
        c[0] = digits[buffer[i] >> 4];
        c[1] = digits[buffer[i] & 15];
        c[2] = 0;
        logtext(c);
    }
}
static void run_station_preflight(void *station, const unsigned char *entityBytes);
static void inspect_station_entity(void *station) {
    if (!g_managed_fields_verified || !g_gameitem_class) {
        logtext("SKIP: ManagedEntity fields did not pass runtime verification.\r\n");
        return;
    }
    if (!station) {
        logtext("SKIP: no selected station reference.\r\n");
        return;
    }
    void *actual = p_object_get_class(station);
    if (!actual || !p_class_is_assignable_from(g_gameitem_class, actual)) {
        logtext("SKIP: _station is not a GameItemInstance subclass.\r\n");
        return;
    }
    unsigned char id[32] = {0}, entity[32] = {0};
    p_field_get_value(station, g_id_field, id);
    p_field_get_value(station, g_entity_field, entity);
    logtext("ManagedEntity.ID raw bytes: ");
    log_bytes(id, (unsigned)g_id_size);
    nl();
    if (g_id_size == 8) {
        UINT_PTR managedId = 0;
        for (unsigned k = 0; k < 8; ++k)
            managedId |= ((UINT_PTR)id[k]) << (k * 8);
        logtext("ManagedEntity.ID uint64: ");
        lognum(managedId);
        nl();
    }
    logtext("Unity.Entities.Entity raw bytes: ");
    log_bytes(entity, (unsigned)g_entity_size);
    nl();
    if (g_entity_size == 8) {
        unsigned idx = *(const unsigned *)(entity);
        unsigned ver = *(const unsigned *)(entity + 4);
        logtext("Unity ECS Entity.Index: ");
        lognum(idx);
        nl();
        logtext("Unity ECS Entity.Version: ");
        lognum(ver);
        nl();
        if (!idx || !ver)
            logtext("NOTE: zero index/version; check validity before any future ECS query.\r\n");
    }
    if (!g_exists_verified || !g_exists_method) {
        logtext("SKIP: ECS EntityManager.Exists runtime signature not verified.\r\n");
    } else if (g_entity_size == 8) {
        unsigned idx = *(const unsigned *)(entity);
        unsigned ver = *(const unsigned *)(entity + 4);
        if (!idx || !ver) {
            logtext("SKIP: invalid zero ECS entity index/version.\r\n");
        } else {
            void *args[1] = {entity};
            void *exception = 0;
            logtext("About to invoke read-only EntityManager.Exists(Entity).\r\n");
            void *boxed = p_runtime_invoke(g_exists_method, 0, args, &exception);
            if (exception) {
                logtext("ECS Exists: managed exception; no further reads.\r\n");
            } else if (!boxed) {
                logtext("ECS Exists: null return value; no further reads.\r\n");
            } else {
                void *unboxed = p_object_unbox(boxed);
                if (!unboxed) {
                    logtext("ECS Exists: unbox failed.\r\n");
                } else {
                    unsigned char exists = *(const unsigned char *)unboxed;
                    logtext("TPS.Core.Entities.EntityManager.Exists(Entity): ");
                    logtext(exists ? "TRUE" : "FALSE");
                    nl();
                    if (exists) {
                        run_station_preflight(station, entity);
                        int bothPresent = probe_component_presence(entity);
                        if (bothPresent)
                            read_snapshot_flag(entity, station);
                        else
                            logtext(
                                "SNAPSHOT READ SKIP: both coaster components not confirmed.\r\n");
                    }
                }
            }
        }
    }
    logtext("v1.1.0 snapshot/count path finished; consult counts or SKIP diagnostics.\r\n");
}

/* Runs ONLY if Unity itself invokes the intercepted managed method.
 * Always execute the original method. Do not call UnityEngine API here.
 */
#include "orientation_verification.inc"
#include "station_preflight.inc"
static void placement_import_finished(int verified, void *context);
#include "first_write.inc"
static void blueprint_browser_service(void);
static void blueprint_save_tick(void);
static void management_tick(void);
static void coaster_ui_service(void);
/* Do not call App.Instance's diagnostic getter during native teardown. */
static volatile long g_app_stopping;
static void *g_app_instance_field, *g_app_quitting_field;
static int app_services_available(void) {
    if (__atomic_load_n(&g_app_stopping, __ATOMIC_ACQUIRE))
        return 0;
    if (!g_app_instance_field || !g_app_quitting_field)
        return 0;
    void *app = 0;
    p_field_static_get_value(g_app_instance_field, &app);
    if (!app)
        return 0;
    unsigned char quitting = 0;
    p_field_get_value(app, g_app_quitting_field, &quitting);
    if (quitting) {
        if (!__atomic_exchange_n(&g_app_stopping, 1, __ATOMIC_ACQ_REL))
            logtext("SHUTDOWN: native app is quitting; mod services stopped.\r\n");
        return 0;
    }
    return 1;
}
static int app_services_prepare(void) {
    void *c = findclass(p_domain_get(), "TPS.Core", "App");
    g_app_instance_field = c ? p_class_get_field_from_name(c, "<Instance>k__BackingField") : 0;
    g_app_quitting_field = c ? pf_field(c, "<IsQuitting>k__BackingField", "System.Boolean") : 0;
    return g_app_instance_field && (p_field_get_flags(g_app_instance_field) & 0x10) &&
           pf_type(p_field_get_type(g_app_instance_field), "TPS.Core.App") && g_app_quitting_field;
}
#include "placement_entry.inc"
#include "blueprint_browser.inc"
#include "blueprint_save.inc"
#include "blueprint_management.inc"
static void coaster_ui_service(void) {
    long long begin = perf_now();
    blueprint_browser_service();
    UINT_PTR browser_us = perf_us(begin);
    long long save_begin = perf_now();
    blueprint_save_tick();
    UINT_PTR save_us = perf_us(save_begin);
    long long management_begin = perf_now();
    management_tick();
    UINT_PTR management_us = perf_us(management_begin);
    static UINT_PTR perf_browser_total, perf_save_total, perf_management_total, perf_samples;
    perf_browser_total += browser_us;
    perf_save_total += save_us;
    perf_management_total += management_us;
    ++perf_samples;
    /* Report idle UI discovery cost too: v1.1.0 only profiled when a dialog
     * was open, missing the normal-gameplay frame-time regression. One
     * aggregate log every 15 seconds; no flush and no extra Unity calls. */
    {
        static UINT_PTR next_report;
        UINT_PTR now = GetTickCount64();
        if (!next_report)
            next_report = now + 15000;
        if (now >= next_report) {
            log_stamp();
            logtext("PERF IDLE save_scan_calls=");
            lognum(save_perf_calls);
            logtext(" mean_us=");
            lognum(save_perf_calls ? save_perf_total_us / save_perf_calls : 0);
            logtext(" peak_us=");
            lognum(save_perf_peak_us);
            logtext(" FindObjectsOfType_calls=");
            lognum(save_perf_global_scans);
            logtext(" browser_mean_us=");
            lognum(perf_samples ? perf_browser_total / perf_samples : 0);
            logtext(" save_mean_us=");
            lognum(perf_samples ? perf_save_total / perf_samples : 0);
            logtext(" management_mean_us=");
            lognum(perf_samples ? perf_management_total / perf_samples : 0);
            nl();
            save_perf_calls = save_perf_total_us = save_perf_peak_us = save_perf_global_scans = 0;
            perf_browser_total = perf_save_total = perf_management_total = perf_samples = 0;
            next_report = now + 15000;
        }
    }
    static UINT_PTR due, peak, total, samples;
    if (browser_menu || save_owner || management_busy()) {
        UINT_PTR elapsed = perf_us(begin);
        total += elapsed;
        ++samples;
        if (elapsed > peak)
            peak = elapsed;
        if (GetTickCount64() >= due) {
            log_stamp();
            logtext("PERF UI samples=");
            lognum(samples);
            logtext(" average_us=");
            lognum(total / samples);
            logtext(" peak_us=");
            lognum(peak);
            perf_memory();
            nl();
            due = GetTickCount64() + 5000;
            peak = total = samples = 0;
        }
    }
}

static void observed_lateupdate(void *self, void *methodInfo) {
    if (!app_services_available()) {
        if (g_original)
            g_original(self, methodInfo);
        return;
    }
    if (__atomic_add_fetch(&g_hook_count, 1, __ATOMIC_RELAXED) == 1)
        g_hook_first_thread = GetCurrentThreadId();
    if (p_thread_current())
        service_pending_write();
    placement_service_ui(self);
    long request = self ? __atomic_exchange_n(&g_pending, 0, __ATOMIC_ACQ_REL) : 0;
    if (self && request) {
        logtext(request == 2
                    ? "\r\n---- F9 v1.1.0 EXPERIMENTAL WRITE ----\r\n"
                    : "\r\n---- F8 v1.1.0 STATION PREFLIGHT + TEMPLATE EXPORT ----\r\n");
        logtext("LateUpdate callback thread: ");
        lognum(GetCurrentThreadId());
        nl();
        logtext("UI object pointer: ");
        loghex((UINT_PTR)self);
        nl();
        if (!p_thread_current()) {
            logtext("SKIP: no IL2CPP thread context. Did not read any fields.\r\n");
        } else {
            void *station = 0;
            void *support = 0;
            /* These are managed OBJECT references, NOT ECS structs. Never dereference
       * their internal data from the probe. */
            if (g_station_field)
                p_field_get_value(self, g_station_field, &station);
            if (g_support_field)
                p_field_get_value(self, g_support_field, &support);
            logtext("CoasterControlsUI._station reference: ");
            loghex((UINT_PTR)station);
            nl();
            logtext("CoasterControlsUI._support reference: ");
            loghex((UINT_PTR)support);
            nl();
            verify_support_orientation();
            if (request == 2) {
                if (placement_wait_item)
                    placement_log("F9 deferred: finish or cancel the armed F10 placement first");
                else
                    run_first_write(station);
            } else
                inspect_station_entity(station);
            logtext("F8 is read-only; F9 experimental write is reported separately.\r\n");
            __atomic_add_fetch(&g_export_count, 1, __ATOMIC_RELAXED);
        }
        FlushFileBuffers(g_log);
    }
    if (g_original)
        g_original(self, methodInfo);
}

static int install_method_slot_hook(HMODULE game) {
    void *cls = findclass(p_domain_get(), "TPS.Game", "CoasterControlsUI");
    if (!cls) {
        logtext("SKIP: CoasterControlsUI class unavailable.\r\n");
        return 0;
    }
    void *m = p_class_get_method_from_name(cls, "LateUpdate", 0);
    if (!m) {
        logtext("SKIP: CoasterControlsUI.LateUpdate method unavailable.\r\n");
        return 0;
    }
    if (!p_method_is_instance(m) || p_method_get_param_count(m) != 0) {
        logtext("SKIP: LateUpdate ABI mismatch (must be instance, no arguments).\r\n");
        return 0;
    }
    char *returnName = p_type_get_name(p_method_get_return_type(m));
    int voidReturn = equals(returnName, "System.Void");
    if (returnName)
        p_free(returnName);
    if (!voidReturn) {
        logtext("SKIP: LateUpdate return type not System.Void.\r\n");
        return 0;
    }
    g_station_field = p_class_get_field_from_name(cls, "_station");
    g_support_field = p_class_get_field_from_name(cls, "_support");
    if (!g_station_field || !g_support_field) {
        logtext("SKIP: station/support fields missing.\r\n");
        return 0;
    }
    void *stationType = p_field_get_type(g_station_field);
    void *supportType = p_field_get_type(g_support_field);
    void *stationClass = stationType ? p_class_from_type(stationType) : 0;
    void *supportClass = supportType ? p_class_from_type(supportType) : 0;
    if (!stationClass || !supportClass || p_class_is_valuetype(stationClass) ||
        p_class_is_valuetype(supportClass)) {
        logtext("SKIP: station/support fields not confirmed managed-reference types.\r\n");
        return 0;
    }
    char *st = p_type_get_name(stationType);
    char *su = p_type_get_name(supportType);
    logtext("_station managed field type: ");
    logtext(st ? st : "?");
    nl();
    logtext("_support managed field type: ");
    logtext(su ? su : "?");
    nl();
    if (st)
        p_free(st);
    if (su)
        p_free(su);
    /* Verify the ManagedEntity base and both inherited value-type fields
   * before intercepting any game callback. Do not invoke ECS methods. */
    g_gameitem_class = findclass(p_domain_get(), "TPS.Game", "GameItemInstance");
    g_managed_class = findclass(p_domain_get(), "TPS.Core.Entities", "ManagedEntity");
    if (!g_gameitem_class || !g_managed_class ||
        !p_class_is_assignable_from(g_managed_class, g_gameitem_class)) {
        logtext("SKIP: GameItemInstance/ManagedEntity inheritance mismatch.\r\n");
        return 0;
    }
    g_id_field = p_class_get_field_from_name(g_managed_class, "<ID>k__BackingField");
    g_entity_field = p_class_get_field_from_name(g_managed_class, "<Entity>k__BackingField");
    if (!inspect_value_field(0, "ManagedEntity.ID", g_id_field, 8) ||
        !inspect_value_field(0, "ManagedEntity.Entity", g_entity_field, 8)) {
        logtext("SKIP: expected 8-byte ID and Entity value fields.\r\n");
        return 0;
    }
    g_id_size = 8;
    g_entity_size = 8;
    g_managed_fields_verified = 1;
    setup_entity_exists();
    setup_hascomponent();
    setup_snapshot_reader();
    void *snapshot = findclass(p_domain_get(), "TPS.Game", "ECTrackRideSnapshot");
    void *snapshotData = findclass(p_domain_get(), "TPS.Game", "TrackRideSnapshotData");
    if (snapshot && snapshotData && p_class_get_field_from_name(snapshot, "SnapshotData") &&
        p_class_get_field_from_name(snapshotData, "TrackSupportData") &&
        p_class_get_field_from_name(snapshotData, "TrackPieceData")) {
        logtext("Snapshot schema found (no ECS components or snapshots read).\r\n");
    } else {
        logtext("WARNING: snapshot schema differs; identity-only mode.\r\n");
    }
    void *slot = m; /* Unity IL2CPP v31 MethodInfo starts with methodPointer. */
    void *orig = *(void **)slot;
    logtext("LateUpdate MethodInfo: ");
    loghex((UINT_PTR)m);
    nl();
    logtext("Original methodPointer: ");
    loghex((UINT_PTR)orig);
    nl();
    /* Check that the method pointer lies in GameAssembly's mapped image. This
   * rejects mismatched MethodInfo layouts and already-hooked entries. */
    unsigned char *base = (unsigned char *)game;
    if (base[0] != 'M' || base[1] != 'Z') {
        logtext("SKIP: GameAssembly missing MZ header.\r\n");
        return 0;
    }
    unsigned peoff = *(unsigned *)(base + 0x3c);
    if (peoff < 64 || peoff > 4096 || base[peoff] != 'P' || base[peoff + 1] != 'E') {
        logtext("SKIP: invalid PE header.\r\n");
        return 0;
    }
    unsigned imageSize = *(unsigned *)(base + peoff + 24 + 0x38);
    if (imageSize < 65536 || imageSize > 1000000000U || (UINT_PTR)orig < (UINT_PTR)base ||
        (UINT_PTR)orig >= (UINT_PTR)(base + imageSize)) {
        logtext("SKIP: native method pointer is not inside GameAssembly image.\r\n");
        return 0;
    }
    unsigned nSections = *(unsigned short *)(base + peoff + 6);
    unsigned optSize = *(unsigned short *)(base + peoff + 20);
    if (nSections == 0 || nSections > 96 || optSize < 0x60 || optSize > 512) {
        logtext("SKIP: unsupported PE section layout.\r\n");
        return 0;
    }
    unsigned char *sections = base + peoff + 24 + optSize;
    unsigned rel = (unsigned)((UINT_PTR)orig - (UINT_PTR)base);
    int executable = 0;
    for (unsigned i = 0; i < nSections; ++i) {
        unsigned char *s = sections + i * 40;
        unsigned virtualSize = *(unsigned *)(s + 8);
        unsigned virtualAddress = *(unsigned *)(s + 12);
        unsigned flags = *(unsigned *)(s + 36);
        if (rel >= virtualAddress && rel - virtualAddress < virtualSize && (flags & 0x20000000U))
            executable = 1;
    }
    if (!executable) {
        logtext("SKIP: methodPointer is not inside an executable PE section.\r\n");
        return 0;
    }
    /* Assign original BEFORE installing hook, so a concurrently entering
   * game callback always has its original implementation available. */
    g_original = (native_ui_lateupdate)orig;
    g_ui_method = m;
    /* A writable MethodInfo slot permits a single aligned atomic pointer swap;
   * NEVER modify or detour native game executable code. */
    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &oldProtect)) {
        logtext("SKIP: cannot make MethodInfo pointer slot writable.\r\n");
        return 0;
    }
    void *previous =
        __atomic_exchange_n((void **)slot, (void *)observed_lateupdate, __ATOMIC_SEQ_CST);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void *), oldProtect, &ignored);
    if (previous != orig) {
        logtext(
            "ERROR: concurrent methodPointer change detected; reverting only if still owned.\r\n");
        if (VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &oldProtect)) {
            void *expected = (void *)observed_lateupdate;
            __atomic_compare_exchange_n((void **)slot, &expected, previous, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_SEQ_CST);
            VirtualProtect(slot, sizeof(void *), oldProtect, &ignored);
        }
        return 0;
    }
    logtext("MethodInfo pointer swap installed (no machine-code patch).\r\n");
    logtext(
        "Awaiting game-owned LateUpdate; method may be cached by Unity and never reach this observer.\r\n");
    return 1;
}

static DWORD WINAPI worker(void *context) {
    (void)context;
    if (!initlog())
        return 1;
    logtext("\r\nCoaster Manager v1.1.5 - SHARED UI ASSET CACHE\r\n");
    logtext(
        "Release: use the Save Template and coaster library buttons. Developer hotkeys are disabled by default.\r\n");
    HMODULE game = 0;
    for (unsigned i = 0; i < 1200 && !game; ++i) {
        game = GetModuleHandleW(L"GameAssembly.dll");
        if (!game)
            Sleep(100);
    }
    if (!game) {
        logtext("ERROR: GameAssembly.dll not loaded.\r\n");
        return 2;
    }
    if (!resolve(game))
        return 3;
    void *domain = 0;
    for (unsigned i = 0; i < 1200 && !domain; ++i) {
        domain = p_domain_get();
        if (!domain)
            Sleep(100);
    }
    void *workerThread = domain ? p_thread_attach(domain) : 0;
    if (!workerThread) {
        logtext("ERROR: IL2CPP domain unavailable.\r\n");
        return 4;
    }
    if (!app_services_prepare()) {
        logtext("DISABLED: app lifecycle fields unavailable.\r\n");
        p_thread_detach(workerThread);
        return 6;
    }
    if (!install_method_slot_hook(game)) {
        logtext("HOOK NOT INSTALLED. v1.1.0 will not attempt ECS reads.\r\n");
        p_thread_detach(workerThread);
        return 5;
    }
    int placementEnabled = install_placement_entry();
    /* All subsequent worker operations are Win32/atomic polling. Managed work
   * runs only in game-owned callbacks, so do not retain a runtime attachment. */
    p_thread_detach(workerThread);
    logtext("WORKER: runtime thread detached after setup; native hotkey polling only.\r\n");
    int prevEntry = 0, prevF8 = 0, prevF9 = 0;
    unsigned ticks = 0;
    unsigned placementQueued = 0;
    for (;;) {
        if (__atomic_load_n(&g_app_stopping, __ATOMIC_ACQUIRE))
            break;
        int down = TPM_ENABLE_DIAGNOSTICS && (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        int entryDown = TPM_ENABLE_DIAGNOSTICS && (GetAsyncKeyState(0x79) & 0x8000) != 0;
        if (entryDown && !prevEntry) {
            if (placementEnabled) {
                long empty = 0;
                if (__atomic_compare_exchange_n(&placement_request, &empty, 1, 0, __ATOMIC_ACQ_REL,
                                                __ATOMIC_ACQUIRE)) {
                    placementQueued = ticks;
                    logtext(
                        "F10 Funfair purchase queued; matching row must be visible in Funfair Templates.\r\n");
                }
            } else
                logtext("F10 unavailable: GameApp observer was not installed.\r\n");
        }
        prevEntry = entryDown;
        if (__atomic_load_n(&placement_request, __ATOMIC_ACQUIRE) &&
            ticks - placementQueued >= 100) {
            if (__atomic_exchange_n(&placement_request, 0, __ATOMIC_ACQ_REL))
                logtext(
                    "F10 TIMEOUT: GameApp did not invoke the observer within 10 seconds. No placement attempted.\r\n");
        }
        if (down && !prevF8) {
            long empty = 0;
            if (__atomic_compare_exchange_n(&g_pending, &empty, 1, 0, __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE)) {
                g_pending_tick = ticks;
                logtext("F8 queued; waiting for game-owned CoasterControlsUI.LateUpdate.\r\n");
            }
        }
        prevF8 = down;
        int writeDown = TPM_ENABLE_DIAGNOSTICS && (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (writeDown && !prevF9) {
            long empty = 0;
            if (__atomic_compare_exchange_n(&g_pending, &empty, 2, 0, __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE)) {
                g_pending_tick = ticks;
                logtext("F9 EXPERIMENTAL WRITE queued; selected station must be empty.\r\n");
            }
        }
        prevF9 = writeDown;

        if (__atomic_load_n(&g_pending, __ATOMIC_ACQUIRE) && ticks - g_pending_tick >= 100) {
            if (__atomic_exchange_n(&g_pending, 0, __ATOMIC_ACQ_REL)) {
                logtext(
                    "F8/F9 TIMEOUT (10s): Unity did not invoke hooked LateUpdate while request pending.\r\n");
                logtext(
                    "Try selecting a coaster and opening its edit controls before F8; if repeated, methodPointer may be cached elsewhere.\r\n");
            }
        }
        Sleep(100);
        ++ticks;
    }
    return 0;
}
__declspec(dllexport) const char *CoasterProbeVersion(void) {
    return "1.1.5-asset-cache";
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, void *reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = h;
        HANDLE th = CreateThread(0, 0, worker, 0, 0, 0);
        if (th)
            CloseHandle(th);
    }
    return 1;
}
