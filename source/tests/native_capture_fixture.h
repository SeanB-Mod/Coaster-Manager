/* Generated from the validated seven-support schema-2 capture. */
static unsigned long long write_source_id=28044ULL;
#define WRITE_SOURCE_ID write_source_id
static long long write_station_definition_id;
static long long write_normal_id=-115856756LL;
#define WRITE_NORMAL_ID write_normal_id
#define WRITE_MAX_SUPPORTS 512
static int write_count=7;
#define WRITE_SNAPSHOT_HASH 0xd2d4a218U
#define WRITE_REVERT_HASH 0xd29e9f1dU
static float write_source_position[3]={46.5f,0.0f,-21.0f};
static unsigned write_source_rotation[4]={0x80000000U,0x3ec3ef16U,0x80000000U,0x3f6c835eU};
typedef struct {unsigned position[3],rotation[4],angle;int height;unsigned modifiers[3];long long piece,decoration;unsigned decorationPosition,reserved;} WriteFixture;
typedef char WriteFixtureSizeCheck[sizeof(WriteFixture)==72?1:-1];
static WriteFixture write_fixture[WRITE_MAX_SUPPORTS]={
 {{0x426a0000U,0x00000000U,0xc0f00000U},{0x80000000U,0x3ec3ef16U,0x80000000U,0x3f6c835eU},0x42340000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x428f0000U,0x00000000U,0xc1c40000U},{0x80000000U,0x3f6c835fU,0x80000000U,0xbec3ef14U},0x43610000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x42850000U,0x00000000U,0xc2000000U},{0x80000000U,0x3f800000U,0x80000000U,0xb33bbd2eU},0x43340000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x42760000U,0x00000000U,0xc2280000U},{0x80000000U,0x3f3504f4U,0x80000000U,0xbf3504f4U},0x43870000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x42500000U,0x00000000U,0xc2400000U},{0x80000000U,0x3f800000U,0x80000000U,0xb33bbd2eU},0x43340000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x42280000U,0x00000000U,0xc2320000U},{0x80000000U,0x3ec3ef17U,0x80000000U,0xbf6c835eU},0x439d8000U,1,{0,0,0},-115856756LL,0,0,0},
 {{0x41f80000U,0x00000000U,0xc2060000U},{0x80000000U,0x3ec3ef16U,0x80000000U,0x3f6c835eU},0x42340000U,1,{0,0,0},-115856756LL,0,0,0},
};
