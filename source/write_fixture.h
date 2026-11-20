/* Runtime import selection; populated only by a validated library plan. */
#ifdef TPM_TEST_FIXTURE
#include "tests/native_capture_fixture.h"
#else
static unsigned long long write_source_id;
#define WRITE_SOURCE_ID write_source_id
static long long write_station_definition_id;
static long long write_normal_id;
#define WRITE_NORMAL_ID write_normal_id
#define WRITE_MAX_SUPPORTS 512
static int write_count;
#define WRITE_SNAPSHOT_HASH 0xd2d4a218U
#define WRITE_REVERT_HASH 0xd29e9f1dU
static float write_source_position[3];
static unsigned write_source_rotation[4] = {0, 0, 0, 0x3f800000U};
typedef struct {
    unsigned position[3], rotation[4], angle;
    int height;
    unsigned modifiers[3];
    long long piece, decoration;
    unsigned decorationPosition, reserved;
} WriteFixture;
typedef char WriteFixtureSizeCheck[sizeof(WriteFixture) == 72 ? 1 : -1];
static WriteFixture write_fixture[WRITE_MAX_SUPPORTS];
#endif
