/* Bounded little-endian import-plan reader. JSON validation is performed by
 * Prepare_Blueprint.py; this independently validates every write-relevant byte. */
static unsigned char write_plan_bytes[60 + WRITE_MAX_SUPPORTS * 72 + 5];
static unsigned plan_u32(const unsigned char *p) {
    unsigned v = 0;
    memcpy(&v, p, 4);
    return v;
}
static int write_decode_plan(const unsigned char *b, unsigned n, int commit) {
    static const unsigned char magic[8] = {'T', 'P', 'M', 'P', 'L', 'A', 'N', '1'};
    if (n < 96)
        return 0;
    for (unsigned i = 0; i < 7; ++i)
        if (b[i] != magic[i])
            return 0;
    unsigned stride = b[7] == '1' ? 32 : ((b[7] == '2' || b[7] == '3') ? 72 : 0);
    if (!stride)
        return 0;
    unsigned count = plan_u32(b + 8);
    if (!count || count > WRITE_MAX_SUPPORTS || plan_u32(b + 12) || n != 64 + count * stride)
        return 0;
    if (write_hash(b, n - 4) != plan_u32(b + n - 4))
        return 0;
    unsigned long long id = 0;
    long long normal = 0;
    float position[3], rotation[4];
    memcpy(&id, b + 16, 8);
    memcpy(&normal, b + 24, 8);
    memcpy(position, b + 32, 12);
    memcpy(rotation, b + 44, 16);
    if (!id || (b[7] == '3' ? normal == 0 : (stride == 32 ? normal == 0 : normal != 0)) ||
        !finite_vector(position, 3) || !finite_vector(rotation, 4))
        return 0;
    double norm[4];
    if (!normalized_quaternion(rotation, norm))
        return 0;
    double length = 0;
    for (unsigned i = 0; i < 4; ++i)
        length += (double)rotation[i] * rotation[i];
    if (length < 0.99999 || length > 1.00001 || rotation[0] < -0.00001f || rotation[0] > 0.00001f ||
        rotation[2] < -0.00001f || rotation[2] > 0.00001f)
        return 0;
    for (unsigned i = 0; i < count; ++i) {
        float p[3], q[4], angle = 0;
        const unsigned char *r = b + 60 + i * stride;
        memcpy(p, r, 12);
        memcpy(q, r + 12, 16);
        memcpy(&angle, r + 28, 4);
        if (!finite_vector(p, 3) || !finite_vector(q, 4) || !finite_vector(&angle, 1) ||
            angle < 0 || angle > 360 || p[1] != position[1])
            return 0;
        for (unsigned k = 0; k < 3; ++k) {
            double d = (double)p[k] - position[k];
            if (d < -1024 || d > 1024)
                return 0;
        }
        if (!normalized_quaternion(q, norm))
            return 0;
        length = 0;
        for (unsigned k = 0; k < 4; ++k)
            length += (double)q[k] * q[k];
        if (length < 0.99999 || length > 1.00001)
            return 0;
        if (stride == 72) {
            int height = 0;
            float modifiers[3], decorationPos = 0;
            long long piece = 0;
            memcpy(&height, r + 32, 4);
            memcpy(modifiers, r + 36, 12);
            memcpy(&piece, r + 48, 8);
            memcpy(&decorationPos, r + 64, 4);
            if (height < 1 || height > 1024 || !piece || plan_u32(r + 68) ||
                !finite_vector(modifiers, 3) || !finite_vector(&decorationPos, 1) ||
                decorationPos < 0 || decorationPos > 1)
                return 0;
            if (modifiers[0] < -100 || modifiers[0] > 100 || modifiers[1] < -100 ||
                modifiers[1] > 100 || modifiers[2] < -360 || modifiers[2] > 360)
                return 0;
        }
    }
    if (!commit)
        return 1;
    write_source_id = id;
    write_station_definition_id = b[7] == '3' ? normal : 0;
    write_normal_id = b[7] == '3' ? 0 : normal;
    write_count = (int)count;
    memcpy(write_source_position, position, 12);
    memcpy(write_source_rotation, rotation, 16);
    for (unsigned i = 0; i < count; ++i) {
        memset(&write_fixture[i], 0, sizeof(WriteFixture));
        memcpy(&write_fixture[i], b + 60 + i * stride, stride);
        if (stride == 32) {
            write_fixture[i].height = 1;
            write_fixture[i].piece = normal;
            write_fixture[i].decorationPosition = i == 0 ? 0x3f000000U : 0;
        }
    }
    logtext("FIRST WRITE: Selected template source station ID=");
    lognum((UINT_PTR)id);
    logtext(" support/piece count=");
    lognum(count);
    nl();
    return 1;
}
static int write_decode_selection(const unsigned char *b, unsigned n) {
    return write_decode_plan(b, n, 1);
}
static int write_selection_frozen, write_library_selection;
static int write_load_selection(void) {
    if (write_selection_frozen || write_library_selection)
        return 1;
    WCHAR path[512];
    if (!coaster_storage_path(path, 512, L"Diagnostics", 0))
        return 0;
    unsigned n = 0;
    while (path[n])
        ++n;
    if (!append(path, 512, &n, L"\\SelectedBlueprint.tpmplan"))
        return 0;
    HANDLE f = CreateFileW(path, 0x80000000, FILE_SHARE_READ, 0, 3, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) {
        write_log("SelectedBlueprint.tpmplan missing or unavailable; run Prepare_Blueprint.py first");
        return 0;
    }
    DWORD got = 0;
    BOOL ok = ReadFile(f, write_plan_bytes, sizeof(write_plan_bytes), &got, 0);
    CloseHandle(f);
    if (!ok || !write_decode_selection(write_plan_bytes, got)) {
        write_log("Selected template plan is truncated, unsupported or invalid");
        return 0;
    }
    return 1;
}
