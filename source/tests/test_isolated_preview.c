#define PREFLIGHT_TEST_ENTRY OriginalPreflightMain
#include "test_preflight.c"
static unsigned sequence, restores, releases, destroys;
static int fail_restore, fail_release;
static int fake_restore(void *previous) {
    if (sequence++ != 0 || previous != (void *)10)
        ExitProcess(30);
    ++restores;
    return !fail_restore;
}
static int fake_release(void *resource) {
    if (resource != (void *)20 && resource != (void *)30)
        ExitProcess(31);
    if (restores && sequence == 0)
        ExitProcess(32);
    ++sequence;
    ++releases;
    return !fail_release;
}
static void fake_destroy(void *resource) {
    if (resource != (void *)30 && resource != (void *)40)
        ExitProcess(33);
    ++sequence;
    ++destroys;
}
static int journal_bytes_differ(const void *a, const void *b, unsigned n) {
    const unsigned char *aa = a, *bb = b;
    for (unsigned i = 0; i < n; ++i)
        if (aa[i] != bb[i])
            return 1;
    return 0;
}
static unsigned journal_calls, journal_restores, journal_fail_at, journal_restore_fail;
static unsigned char journal_live[7][16], journal_original[7][16];
static int journal_restoring;
static int fake_journal_apply(const IsolatedChange *change, const void *value) {
    unsigned i = (unsigned)(UINT_PTR)change->owner - 1;
    unsigned bytes = isolated_change_size(change->kind);
    if (i >= 7 || !bytes)
        ExitProcess(60);
    if (!journal_restoring) {
        if (isolated_change_count != journal_calls + 1)
            ExitProcess(61); /* Snapshot must precede a throwing setter. */
        ++journal_calls;
        memcpy(journal_live[i], value, bytes);
        return journal_calls != journal_fail_at;
    }
    if (i != journal_calls - journal_restores - 1 ||
        journal_bytes_differ(value, journal_original[i], bytes))
        ExitProcess(62);
    ++journal_restores;
    memcpy(journal_live[i], value, bytes);
    return journal_restores != journal_restore_fail;
}
static void check_render_journal(void) {
    for (unsigned fail = 0; fail <= 7; ++fail) {
        for (unsigned restoreFailure = 0; restoreFailure < 2; ++restoreFailure) {
            journal_calls = journal_restores = 0;
            journal_fail_at = fail;
            journal_restore_fail = restoreFailure ? 1 : 0;
            journal_restoring = 0;
            /* Non-default originals catch accidental restore-to-default bugs,
             * including enabled=true -> false -> true scene renderer flags. */
            for (unsigned i = 0; i < 7; ++i)
                memset(journal_original[i], (int)i + 1, sizeof(journal_original[i]));
            memset(journal_original[5], 0, sizeof(journal_original[5]));
            memcpy(journal_live, journal_original, sizeof(journal_live));
            for (unsigned i = 0; i < 7; ++i) {
                unsigned char after[16];
                memset(after, i == 5 ? 1 : i ? (int)i + 9 : 0, sizeof(after));
                if (!isolated_change_using((void *)(UINT_PTR)(i + 1), (void *)1, i + 1, 100 + i,
                                           journal_original[i], after, fake_journal_apply))
                    break;
            }
            if (journal_calls != (fail ? fail : 7))
                ExitProcess(63);
            journal_restoring = 1;
            if (isolated_restore_using(fake_journal_apply) != !restoreFailure ||
                journal_restores != journal_calls || isolated_change_count ||
                journal_bytes_differ(journal_live, journal_original, sizeof(journal_live)))
                ExitProcess(64);
            if (!isolated_restore_using(fake_journal_apply) || journal_restores != journal_calls)
                ExitProcess(65);
        }
    }
    unsigned char zero[16] = {0};
    isolated_change_count = ISOLATED_CHANGES;
    if (isolated_change_using((void *)1, (void *)1, 1, 0, zero, zero, fake_journal_apply))
        ExitProcess(66);
    isolated_change_count = 0;
    if (isolated_change_using((void *)1, (void *)1, 0, 0, zero, zero, fake_journal_apply))
        ExitProcess(67);
    isolated_track_id_count = 0;
    for (unsigned i = 1; i <= 1024; ++i)
        if (!isolated_track_id(i) || !isolated_track_id(i))
            ExitProcess(68);
    if (isolated_track_id_count != 1024 || !isolated_track_id(0) || isolated_track_id(1025))
        ExitProcess(69);
    isolated_track_id_count = 0;
}
void TestMain(void) {
    pf_setup();
    check_render_journal();
    void *candidate = 0;
    if (!isolated_pick_visual(&candidate, (void *)1, 0) || candidate ||
        !isolated_pick_visual(&candidate, (void *)1, 1) || candidate != (void *)1 ||
        !isolated_pick_visual(&candidate, (void *)1, 1) ||
        isolated_pick_visual(&candidate, (void *)2, 1) || candidate != (void *)1 ||
        !isolated_pick_visual(&candidate, 0, 1))
        ExitProcess(72);
    if (isolated_track_ready(0, 0) || !isolated_track_ready(1, 0) || !isolated_track_ready(0, 1) ||
        !isolated_track_ready(1, 1))
        ExitProcess(70);
    isolated_spline_instances = 7;
    isolated_request_release();
    if (isolated_spline_instances || isolated_track_id_count || isolated_request_root_count)
        ExitProcess(71);
    isolated_clear();
    for (unsigned i = 0; i < ISOLATED_RENDERERS; ++i)
        if (!isolated_hold_renderer((void *)(UINT_PTR)(i + 1)))
            ExitProcess(10);
    if (isolated_count != ISOLATED_RENDERERS || liveHandles != ISOLATED_RENDERERS)
        ExitProcess(11);
    if (!isolated_hold_renderer((void *)1) ||
        isolated_hold_renderer((void *)(UINT_PTR)(ISOLATED_RENDERERS + 1)) ||
        liveHandles != ISOLATED_RENDERERS)
        ExitProcess(12);
    isolated_decoration_count = 3;
    isolated_clear();
    if (liveHandles || isolated_count || isolated_decoration_count)
        ExitProcess(13);
    isolated_clear();
    if (liveHandles)
        ExitProcess(14);
    IsolatedCleanupOps ops = {fake_restore, fake_release, fake_destroy};
    for (unsigned failure = 0; failure < 4; ++failure) {
        sequence = restores = releases = destroys = 0;
        fail_restore = failure & 1;
        fail_release = failure & 2;
        int ok =
            isolated_cleanup_resources((void *)10, (void *)20, (void *)30, (void *)40, 1, 1, &ops);
        if (ok != (failure == 0) || restores != 1 || releases != 2 || destroys != 2)
            ExitProcess(40 + failure);
    }
    sequence = restores = releases = destroys = 0;
    fail_restore = fail_release = 0;
    if (!isolated_cleanup_resources(0, 0, 0, 0, 0, 0, &ops) || restores || releases || destroys)
        ExitProcess(50);
    sequence = restores = releases = destroys = 0;
    if (!isolated_cleanup_resources(0, (void *)20, (void *)30, 0, 0, 0, &ops) || restores ||
        releases != 1 || destroys != 1)
        ExitProcess(51);
    ExitProcess(0);
}
