// tweaked for performance on gtx1650 maxq (bad idea).
// comes out at 217ms (shmem) vs 225ms (microkernels) @1080p
// (goes down to 185 with only a border of 1.. :/ )
// 2080Ti: 20 56ms, 32 200ms :(
// 24 1border 50ms
// 20 1border 44ms
#define DT_CNN_TILE_WD 20
#define DT_CNN_TILE_HT 20
#define DT_CNN_BORDER  2
