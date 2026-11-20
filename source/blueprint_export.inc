/* Included after the validated snapshot/GC helpers. All managed work occurs
 * in the F8 game-owned callback. No record offsets or array layouts used. */
#include "blueprint_json.h"
typedef struct {
    const char *name;
    const char *type;
    int size;
} RecordField;
static const RecordField supportSchema[] = {
    {"SectionIndex", "System.Int32", 4},
    {"Position", "UnityEngine.Vector3", 12},
    {"Rotation", "UnityEngine.Quaternion", 16},
    {"LocalRotation", "System.Single", 4},
    {"WorldPosition", "UnityEngine.Vector3", 12},
    {"FloorAreaInstance", "TPS.Core.Utils.ManagedEntityID", 8},
    {"Valid", "System.Boolean", 1},
    {"Height", "System.Int32", 4},
    {"TangentInModifier", "System.Single", 4},
    {"TangentOutModifier", "System.Single", 4},
    {"BankingModifier", "System.Single", 4}};
static const RecordField pieceSchema[] = {
    {"SectionIndex", "System.Int32", 4},
    {"TrackPieceDefinitionID", "System.Int64", 8},
    {"TrackPieceDecorationDefinition", "TPS.Game.GameItemDefinitionID", 8},
    {"DecorationPosition", "System.Single", 4}};
static int read_field_bytes(void *obj, void *cls, const RecordField *spec, void *out) {
    void *f = p_class_get_field_from_name(cls, spec->name);
    if (!instance_field(f, cls) || !exact_type(p_field_get_type(f), spec->type))
        return 0;
    void *t = p_class_from_type(p_field_get_type(f));
    unsigned align = 0;
    if (!t || !p_class_is_valuetype(t) || p_class_value_size(t, &align) != spec->size) {
        logtext("EXPORT SKIP: field value size mismatch.\r\n");
        return 0;
    }
    p_field_get_value(obj, f, out);
    return 1;
}
static int vector_fields(void *obj, void *cls, unsigned n, float *out) {
    const char *names[4] = {"x", "y", "z", "w"};
    for (unsigned i = 0; i < n; ++i) {
        RecordField f = {names[i], "System.Single", 4};
        if (!read_field_bytes(obj, cls, &f, &out[i]))
            return 0;
    }
    return 1;
}
static void jvector(const float *v, unsigned n) {
    js("[");
    for (unsigned i = 0; i < n; ++i) {
        if (i)
            js(",");
        jf(v[i]);
    }
    js("]");
}
static int g_frame;
static int g_station_position_verified;
static int g_transform_probe_read;
static int g_transform_position_agrees;
static float g_transform_probe_position[3], g_transform_probe_rotation[4];
static const char *g_station_position_source;
static const char *g_station_rotation_source;
static volatile long g_blueprint_busy;
static float g_origin[3], g_rotation[4];
static void *get_station_method(void *stationClass, const char *name, const char *type) {
    if (!stationClass)
        return 0;
    void *m = p_class_get_method_from_name(stationClass, name, 0);
    if (!m || !p_method_is_instance(m) || p_method_is_generic(m) ||
        p_method_get_param_count(m) != 0 || !p_method_get_class(m) ||
        !p_class_is_assignable_from(p_method_get_class(m), stationClass) ||
        !exact_type(p_method_get_return_type(m), type))
        return 0;
    return m;
}
static int station_value(void *station, void *m, const char *type, int size, unsigned n,
                         float *out) {
    void *t = p_method_get_return_type(m);
    void *cls = p_class_from_type(t);
    unsigned align = 0;
    if (!cls || !p_class_is_valuetype(cls) || p_class_value_size(cls, &align) != size)
        return 0;
    logtext("About to invoke station ");
    logtext(p_method_get_name(m));
    nl();
    FlushFileBuffers(g_log);
    void *ex = 0;
    void *box = p_runtime_invoke(m, station, 0, &ex);
    if (ex || !box || p_object_get_class(box) != cls) {
        logtext("EXPORT NOTE: station accessor exception/null/type mismatch.\r\n");
        return 0;
    }
    GCHandle root = pin_reference(box);
    if (!root)
        return 0;
    int ok = exact_type(p_class_get_type(cls), type) && vector_fields(box, cls, n, out);
    release_reference(root);
    return ok;
}

static int finite_vector(const float *v, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        unsigned bits = 0;
        for (unsigned k = 0; k < 4; ++k)
            ((unsigned char *)&bits)[k] = ((const unsigned char *)&v[i])[k];
        if ((bits & 0x7f800000U) == 0x7f800000U)
            return 0;
    }
    return 1;
}
static void log_station_vector(const char *label, const float *v, unsigned n) {
    /* Called before the document starts. Reset the shared scratch JSON buffer. */
    jpos = 0;
    jok = 1;
    jbuf[0] = 0;
    jvector(v, n);
    logtext(label);
    logtext(jok ? jbuf : "nonfinite");
    nl();
    jpos = 0;
    jok = 1;
    jbuf[0] = 0;
}

/* Uses only the game-provided station Transform reference and validated Unity
 * world getters. No native Transform layout or scalar angle conversion. */
static int transform_frame(void *station, void *stationClass) {
    void *getter =
        get_station_method(stationClass, "get_GameObjectTransform", "UnityEngine.Transform");
    if (!getter) {
        logtext("TRANSFORM FRAME SKIP: station Transform getter signature unavailable.\r\n");
        return 0;
    }
    void *transformClass = p_class_from_type(p_method_get_return_type(getter));
    if (!transformClass || p_class_is_valuetype(transformClass)) {
        logtext("TRANSFORM FRAME SKIP: Transform is not a reference type.\r\n");
        return 0;
    }
    logtext("About to invoke station get_GameObjectTransform().\r\n");
    FlushFileBuffers(g_log);
    void *exception = 0;
    void *object = p_runtime_invoke(getter, station, 0, &exception);
    if (exception || !object) {
        logtext("TRANSFORM FRAME SKIP: Transform getter exception or NULL.\r\n");
        return 0;
    }
    void *actual = p_object_get_class(object);
    if (!actual || !p_class_is_assignable_from(transformClass, actual)) {
        logtext("TRANSFORM FRAME SKIP: returned object is not Transform.\r\n");
        return 0;
    }
    GCHandle root = pin_reference(object);
    if (!root)
        return 0;
    int ok = 0;
    void *position = get_station_method(transformClass, "get_position", "UnityEngine.Vector3");
    void *rotation = get_station_method(transformClass, "get_rotation", "UnityEngine.Quaternion");
    if (!position || !rotation || p_method_get_class(position) != transformClass ||
        p_method_get_class(rotation) != transformClass) {
        logtext(
            "TRANSFORM FRAME SKIP: Unity Transform world getter signature/declaring class differs.\r\n");
        goto done;
    }
    if (!station_value(object, position, "UnityEngine.Vector3", 12, 3,
                       g_transform_probe_position) ||
        !station_value(object, rotation, "UnityEngine.Quaternion", 16, 4,
                       g_transform_probe_rotation) ||
        !finite_vector(g_transform_probe_position, 3) ||
        !finite_vector(g_transform_probe_rotation, 4)) {
        logtext(
            "TRANSFORM FRAME SKIP: world getter exception, invalid result or nonfinite values.\r\n");
        goto done;
    }
    g_transform_probe_read = 1;
    log_station_vector("Transform world position probe = ", g_transform_probe_position, 3);
    log_station_vector("Transform world quaternion probe = ", g_transform_probe_rotation, 4);
    g_transform_position_agrees = g_station_position_verified;
    if (g_station_position_verified) {
        for (unsigned i = 0; i < 3; ++i) {
            double delta = (double)g_origin[i] - g_transform_probe_position[i];
            if (delta > 0.001 || delta < -0.001)
                g_transform_position_agrees = 0;
        }
        if (!g_transform_position_agrees) {
            logtext(
                "TRANSFORM FRAME SKIP: Transform/station position mismatch exceeds 0.001; native export retained for review.\r\n");
            goto done;
        }
    }
    double norm = 0;
    for (unsigned i = 0; i < 4; ++i)
        norm += (double)g_transform_probe_rotation[i] * g_transform_probe_rotation[i];
    if (norm < 0.999 || norm > 1.001) {
        logtext("TRANSFORM FRAME SKIP: world quaternion is not unit length.\r\n");
        goto done;
    }
    for (unsigned i = 0; i < 3; ++i)
        g_origin[i] = g_transform_probe_position[i];
    for (unsigned i = 0; i < 4; ++i)
        g_rotation[i] = g_transform_probe_rotation[i];
    g_station_position_verified = 1;
    g_station_position_source = "get_GameObjectTransform().get_position";
    g_station_rotation_source = "get_GameObjectTransform().get_rotation";
    ok = 1;
done:
    release_reference(root);
    return ok;
}

static int g_floor_frame_read;
static const char *g_floor_frame_status;
static float g_floor_position[3], g_floor_rotation[4];
static void floor_frame(void *station) {
    g_floor_frame_read = 0;
    g_floor_frame_status = "getter-unavailable";
    void *cls = p_object_get_class(station);
    void *getter = get_station_method(cls, "GetFloorAreaTransform", "UnityEngine.Transform");
    if (!getter) {
        logtext("FLOOR FRAME SKIP: zero-argument GetFloorAreaTransform signature unavailable.\r\n");
        return;
    }
    void *tc = p_class_from_type(p_method_get_return_type(getter));
    if (!tc || p_class_is_valuetype(tc)) {
        g_floor_frame_status = "unexpected-transform-type";
        return;
    }
    logtext("About to invoke read-only station GetFloorAreaTransform().\r\n");
    FlushFileBuffers(g_log);
    void *ex = 0;
    void *object = p_runtime_invoke(getter, station, 0, &ex);
    if (ex || !object) {
        g_floor_frame_status = ex ? "getter-exception" : "null-transform";
        logtext("FLOOR FRAME SKIP: floor getter exception or NULL.\r\n");
        return;
    }
    void *actual = p_object_get_class(object);
    if (!actual || !p_class_is_assignable_from(tc, actual)) {
        g_floor_frame_status = "unexpected-object-class";
        return;
    }
    GCHandle root = pin_reference(object);
    if (!root) {
        g_floor_frame_status = "root-failed";
        return;
    }
    void *pos = get_station_method(tc, "get_position", "UnityEngine.Vector3");
    void *rot = get_station_method(tc, "get_rotation", "UnityEngine.Quaternion");
    g_floor_frame_status = "world-getter-signature-mismatch";
    if (!pos || !rot || p_method_get_class(pos) != tc || p_method_get_class(rot) != tc)
        goto done;
    g_floor_frame_status = "world-getter-failed";
    if (!station_value(object, pos, "UnityEngine.Vector3", 12, 3, g_floor_position) ||
        !station_value(object, rot, "UnityEngine.Quaternion", 16, 4, g_floor_rotation) ||
        !finite_vector(g_floor_position, 3) || !finite_vector(g_floor_rotation, 4))
        goto done;
    double normalized[4];
    g_floor_frame_status = "non-unit-quaternion";
    if (!normalized_quaternion(g_floor_rotation, normalized))
        goto done;
    g_floor_frame_read = 1;
    g_floor_frame_status = "world-frame-read";
    log_station_vector("Floor Transform world position probe = ", g_floor_position, 3);
    log_station_vector("Floor Transform world quaternion probe = ", g_floor_rotation, 4);
    logtext(
        "Floor frame read; support world/floor-local rotation semantics remain unconfirmed.\r\n");
done:
    if (!g_floor_frame_read) {
        logtext("FLOOR FRAME SKIP: ");
        logtext(g_floor_frame_status);
        nl();
    }
    release_reference(root);
}
static void station_frame(void *station) {
    g_frame = 0;
    g_station_position_verified = 0;
    g_station_position_source = 0;
    g_station_rotation_source = 0;
    g_transform_probe_read = 0;
    g_transform_position_agrees = 0;
    void *cls = p_object_get_class(station);
    if (!cls) {
        logtext("FRAME SKIP: station class unavailable.\r\n");
        return;
    }
    void *pos = get_station_method(cls, "get_Position", "UnityEngine.Vector3");
    void *rot = get_station_method(cls, "get_Rotation", "UnityEngine.Quaternion");
    const char *posSource = "get_Position";
    const char *rotSource = "get_Rotation";
    /* Prefer an explicit world/world pair if the existing pair is incomplete.
   * Signature checks precede every invocation. No other discovery candidate
   * is called, including Single/enum Rotation properties. */
    if (!pos || !rot) {
        void *worldPos = get_station_method(cls, "get_WorldPosition", "UnityEngine.Vector3");
        void *worldRot = get_station_method(cls, "get_WorldRotation", "UnityEngine.Quaternion");
        if (worldPos && worldRot) {
            pos = worldPos;
            rot = worldRot;
            posSource = "get_WorldPosition";
            rotSource = "get_WorldRotation";
        }
    }
    if (pos && station_value(station, pos, "UnityEngine.Vector3", 12, 3, g_origin) &&
        finite_vector(g_origin, 3)) {
        g_station_position_verified = 1;
        g_station_position_source = posSource;
        log_station_vector("Station position probe = ", g_origin, 3);
    }
    if (!g_station_position_verified || !rot) {
        if (!transform_frame(station, cls)) {
            logtext(
                "FRAME UNRESOLVED: no validated station/Transform pair; native export retained.\r\n");
            return;
        }
    } else {
        if (!station_value(station, rot, "UnityEngine.Quaternion", 16, 4, g_rotation) ||
            !finite_vector(g_rotation, 4)) {
            logtext("FRAME UNRESOLVED: rotation accessor failed or returned nonfinite data.\r\n");
            return;
        }
        g_station_rotation_source = rotSource;
    }
    double norm = 0;
    for (unsigned i = 0; i < 4; ++i)
        norm += (double)g_rotation[i] * g_rotation[i];
    if (norm < 0.999 || norm > 1.001) {
        logtext("FRAME UNRESOLVED: station quaternion not unit length.\r\n");
        return;
    }
    double inv = 1;
    for (unsigned i = 0; i < 4; ++i)
        inv = inv * (1.5 - 0.5 * norm * inv * inv);
    for (unsigned i = 0; i < 4; ++i)
        g_rotation[i] = (float)(g_rotation[i] * inv);
    g_frame = 1;
    log_station_vector("Station rotation probe = ", g_rotation, 4);
    logtext(
        "Station frame accessors validated; relative WorldPosition output enabled (frame semantics require review).\r\n");
}
static int write_record_field(void *obj, void *cls, const RecordField *spec) {
    void *f = p_class_get_field_from_name(cls, spec->name);
    if (!instance_field(f, cls) || !exact_type(p_field_get_type(f), spec->type))
        return 0;
    void *fc = p_class_from_type(p_field_get_type(f));
    unsigned align = 0;
    if (!fc || !p_class_is_valuetype(fc) || p_class_value_size(fc, &align) != spec->size) {
        logtext("EXPORT SKIP: record field size mismatch.\r\n");
        return 0;
    }
    jq(spec->name);
    js(":");
    if (equals(spec->type, "System.Int32")) {
        int v = 0;
        p_field_get_value(obj, f, &v);
        ji(v);
    } else if (equals(spec->type, "System.Int64")) {
        long long v = 0;
        p_field_get_value(obj, f, &v);
        js("\"");
        ji(v);
        js("\"");
    } else if (equals(spec->type, "System.Boolean")) {
        unsigned char v = 0;
        p_field_get_value(obj, f, &v);
        if (v > 1)
            return 0;
        js(v ? "true" : "false");
    } else if (equals(spec->type, "System.Single")) {
        float v = 0;
        p_field_get_value(obj, f, &v);
        if (!jf(v))
            return 0;
        js(",\"");
        js(spec->name);
        js("Float32Bits\":");
        jfloatbits(v);
    } else {
        void *box = p_field_get_value_object(f, obj);
        if (!box || p_object_get_class(box) != fc)
            return 0;
        GCHandle root = pin_reference(box);
        if (!root)
            return 0;
        int ok = 1;
        if (equals(spec->type, "UnityEngine.Vector3") ||
            equals(spec->type, "UnityEngine.Quaternion")) {
            float v[4] = {0};
            unsigned n = spec->size == 12 ? 3U : 4U;
            ok = vector_fields(box, fc, n, v);
            if (ok) {
                jvector(v, n);
                if (equals(spec->name, "Rotation")) {
                    float relative[4];
                    if (g_support_world_verified && g_frame &&
                        relative_quaternion(g_rotation, v, relative)) {
                        js(",\"StationRelativeRotation\":");
                        jvector(relative, 4);
                        js(",\"StationRelativeRotationStatus\":\"confirmed-world-source\"");
                        logtext(
                            "Support station-relative quaternion computed from confirmed world-space Rotation.\r\n");
                    } else {
                        js(",\"StationRelativeRotationStatus\":");
                        jq(!g_support_world_verified ? "source-space-unverified"
                                                     : (g_frame ? "source-quaternion-not-near-unit"
                                                                : "station-frame-unavailable"));
                        logtext(
                            "ORIENTATION NOTE: relative quaternion omitted; native Rotation retained.\r\n");
                    }
                }
                if (g_frame && equals(spec->name, "WorldPosition")) {
                    float rel[3];
                    inverse_point(v, g_origin, g_rotation, rel);
                    js(",\"StationRelativeWorldPosition\":");
                    jvector(rel, 3);
                }
                js(",\"");
                js(spec->name);
                js("Float32Bits\":[");
                for (unsigned i = 0; i < n; ++i) {
                    if (i)
                        js(",");
                    jfloatbits(v[i]);
                }
                js("]");
            }
        } else {
            /* ID wrapper internals are discovered, not reinterpreted as Int64.
       * Accept exactly one non-static Int64/UInt64 instance field of size 8. */
            void *it = 0;
            void *inner = 0;
            void *found = 0;
            unsigned fields = 0;
            while ((inner = p_class_get_fields(fc, &it))) {
                if (p_field_get_flags(inner) & 0x0010)
                    continue;
                found = inner;
                ++fields;
            }
            if (fields != 1 || !instance_field(found, fc)) {
                logtext("EXPORT SKIP: ID wrapper is not a single-field value.\r\n");
                ok = 0;
            } else {
                void *t = p_field_get_type(found);
                char *name = t ? p_type_get_name(t) : 0;
                void *ic = t ? p_class_from_type(t) : 0;
                int signedId = equals(name, "System.Int64");
                ok = (signedId || equals(name, "System.UInt64")) && ic &&
                     p_class_is_valuetype(ic) && p_class_value_size(ic, &align) == 8;
                if (name)
                    p_free(name);
                if (ok) {
                    unsigned long long value = 0;
                    p_field_get_value(box, found, &value);
                    js("{");
                    jq(p_field_get_name(found));
                    js(":\"");
                    if (signedId)
                        ji((long long)value);
                    else
                        ju(value);
                    js("\"}");
                } else
                    logtext("EXPORT SKIP: ID wrapper field type differs.\r\n");
            }
        }
        release_reference(root);
        if (!ok)
            return 0;
    }
    return jok;
}
static int invoke_count(void *list, void *cls, int *out) {
    void *m = p_class_get_method_from_name(cls, "get_Count", 0);
    if (!m || p_method_get_class(m) != cls || !p_method_is_instance(m) || p_method_is_generic(m) ||
        p_method_get_param_count(m) != 0 ||
        !exact_type(p_method_get_return_type(m), "System.Int32"))
        return 0;
    void *ex = 0;
    void *box = p_runtime_invoke(m, list, 0, &ex);
    if (ex || !box)
        return 0;
    void *bc = p_object_get_class(box);
    unsigned align = 0;
    if (!bc || !p_class_is_valuetype(bc) || p_class_value_size(bc, &align) != 4 ||
        !exact_type(p_class_get_type(bc), "System.Int32"))
        return 0;
    void *v = p_object_unbox(box);
    if (!v)
        return 0;
    for (unsigned i = 0; i < 4; ++i)
        ((unsigned char *)out)[i] = ((unsigned char *)v)[i];
    return *out >= 0 && *out <= 4096;
}
static int export_list(void *data, void *dataClass, const char *fieldName, const char *listType,
                       const char *recordType, int recordSize, const RecordField *schema,
                       unsigned fields, int *outCount) {
    void *f = p_class_get_field_from_name(dataClass, fieldName);
    if (!instance_field(f, dataClass) || !exact_type(p_field_get_type(f), listType))
        return 0;
    void *cls = p_class_from_type(p_field_get_type(f));
    void *list = 0;
    p_field_get_value(data, f, &list);
    if (!list || !cls || p_class_is_valuetype(cls) || p_object_get_class(list) != cls) {
        logtext("EXPORT SKIP: null or unexpected concrete list.\r\n");
        return 0;
    }
    GCHandle root = pin_reference(list);
    if (!root)
        return 0;
    int ok = 0, count = 0;
    if (!invoke_count(list, cls, &count)) {
        logtext("EXPORT SKIP: list count unavailable or exceeds 4096.\r\n");
        goto done;
    }
    void *m = p_class_get_method_from_name(cls, "get_Item", 1);
    if (!m || p_method_get_class(m) != cls || !p_method_is_instance(m) || p_method_is_generic(m) ||
        p_method_get_param_count(m) != 1 || !exact_type(p_method_get_param(m, 0), "System.Int32") ||
        !exact_type(p_method_get_return_type(m), recordType)) {
        logtext("EXPORT SKIP: get_Item ABI mismatch.\r\n");
        goto done;
    }
    void *recordClass = p_class_from_type(p_method_get_return_type(m));
    unsigned align = 0;
    if (!recordClass || !p_class_is_valuetype(recordClass) ||
        p_class_value_size(recordClass, &align) != recordSize) {
        logtext("EXPORT SKIP: record size differs.\r\n");
        goto done;
    }
    void *fieldIter = 0;
    void *eachField = 0;
    unsigned actualFields = 0;
    while ((eachField = p_class_get_fields(recordClass, &fieldIter)))
        if (!(p_field_get_flags(eachField) & 0x0010))
            ++actualFields;
    if (actualFields != fields) {
        logtext("EXPORT SKIP: record has unknown or missing instance fields.\r\n");
        goto done;
    }
    /* Validate every required field before invoking the first item. */
    for (unsigned k = 0; k < fields; ++k) {
        void *rf = p_class_get_field_from_name(recordClass, schema[k].name);
        if (!instance_field(rf, recordClass) || !exact_type(p_field_get_type(rf), schema[k].type))
            goto done;
        void *fc = p_class_from_type(p_field_get_type(rf));
        if (!fc || !p_class_is_valuetype(fc) || p_class_value_size(fc, &align) != schema[k].size)
            goto done;
    }
    jq(fieldName);
    js(":[");
    for (int i = 0; i < count; ++i) {
        logtext("About to read ");
        logtext(fieldName);
        logtext("[");
        lognum((UINT_PTR)i);
        logtext("] via get_Item.\r\n");
        FlushFileBuffers(g_log);
        void *args[1] = {&i};
        void *ex = 0;
        void *box = p_runtime_invoke(m, list, args, &ex);
        if (ex || !box || p_object_get_class(box) != recordClass) {
            logtext("EXPORT SKIP: get_Item exception/null/type mismatch.\r\n");
            goto done;
        }
        GCHandle itemRoot = pin_reference(box);
        if (!itemRoot)
            goto done;
        if (i)
            js(",");
        js("{\"listIndex\":");
        ji(i);
        int itemOk = 1;
        for (unsigned k = 0; k < fields; ++k) {
            js(",");
            if (!write_record_field(box, recordClass, &schema[k])) {
                itemOk = 0;
                break;
            }
        }
        js("}");
        release_reference(itemRoot);
        if (!itemOk)
            goto done;
    }
    js("]");
    int finalCount = 0;
    if (!invoke_count(list, cls, &finalCount) || finalCount != count) {
        logtext("EXPORT SKIP: list count changed during export.\r\n");
        goto done;
    }
    *outCount = count;
    ok = jok;
done:
    release_reference(root);
    return ok;
}
static void publish_blueprint(void) {
    WCHAR dir[512], temp[512], final[512];
    if (!coaster_storage_path(dir, 512, L"Diagnostics", 1)) {
        logtext("EXPORT SKIP: LocalLow storage unavailable.\r\n");
        return;
    }
    unsigned pos = 0;
    while (dir[pos])
        ++pos;
    /* CREATE_NEW and a no-replace rename prevent overwriting previous exports. */
    HANDLE file = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 100 && file == INVALID_HANDLE_VALUE; ++attempt) {
        for (unsigned i = 0; i <= pos; ++i) {
            temp[i] = dir[i];
            final[i] = dir[i];
        }
        unsigned tp = pos, fp = pos;
        WCHAR suffix[80];
        unsigned k = 0;
        const WCHAR *prefix = L"\\coaster-";
        while (*prefix)
            suffix[k++] = *prefix++;
        UINT_PTR token = GetTickCount64() + attempt;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned d = (unsigned)((token >> ((15 - i) * 4)) & 15);
            suffix[k++] = (WCHAR)(d < 10 ? '0' + d : 'a' + d - 10);
        }
        suffix[k] = 0;
        if (!append(temp, 512, &tp, suffix) || !append(final, 512, &fp, suffix) ||
            !append(temp, 512, &tp, L".tmp") || !append(final, 512, &fp, L".json"))
            return;
        file = CreateFileW(temp, 0x40000000, 0, 0, 1, FILE_ATTRIBUTE_NORMAL, 0);
    }
    if (file == INVALID_HANDLE_VALUE) {
        logtext("EXPORT SKIP: cannot create unique template temp file.\r\n");
        return;
    }
    DWORD written = 0;
    int ok = WriteFile(file, jbuf, jpos, &written, 0) && written == jpos;
    if (ok)
        ok = FlushFileBuffers(file) != 0;
    if (!CloseHandle(file))
        ok = 0;
    if (ok)
        ok = MoveFileExW(temp, final, 0) != 0;
    if (!ok) {
        DeleteFileW(temp);
        logtext(
            "EXPORT FAILED: write/flush/close/rename failed; no completed template published.\r\n");
        return;
    }
    logtext("TEMPLATE EXPORT COMPLETE: LocalLow/TPM Coaster Library/Diagnostics/");
    for (unsigned i = pos + 1; final[i]; ++i) {
        char b[2] = {(char) final[i], 0};
        logtext(b);
    }
    nl();
    logtext(
        g_frame
            ? "Station-relative WorldPosition included; native source fields retained.\r\n"
            : "Native snapshot export only: station frame unresolved; not a placement-ready template.\r\n");
}
static long long export_station_definition_id(void *station) {
    void *m = get_station_method(p_object_get_class(station), "get_Definition",
                                 "TPS.Game.GameItemDefinition");
    if (!m)
        return 0;
    void *ex = 0;
    void *d = p_runtime_invoke(m, station, 0, &ex);
    void *expected = p_class_from_type(p_method_get_return_type(m));
    if (ex || !d || !expected || !p_class_is_assignable_from(expected, p_object_get_class(d)))
        return 0;
    GCHandle root = pin_reference(d);
    if (!root)
        return 0;
    long long id = 0;
    void *get = get_station_method(p_object_get_class(d), "get_ID", "System.Int64");
    void *b = get ? p_runtime_invoke(get, d, 0, &ex) : 0;
    unsigned align = 0;
    if (!ex && b) {
        void *c = p_object_get_class(b);
        if (c && exact_type(p_class_get_type(c), "System.Int64") && p_class_is_valuetype(c) &&
            p_class_value_size(c, &align) == 8) {
            void *v = p_object_unbox(b);
            if (v)
                memcpy(&id, v, 8);
        }
    }
    release_reference(root);
    return id;
}
static void export_blueprint(void *data, void *dataClass, void *station) {
    if (__atomic_exchange_n(&g_blueprint_busy, 1, __ATOMIC_ACQ_REL)) {
        logtext("EXPORT SKIP: an export is already in progress.\r\n");
        return;
    }
    jpos = 0;
    jok = 1;
    jbuf[0] = 0;
    GCHandle stationRoot = pin_reference(station);
    if (!stationRoot) {
        __atomic_store_n(&g_blueprint_busy, 0, __ATOMIC_RELEASE);
        return;
    }
    station_frame(station);
    floor_frame(station);
    js("{\"schema\":\"tpm-coaster-snapshot\",\"schemaVersion\":2,\"exporterVersion\":\"0.5.7\",\"readOnly\":true,\"placementReady\":false,");
    js("\"coordinateFrame\":");
    jq(g_frame ? "native-with-station-relative-world-position" : "snapshot-native");
    js(",\"supportOrientation\":{\"source\":\"TrackSupportSnapshotData.Rotation\",\"sourceSpace\":");
    jq(g_support_world_verified ? "world" : "unverified");
    js(",\"formula\":\"inverse(normalized station world quaternion) * normalized source quaternion\",\"sourceSpaceConfirmed\":");
    js(g_support_world_verified ? "true" : "false");
    js(",\"verification\":\"runtime-writer-and-Transform-getter-slot-match\",\"placementReady\":false}");
    unsigned long long id = 0;
    p_field_get_value(station, g_id_field, &id);
    js(",\"sourceStationId\":\"");
    ju(id);
    js("\"");
    long long definitionId = export_station_definition_id(station);
    js(",\"sourceStationDefinitionId\":");
    if (definitionId) {
        js("\"");
        ji(definitionId);
        js("\"");
        logtext("EXPORT: exact station definition ID captured.\r\n");
    } else {
        js("null");
        logtext(
            "EXPORT NOTE: station definition unavailable; F9 remains available, F10 requires a new definition capture.\r\n");
    }
    unsigned entity[2] = {0};
    p_field_get_value(station, g_entity_field, entity);
    js(",\"sourceEntity\":{\"index\":");
    ju(entity[0]);
    js(",\"version\":");
    ju(entity[1]);
    js("}");
    js(",\"stationFrameProbe\":{\"status\":");
    jq(g_frame ? "position-and-quaternion"
               : (g_station_position_verified ? "position-only" : "unresolved"));
    js(",\"semanticsConfirmed\":false");
    if (g_station_position_verified) {
        js(",\"positionAccessor\":");
        jq(g_station_position_source);
        js(",\"position\":");
        jvector(g_origin, 3);
    }
    if (g_frame) {
        js(",\"rotationAccessor\":");
        jq(g_station_rotation_source);
        js(",\"rotation\":");
        jvector(g_rotation, 4);
    }
    if (g_transform_probe_read) {
        js(",\"transformWorldPosition\":");
        jvector(g_transform_probe_position, 3);
        js(",\"transformWorldRotation\":");
        jvector(g_transform_probe_rotation, 4);
        js(",\"transformPositionAgreesWithStation\":");
        js(g_transform_position_agrees ? "true" : "false");
    }
    js("}");
    if (g_frame) {
        js(",\"stationPosition\":");
        jvector(g_origin, 3);
        js(",\"stationRotation\":");
        jvector(g_rotation, 4);
    }
    js(",\"floorFrameProbe\":{\"status\":");
    jq(g_floor_frame_status);
    js(",\"source\":\"station.GetFloorAreaTransform()\",\"supportFloorAssociationConfirmed\":false,\"supportRotationSpaceConfirmed\":");
    js(g_support_world_verified ? "true" : "false");
    if (g_floor_frame_read) {
        js(",\"worldPosition\":");
        jvector(g_floor_position, 3);
        js(",\"worldRotation\":");
        jvector(g_floor_rotation, 4);
    }
    js("}");
    js(",");
    int supports = 0, pieces = 0;
    int ok = export_list(data, dataClass, "TrackSupportData",
                         "System.Collections.Generic.List<TPS.Game.TrackSupportSnapshotData>",
                         "TPS.Game.TrackSupportSnapshotData", 80, supportSchema, 11, &supports);
    if (ok) {
        js(",");
        ok = export_list(data, dataClass, "TrackPieceData",
                         "System.Collections.Generic.List<TPS.Game.TrackPieceSnapshotData>",
                         "TPS.Game.TrackPieceSnapshotData", 32, pieceSchema, 4, &pieces);
    }
    if (ok) {
        js(",\"supportCount\":");
        ji(supports);
        js(",\"pieceCount\":");
        ji(pieces);
        js("}\n");
        if (jok) {
            if (blueprint_save_export_request)
                blueprint_save_exported();
            else
                publish_blueprint();
        }
    }
    if (!ok || !jok)
        logtext(
            "EXPORT SKIP: incomplete/unsupported snapshot or JSON limit; no template file written.\r\n");
    release_reference(stationRoot);
    __atomic_store_n(&g_blueprint_busy, 0, __ATOMIC_RELEASE);
}
