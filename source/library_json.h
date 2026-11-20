/* Bounded, allocation-free JSON reader shared by library validation and host tests.
 * Decodes escaped keys before duplicate checking; no executable or managed JSON input. */
#define LJ_MAX 65536
#define LJ_TEXT (4U * 1024U * 1024U)
typedef struct {
    unsigned text, child, next, count, rawStart, rawEnd;
    int kind;
    double number;
} LJToken;
static LJToken lj_tokens[LJ_MAX];
static char lj_text[LJ_TEXT];
static const unsigned char *lj_input;
static unsigned lj_size, lj_at, lj_used, lj_chars;
static int lj_ok;
static int lj_hex(unsigned c) {
    return c >= '0' && c <= '9'   ? (int)(c - '0')
           : c >= 'a' && c <= 'f' ? (int)(c - 'a' + 10)
           : c >= 'A' && c <= 'F' ? (int)(c - 'A' + 10)
                                  : -1;
}
static void lj_space(void) {
    while (lj_at < lj_size && (lj_input[lj_at] == ' ' || lj_input[lj_at] == '\r' ||
                               lj_input[lj_at] == '\n' || lj_input[lj_at] == '\t'))
        ++lj_at;
}
static void lj_emit(unsigned c) {
    if (lj_chars >= LJ_TEXT - 1) {
        lj_ok = 0;
        return;
    }
    lj_text[lj_chars++] = (char)c;
}
static unsigned lj_u16(void) {
    unsigned v = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (lj_at >= lj_size) {
            lj_ok = 0;
            return 0;
        }
        int h = lj_hex(lj_input[lj_at++]);
        if (h < 0) {
            lj_ok = 0;
            return 0;
        }
        v = (v << 4) | (unsigned)h;
    }
    return v;
}
static unsigned lj_string(void) {
    unsigned start = lj_chars;
    if (lj_at >= lj_size || lj_input[lj_at++] != '"') {
        lj_ok = 0;
        return 0;
    }
    int closed = 0;
    while (lj_ok && lj_at < lj_size) {
        unsigned c = lj_input[lj_at++];
        if (c == '"') {
            closed = 1;
            break;
        }
        if (c < 32) {
            lj_ok = 0;
            break;
        }
        if (c == '\\') {
            if (lj_at >= lj_size) {
                lj_ok = 0;
                break;
            }
            c = lj_input[lj_at++];
            if (c == '"' || c == '\\' || c == '/')
                lj_emit(c);
            else if (c == 'b')
                lj_emit(8);
            else if (c == 'f')
                lj_emit(12);
            else if (c == 'n')
                lj_emit(10);
            else if (c == 'r')
                lj_emit(13);
            else if (c == 't')
                lj_emit(9);
            else if (c == 'u') {
                c = lj_u16();
                if (c >= 0xd800 && c <= 0xdbff) {
                    if (lj_at + 2 > lj_size || lj_input[lj_at++] != '\\' ||
                        lj_input[lj_at++] != 'u') {
                        lj_ok = 0;
                        break;
                    }
                    unsigned low = lj_u16();
                    if (low < 0xdc00 || low > 0xdfff) {
                        lj_ok = 0;
                        break;
                    }
                    c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                } else if (c >= 0xdc00 && c <= 0xdfff) {
                    lj_ok = 0;
                    break;
                }
                if (!c) {
                    lj_ok = 0;
                    break;
                } /* Embedded NUL cannot be a library key/name. */
                if (c < 128)
                    lj_emit(c);
                else if (c < 2048) {
                    lj_emit(0xc0 | (c >> 6));
                    lj_emit(0x80 | (c & 63));
                } else if (c < 65536) {
                    lj_emit(0xe0 | (c >> 12));
                    lj_emit(0x80 | ((c >> 6) & 63));
                    lj_emit(0x80 | (c & 63));
                } else {
                    lj_emit(0xf0 | (c >> 18));
                    lj_emit(0x80 | ((c >> 12) & 63));
                    lj_emit(0x80 | ((c >> 6) & 63));
                    lj_emit(0x80 | (c & 63));
                }
            } else
                lj_ok = 0;
        } else if (c < 128)
            lj_emit(c);
        else { /* Strict UTF-8: reject overlong forms, surrogate scalars and > U+10FFFF. */
            unsigned need = c >= 0xc2 && c <= 0xdf   ? 1
                            : c >= 0xe0 && c <= 0xef ? 2
                            : c >= 0xf0 && c <= 0xf4 ? 3
                                                     : 0;
            unsigned scalar = c & (need == 1 ? 31 : need == 2 ? 15 : 7), first = c;
            if (!need || lj_at + need > lj_size) {
                lj_ok = 0;
                break;
            }
            unsigned at = lj_at;
            for (unsigned k = 0; k < need; ++k) {
                unsigned d = lj_input[lj_at++];
                if ((d & 0xc0) != 0x80)
                    lj_ok = 0;
                scalar = (scalar << 6) | (d & 63);
            }
            if ((need == 2 && scalar < 2048) || (need == 3 && scalar < 65536) ||
                scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
                lj_ok = 0;
                break;
            }
            lj_emit(first);
            for (unsigned k = 0; k < need; ++k)
                lj_emit(lj_input[at + k]);
        }
    }
    if (!closed)
        lj_ok = 0;
    lj_emit(0);
    return start;
}
static unsigned lj_value(unsigned depth);
static unsigned lj_value_inner(unsigned depth) {
    lj_space();
    if (!lj_ok || depth > 32 || lj_used >= LJ_MAX || lj_at >= lj_size) {
        lj_ok = 0;
        return 0;
    }
    unsigned id = lj_used++;
    LJToken *t = &lj_tokens[id];
    memset(t, 0, sizeof(*t));
    t->rawStart = lj_at;
    unsigned c = lj_input[lj_at];
    if (c == '{' || c == '[') {
        t->kind = c == '{' ? 1 : 2;
        ++lj_at;
        lj_space();
        unsigned last = 0;
        if (lj_at < lj_size && lj_input[lj_at] == (c == '{' ? '}' : ']')) {
            ++lj_at;
            return id;
        }
        while (lj_ok) {
            unsigned key = 0;
            if (c == '{') {
                key = lj_string();
                lj_space();
                if (lj_at >= lj_size || lj_input[lj_at++] != ':') {
                    lj_ok = 0;
                    break;
                }
                for (unsigned v = t->child; v; v = lj_tokens[v].next)
                    if (equals(lj_text + key, lj_text + lj_tokens[v].text)) {
                        lj_ok = 0;
                        break;
                    }
            }
            unsigned child = lj_value(depth + 1);
            if (!lj_ok)
                break;
            if (c == '{')
                lj_tokens[child].text = key;
            if (last)
                lj_tokens[last].next = child;
            else
                t->child = child;
            last = child;
            if (++t->count > (c == '{' ? 256U : 4096U)) {
                lj_ok = 0;
                break;
            }
            lj_space();
            if (lj_at >= lj_size) {
                lj_ok = 0;
                break;
            }
            unsigned end = lj_input[lj_at++];
            if (end == (c == '{' ? '}' : ']'))
                break;
            if (end != ',') {
                lj_ok = 0;
                break;
            }
            lj_space();
        }
        return id;
    }
    if (c == '"') {
        t->kind = 3;
        t->number = (double)lj_string();
        t->rawEnd = lj_at;
        return id;
    } /* String offset separate from object member name. */
    if (c == 't' || c == 'f' || c == 'n') {
        const char *s = c == 't' ? "true" : c == 'f' ? "false" : "null";
        t->kind = c == 'n' ? 6 : 4;
        t->number = c == 't' ? 1 : 0;
        while (*s)
            if (lj_at >= lj_size || lj_input[lj_at++] != (unsigned char)*s++) {
                lj_ok = 0;
                break;
            }
        return id;
    }
    t->kind = 5;
    t->text = lj_chars;
    unsigned start = lj_at;
    int negative = 0, exp = 0, fraction = 0;
    if (c == '-') {
        negative = 1;
        ++lj_at;
    }
    if (lj_at >= lj_size) {
        lj_ok = 0;
        return id;
    }
    c = lj_input[lj_at++];
    double value = 0;
    if (c == '0') {
        if (lj_at < lj_size && lj_input[lj_at] >= '0' && lj_input[lj_at] <= '9')
            lj_ok = 0;
    } else if (c >= '1' && c <= '9') {
        value = c - '0';
        while (lj_at < lj_size && (c = lj_input[lj_at]) >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            ++lj_at;
        }
    } else
        lj_ok = 0;
    if (lj_at < lj_size && lj_input[lj_at] == '.') {
        ++lj_at;
        unsigned before = lj_at;
        while (lj_at < lj_size && (c = lj_input[lj_at]) >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            ++fraction;
            ++lj_at;
        }
        if (lj_at == before)
            lj_ok = 0;
    }
    if (lj_at < lj_size && (lj_input[lj_at] == 'e' || lj_input[lj_at] == 'E')) {
        ++lj_at;
        int sign = 1;
        if (lj_at < lj_size && (lj_input[lj_at] == '+' || lj_input[lj_at] == '-'))
            sign = lj_input[lj_at++] == '-' ? -1 : 1;
        unsigned before = lj_at;
        while (lj_at < lj_size && (c = lj_input[lj_at]) >= '0' && c <= '9') {
            if (exp > 400)
                lj_ok = 0;
            else
                exp = exp * 10 + (int)(c - '0');
            ++lj_at;
        }
        if (lj_at == before)
            lj_ok = 0;
        exp *= sign;
    }
    if (lj_at - start > 128) {
        lj_ok = 0;
        return id;
    }
    exp -= fraction;
    if (exp > 400 || exp < -400)
        lj_ok = 0;
    while (lj_ok && exp > 0) {
        value *= 10;
        --exp;
    }
    while (lj_ok && exp < 0) {
        value /= 10;
        ++exp;
    }
    t->number = negative ? -value : value;
    /* Preserve exact integer spelling for 64-bit identifiers. */
    unsigned raw = lj_chars;
    for (unsigned i = start; i < lj_at; ++i)
        lj_emit(lj_input[i]);
    lj_emit(0);
    t->child = raw;
    return id;
}
static unsigned lj_value(unsigned depth) {
    unsigned id = lj_value_inner(depth);
    if (id)
        lj_tokens[id].rawEnd = lj_at;
    return id;
}
static unsigned lj_parse(const unsigned char *bytes, unsigned n) {
    lj_input = bytes;
    lj_size = n;
    lj_at = n >= 3 && bytes[0] == 239 && bytes[1] == 187 && bytes[2] == 191 ? 3 : 0;
    lj_used = 1;
    lj_chars = 0;
    lj_ok = 1;
    unsigned root = lj_value(0);
    lj_space();
    return lj_ok && lj_at == n ? root : 0;
}
static unsigned lj_member(unsigned object, const char *key) {
    if (!object || lj_tokens[object].kind != 1)
        return 0;
    for (unsigned i = lj_tokens[object].child; i; i = lj_tokens[i].next)
        if (equals(lj_text + lj_tokens[i].text, key))
            return i;
    return 0;
}
static const char *lj_string_value(unsigned id) {
    return id && lj_tokens[id].kind == 3 ? lj_text + (unsigned)lj_tokens[id].number : 0;
}
static int lj_is(unsigned id, const char *s) {
    return equals(lj_string_value(id), s);
}
static int lj_int(unsigned id, long long *out) {
    if (!id)
        return 0;
    LJToken *t = &lj_tokens[id];
    if (t->kind == 1) {
        if (t->count != 1)
            return 0;
        id = lj_member(id, "_id");
        if (!id)
            return 0;
        t = &lj_tokens[id];
    }
    const char *s = t->kind == 3 ? lj_string_value(id) : t->kind == 5 ? lj_text + t->child : 0;
    if (!s)
        return 0;
    int neg = *s == '-';
    if (neg)
        ++s;
    if (!*s)
        return 0;
    unsigned long long v = 0, limit = neg ? 0x8000000000000000ULL : 0x7fffffffffffffffULL;
    while (*s) {
        if (*s < '0' || *s > '9')
            return 0;
        unsigned d = (unsigned)(*s++ - '0');
        if (v > (limit - d) / 10)
            return 0;
        v = v * 10 + d;
    }
    *out = neg ? (long long)(0ULL - v) : (long long)v;
    return 1;
}
static int lj_integer(unsigned o, const char *k, long long *out) {
    unsigned v = lj_member(o, k);
    return v && lj_tokens[v].kind == 5 && lj_int(v, out);
}
static int lj_true(unsigned o, const char *k) {
    unsigned v = lj_member(o, k);
    return v && lj_tokens[v].kind == 4 && lj_tokens[v].number == 1;
}
static int lj_floats(unsigned id, float *out, unsigned n) {
    if (!id || lj_tokens[id].kind != 2 || lj_tokens[id].count != n)
        return 0;
    unsigned t = lj_tokens[id].child;
    for (unsigned i = 0; i < n; ++i, t = lj_tokens[t].next) {
        if (lj_tokens[t].kind != 5)
            return 0;
        out[i] = (float)lj_tokens[t].number;
    }
    return finite_vector(out, n);
}
static int lj_native(unsigned object, const char *key, unsigned *out, unsigned n) {
    char bitskey[64];
    unsigned k = 0;
    while (key[k] && k < 40) {
        bitskey[k] = key[k];
        ++k;
    }
    const char *s = "Float32Bits";
    while (*s)
        bitskey[k++] = *s++;
    bitskey[k] = 0;
    unsigned v = lj_member(object, key), b = lj_member(object, bitskey);
    if (!v || !b)
        return 0;
    if (n > 1) {
        if (lj_tokens[v].kind != 2 || lj_tokens[b].kind != 2 || lj_tokens[v].count != n ||
            lj_tokens[b].count != n)
            return 0;
        v = lj_tokens[v].child;
        b = lj_tokens[b].child;
    }
    for (unsigned i = 0; i < n; ++i) {
        const char *word = lj_string_value(b);
        if (!word || length(word) != 10 || word[0] != '0' || word[1] != 'x' ||
            lj_tokens[v].kind != 5)
            return 0;
        unsigned u = 0;
        for (unsigned j = 2; j < 10; ++j) {
            int h = lj_hex((unsigned char)word[j]);
            if (h < 0)
                return 0;
            u = (u << 4) | (unsigned)h;
        }
        float actual = 0, expected = (float)lj_tokens[v].number;
        memcpy(&actual, &u, 4);
        if (!finite_vector(&actual, 1) || !finite_vector(&expected, 1) || actual != expected)
            return 0;
        out[i] = u;
        v = lj_tokens[v].next;
        b = lj_tokens[b].next;
    }
    return 1;
}
static int library_metadata_readonly(unsigned root) {
    /* A local published blueprint remains editable; downloads have explicit source guards.
   * Keep legacy Workshop-only records protected when no publishing owner is stored. */
    return lj_true(root, "readOnly") ||
           (lj_member(root, "workshopItemId") && !lj_member(root, "workshopOwnerId")) ||
           lj_member(root, "workshopPublishedFileId") ||
           lj_is(lj_member(root, "origin"), "workshop") ||
           lj_is(lj_member(root, "origin"), "steam-workshop") ||
           lj_is(lj_member(root, "source"), "workshop") ||
           lj_is(lj_member(root, "source"), "steam-workshop");
}
static __declspec(noinline) unsigned library_prepare(unsigned root, unsigned char *out) {
    if (!lj_is(lj_member(root, "schema"), "tpm-coaster-snapshot"))
        return 0;
    long long version = 0;
    if (!lj_integer(root, "schemaVersion", &version) || version != 2)
        return 0;
    unsigned orient = lj_member(root, "supportOrientation"),
             frame = lj_member(root, "stationFrameProbe"),
             floor = lj_member(root, "floorFrameProbe");
    if (!lj_is(lj_member(orient, "sourceSpace"), "world") ||
        !lj_true(orient, "sourceSpaceConfirmed") ||
        !lj_is(lj_member(frame, "status"), "position-and-quaternion") ||
        !lj_is(lj_member(floor, "status"), "world-frame-read"))
        return 0;
    float p[3], q[4], f[4];
    if (!lj_floats(lj_member(frame, "transformWorldPosition"), p, 3) ||
        !lj_floats(lj_member(frame, "transformWorldRotation"), q, 4) ||
        !lj_floats(lj_member(floor, "worldRotation"), f, 4) || f[0] != 0 || f[1] != 0 ||
        f[2] != 0 || (f[3] != 1 && f[3] != -1))
        return 0;
    unsigned floorToken = lj_tokens[lj_member(floor, "worldRotation")].child;
    for (unsigned i = 0; i < 4; ++i, floorToken = lj_tokens[floorToken].next) {
        double v = lj_tokens[floorToken].number;
        if (i < 3 ? v != 0 : (v != 1 && v != -1))
            return 0;
    }
    long long source = 0, definition = 0;
    if (!lj_int(lj_member(root, "sourceStationId"), &source) || source <= 0 ||
        !lj_int(lj_member(root, "sourceStationDefinitionId"), &definition) || !definition)
        return 0;
    unsigned supports = lj_member(root, "TrackSupportData"),
             pieces = lj_member(root, "TrackPieceData");
    if (!supports || !pieces || lj_tokens[supports].kind != 2 || lj_tokens[pieces].kind != 2)
        return 0;
    unsigned count = lj_tokens[supports].count;
    if (!count || count > WRITE_MAX_SUPPORTS || lj_tokens[pieces].count != count)
        return 0;
    memset(out, 0, 64 + count * 72);
    memcpy(out, "TPMPLAN3", 8);
    memcpy(out + 8, &count, 4);
    memcpy(out + 16, &source, 8);
    memcpy(out + 24, &definition, 8);
    memcpy(out + 32, p, 12);
    memcpy(out + 44, q, 16);
    static unsigned ordered[2][WRITE_MAX_SUPPORTS];
    memset(ordered, 0, sizeof(ordered));
    for (unsigned list = 0; list < 2; ++list) {
        unsigned t = lj_tokens[list ? pieces : supports].child;
        for (unsigned i = 0; i < count; ++i, t = lj_tokens[t].next) {
            long long index = 0, section = 0;
            if (!lj_integer(t, "listIndex", &index) || index != (long long)i ||
                !lj_integer(t, "SectionIndex", &section) || section < 0 ||
                section >= (long long)count || ordered[list][section])
                return 0;
            ordered[list][section] = t;
        }
    }
    long long sourcefloor = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned s0 = ordered[0][i], piece = ordered[1][i], words[12];
        unsigned char *r = out + 60 + i * 72;
        if (!lj_native(s0, "Position", words, 3) || !lj_native(s0, "WorldPosition", words + 3, 3))
            return 0;
        float pos[3], world[3];
        memcpy(pos, words, 12);
        memcpy(world, words + 3, 12);
        for (unsigned j = 0; j < 3; ++j)
            if (pos[j] != world[j])
                return 0;
        memcpy(r, words, 12);
        if (!lj_native(s0, "Rotation", words, 4))
            return 0;
        memcpy(r + 12, words, 16);
        if (!lj_native(s0, "LocalRotation", words, 1))
            return 0;
        memcpy(r + 28, words, 4);
        long long height = 0, floorid = 0;
        if (!lj_true(s0, "Valid") || !lj_integer(s0, "Height", &height) || height < 1 ||
            height > 1024 || !lj_int(lj_member(s0, "FloorAreaInstance"), &floorid) ||
            floorid <= 0 || (i && floorid != sourcefloor))
            return 0;
        sourcefloor = floorid;
        int h = (int)height;
        memcpy(r + 32, &h, 4);
        if (!lj_native(s0, "TangentInModifier", words, 1) ||
            !lj_native(s0, "TangentOutModifier", words + 1, 1) ||
            !lj_native(s0, "BankingModifier", words + 2, 1))
            return 0;
        memcpy(r + 36, words, 12);
        long long pieceid = 0, deco = 0;
        if (!lj_int(lj_member(piece, "TrackPieceDefinitionID"), &pieceid) || !pieceid ||
            !lj_int(lj_member(piece, "TrackPieceDecorationDefinition"), &deco))
            return 0;
        memcpy(r + 48, &pieceid, 8);
        memcpy(r + 56, &deco, 8);
        if (!lj_native(piece, "DecorationPosition", words, 1))
            return 0;
        memcpy(r + 64, words, 4);
    }
    unsigned n = 64 + count * 72, hash = write_hash(out, n - 4);
    memcpy(out + n - 4, &hash, 4);
    return n;
}
