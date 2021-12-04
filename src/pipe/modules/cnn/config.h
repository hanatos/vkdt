// tweaked for performance on gtx1650 maxq (bad idea).
// comes out at 217ms (shmem) vs 225ms (microkernels) @1080p
#define DT_CNN_TILE_WD 24
#define DT_CNN_TILE_HT 24
#define DT_CNN_BORDER  2
