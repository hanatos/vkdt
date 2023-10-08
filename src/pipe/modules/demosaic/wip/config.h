// maxComputeWorkGroupInvocations = 1024 on 1650 max-q which means
// a max of 32x32 local size means max 64x32 tiles
// maybe we'll have more on better gpu?
#define DT_RCD_LOCAL_SIZE_X 32
#define DT_RCD_LOCAL_SIZE_Y 32
#define DT_RCD_TILE_SIZE_X 64
#define DT_RCD_TILE_SIZE_Y 32
#define DT_RCD_BORDER 3
