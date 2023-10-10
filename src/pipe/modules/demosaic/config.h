// maxComputeWorkGroupInvocations = 1024 on 1650 max-q as well as 3080Ti which
// means a max of 32x32 local size means max 64x32 tiles
// (we could do manual iteration doubling the latency, not sure if worth it)
#define DT_RCD_LOCAL_SIZE_X 32
#define DT_RCD_LOCAL_SIZE_Y 32
#define DT_RCD_TILE_SIZE_X 64
#define DT_RCD_TILE_SIZE_Y 32
#define DT_RCD_BORDER 3
