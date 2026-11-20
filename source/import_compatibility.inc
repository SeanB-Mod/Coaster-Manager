/* Query the destination configuration through exact managed signatures.
 * An exported definition ID is never accepted just because it resolves. */
static int write_array_count_limit(void *array, int *out, int limit) {
    void *c = findclass(p_domain_get(), "System", "Array");
    if (!c || !array || !p_class_is_assignable_from(c, p_object_get_class(array)))
        return 0;
    void *m = pf_method(c, "get_Length", "System.Int32", 0, 0, 1);
    void *b = pf_invoke(m, array, 0);
    if (!b || !pf_class(p_object_get_class(b), "System.Int32") ||
        !write_value_layout(p_object_get_class(b), 4))
        return 0;
    void *v = p_object_unbox(b);
    if (!v)
        return 0;
    memcpy(out, v, 4);
    return *out >= 0 && *out <= limit;
}
/* Import/config arrays keep their original conservative bound. */
static int write_array_count(void *array, int *out) {
    return write_array_count_limit(array, out, 256);
}
static void *write_array_item(void *array, int index) {
    void *c = findclass(p_domain_get(), "System", "Array");
    const char *ip[] = {"System.Int32"};
    void *m = c ? pf_method(c, "GetValue", "System.Object", 1, ip, 1) : 0;
    void *args[] = {&index};
    return pf_invoke(m, array, args);
}
static int write_read_limit(void *config, void *c, const char *name, const char *type, void *out,
                            int size) {
    void *f = pf_field(c, name, type);
    if (!f || !write_value_layout(p_class_from_type(p_field_get_type(f)), size))
        return 0;
    p_field_get_value(config, f, out);
    return 1;
}
static int write_definition_id(void *d, long long *out) {
    void *m = get_station_method(p_object_get_class(d), "get_ID", "System.Int64");
    void *b = pf_invoke(m, d, 0);
    if (!b || !pf_class(p_object_get_class(b), "System.Int64") ||
        !write_value_layout(p_object_get_class(b), 8))
        return 0;
    void *v = p_object_unbox(b);
    if (!v)
        return 0;
    memcpy(out, v, 8);
    return *out != 0;
}
static int write_configuration_matches(void *config, PFRoots *roots) {
    void *c = p_object_get_class(config);
    int maxHeight = 0, maxBank = 0;
    float maxTangent = 0;
    if (!write_read_limit(config, c, "MaxHeight", "System.Int32", &maxHeight, 4) || maxHeight < 1 ||
        maxHeight > 1024 ||
        !write_read_limit(config, c, "MaxBankingAngle", "System.Int32", &maxBank, 4) ||
        maxBank < 0 || maxBank > 360 ||
        !write_read_limit(config, c, "MaxTangentModifier", "System.Single", &maxTangent, 4) ||
        !finite_vector(&maxTangent, 1) || maxTangent < 0 || maxTangent > 100) {
        write_log("CONFIG SKIP: destination support limits unavailable");
        return 0;
    }
    for (int i = 0; i < write_count; ++i) {
        float m[3];
        memcpy(m, write_fixture[i].modifiers, 12);
        if (write_fixture[i].height > maxHeight || m[0] < -maxTangent - 0.00001f ||
            m[0] > maxTangent + 0.00001f || m[1] < -maxTangent - 0.00001f ||
            m[1] > maxTangent + 0.00001f || m[2] < -maxBank || m[2] > maxBank) {
            write_log("CONFIG SKIP: support height/tangent/banking exceeds destination limits");
            return 0;
        }
    }
    const char *bp[] = {"System.Boolean"};
    void *get = pf_method(c, "GetTrackPieces", "TPS.Game.TrackPieceDefinition[]", 1, bp, 1);
    unsigned char yes = 1;
    void *args[] = {&yes};
    void *array = pf_invoke(get, config, args);
    if (!array || !pf_class(p_object_get_class(array), "TPS.Game.TrackPieceDefinition[]") ||
        !pf_root(roots, array)) {
        write_log("CONFIG SKIP: destination allowed pieces unavailable");
        return 0;
    }
    int count = 0;
    if (!write_array_count(array, &count) || count == 0)
        return 0;
    long long allowed[260];
    for (int i = 0; i < count; ++i) {
        PFRoots temp = {0};
        void *d = write_array_item(array, i);
        int ok = d && pf_class(p_object_get_class(d), "TPS.Game.TrackPieceDefinition") &&
                 pf_root(&temp, d) && write_definition_id(d, &allowed[i]);
        pf_release(&temp);
        if (!ok)
            return 0;
    }
    /* Chain/booster/toggle are separate config fields; GetTrackPieces only
   * enumerates NormalTrackPiece plus TrackPieces (verified native body). */
    const char *extraNames[] = {"ToggleTrackPiece", "ChainTrackPiece", "BoosterTrackPiece"};
    for (unsigned k = 0; k < 3; ++k) {
        long long id = 0;
        if (!write_read_limit(config, c, extraNames[k], "System.Int64", &id, 8))
            return 0;
        if (id)
            allowed[count++] = id;
    }
    for (int i = 0; i < write_count; ++i) {
        int found = 0;
        for (int k = 0; k < count; ++k)
            if (allowed[k] == write_fixture[i].piece)
                found = 1;
        if (!found) {
            write_log("CONFIG SKIP: a piece definition is not offered by the destination coaster");
            return 0;
        }
    }
    int hasDecorations = 0;
    for (int i = 0; i < write_count; ++i)
        if (write_fixture[i].decoration)
            hasDecorations = 1;
    if (hasDecorations) {
        void *decor = pf_ref(config, c, "TrackDecorations", "TPS.Game.GameItemDefinitionID[]");
        if (!decor || !pf_root(roots, decor) || !write_array_count(decor, &count))
            return 0;
        for (int i = 0; i < count; ++i) {
            void *b = write_array_item(decor, i);
            if (!b || !pf_class(p_object_get_class(b), "TPS.Game.GameItemDefinitionID") ||
                !write_value_layout(p_object_get_class(b), 8))
                return 0;
            void *v = p_object_unbox(b);
            if (!v)
                return 0;
            memcpy(&allowed[i], v, 8);
        }
        void *db = findclass(p_domain_get(), "TPS.Game", "LevelDatabaseUtils");
        const char *idp[] = {"TPS.Game.GameItemDefinitionID"};
        void *resolve = db ? pf_method(db, "Get", "TPS.Game.GameItemDefinition", 1, idp, 0) : 0;
        if (!resolve)
            return 0;
        for (int i = 0; i < write_count; ++i) {
            long long id = write_fixture[i].decoration;
            if (!id)
                continue;
            int found = 0;
            for (int k = 0; k < count; ++k)
                if (allowed[k] == id)
                    found = 1;
            if (!found) {
                write_log("CONFIG SKIP: a decoration is not offered by the destination coaster");
                return 0;
            }
            void *da[] = {&id};
            void *d = pf_invoke(resolve, 0, da);
            void *expected = p_class_from_type(p_method_get_return_type(resolve));
            PFRoots temp = {0};
            long long resolved = 0;
            int ok = d && expected && p_class_is_assignable_from(expected, p_object_get_class(d)) &&
                     pf_root(&temp, d) && write_definition_id(d, &resolved) && resolved == id;
            pf_release(&temp);
            if (!ok)
                return 0;
        }
    }
    write_log(
        "CONFIG PASSED: all pieces/decorations offered by destination; support limits checked");
    return 1;
}
