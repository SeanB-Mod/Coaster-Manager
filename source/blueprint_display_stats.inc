/* Optional display metadata, independent of the placement plan and its identity. */
typedef struct {
    char value[5][48];
} BlueprintDisplayStats;
static const char *blueprint_stat_keys[5] = {"cost", "buzz", "thrill", "ferocity", "nausea"};
static int blueprint_stat_text(const char *input, char *out) {
    unsigned n = 0;
    int tag = 0, digits = 0;
    out[0] = 0;
    if (!input)
        return 0;
    for (unsigned i = 0; input[i]; ++i) {
        unsigned char ch = (unsigned char)input[i];
        if (ch == '<') {
            if (tag) {
                out[0] = 0;
                return 0;
            }
            tag = 1;
            continue;
        }
        if (ch == '>') {
            if (!tag) {
                out[0] = 0;
                return 0;
            }
            tag = 0;
            continue;
        }
        if (tag)
            continue;
        if (ch < 32 || ch == 127 || ch == '?' || n >= 47) {
            out[0] = 0;
            return 0;
        }
        if (ch >= '0' && ch <= '9')
            digits = 1;
        out[n++] = (char)ch;
    }
    if (tag || !digits) {
        out[0] = 0;
        return 0;
    }
    while (n && out[n - 1] == ' ')
        --n;
    unsigned start = 0;
    while (start < n && out[start] == ' ')
        ++start;
    if (start) {
        for (unsigned i = start; i < n; ++i)
            out[i - start] = out[i];
        n -= start;
    }
    out[n] = 0;
    return n != 0;
}
static void blueprint_stats_read(unsigned metadata, BlueprintDisplayStats *out) {
    memset(out, 0, sizeof(*out));
    unsigned stats = lj_member(metadata, "displayStats");
    for (unsigned i = 0; i < 5; ++i) {
        char value[48] = {0};
        const char *s = lj_string_value(lj_member(stats, blueprint_stat_keys[i]));
        if (blueprint_stat_text(s, value))
            memcpy(out->value[i], value, sizeof(value));
    }
}
static inline void blueprint_stats_label(const BlueprintDisplayStats *stats, char *out,
                                         unsigned capacity) {
    const char *labels[] = {"Cost: ", "\nBuzz: ", "    Thrill: ", "\nFerocity: ", "    Nausea: "};
    unsigned n = 0;
    for (unsigned i = 0; i < 5; ++i) {
        const char *parts[] = {labels[i], stats->value[i][0] ? stats->value[i] : "?"};
        for (unsigned j = 0; j < 2; ++j) {
            const char *s = parts[j];
            while (*s && n + 1 < capacity)
                out[n++] = *s++;
        }
    }
    if (capacity)
        out[n] = 0;
}

static void blueprint_cost_text(int cost, char *out) {
    out[0] = 0;
    if (cost < 0)
        return;
    char reverse[16];
    unsigned n = 0, value = (unsigned)cost;
    do {
        if (n && n % 4 == 3)
            reverse[n++] = ',';
        reverse[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    unsigned at = 0;
    out[at++] = '$';
    while (n)
        out[at++] = reverse[--n];
    out[at] = 0;
}

/* The stored displayStats.cost omits the station and support/decoration
 * construction transactions. Read-only template list prices add both.
 * Accept strictly formatted numeric money values, never label text. */
static int blueprint_track_cost_number(const char *text, int *out) {
    if (!text || !out)
        return 0;
    unsigned i = 0, groupDigits = 0, separators = 0, digits = 0;
    unsigned long long value = 0;
    while (text[i] == ' ')
        ++i;
    if (text[i] == '$')
        ++i;
    else if ((unsigned char)text[i] == 0xc2 && (unsigned char)text[i + 1] == 0xa3)
        i += 2; /* Accept a localized pound sign without changing its display. */
    for (; text[i]; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            if (value > 2147483647ULL)
                return 0;
            ++digits;
            ++groupDigits;
            if (separators && groupDigits > 3)
                return 0;
        } else if (c == ',') {
            if (!groupDigits || groupDigits > 3 || (separators && groupDigits != 3))
                return 0;
            ++separators;
            groupDigits = 0;
        } else
            break;
    }
    if (!digits || !groupDigits || (separators && groupDigits != 3))
        return 0;
    while (text[i] == ' ')
        ++i;
    if (text[i])
        return 0;
    *out = (int)value;
    return 1;
}
/* Current game prices, confirmed from real construction transactions:
 * one support and each decorated track section cost 500. Display only;
 * these purchases are independently charged during native construction. */
static int blueprint_component_display_cost(unsigned supports, unsigned decorations,
                                            int *out) {
    if (!out || supports > WRITE_MAX_SUPPORTS || decorations > supports ||
        supports > (2147483647U / 500U) - decorations)
        return 0;
    *out = (int)((supports + decorations) * 500U);
    return 1;
}
static int blueprint_template_total_cost(const char *trackPrice, int stationCost,
                                         int componentsCost, char *out, unsigned capacity) {
    int trackCost = 0;
    if (!out || capacity < 48 || stationCost < 0 || componentsCost < 0 ||
        !blueprint_track_cost_number(trackPrice, &trackCost) ||
        stationCost > 2147483647 - trackCost ||
        componentsCost > 2147483647 - (stationCost + trackCost))
        return 0;
    blueprint_cost_text(stationCost + trackCost + componentsCost, out);
    /* Preserve the currency symbol already used by this saved template. */
    if ((unsigned char)trackPrice[0] == 0xc2 && (unsigned char)trackPrice[1] == 0xa3) {
        unsigned n = length(out);
        if (n + 1 >= capacity)
            return 0;
        volatile char *shifted = (volatile char *)out;
        for (unsigned k = n + 1; k > 1; --k)
            shifted[k] = shifted[k - 1];
        out[0] = (char)0xc2;
        out[1] = (char)0xa3;
    }
    return 1;
}

/* Only raw non-negative numbers, optionally current / maximum, are eligible.
 * Labelled strings remain unchanged. Zero is a real rating. */
static int blueprint_rating_number(const char *s, float *out, char *number) {
    unsigned i = 0, n = 0;
    float value = 0, place = .1f;
    int digits = 0, decimal = 0;
    while (s[i] == ' ')
        ++i;
    for (; s[i]; ++i) {
        char ch = s[i];
        if (ch >= '0' && ch <= '9') {
            if (n >= 30)
                return 0;
            number[n++] = ch;
            digits = 1;
            if (decimal) {
                value += (ch - '0') * place;
                place *= .1f;
            } else
                value = value * 10 + (ch - '0');
            if (value > 1000000)
                return 0;
        } else if (ch == '.' && !decimal) {
            if (!digits)
                return 0;
            decimal = 1;
            number[n++] = ch;
        } else
            break;
    }
    if (!digits || !n || number[n - 1] == '.')
        return 0;
    number[n] = 0;
    while (s[i] == ' ')
        ++i;
    if (s[i] == '/') {
        ++i;
        while (s[i] == ' ')
            ++i;
        int maximumDigits = 0, dot = 0;
        for (; s[i]; ++i) {
            if (s[i] >= '0' && s[i] <= '9')
                maximumDigits = 1;
            else if (s[i] == '.' && !dot && maximumDigits) {
                dot = 1;
                maximumDigits = 0;
            } else
                break;
        }
        if (!maximumDigits)
            return 0;
        while (s[i] == ' ')
            ++i;
    }
    if (s[i])
        return 0;
    *out = value;
    return 1;
}
