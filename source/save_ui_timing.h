/* The UI scan may update seen after the caller captured now. A newer seen
 * timestamp is fresh, not an unsigned elapsed-time wraparound. */
static int save_ui_owner_expired(unsigned long long now, unsigned long long seen, int alive) {
    return !alive || (now >= seen && now - seen > 1000);
}
