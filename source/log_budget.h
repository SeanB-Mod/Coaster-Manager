/* Atomic byte budget shared by worker/main-thread diagnostic fragments. */
#define LOG_BYTE_LIMIT (8L * 1024L * 1024L)
#define LOG_DATA_LIMIT (LOG_BYTE_LIMIT - 128L)
static volatile long log_budget_bytes;
long _InterlockedCompareExchange(long volatile *, long, long);
#pragma intrinsic(_InterlockedCompareExchange)
/* 1 = reserved, -1 = sole truncation marker owner, 0 = drop. */
static int log_budget_reserve(unsigned n) {
    if (!n)
        return 0;
    for (;;) {
        long before = _InterlockedCompareExchange(&log_budget_bytes, 0, 0);
        if (before >= LOG_DATA_LIMIT)
            return 0;
        int fits = n <= (unsigned)(LOG_DATA_LIMIT - before);
        long after = fits ? before + (long)n : LOG_BYTE_LIMIT;
        if (_InterlockedCompareExchange(&log_budget_bytes, after, before) == before)
            return fits ? 1 : -1;
    }
}
