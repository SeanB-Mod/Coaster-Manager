#include "preview_resample.h"
static unsigned long long preview_array_count(void *a) {
    unsigned long long n = 0;
    SIZE_T got = 0;
    if (!a || !ReadProcessMemory(GetCurrentProcess(), (unsigned char *)a + 24, &n, 8, &got) ||
        got != 8)
        return 0;
    return n;
}
static void preview_destroy(void *o) {
    if (!o)
        return;
    void *c = findclass(p_domain_get(), "UnityEngine", "Object");
    const char *p[] = {"UnityEngine.Object"};
    void *a[] = {o};
    write_void(browser_method(c, "Destroy", "System.Void", 1, p, 0), 0, a);
}
static void *preview_small_texture(void *source, unsigned w, unsigned h, PFRoots *owner) {
    if (!source || !w || !h || w > 4096 || h > 4096)
        return 0;
    PFRoots roots = {0};
    void *out = 0, *created = 0;
    void *c = findclass(p_domain_get(), "UnityEngine", "Texture2D"),
         *color = findclass(p_domain_get(), "UnityEngine", "Color32");
    if (!color || !write_value_layout(color, 4))
        return 0;
    void *pixels =
        pf_invoke(browser_method(c, "GetPixels32", "UnityEngine.Color32[]", 0, 0, 1), source, 0);
    if (!pixels || !pf_root(&roots, pixels) ||
        preview_array_count(pixels) != (unsigned long long)w * h)
        goto done;
    void *small = p_array_new(color, 960 * 540);
    if (!small || !pf_root(&roots, small) || preview_array_count(small) != 960 * 540)
        goto done;
    if (!preview_resample((unsigned char *)pixels + 32, w, h, (unsigned char *)small + 32))
        goto done;
    out = p_object_new(c);
    created = out;
    if (!out || !pf_root(owner, out)) {
        out = 0;
        goto done;
    }
    const char *cp[] = {"System.Int32", "System.Int32"};
    int width = 960, height = 540;
    void *ca[] = {&width, &height};
    if (!write_void(browser_method(c, ".ctor", "System.Void", 2, cp, 1), out, ca)) {
        out = 0;
        goto done;
    }
    const char *sp[] = {"UnityEngine.Color32[]"};
    void *sa[] = {small};
    if (!write_void(browser_method(c, "SetPixels32", "System.Void", 1, sp, 1), out, sa)) {
        out = 0;
        goto done;
    }
    const char *bp[] = {"System.Boolean", "System.Boolean"};
    unsigned char no = 0;
    void *ba[] = {&no, &no};
    if (!write_void(browser_method(c, "Apply", "System.Void", 2, bp, 1), out, ba))
        out = 0;
done:
    pf_release(&roots);
    if (out)
        preview_destroy(source);
    else
        preview_destroy(created);
    return out;
}
/* Modify only the owned upload texture. The saved PNG and UI backdrop are
 * deliberately independent of the JPEG that Steam receives. */
static int preview_texture_black(void *texture) {
    PFRoots roots = {0};
    int ok = 0;
    void *pixels = browser_optional(texture, "GetPixels32", "UnityEngine.Color32[]");
    if (!pixels || !pf_root(&roots, pixels) || preview_array_count(pixels) != 960ULL * 540)
        goto done;
    if (preview_flatten_black((unsigned char *)pixels + 32, 960 * 540)) {
        const char *p[] = {"UnityEngine.Color32[]"};
        void *a[] = {pixels};
        void *c = p_object_get_class(texture);
        if (!write_void(browser_method(c, "SetPixels32", "System.Void", 1, p, 1), texture, a) ||
            !write_void(browser_method(c, "Apply", "System.Void", 0, 0, 1), texture, 0))
            goto done;
    }
    ok = 1;
done:
    pf_release(&roots);
    return ok;
}
/* Only resize owned staging captures. Existing library originals are not rewritten. */
static int preview_resize_capture(const WCHAR *path) {
    unsigned n = library_read(path, library_image_bytes, 20U * 1024U * 1024U), w = 0, h = 0;
    if (!n || !save_complete_png(library_image_bytes, n) ||
        !library_image_size(library_image_bytes, n, &w, &h))
        return 0;
    if (w <= 960 && h <= 540)
        return 1;
    PFRoots roots = {0};
    int ok = 0;
    void *source = 0, *small = 0;
    static WCHAR temp[1200];
    unsigned pos = 0;
    if (!append(temp, 1200, &pos, path) || !append(temp, 1200, &pos, L".small.tmp"))
        return 0;
    void *c = findclass(p_domain_get(), "UnityEngine", "Texture2D"),
         *bytes = findclass(p_domain_get(), "System", "Byte");
    source = p_object_new(c);
    if (!source || !pf_root(&roots, source))
        goto done;
    const char *cp[] = {"System.Int32", "System.Int32"};
    int two = 2;
    void *ca[] = {&two, &two};
    if (!write_void(browser_method(c, ".ctor", "System.Void", 2, cp, 1), source, ca))
        goto done;
    void *data = p_array_new(bytes, n);
    if (!data || !pf_root(&roots, data) || preview_array_count(data) != n)
        goto done;
    memcpy((unsigned char *)data + 32, library_image_bytes, n);
    void *convert = findclass(p_domain_get(), "UnityEngine", "ImageConversion");
    const char *lp[] = {"UnityEngine.Texture2D", "System.Byte[]", "System.Boolean"};
    unsigned char no = 0;
    void *la[] = {source, data, &no};
    void *b = pf_invoke(browser_method(convert, "LoadImage", "System.Boolean", 3, lp, 0), 0, la);
    if (!b || !*(unsigned char *)p_object_unbox(b))
        goto done;
    small = preview_small_texture(source, w, h, &roots);
    if (!small)
        goto done;
    const char *ep[] = {"UnityEngine.Texture2D"};
    void *ea[] = {small};
    void *png = pf_invoke(browser_method(convert, "EncodeToPNG", "System.Byte[]", 1, ep, 0), 0, ea);
    if (!png || !pf_root(&roots, png))
        goto done;
    unsigned long long size = preview_array_count(png);
    if (size < 57 || size > 20U * 1024U * 1024U ||
        !save_complete_png((unsigned char *)png + 32, (unsigned)size))
        goto done;
    if (!save_write_new(temp, (unsigned char *)png + 32, (unsigned)size))
        goto done;
    ok = MoveFileExW(temp, path, 1 | 8) != 0;
    if (!ok)
        DeleteFileW(temp);
done:
    preview_destroy(small);
    if (!small)
        preview_destroy(source);
    pf_release(&roots);
    return ok;
}
