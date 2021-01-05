/*
lj92.c
(c) Andrew Baldwin 2014

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lj92.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

//#define SLOW_HUFF
//#define DEBUG

typedef struct _ljp {
    u8* data;
    u8* dataend;
    int datalen;
    int scanstart;
    int ix;
    int x; // Width
    int y; // Height
    int bits; // Bit depth
    int components;  // Components(Nf)
    int writelen; // Write rows this long
    int skiplen; // Skip this many values after each row
    u16* linearize; // Linearization table
    int linlen;
    int sssshist[16];

    // Huffman table - only one supported, and probably needed
#ifdef SLOW_HUFF
    int* maxcode;
    int* mincode;
    int* valptr;
    u8* huffval;
    int* huffsize;
    int* huffcode;
#else
    u16* hufflut;
    int huffbits;
#endif
    // Parse state
    int cnt;
    u32 b;
    u16* image;
    u16* rowcache;
    u16* outrow[2];
} ljp;

static int find(ljp* self) {
    int ix = self->ix;
    u8* data = self->data;
    while (data[ix] != 0xFF && ix<(self->datalen-1)) {
        ix += 1;
    }
    ix += 2;
    if (ix>=self->datalen) return -1;
    self->ix = ix;
    return data[ix-1];
}

#define BEH(ptr) ((((int)(*&ptr))<<8)|(*(&ptr+1)))

static int parseHuff(ljp* self) {
    int ret = LJ92_ERROR_CORRUPT;
    u8* huffhead = &self->data[self->ix]; // xstruct.unpack('>HB16B',self.data[self.ix:self.ix+19])
    u8* bits = &huffhead[2];
    bits[0] = 0; // Because table starts from 1
    int hufflen = BEH(huffhead[0]);
    if ((self->ix + hufflen) >= self->datalen) return ret;
#ifdef SLOW_HUFF
    u8* huffval = calloc(hufflen - 19,sizeof(u8));
    if (huffval == NULL) return LJ92_ERROR_NO_MEMORY;
    self->huffval = huffval;
    for (int hix=0;hix<(hufflen-19);hix++) {
        huffval[hix] = self->data[self->ix+19+hix];
#ifdef DEBUG
        printf("huffval[%d]=%d\n",hix,huffval[hix]);
#endif
    }
    self->ix += hufflen;
    // Generate huffman table
    int k = 0;
    int i = 1;
    int j = 1;
    int huffsize_needed = 1;
    // First calculate how long huffsize needs to be
    while (i<=16) {
        while (j<=bits[i]) {
            huffsize_needed++;
            k = k+1;
            j = j+1;
        }
        i = i+1;
        j = 1;
    }
    // Now allocate and do it
    int* huffsize = calloc(huffsize_needed,sizeof(int));
    if (huffsize == NULL) return LJ92_ERROR_NO_MEMORY;
    self->huffsize = huffsize;
    k = 0;
    i = 1;
    j = 1;
    // First calculate how long huffsize needs to be
    int hsix = 0;
    while (i<=16) {
        while (j<=bits[i]) {
            huffsize[hsix++] = i;
            k = k+1;
            j = j+1;
        }
        i = i+1;
        j = 1;
    }
    huffsize[hsix++] = 0;

    // Calculate the size of huffcode array
    int huffcode_needed = 0;
    k = 0;
    int code = 0;
    int si = huffsize[0];
    while (1) {
        while (huffsize[k] == si) {
            huffcode_needed++;
            code = code+1;
            k = k+1;
        }
        if (huffsize[k] == 0)
            break;
        while (huffsize[k] != si) {
            code = code << 1;
            si = si + 1;
        }
    }
    // Now fill it
    int* huffcode = calloc(huffcode_needed,sizeof(int));
    if (huffcode == NULL) return LJ92_ERROR_NO_MEMORY;
    self->huffcode = huffcode;
    int hcix = 0;
    k = 0;
    code = 0;
    si = huffsize[0];
    while (1) {
        while (huffsize[k] == si) {
            huffcode[hcix++] = code;
            code = code+1;
            k = k+1;
        }
        if (huffsize[k] == 0)
            break;
        while (huffsize[k] != si) {
            code = code << 1;
            si = si + 1;
        }
    }

    i = 0;
    j = 0;

    int* maxcode = calloc(17,sizeof(int));
    if (maxcode == NULL) return LJ92_ERROR_NO_MEMORY;
    self->maxcode = maxcode;
    int* mincode = calloc(17,sizeof(int));
    if (mincode == NULL) return LJ92_ERROR_NO_MEMORY;
    self->mincode = mincode;
    int* valptr = calloc(17,sizeof(int));
    if (valptr == NULL) return LJ92_ERROR_NO_MEMORY;
    self->valptr = valptr;

    while (1) {
        while (1) {
            i++;
            if (i>16)
                break;
            if (bits[i]!=0)
                break;
            maxcode[i] = -1;
        }
        if (i>16)
            break;
        valptr[i] = j;
        mincode[i] = huffcode[j];
        j = j+bits[i]-1;
        maxcode[i] = huffcode[j];
        j++;
    }
    free(huffsize);
    self->huffsize = NULL;
    free(huffcode);
    self->huffcode = NULL;
    ret = LJ92_ERROR_NONE;
#else
    /* Calculate huffman direct lut */
    // How many bits in the table - find highest entry
    u8* huffvals = &self->data[self->ix+19];
    int maxbits = 16;
    while (maxbits>0) {
        if (bits[maxbits]) break;
        maxbits--;
    }
    self->huffbits = maxbits;
    /* Now fill the lut */
    u16* hufflut = malloc((1<<maxbits) * sizeof(u16));
    if (hufflut == NULL) return LJ92_ERROR_NO_MEMORY;
    self->hufflut = hufflut;
    int i = 0;
    int hv = 0;
    int rv = 0;
    int vl = 0; // i
    int hcode;
    int bitsused = 1;
#ifdef DEBUG
    printf("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
#endif
    while (i<1<<maxbits) {
        if (bitsused>maxbits) {
            break; // Done. Should never get here!
        }
        if (vl >= bits[bitsused]) {
            bitsused++;
            vl = 0;
            continue;
        }
        if (rv == 1 << (maxbits-bitsused)) {
            rv = 0;
            vl++;
            hv++;
#ifdef DEBUG
            printf("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
#endif
            continue;
        }
        hcode = huffvals[hv];
        hufflut[i] = hcode<<8 | bitsused;
        //printf("%d %d %d\n",i,bitsused,hcode);
        i++;
        rv++;
    }
    ret = LJ92_ERROR_NONE;
#endif
    return ret;
}

static int parseSof3(ljp* self) {
    if (self->ix+6 >= self->datalen) return LJ92_ERROR_CORRUPT;
    self->y = BEH(self->data[self->ix+3]);
    self->x = BEH(self->data[self->ix+5]);
    self->bits = self->data[self->ix+2];
    self->components = self->data[self->ix + 7];
    self->ix += BEH(self->data[self->ix]);
    return LJ92_ERROR_NONE;
}

static int parseBlock(ljp* self) {
    self->ix += BEH(self->data[self->ix]);
    if (self->ix >= self->datalen) return LJ92_ERROR_CORRUPT;
    return LJ92_ERROR_NONE;
}

#ifdef SLOW_HUFF
static int nextbit(ljp* self) {
    u32 b = self->b;
    if (self->cnt == 0) {
        u8* data = &self->data[self->ix];
        u32 next = *data++;
        b = next;
        if (next == 0xff) {
            data++;
            self->ix++;
        }
        self->ix++;
        self->cnt = 8;
    }
    int bit = b >> 7;
    self->cnt--;
    self->b = (b << 1)&0xFF;
    return bit;
}

static int decode(ljp* self) {
    int i = 1;
    int code = nextbit(self);
    while (code > self->maxcode[i]) {
        i++;
        code = (code << 1) + nextbit(self);
    }
    int j = self->valptr[i];
    j = j + code - self->mincode[i];
    int value = self->huffval[j];
    return value;
}

static int receive(ljp* self,int ssss) {
    int i = 0;
    int v = 0;
    while (i != ssss) {
        i++;
        v = (v<<1) + nextbit(self);
    }
    return v;
}

static int extend(ljp* self,int v,int t) {
    int vt = 1<<(t-1);
    if (v < vt) {
        vt = (-1 << t) + 1;
        v = v + vt;
    }
    return v;
}
#endif

inline static int nextdiff(ljp* self) {
#ifdef SLOW_HUFF
    int t = decode(self);
    int diff = receive(self,t);
    diff = extend(self,diff,t);
#else
    u32 b = self->b;
    int cnt = self->cnt;
    int huffbits = self->huffbits;
    int ix = self->ix;
    int next;
    while (cnt < huffbits) {
        next = *(u16*)&self->data[ix];
        int one = next&0xFF;
        int two = next>>8;
        b = (b<<16)|(one<<8)|two;
        cnt += 16;
        ix += 2;
        if (one==0xFF) {
            //printf("%x %x %x %x %d\n",one,two,b,b>>8,cnt);
            b >>= 8;
            cnt -= 8;
        } else if (two==0xFF) ix++;
    }
    int index = b >> (cnt - huffbits);
    u16 ssssused = self->hufflut[index];
    int usedbits = ssssused&0xFF;
    int t = ssssused>>8;
    self->sssshist[t]++;
    cnt -= usedbits;
    int keepbitsmask = (1 << cnt)-1;
    b &= keepbitsmask;
    while (cnt < t) {
        next = *(u16*)&self->data[ix];
        int one = next&0xFF;
        int two = next>>8;
        b = (b<<16)|(one<<8)|two;
        cnt += 16;
        ix += 2;
        if (one==0xFF) {
            b >>= 8;
            cnt -= 8;
        } else if (two==0xFF) ix++;
    }
    cnt -= t;
    int diff = b >> cnt;
    int vt = 1<<(t-1);
    if (diff < vt) {
        vt = (-1 << t) + 1;
        diff += vt;
    }
    keepbitsmask = (1 << cnt)-1;
    self->b = b & keepbitsmask;
    self->cnt = cnt;
    self->ix = ix;
    //printf("%d %d\n",t,diff);
#ifdef DEBUG
#endif
#endif
    return diff;
}

static int parsePred6(ljp* self) {
    int ret = LJ92_ERROR_CORRUPT;
    self->ix = self->scanstart;
    //int compcount = self->data[self->ix+2];
    self->ix += BEH(self->data[self->ix]);
    self->cnt = 0;
    self->b = 0;
    int write = self->writelen;
    // Now need to decode huffman coded values
    int c = 0;
    int pixels = self->y * self->x;
    u16* out = self->image;
    u16* temprow;
    u16* thisrow = self->outrow[0];
    u16* lastrow = self->outrow[1];

    // First pixel predicted from base value
    int diff;
    int Px;
    int col = 0;
    int row = 0;
    int left = 0;
    int linear;

    // First pixel
    diff = nextdiff(self);
    Px = 1 << (self->bits-1);
    left = Px + diff;
    left = (u16) (left%65536);
    if (self->linearize)
        linear = self->linearize[left];
    else
        linear = left;
    thisrow[col++] = left;
    out[c++] = linear;
    if (self->ix >= self->datalen) return ret;
    --write;
    int rowcount = self->x-1;
    while (rowcount--) {
        diff = nextdiff(self);
        Px = left;
        left = Px + diff;
        left = (u16) (left%65536);
        if (self->linearize)
            linear = self->linearize[left];
        else
            linear = left;
        thisrow[col++] = left;
        out[c++] = linear;
        //printf("%d %d %d %d %x\n",col-1,diff,left,thisrow[col-1],&thisrow[col-1]);
        if (self->ix >= self->datalen) return ret;
        if (--write==0) {
            out += self->skiplen;
            write = self->writelen;
        }
    }
    temprow = lastrow;
    lastrow = thisrow;
    thisrow = temprow;
    row++;
    //printf("%x %x\n",thisrow,lastrow);
    while (c<pixels) {
        col = 0;
        diff = nextdiff(self);
        Px = lastrow[col]; // Use value above for first pixel in row
        left = Px + diff;
        left = (u16) (left%65536);
        if (self->linearize) {
            if (left>self->linlen) return LJ92_ERROR_CORRUPT;
            linear = self->linearize[left];
        } else
            linear = left;
        thisrow[col++] = left;
        //printf("%d %d %d %d\n",col,diff,left,lastrow[col]);
        out[c++] = linear;
        if (self->ix >= self->datalen) break;
        rowcount = self->x-1;
        if (--write==0) {
            out += self->skiplen;
            write = self->writelen;
        }
        while (rowcount--) {
            diff = nextdiff(self);
            Px = lastrow[col] + ((left - lastrow[col-1])>>1);
            left = Px + diff;
            left = (u16) (left%65536);
            //printf("%d %d %d %d %d %x\n",col,diff,left,lastrow[col],lastrow[col-1],&lastrow[col]);
            if (self->linearize) {
                if (left>self->linlen) return LJ92_ERROR_CORRUPT;
                linear = self->linearize[left];
            } else
                linear = left;
            thisrow[col++] = left;
            out[c++] = linear;
            if (--write==0) {
                out += self->skiplen;
                write = self->writelen;
            }
        }
        temprow = lastrow;
        lastrow = thisrow;
        thisrow = temprow;
        if (self->ix >= self->datalen) break;
    }
    if (c >= pixels) ret = LJ92_ERROR_NONE;
    return ret;
}

static int parseScan(ljp* self) {
    int ret = LJ92_ERROR_CORRUPT;
    memset(self->sssshist,0,sizeof(self->sssshist));
    self->ix = self->scanstart;
    int compcount = self->data[self->ix+2];
    int pred = self->data[self->ix+3+2*compcount];
    if (pred<0 || pred>7) return ret;
    if (pred==6) return parsePred6(self); // Fast path
    self->ix += BEH(self->data[self->ix]);
    self->cnt = 0;
    self->b = 0;
    u16* out = self->image;
    u16* thisrow = self->outrow[0];
    u16* lastrow = self->outrow[1];

    // First pixel predicted from base value
    int diff;
    int Px = 0;
    int left = 0;
    for (int row = 0; row < self->y; row++) {
        for (int col = 0; col < self->x; col++) {
            int colx = col * self->components;
            for (int c = 0; c < self->components; c++) {
                if ((col==0)&&(row==0)) {
                    Px = 1 << (self->bits-1);
                } else if (row==0) {
                    // Px = left;
                    Px = thisrow[(col - 1) * self->components + c];
                } else if (col==0) {
                    Px = lastrow[c];  // Use value above for first pixel in row
                } else {
                    int prev_colx = (col - 1) * self->components;
   
                    switch (pred) {
                        case 0:
                          Px = 0;
                          break;  // No prediction... should not be used
                        case 1:
                          Px = thisrow[prev_colx + c];
                          break;
                        case 2:
                          Px = lastrow[colx + c];
                          break;
                        case 3:
                          Px = lastrow[prev_colx + c];
                          break;
                        case 4:
                          Px = left + lastrow[colx + c] - lastrow[prev_colx + c];
                          break;
                        case 5:
                          Px = left + ((lastrow[colx + c] - lastrow[prev_colx + c]) >> 1);
                          break;
                        case 6:
                          Px = lastrow[colx + c] + ((left - lastrow[prev_colx + c]) >> 1);
                          break;
                        case 7:
                          Px = (left + lastrow[colx + c]) >> 1;
                          break;
                    }
                }
                
                diff = nextdiff(self);
                left = Px + diff;
                left = (u16) (left%65536);
                //printf("%d %d %d\n",c,diff,left);
                int linear;
                if (self->linearize) {
                    if (left>self->linlen) return LJ92_ERROR_CORRUPT;
                    linear = self->linearize[left];
                } else
                    linear = left;

                thisrow[colx + c] = left;
                out[colx + c] = linear; // HACK
            } // c
        } // col

        u16* temprow = lastrow;
        lastrow = thisrow;
        thisrow = temprow;

        out += self->x * self->components + self->skiplen;
    } // row

    ret = LJ92_ERROR_NONE;
    return ret;
}

static int parseImage(ljp* self) {
    int ret = LJ92_ERROR_NONE;
    while (1) {
        int nextMarker = find(self);
        if (nextMarker == 0xc4)
            ret = parseHuff(self);
        else if (nextMarker == 0xc3)
            ret = parseSof3(self);
        else if (nextMarker == 0xfe)// Comment
            ret = parseBlock(self);
        else if (nextMarker == 0xd9) // End of image
            break;
        else if (nextMarker == 0xda) {
            self->scanstart = self->ix;
            ret = LJ92_ERROR_NONE;
            break;
        } else if (nextMarker == -1) {
            ret = LJ92_ERROR_CORRUPT;
            break;
        } else
            ret = parseBlock(self);
        if (ret != LJ92_ERROR_NONE) break;
    }
    return ret;
}

static int findSoI(ljp* self) {
    int ret = LJ92_ERROR_CORRUPT;
    if (find(self)==0xd8)
        ret = parseImage(self);
    return ret;
}

static void free_memory(ljp* self) {
#ifdef SLOW_HUFF
    free(self->maxcode);
    self->maxcode = NULL;
    free(self->mincode);
    self->mincode = NULL;
    free(self->valptr);
    self->valptr = NULL;
    free(self->huffval);
    self->huffval = NULL;
    free(self->huffsize);
    self->huffsize = NULL;
    free(self->huffcode);
    self->huffcode = NULL;
#else
    free(self->hufflut);
    self->hufflut = NULL;
#endif
    free(self->rowcache);
    self->rowcache = NULL;
}

int lj92_open(lj92* lj,
              uint8_t* data, int datalen,
              int* width,int* height, int* bitdepth, int* components) {
    ljp* self = (ljp*)calloc(sizeof(ljp),1);
    if (self==NULL) return LJ92_ERROR_NO_MEMORY;

    self->data = (u8*)data;
    self->dataend = self->data + datalen;
    self->datalen = datalen;

    int ret = findSoI(self);

    if (ret == LJ92_ERROR_NONE) {
        u16* rowcache = (u16*)calloc(self->x * self->components * 2, sizeof(u16));
        if (rowcache == NULL) ret = LJ92_ERROR_NO_MEMORY;
        else {
            self->rowcache = rowcache;
            self->outrow[0] = rowcache;
            self->outrow[1] = &rowcache[self->x];
        }
    }

    if (ret != LJ92_ERROR_NONE) { // Failed, clean up
        *lj = NULL;
        free_memory(self);
        free(self);
    } else {
        *width = self->x;
        *height = self->y;
        *bitdepth = self->bits;
        *components = self->components;
        *lj = self;
    }
    return ret;
}

int lj92_decode(lj92 lj,
                uint16_t* target,int writeLength, int skipLength,
                uint16_t* linearize,int linearizeLength) {
    int ret = LJ92_ERROR_NONE;
    ljp* self = lj;
    if (self == NULL) return LJ92_ERROR_BAD_HANDLE;
    self->image = target;
    self->writelen = writeLength;
    self->skiplen = skipLength;
    self->linearize = linearize;
    self->linlen = linearizeLength;
    ret = parseScan(self);
    return ret;
}

void lj92_close(lj92 lj) {
    ljp* self = lj;
    if (self != NULL)
        free_memory(self);
    free(self);
}

/* Encoder implementation */

typedef struct _lje {
    uint16_t* image;
    int width;
    int height;
    int bitdepth;
    int readLength;
    int skipLength;
    uint16_t* delinearize;
    int delinearizeLength;
    uint8_t* encoded;
    int encodedWritten;
    int encodedLength;
    int hist[18]; // SSSS frequency histogram
    int bits[18];
    int huffval[18];
    u16 huffenc[18];
    u16 huffbits[18];
    int huffsym[18];
} lje;

int frequencyScan(lje* self) {
    // Scan through the tile using the standard type 6 prediction
    // Need to cache the previous 2 row in target coordinates because of tiling
    uint16_t* pixel = self->image;
    int pixcount = self->width*self->height;
    int scan = self->readLength;
    uint16_t* rowcache = (uint16_t*)calloc(1,self->width*4);
    uint16_t* rows[2];
    rows[0] = rowcache;
    rows[1] = &rowcache[self->width];

    int col = 0;
    int row = 0;
    int Px = 0;
    int32_t diff = 0;
    // int maxval = (1 << self->bitdepth);
    while (pixcount--) {
        uint16_t p = *pixel;
        /* if (self->delinearize) {
            if (p>=self->delinearizeLength) {
                free(rowcache);
                return LJ92_ERROR_TOO_WIDE;
            }
            p = self->delinearize[p];
        }
        if (p>=maxval) {
            free(rowcache);
            return LJ92_ERROR_TOO_WIDE;
        } */
        rows[1][col] = p;

        if ((row == 0)&&(col == 0))
            Px = 1 << (self->bitdepth-1);
        else if (row == 0)
            Px = rows[1][col-1];
        else if (col == 0)
            Px = rows[0][col];
        else
            Px = rows[0][col] + ((rows[1][col-1] - rows[0][col-1])>>1);
        diff = rows[1][col] - Px;
        diff = diff%65536;
        int ssss = 32 - __builtin_clz(abs(diff));
        if (diff==0) ssss=0;
        self->hist[ssss]++;
        //printf("%d %d %d %d %d %d\n",col,row,p,Px,diff,ssss);
        pixel++;
        scan--;
        col++;
        if (scan==0) { pixel += self->skipLength; scan = self->readLength; }
        if (col==self->width) {
            uint16_t* tmprow = rows[1];
            rows[1] = rows[0];
            rows[0] = tmprow;
            col=0;
            row++;
        }
    }
#ifdef DEBUG
    int sort[17];
    for (int h=0;h<17;h++) {
        sort[h] = h;
        printf("%d:%d\n",h,self->hist[h]);
    }
#endif
    free(rowcache);
    return LJ92_ERROR_NONE;
}

void createEncodeTable(lje* self) {
    float freq[18];
    int codesize[18];
    int others[18];

    // Calculate frequencies
    float totalpixels = self->width * self->height;
    for (int i=0;i<17;i++) {
        freq[i] = (float)(self->hist[i])/totalpixels;
#ifdef DEBUG
        printf("%d:%f\n",i,freq[i]);
#endif
        codesize[i] = 0;
        others[i] = -1;
    }
    codesize[17] = 0;
    others[17] = -1;
    freq[17] = 1.0f;

    float v1f,v2f;
    int v1,v2;

    while (1) {
        v1f=3.0f;
        v1=-1;
        for (int i=0;i<18;i++) {
            if ((freq[i]<=v1f) && (freq[i]>0.0f)) {
                v1f = freq[i];
                v1 = i;
            }
        }
#ifdef DEBUG
        printf("v1:%d,%f\n",v1,v1f);
#endif
        v2f=3.0f;
        v2=-1;
        for (int i=0;i<18;i++) {
            if (i==v1) continue;
            if ((freq[i]<v2f) && (freq[i]>0.0f)) {
                v2f = freq[i];
                v2 = i;
            }
        }
        if (v2==-1) break; // Done

        freq[v1] += freq[v2];
        freq[v2] = 0.0f;

        while (1) {
            codesize[v1]++;
            if (others[v1]==-1) break;
            v1 = others[v1];
        }
        others[v1] = v2;
        while (1) {
            codesize[v2]++;
            if (others[v2]==-1) break;
            v2 = others[v2];
        }
    }
    int* bits = self->bits;
    memset(bits,0,sizeof(self->bits));
    for (int i=0;i<18;i++) {
        if (codesize[i]!=0) {
            bits[codesize[i]]++;
        }
    }
#ifdef DEBUG
    for (int i=0;i<18;i++) {
        printf("bits:%d,%d,%d\n",i,bits[i],codesize[i]);
    }
#endif
    //adjust bits, this step is a must to remove a code with all ones
    //and fix bug of overriding SSSS-0 category with the code with all ones.
    int I = 17;
    while(1) {
        if(bits[I] > 0) {
            int J = I - 1;
            do {
                J = J - 1;
            } while(bits[J] <= 0);
            bits[I] = bits[I] - 2;
            bits[I - 1] = bits[I - 1] + 1; 
            bits[J + 1] = bits[J + 1] + 2;
            bits[J] = bits [J] - 1;
        } else {
            I = I - 1;
            if(I != 16) {
                continue;
            }
            while(bits[I] == 0) {
                I = I - 1;
            }
            bits[I] = bits[I] - 1;
            break;
        }  
    }
#ifdef DEBUG
for (int i=0;i<18;i++) {
    printf("Adjusted bits:%d,%d,%d\n",i,bits[i],codesize[i]);
}
#endif
    int* huffval = self->huffval;
    int i=1;
    int k=0;
    int j;
    memset(huffval,0,sizeof(self->huffval));
    while (i<=32) {
        j=0;
        while (j<17) {
            if (codesize[j]==i) {
                huffval[k++] = j;
            }
            j++;
        }
        i++;
    }
#ifdef DEBUG
    for (i=0;i<18;i++) {
        printf("i=%d,huffval[i]=%x\n",i,huffval[i]);
    }
#endif
    int maxbits = 16;
    while (maxbits>0) {
        if (bits[maxbits]) break;
        maxbits--;
    }
    u16* huffenc = self->huffenc;
    u16* huffbits = self->huffbits;
    int* huffsym = self->huffsym;
    memset(huffenc,0,sizeof(self->huffenc));
    memset(huffbits,0,sizeof(self->huffbits));
    memset(self->huffsym,0,sizeof(self->huffsym));
    i = 0;
    int hv = 0;
    int rv = 0;
    int vl = 0; // i
    //int hcode;
    int bitsused = 1;
    int sym = 0;
    //printf("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
    while (i<1<<maxbits) {
        if (bitsused>maxbits) {
            break; // Done. Should never get here!
        }
        if (vl >= bits[bitsused]) {
            bitsused++;
            vl = 0;
            continue;
        }
        if (rv == 1 << (maxbits-bitsused)) {
            rv = 0;
            vl++;
            hv++;
            //printf("%04x:%x:%d:%x\n",i,huffvals[hv],bitsused,1<<(maxbits-bitsused));
            continue;
        }
        huffbits[sym] = bitsused;
        huffenc[sym++] = i>>(maxbits-bitsused);
        //printf("%d %d %d\n",i,bitsused,hcode);
        i+= (1<<(maxbits-bitsused));
        rv = 1<<(maxbits-bitsused);
    }
    for (i=0;i<18;i++) {
        if (huffbits[i]>0) {
            huffsym[huffval[i]] = i;
        }
#ifdef DEBUG
        printf("huffval[%d]=%d,huffenc[%d]=%x,bits=%d\n",i,huffval[i],i,huffenc[i],huffbits[i]);
#endif
        if (huffbits[i]>0) {
            huffsym[huffval[i]] = i;
        }
    }
#ifdef DEBUG
    for (i=0;i<18;i++) {
        printf("huffsym[%d]=%d\n",i,huffsym[i]);
    }
#endif
}

void writeHeader(lje* self) {
    int w = self->encodedWritten;
    uint8_t* e = self->encoded;
    e[w++] = 0xff; e[w++] = 0xd8; //SOI
    e[w++] = 0xff; e[w++] = 0xc4; //HUFF
    // Write HUFF
        int count = 0;
        for (int i=0;i<17;i++) {
            count += self->bits[i];
        }
        e[w++] = 0x0; e[w++] = 17+2+count; //Lf, frame header length
        e[w++] = 0; // Table ID
        for (int i=1;i<17;i++) {
            e[w++] = self->bits[i];
        }
        for (int i=0;i<count;i++) {
            e[w++] = self->huffval[i];
        }
    e[w++] = 0xff; e[w++] = 0xc3; //SOF3
        // Write SOF
        e[w++] = 0x0; e[w++] = 11; //Lf, frame header length
        e[w++] = self->bitdepth;
        e[w++] = self->height>>8; e[w++] = self->height&0xFF;
        e[w++] = self->width>>8; e[w++] = self->width&0xFF;
        e[w++] = 1; // Components
        e[w++] = 0; // Component ID
        e[w++] = 0x11; // Component X/Y
        e[w++] = 0; // Unused (Quantisation)
    e[w++] = 0xff; e[w++] = 0xda; //SCAN
    // Write SCAN
        e[w++] = 0x0; e[w++] = 8; //Ls, scan header length
        e[w++] = 1; // Components
        e[w++] = 0; //
        e[w++] = 0; //
        e[w++] = 6; // Predictor
        e[w++] = 0; //
        e[w++] = 0; //
    self->encodedWritten = w;
}

void writePost(lje* self) {
    int w = self->encodedWritten;
    uint8_t* e = self->encoded;
    e[w++] = 0xff; e[w++] = 0xd9; //EOI
    self->encodedWritten = w;
}

int writeBody(lje* self) {
    // Scan through the tile using the standard type 6 prediction
    // Need to cache the previous 2 row in target coordinates because of tiling
    uint16_t* pixel = self->image;
    int pixcount = self->width*self->height;
    int scan = self->readLength;
    uint16_t* rowcache = (uint16_t*)calloc(1,self->width*4);
    uint16_t* rows[2];
    rows[0] = rowcache;
    rows[1] = &rowcache[self->width];

    int col = 0;
    int row = 0;
    int Px = 0;
    int32_t diff = 0;
    int bitcount = 0;
    uint8_t* out = self->encoded;
    int w = self->encodedWritten;
    uint8_t next = 0;
    uint8_t nextbits = 8;
    while (pixcount--) {
        uint16_t p = *pixel;
        if (self->delinearize) p = self->delinearize[p];
        rows[1][col] = p;

        if ((row == 0)&&(col == 0))
            Px = 1 << (self->bitdepth-1);
        else if (row == 0)
            Px = rows[1][col-1];
        else if (col == 0)
            Px = rows[0][col];
        else
            Px = rows[0][col] + ((rows[1][col-1] - rows[0][col-1])>>1);
        diff = rows[1][col] - Px;
        diff = diff%65536;
        int ssss = 32 - __builtin_clz(abs(diff));
        if (diff==0) ssss=0;
        //printf("%d %d %d %d %d\n",col,row,Px,diff,ssss);

        // Write the huffman code for the ssss value
        int huffcode = self->huffsym[ssss];
        int huffenc = self->huffenc[huffcode];
        int huffbits = self->huffbits[huffcode];
        bitcount += huffbits + ssss;

        int vt = ssss>0?(1<<(ssss-1)):0;
        //printf("%d %d %d %d\n",rows[1][col],Px,diff,Px+diff);
#ifdef DEBUG
#endif
        if (diff < vt)
            diff += (1 << (ssss))-1;

        // Write the ssss
        while (huffbits>0) {
            int usebits = huffbits>nextbits?nextbits:huffbits;
            // Add top usebits from huffval to next usebits of nextbits
            int tophuff = huffenc >> (huffbits - usebits);
            next |= (tophuff << (nextbits-usebits));
            nextbits -= usebits;
            huffbits -= usebits;
            huffenc &= (1<<huffbits)-1;
            if (nextbits==0) {
                if(w >= self->encodedLength - 1)
                {
                    free(rowcache);
                    return LJ92_ERROR_ENCODER;
                }
                out[w++] = next;
                if (next==0xff) out[w++] = 0x0;
                next = 0;
                nextbits = 8;
            }
        }
        // Write the rest of the bits for the value

        while (ssss>0) {
            int usebits = ssss>nextbits?nextbits:ssss;
            // Add top usebits from huffval to next usebits of nextbits
            int tophuff = diff >> (ssss - usebits);
            next |= (tophuff << (nextbits-usebits));
            nextbits -= usebits;
            ssss -= usebits;
            diff &= (1<<ssss)-1;
            if (nextbits==0) {
                if(w >= self->encodedLength - 1)
                {
                    free(rowcache);
                    return LJ92_ERROR_ENCODER;
                }
                out[w++] = next;
                if (next==0xff) out[w++] = 0x0;
                next = 0;
                nextbits = 8;
            }
        }

        //printf("%d %d\n",diff,ssss);
        pixel++;
        scan--;
        col++;
        if (scan==0) { pixel += self->skipLength; scan = self->readLength; }
        if (col==self->width) {
            uint16_t* tmprow = rows[1];
            rows[1] = rows[0];
            rows[0] = tmprow;
            col=0;
            row++;
        }
    }
    // Flush the final bits
    if (nextbits<8) {
        out[w++] = next;
        if (next==0xff) out[w++] = 0x0;
    }
#ifdef DEBUG
    int sort[18];
    for (int h=0;h<18;h++) {
        sort[h] = h;
        printf("%d:%d\n",h,self->hist[h]);
    }
    printf("Total bytes: %d\n",bitcount>>3);
#endif
    free(rowcache);
    self->encodedWritten = w;
    return LJ92_ERROR_NONE;
}
/* Encoder
 * Read tile from an image and encode in one shot
 * Return the encoded data
 */
int lj92_encode(uint16_t* image, int width, int height, int bitdepth,
                int readLength, int skipLength,
                uint16_t* delinearize,int delinearizeLength,
                uint8_t** encoded, int* encodedLength) {
    int ret = LJ92_ERROR_NONE;

    lje* self = (lje*)calloc(sizeof(lje),1);
    if (self==NULL) return LJ92_ERROR_NO_MEMORY;
    self->image = image;
    self->width = width;
    self->height = height;
    self->bitdepth = bitdepth;
    self->readLength = readLength;
    self->skipLength = skipLength;
    self->delinearize = delinearize;
    self->delinearizeLength = delinearizeLength;
    self->encodedLength = width*height*3+200;
    self->encoded = malloc(self->encodedLength);
    if (self->encoded==NULL) { free(self); return LJ92_ERROR_NO_MEMORY; }
    // Scan through data to gather frequencies of ssss prefixes
    ret = frequencyScan(self);
    if (ret != LJ92_ERROR_NONE) {
        free(self->encoded);
        free(self);
        return ret;
    }
    // Create encoded table based on frequencies
    createEncodeTable(self);
    // Write JPEG head and scan header
    writeHeader(self);
    // Scan through and do the compression
    ret = writeBody(self);
    if (ret != LJ92_ERROR_NONE) {
        free(self->encoded);
        free(self);
        return ret;
    }
    // Finish
    writePost(self);
#ifdef DEBUG
    printf("written:%d\n",self->encodedWritten);
#endif
    self->encoded = realloc(self->encoded,self->encodedWritten);
    self->encodedLength = self->encodedWritten;
    *encoded = self->encoded;
    *encodedLength = self->encodedLength;

    free(self);

    return ret;
}


