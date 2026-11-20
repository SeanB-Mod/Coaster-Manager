/* Save and side-button metadata caches. IL2CPP class/method pointers are process-lifetime.
 * No managed object references are stored here; those retain the existing GC roots. */
typedef struct {
    const char *ns, *name;
    void *value;
} SaveClassCache;
static SaveClassCache save_class_cache[48];
static unsigned save_class_count;
static void *save_cached_class(void *domain, const char *ns, const char *name) {
    for (unsigned i = 0; i < save_class_count; ++i)
        if (equals(ns, save_class_cache[i].ns) && equals(name, save_class_cache[i].name))
            return save_class_cache[i].value;
    void *v = findclass(domain, ns, name);
    if (save_class_count < 48)
        save_class_cache[save_class_count++] = (SaveClassCache){ns, name, v};
    return v;
}
typedef struct {
    void *c, *value;
    const char *n, *r, *p[4];
    unsigned count;
    int instance;
} SaveMethodCache;
static SaveMethodCache save_method_cache[192];
static unsigned save_method_count;
static void *save_cached_method(void *c, const char *n, const char *r, unsigned count,
                                const char **p, int instance) {
    if (!c || count > 4)
        return browser_method(c, n, r, count, p, instance);
    for (unsigned i = 0; i < save_method_count; ++i) {
        SaveMethodCache *e = &save_method_cache[i];
        if (e->c != c || e->count != count || e->instance != instance || !equals(e->n, n) ||
            !equals(e->r, r))
            continue;
        unsigned j = 0;
        for (; j < count; ++j)
            if (!equals(e->p[j], p[j]))
                break;
        if (j == count)
            return e->value;
    }
    void *v = browser_method(c, n, r, count, p, instance);
    if (save_method_count < 192) {
        SaveMethodCache *e = &save_method_cache[save_method_count++];
        e->c = c;
        e->n = n;
        e->r = r;
        e->count = count;
        e->instance = instance;
        e->value = v;
        for (unsigned j = 0; j < count; ++j)
            e->p[j] = p[j];
    }
    return v;
}
static void *save_cached_get(void *o, const char *n, const char *r) {
    void *m = o ? save_cached_method(p_object_get_class(o), n, r, 0, 0, 1) : 0;
    void *v = pf_invoke(m, o, 0);
    if (!v)
        browser_failed = 1;
    return v;
}
static void *save_cached_optional(void *o, const char *n, const char *r) {
    int old = browser_failed;
    void *v = save_cached_get(o, n, r);
    browser_failed = old;
    return v;
}
static int save_cached_set(void *o, const char *n, const char *t, void *v) {
    const char *p[] = {t};
    void *m = o ? save_cached_method(p_object_get_class(o), n, "System.Void", 1, p, 1) : 0;
    void *a[] = {v};
    if (!write_void(m, o, a)) {
        browser_failed = 1;
        return 0;
    }
    return 1;
}
static int save_cached_alive(void *o) {
    if (!o)
        return 0;
    void *c = save_cached_class(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *a[] = {o};
    void *b = pf_invoke(save_cached_method(c, "op_Implicit", "System.Boolean", 1, p, 0), 0, a);
    return b && *(unsigned char *)p_object_unbox(b) == 1;
}
static int save_cached_true(void *o, const char *n) {
    void *b = o ? save_cached_optional(o, n, "System.Boolean") : 0;
    return b && *(unsigned char *)p_object_unbox(b) == 1;
}
static int save_cached_pressed(void *t) {
    if (!save_cached_true(t, "get_isOn"))
        return 0;
    unsigned char off = 0;
    save_cached_set(t, "SetIsOnWithoutNotify", "System.Boolean", &off);
    return 1;
}
static void *save_cached_type(const char *ns, const char *n) {
    void *c = save_cached_class(p_domain_get(), ns, n);
    return c ? p_type_get_object(p_class_get_type(c)) : 0;
}
static int save_cached_array_count(void *array, int *out) {
    void *c = save_cached_class(p_domain_get(), "System", "Array");
    if (!c || !array || !p_class_is_assignable_from(c, p_object_get_class(array)))
        return 0;
    void *b = pf_invoke(save_cached_method(c, "get_Length", "System.Int32", 0, 0, 1), array, 0);
    if (!b || !pf_class(p_object_get_class(b), "System.Int32") ||
        !write_value_layout(p_object_get_class(b), 4))
        return 0;
    void *v = p_object_unbox(b);
    if (!v)
        return 0;
    memcpy(out, v, 4);
    return *out >= 0 && *out <= 256;
}
static void *save_cached_array_item(void *array, int index) {
    void *c = save_cached_class(p_domain_get(), "System", "Array");
    const char *p[] = {"System.Int32"};
    void *a[] = {&index};
    return pf_invoke(save_cached_method(c, "GetValue", "System.Object", 1, p, 1), array, a);
}
#define browser_type save_cached_type
#define write_array_count save_cached_array_count
#define write_array_item save_cached_array_item
#define findclass save_cached_class
#define browser_method save_cached_method
#define browser_get save_cached_get
#define browser_optional save_cached_optional
#define browser_set save_cached_set
#define browser_alive save_cached_alive
#define funfair_property_true save_cached_true
#define browser_pressed save_cached_pressed
