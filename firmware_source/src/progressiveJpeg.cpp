#include "progressiveJpeg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

const uint8_t kZigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63};

struct Huff {
    bool defined = false;
    uint8_t vals[256];
    int maxcode[18];
    int valptr[17];
    int mincode[17];
};

struct Comp {
    int id = 0, h = 1, v = 1, tq = 0;
};

// Bit reader with JPEG byte stuffing (FF 00) and marker detection
struct BitReader {
    const uint8_t *p = nullptr;
    const uint8_t *end = nullptr;
    uint32_t buf = 0;
    int cnt = 0;
    bool hitMarker = false;

    void reset() { buf = 0; cnt = 0; hitMarker = false; }
    void fill() {
        while (cnt <= 24) {
            uint32_t b = 0;
            if (!hitMarker && p < end) {
                b = *p++;
                if (b == 0xFF) {
                    while (p < end && *p == 0xFF) p++;       // fill bytes
                    if (p < end && *p == 0x00) { p++; }       // stuffed 0xFF
                    else { hitMarker = true; p--; b = 0; }    // a marker: stay in front of it, feed zeros
                }
            }
            buf |= b << (24 - cnt);
            cnt += 8;
        }
    }
    inline int bit() {
        if (cnt < 1) fill();
        int r = (int)(buf >> 31);
        buf <<= 1;
        cnt--;
        return r;
    }
    inline int bits(int n) {   // n <= 16
        if (n == 0) return 0;
        if (cnt < n) fill();
        int r = (int)(buf >> (32 - n));
        buf <<= n;
        cnt -= n;
        return r;
    }
};

inline int extend(int v, int t) { return v < (1 << (t - 1)) ? v - (1 << t) + 1 : v; }

inline int huffDecode(BitReader &br, const Huff &h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | br.bit();
        if (h.maxcode[l] >= 0 && code <= h.maxcode[l] && code >= h.mincode[l]) {
            return h.vals[h.valptr[l] + code - h.mincode[l]];
        }
    }
    return -1;
}

// One luma block: the lowest N x N coefficients plus one "is non-zero" flag for each of the 64 coefficients
// (needed to follow the refinement scans correctly).
struct Block {
    uint64_t nz;
    // int16_t lo[N*N] follows
};

struct Decoder {
    const uint8_t *data;
    size_t size;
    int N;                // kept frequencies per axis (and output pixels per block axis)
    size_t blockBytes;

    int width = 0, height = 0, ncomp = 0;
    Comp comp[4];
    int hmax = 1, vmax = 1;
    int mcusX = 0, mcusY = 0;
    int bwPad = 0, bhPad = 0;      // luma block grid (padded to whole MCUs)
    int bwReal = 0, bhReal = 0;    // luma blocks that really exist (non-interleaved scans)
    uint16_t quant[4][64];         // natural order
    bool quantDefined[4] = {false, false, false, false};
    Huff dc[4], ac[4];
    int restartInterval = 0;
    bool progressive = false;
    bool haveFrame = false;
    uint8_t *blocks = nullptr;

    inline uint8_t *blockAt(int bx, int by) { return blocks + ((size_t)by * bwPad + bx) * blockBytes; }

    static void buildHuff(Huff &h, const uint8_t *counts, const uint8_t *vals, int nvals) {
        memset(h.vals, 0, sizeof(h.vals));
        memcpy(h.vals, vals, nvals);
        int code = 0, k = 0;
        for (int l = 1; l <= 16; l++) {
            h.valptr[l] = k;
            h.mincode[l] = code;
            code += counts[l - 1];
            k += counts[l - 1];
            h.maxcode[l] = counts[l - 1] ? code - 1 : -1;
            code <<= 1;
        }
        h.defined = true;
    }

    bool parseDHT(const uint8_t *p, int len) {
        while (len >= 17) {
            int tc = p[0] >> 4, th = p[0] & 15;
            const uint8_t *counts = p + 1;
            int n = 0;
            for (int i = 0; i < 16; i++) n += counts[i];
            if (n > 256 || len < 17 + n || th > 3 || tc > 1) return false;
            buildHuff(tc ? ac[th] : dc[th], counts, p + 17, n);
            p += 17 + n;
            len -= 17 + n;
        }
        return true;
    }

    bool parseDQT(const uint8_t *p, int len) {
        while (len >= 65) {
            int pq = p[0] >> 4, tq = p[0] & 15;
            if (tq > 3) return false;
            p++; len--;
            for (int i = 0; i < 64; i++) {
                int v;
                if (pq) { if (len < 2) return false; v = (p[0] << 8) | p[1]; p += 2; len -= 2; }
                else { if (len < 1) return false; v = p[0]; p++; len--; }
                quant[tq][kZigzag[i]] = (uint16_t)v;
            }
            quantDefined[tq] = true;
        }
        return true;
    }

    bool parseSOF(const uint8_t *p, int len) {
        if (len < 6 || p[0] != 8) return false;
        height = (p[1] << 8) | p[2];
        width = (p[3] << 8) | p[4];
        ncomp = p[5];
        if (width <= 0 || height <= 0 || ncomp < 1 || ncomp > 4 || len < 6 + 3 * ncomp) return false;
        hmax = vmax = 1;
        for (int i = 0; i < ncomp; i++) {
            comp[i].id = p[6 + 3 * i];
            comp[i].h = p[7 + 3 * i] >> 4;
            comp[i].v = p[7 + 3 * i] & 15;
            comp[i].tq = p[8 + 3 * i] & 3;
            if (comp[i].h < 1 || comp[i].h > 4 || comp[i].v < 1 || comp[i].v > 4) return false;
            if (comp[i].h > hmax) hmax = comp[i].h;
            if (comp[i].v > vmax) vmax = comp[i].v;
        }
        mcusX = (width + 8 * hmax - 1) / (8 * hmax);
        mcusY = (height + 8 * vmax - 1) / (8 * vmax);
        bwPad = mcusX * comp[0].h;
        bhPad = mcusY * comp[0].v;
        int cw = (width * comp[0].h + hmax - 1) / hmax;
        int ch = (height * comp[0].v + vmax - 1) / vmax;
        bwReal = (cw + 7) / 8;
        bhReal = (ch + 7) / 8;
        haveFrame = true;
        return true;
    }
};

// ---- finding markers ---------------------------------------------------------------------------------------------
const uint8_t *nextMarker(const uint8_t *p, const uint8_t *end) {
    while (p + 1 < end) {
        if (p[0] == 0xFF && p[1] != 0x00 && p[1] != 0xFF && !(p[1] >= 0xD0 && p[1] <= 0xD7)) return p;
        p++;
    }
    return end;
}

bool scanHeaders(const uint8_t *data, size_t size, Decoder *dec, ProgressiveJpegInfo *info) {
    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) return false;
    const uint8_t *p = data + 2, *end = data + size;
    while (p + 4 <= end) {
        if (p[0] != 0xFF) { p++; continue; }
        int m = p[1];
        if (m == 0xFF) { p++; continue; }
        if (m == 0x00 || m == 0x01 || (m >= 0xD0 && m <= 0xD8)) { p += 2; continue; }
        if (m == 0xD9) break;
        int len = (p[2] << 8) | p[3];
        if (len < 2 || p + 2 + len > end) return false;
        const uint8_t *body = p + 4;
        int blen = len - 2;
        if (m == 0xC0 || m == 0xC1 || m == 0xC2) {
            Decoder tmp;
            Decoder &d = dec ? *dec : tmp;
            if (!d.parseSOF(body, blen)) return false;
            d.progressive = (m == 0xC2);
            if (info) { info->width = d.width; info->height = d.height; info->components = d.ncomp; info->progressive = d.progressive; }
            return true;
        }
        if (m == 0xC3 || (m >= 0xC5 && m <= 0xCF && m != 0xC8 && m != 0xCC && m != 0xC4)) return false; // lossless / arithmetic
        p += 2 + len;
    }
    return false;
}

// ---- the actual scan decoding -----------------------------------------------------------------------------------
struct ScanState {
    BitReader br;
    int eobrun = 0;
    int pred[4] = {0, 0, 0, 0};
};

}  // namespace

bool pjpeg_get_info(const uint8_t *data, size_t size, ProgressiveJpegInfo *info) {
    if (info) *info = ProgressiveJpegInfo();
    return scanHeaders(data, size, nullptr, info);
}

int pjpeg_pick_scale(int srcW, int srcH, int targetW, int targetH) {
    for (int n = 1; n <= 8; n++) {
        if ((long)srcW * n >= (long)targetW * 8 && (long)srcH * n >= (long)targetH * 8) return n;
    }
    return 8;
}

bool pjpeg_decode_gray(const uint8_t *data, size_t size, int scaleN, size_t maxBytes,
                       uint8_t **out, int *outW, int *outH) {
    *out = nullptr; *outW = *outH = 0;
    if (scaleN < 1) scaleN = 1;
    if (scaleN > 8) scaleN = 8;

    Decoder dec;
    dec.data = data; dec.size = size;
    ProgressiveJpegInfo info;
    if (!scanHeaders(data, size, &dec, &info) || !dec.progressive) return false;

    // pick the largest scale that fits in the memory budget
    int N = scaleN;
    for (;; N--) {
        size_t bb = sizeof(uint64_t) + (size_t)N * N * sizeof(int16_t);
        bb = (bb + 7) & ~(size_t)7;
        size_t total = bb * (size_t)dec.bwPad * dec.bhPad + (size_t)dec.bwPad * N * dec.bhPad * N;
        if (total <= maxBytes || N == 1) { dec.N = N; dec.blockBytes = bb; break; }
    }
    size_t blocksSize = dec.blockBytes * (size_t)dec.bwPad * dec.bhPad;
    dec.blocks = (uint8_t *)calloc(1, blocksSize);
    if (!dec.blocks) return false;

    const int Nn = dec.N;
    ScanState st;
    const uint8_t *end = data + size;
    const uint8_t *p = data + 2;
    bool gotScan = false;

    while (p + 4 <= end) {
        if (p[0] != 0xFF) { p++; continue; }
        int m = p[1];
        if (m == 0xFF) { p++; continue; }
        if (m == 0x00 || m == 0x01 || (m >= 0xD0 && m <= 0xD8)) { p += 2; continue; }
        if (m == 0xD9) break;
        int len = (p[2] << 8) | p[3];
        if (len < 2 || p + 2 + len > end) break;
        const uint8_t *body = p + 4;
        int blen = len - 2;

        if (m == 0xC4) { if (!dec.parseDHT(body, blen)) break; p += 2 + len; continue; }
        if (m == 0xDB) { if (!dec.parseDQT(body, blen)) break; p += 2 + len; continue; }
        if (m == 0xDD) { if (blen >= 2) dec.restartInterval = (body[0] << 8) | body[1]; p += 2 + len; continue; }
        if (m != 0xDA) { p += 2 + len; continue; }

        // ---------------- SOS ----------------
        int ns = body[0];
        if (ns < 1 || ns > 4 || blen < 1 + 2 * ns + 3) break;
        int sidx[4], std_[4], sta[4];
        for (int i = 0; i < ns; i++) {
            int cid = body[1 + 2 * i];
            int found = -1;
            for (int c = 0; c < dec.ncomp; c++) if (dec.comp[c].id == cid) found = c;
            if (found < 0) found = i;       // be forgiving
            sidx[i] = found;
            std_[i] = body[2 + 2 * i] >> 4;
            sta[i] = body[2 + 2 * i] & 15;
        }
        int Ss = body[1 + 2 * ns], Se = body[2 + 2 * ns];
        int Ah = body[3 + 2 * ns] >> 4, Al = body[3 + 2 * ns] & 15;
        const uint8_t *dataStart = p + 2 + len;

        // a scan that only contains chroma carries nothing we need: skip it
        if (ns == 1 && sidx[0] != 0) { p = nextMarker(dataStart, end); continue; }

        st.br.p = dataStart; st.br.end = end; st.br.reset();
        st.eobrun = 0;
        for (int i = 0; i < 4; i++) st.pred[i] = 0;
        BitReader &br = st.br;

        bool ok = true;
        const bool isDC = (Ss == 0);
        if (isDC && (Se != 0 || ns < 1)) { ok = false; }
        if (!isDC && ns != 1) ok = false;
        if (!ok) break;

        int restartsLeft = dec.restartInterval;
        auto doRestart = [&]() {
            // find and skip the RSTn marker, then reset predictors
            const uint8_t *q = br.p;
            while (q + 1 < end && !(q[0] == 0xFF && q[1] >= 0xD0 && q[1] <= 0xD7)) {
                if (q[0] == 0xFF && q[1] != 0x00 && q[1] != 0xFF) break;   // some other marker
                q++;
            }
            if (q + 1 < end && q[0] == 0xFF && q[1] >= 0xD0 && q[1] <= 0xD7) q += 2;
            br.p = q; br.reset();
            st.eobrun = 0;
            for (int i = 0; i < 4; i++) st.pred[i] = 0;
            restartsLeft = dec.restartInterval;
        };

        // decode one luma block for AC scans
        auto acFirst = [&](uint8_t *blk, const Huff &h) -> bool {
            if (st.eobrun > 0) { st.eobrun--; return true; }
            uint64_t *nz = (uint64_t *)blk;
            int16_t *lo = (int16_t *)(blk + sizeof(uint64_t));
            for (int k = Ss; k <= Se; k++) {
                int rs = huffDecode(br, h);
                if (rs < 0) return false;
                int r = rs >> 4, s = rs & 15;
                if (s) {
                    k += r;
                    if (k > 63) return false;
                    int v = extend(br.bits(s), s) * (1 << Al);
                    int n = kZigzag[k], u = n & 7, vv = n >> 3;
                    *nz |= 1ull << n;
                    if (u < Nn && vv < Nn) lo[vv * Nn + u] = (int16_t)v;
                } else {
                    if (r == 15) { k += 15; }
                    else {
                        st.eobrun = (1 << r);
                        if (r) st.eobrun += br.bits(r);
                        st.eobrun--;
                        break;
                    }
                }
            }
            return true;
        };

        auto acRefine = [&](uint8_t *blk, const Huff &h) -> bool {
            uint64_t *nz = (uint64_t *)blk;
            int16_t *lo = (int16_t *)(blk + sizeof(uint64_t));
            const int bit = 1 << Al;
            auto refineOne = [&](int n) {
                if (br.bit()) {
                    int u = n & 7, vv = n >> 3;
                    if (u < Nn && vv < Nn) {
                        int16_t &c = lo[vv * Nn + u];
                        if ((c & bit) == 0) { if (c > 0) c += bit; else c -= bit; }
                    }
                }
            };
            int k = Ss;
            if (st.eobrun > 0) {
                st.eobrun--;
                for (; k <= Se; k++) { int n = kZigzag[k]; if ((*nz >> n) & 1) refineOne(n); }
                return true;
            }
            while (k <= Se) {
                int rs = huffDecode(br, h);
                if (rs < 0) return false;
                int s = rs & 15, r = rs >> 4;
                int newVal = 0;
                if (s == 0) {
                    if (r < 15) {
                        st.eobrun = (1 << r) - 1;
                        if (r) st.eobrun += br.bits(r);
                        r = 64;
                    }
                } else {
                    if (s != 1) return false;
                    newVal = br.bit() ? bit : -bit;
                }
                while (k <= Se) {
                    int n = kZigzag[k++];
                    if ((*nz >> n) & 1) {
                        refineOne(n);
                    } else {
                        if (r == 0) {
                            if (newVal) {
                                *nz |= 1ull << n;
                                int u = n & 7, vv = n >> 3;
                                if (u < Nn && vv < Nn) lo[vv * Nn + u] = (int16_t)newVal;
                            }
                            break;
                        }
                        --r;
                    }
                }
            }
            return true;
        };

        if (isDC) {
            // possibly interleaved: iterate over MCUs (or over blocks when a single component is coded)
            const bool interleaved = (ns > 1);
            int totalMcus = interleaved ? dec.mcusX * dec.mcusY : dec.bwReal * dec.bhReal;
            for (int mcu = 0; mcu < totalMcus && ok; mcu++) {
                if (dec.restartInterval && restartsLeft == 0) doRestart();
                for (int i = 0; i < ns && ok; i++) {
                    const int c = sidx[i];
                    int hh = interleaved ? dec.comp[c].h : 1;
                    int vv2 = interleaved ? dec.comp[c].v : 1;
                    for (int by = 0; by < vv2 && ok; by++) {
                        for (int bx = 0; bx < hh && ok; bx++) {
                            uint8_t *blk = nullptr;
                            if (c == 0) {
                                int gx, gy;
                                if (interleaved) { gx = (mcu % dec.mcusX) * hh + bx; gy = (mcu / dec.mcusX) * vv2 + by; }
                                else { gx = mcu % dec.bwReal; gy = mcu / dec.bwReal; }
                                blk = dec.blockAt(gx, gy);
                            }
                            if (Ah == 0) {
                                int t = huffDecode(br, dec.dc[std_[i] & 3]);
                                if (t < 0 || t > 16) { ok = false; break; }
                                int diff = t ? extend(br.bits(t), t) : 0;
                                st.pred[c] += diff;
                                if (blk) {
                                    *(uint64_t *)blk |= (st.pred[c] != 0) ? 1ull : 0ull;
                                    ((int16_t *)(blk + sizeof(uint64_t)))[0] = (int16_t)(st.pred[c] * (1 << Al));
                                }
                            } else {
                                int b = br.bit();
                                if (blk && b) {
                                    ((int16_t *)(blk + sizeof(uint64_t)))[0] |= (int16_t)(1 << Al);
                                    *(uint64_t *)blk |= 1ull;
                                }
                            }
                        }
                    }
                }
                if (dec.restartInterval) restartsLeft--;
            }
        } else {
            // AC scan of the luma component, one block at a time in raster order
            const Huff &h = dec.ac[sta[0] & 3];
            int total = dec.bwReal * dec.bhReal;
            for (int i = 0; i < total && ok; i++) {
                if (dec.restartInterval && restartsLeft == 0) doRestart();
                uint8_t *blk = dec.blockAt(i % dec.bwReal, i / dec.bwReal);
                ok = (Ah == 0) ? acFirst(blk, h) : acRefine(blk, h);
                if (dec.restartInterval) restartsLeft--;
            }
        }
        gotScan = true;
        if (!ok) break;               // keep what we have: a truncated file still gives a usable picture
        p = nextMarker(br.p, end);
    }

    bool success = false;
    if (gotScan) {
        // ---------------- reconstruct ----------------
        const int tq = dec.comp[0].tq;
        const uint16_t *q = dec.quant[tq];
        const int W = (dec.width * Nn + 7) / 8, H = (dec.height * Nn + 7) / 8;
        const int fullW = dec.bwPad * Nn;
        uint8_t *img = (uint8_t *)malloc((size_t)fullW * dec.bhPad * Nn);
        if (img) {
            float M[8][8];
            for (int x = 0; x < Nn; x++)
                for (int u = 0; u < Nn; u++)
                    M[x][u] = (float)(sqrt((u == 0 ? 1.0 : 2.0) / Nn) * cos((2 * x + 1) * u * M_PI / (2.0 * Nn)));
            const float norm = (float)Nn / 8.0f;
            for (int by = 0; by < dec.bhPad; by++) {
                for (int bx = 0; bx < dec.bwPad; bx++) {
                    const int16_t *lo = (const int16_t *)(dec.blockAt(bx, by) + sizeof(uint64_t));
                    float F[8][8];
                    for (int v = 0; v < Nn; v++)
                        for (int u = 0; u < Nn; u++) F[v][u] = (float)lo[v * Nn + u] * q[v * 8 + u];
                    float tmp[8][8];     // tmp[v][x] = sum_u F[v][u] * M[x][u]
                    for (int v = 0; v < Nn; v++)
                        for (int x = 0; x < Nn; x++) {
                            float s = 0;
                            for (int u = 0; u < Nn; u++) s += F[v][u] * M[x][u];
                            tmp[v][x] = s;
                        }
                    for (int y = 0; y < Nn; y++)
                        for (int x = 0; x < Nn; x++) {
                            float s = 0;
                            for (int v = 0; v < Nn; v++) s += tmp[v][x] * M[y][v];
                            int val = (int)lrintf(s * norm + 128.0f);
                            if (val < 0) val = 0; else if (val > 255) val = 255;
                            img[(size_t)(by * Nn + y) * fullW + bx * Nn + x] = (uint8_t)val;
                        }
                }
            }
            // crop to the real size
            uint8_t *res = (uint8_t *)malloc((size_t)W * H);
            if (res) {
                for (int y = 0; y < H; y++) memcpy(res + (size_t)y * W, img + (size_t)y * fullW, W);
                *out = res; *outW = W; *outH = H;
                success = true;
            }
            free(img);
        }
    }
    free(dec.blocks);
    return success;
}
