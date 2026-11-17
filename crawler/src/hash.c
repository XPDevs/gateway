#include "crawler.h"

/* MD5, SHA-1, SHA-256, base64 and the bloom filter */
/* ==========================================================================
 * Section 2 - MD5 / SHA-1 / SHA-256 / base64 (hashlib equivalents)
 * ========================================================================== */


const uint32_t MD5_K[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
    0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
    0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
    0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
    0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
    0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
    0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
    0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
    0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
    0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
    0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
    0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
    0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
    0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
};

const unsigned char MD5_S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

void md5_init(md5_ctx *c)
{
    c->a = 0x67452301u;
    c->b = 0xefcdab89u;
    c->c = 0x98badcfeu;
    c->d = 0x10325476u;
    c->len = 0;
    c->buflen = 0;
}

uint32_t rotl32(uint32_t x, unsigned n)
{
    return (x << n) | (x >> (32 - n));
}

void md5_block(md5_ctx *c, const unsigned char *p)
{
    uint32_t m[16];
    for (int i = 0; i < 16; i++)
        m[i] = (uint32_t)p[i * 4] | ((uint32_t)p[i * 4 + 1] << 8) |
               ((uint32_t)p[i * 4 + 2] << 16) | ((uint32_t)p[i * 4 + 3] << 24);
    uint32_t a = c->a, b = c->b, cc = c->c, d = c->d;
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        if (i < 16)      { f = (b & cc) | (~b & d);        g = i; }
        else if (i < 32) { f = (d & b) | (~d & cc);        g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ cc ^ d;                 g = (3 * i + 5) & 15; }
        else             { f = cc ^ (b | ~d);              g = (7 * i) & 15; }
        uint32_t tmp = d;
        d = cc;
        cc = b;
        b = b + rotl32(a + f + MD5_K[i] + m[g], MD5_S[i]);
        a = tmp;
    }
    c->a += a;
    c->b += b;
    c->c += cc;
    c->d += d;
}

void md5_update(md5_ctx *c, const void *data, size_t n)
{
    const unsigned char *p = data;
    c->len += n;
    if (c->buflen) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take;
        p += take;
        n -= take;
        if (c->buflen == 64) {
            md5_block(c, c->buf);
            c->buflen = 0;
        }
    }
    while (n >= 64) {
        md5_block(c, p);
        p += 64;
        n -= 64;
    }
    if (n) {
        memcpy(c->buf, p, n);
        c->buflen = n;
    }
}

void md5_final(md5_ctx *c, unsigned char out[16])
{
    uint64_t bits = c->len * 8;
    unsigned char pad[72];
    size_t padlen = (c->buflen < 56) ? (56 - c->buflen) : (120 - c->buflen);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) pad[padlen + i] = (unsigned char)(bits >> (8 * i));
    md5_update(c, pad, padlen + 8);
    uint32_t v[4] = { c->a, c->b, c->c, c->d };
    for (int i = 0; i < 4; i++) {
        out[i * 4] = (unsigned char)(v[i] & 0xff);
        out[i * 4 + 1] = (unsigned char)((v[i] >> 8) & 0xff);
        out[i * 4 + 2] = (unsigned char)((v[i] >> 16) & 0xff);
        out[i * 4 + 3] = (unsigned char)((v[i] >> 24) & 0xff);
    }
}

void md5_hex(const void *data, size_t n, char out[33])
{
    unsigned char d[16];
    md5_ctx c;
    md5_init(&c);
    md5_update(&c, data, n);
    md5_final(&c, d);
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[i * 2] = H[d[i] >> 4];
        out[i * 2 + 1] = H[d[i] & 15];
    }
    out[32] = '\0';
}

/* -- SHA-1 ----------------------------------------------------------------- */

void sha1_init(sha1_ctx *c)
{
    c->h[0] = 0x67452301u;
    c->h[1] = 0xEFCDAB89u;
    c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u;
    c->h[4] = 0xC3D2E1F0u;
    c->len = 0;
    c->buflen = 0;
}

void sha1_block(sha1_ctx *c, const unsigned char *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & cc) | (~b & d);          k = 0x5A827999u; }
        else if (i < 40) { f = b ^ cc ^ d;                   k = 0x6ED9EBA1u; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDCu; }
        else             { f = b ^ cc ^ d;                   k = 0xCA62C1D6u; }
        uint32_t tmp = rotl32(a, 5) + f + e + k + w[i];
        e = d;
        d = cc;
        cc = rotl32(b, 30);
        b = a;
        a = tmp;
    }
    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
}

void sha1_update(sha1_ctx *c, const void *data, size_t n)
{
    const unsigned char *p = data;
    c->len += n;
    if (c->buflen) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take;
        p += take;
        n -= take;
        if (c->buflen == 64) {
            sha1_block(c, c->buf);
            c->buflen = 0;
        }
    }
    while (n >= 64) {
        sha1_block(c, p);
        p += 64;
        n -= 64;
    }
    if (n) {
        memcpy(c->buf, p, n);
        c->buflen = n;
    }
}

void sha1_final(sha1_ctx *c, unsigned char out[20])
{
    uint64_t bits = c->len * 8;
    unsigned char pad[72];
    size_t padlen = (c->buflen < 56) ? (56 - c->buflen) : (120 - c->buflen);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) pad[padlen + i] = (unsigned char)(bits >> (8 * (7 - i)));
    sha1_update(c, pad, padlen + 8);
    for (int i = 0; i < 5; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

void sha1_hex(const void *data, size_t n, char *out /* >=41 */)
{
    unsigned char d[20];
    sha1_ctx c;
    sha1_init(&c);
    sha1_update(&c, data, n);
    sha1_final(&c, d);
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        out[i * 2] = H[d[i] >> 4];
        out[i * 2 + 1] = H[d[i] & 15];
    }
    out[40] = '\0';
}

/* -- SHA-256 --------------------------------------------------------------- */

const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

void sha256_init(sha256_ctx *c)
{
    c->h[0] = 0x6a09e667u;
    c->h[1] = 0xbb67ae85u;
    c->h[2] = 0x3c6ef372u;
    c->h[3] = 0xa54ff53au;
    c->h[4] = 0x510e527fu;
    c->h[5] = 0x9b05688cu;
    c->h[6] = 0x1f83d9abu;
    c->h[7] = 0x5be0cd19u;
    c->len = 0;
    c->buflen = 0;
}

uint32_t rotr32(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32 - n));
}

void sha256_block(sha256_ctx *c, const unsigned char *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

void sha256_update(sha256_ctx *c, const void *data, size_t n)
{
    const unsigned char *p = data;
    c->len += n;
    if (c->buflen) {
        size_t take = 64 - c->buflen;
        if (take > n) take = n;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take;
        p += take;
        n -= take;
        if (c->buflen == 64) {
            sha256_block(c, c->buf);
            c->buflen = 0;
        }
    }
    while (n >= 64) {
        sha256_block(c, p);
        p += 64;
        n -= 64;
    }
    if (n) {
        memcpy(c->buf, p, n);
        c->buflen = n;
    }
}

void sha256_final(sha256_ctx *c, unsigned char out[32])
{
    uint64_t bits = c->len * 8;
    unsigned char pad[72];
    size_t padlen = (c->buflen < 56) ? (56 - c->buflen) : (120 - c->buflen);
    memset(pad, 0, sizeof pad);
    pad[0] = 0x80;
    for (int i = 0; i < 8; i++) pad[padlen + i] = (unsigned char)(bits >> (8 * (7 - i)));
    sha256_update(c, pad, padlen + 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)(c->h[i]);
    }
}

void sha256_hex(const void *data, size_t n, char out[65])
{
    unsigned char d[32];
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, n);
    sha256_final(&c, d);
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2] = H[d[i] >> 4];
        out[i * 2 + 1] = H[d[i] & 15];
    }
    out[64] = '\0';
}

/* -- base64 ---------------------------------------------------------------- */
const char B64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const unsigned char *data, size_t n)
{
    size_t outlen = 4 * ((n + 2) / 3);
    char *out = xmalloc(outlen + 1);
    size_t o = 0;
    size_t i = 0;
    while (i + 2 < n) {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8) | data[i + 2];
        out[o++] = B64_CHARS[(v >> 18) & 63];
        out[o++] = B64_CHARS[(v >> 12) & 63];
        out[o++] = B64_CHARS[(v >> 6) & 63];
        out[o++] = B64_CHARS[v & 63];
        i += 3;
    }
    if (i + 1 == n) {
        uint32_t v = (uint32_t)data[i] << 16;
        out[o++] = B64_CHARS[(v >> 18) & 63];
        out[o++] = B64_CHARS[(v >> 12) & 63];
        out[o++] = '=';
        out[o++] = '=';
    } else if (i + 2 == n) {
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i + 1] << 8);
        out[o++] = B64_CHARS[(v >> 18) & 63];
        out[o++] = B64_CHARS[(v >> 12) & 63];
        out[o++] = B64_CHARS[(v >> 6) & 63];
        out[o++] = '=';
    }
    out[o] = '\0';
    return out;
}

unsigned char *base64_decode(const char *src, size_t *outlen)
{
    static int8_t rev[256];
    static int built = 0;
    if (!built) {
        memset(rev, -1, sizeof rev);
        for (int i = 0; i < 64; i++) rev[(unsigned char)B64_CHARS[i]] = (int8_t)i;
        built = 1;
    }
    size_t n = strlen(src);
    unsigned char *out = xmalloc((n / 4 + 2) * 3 + 1);
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '=' ) break;
        int v = rev[ch];
        if (v < 0) continue;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((acc >> bits) & 0xff);
        }
    }
    if (outlen) *outlen = o;
    return out;
}

/* -- python str(value) for JSON scalars ------------------------------------ */
char *num_to_pystr(double v, int is_int, long long iv)
{
    char tmp[64];
    if (is_int) {
        snprintf(tmp, sizeof tmp, "%lld", iv);
        return xstrdup(tmp);
    }
    if (v == 0) return xstrdup("0.0");
    if (fabs(v) < 1e16 && v == floor(v) && fabs(v) < 1e16) {
        snprintf(tmp, sizeof tmp, "%.1f", v);
        return xstrdup(tmp);
    }
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) return xstrdup(tmp);
    }
    snprintf(tmp, sizeof tmp, "%.17g", v);
    return xstrdup(tmp);
}

/* ==========================================================================
 * Section 3 - Bloom filter (identical sizing maths to the Python version)
 * ========================================================================== */


void bloom_init(bloom_t *b, uint64_t capacity, double error_rate)
{
    double denom = log(2.0) * log(2.0);
    double m = -(double)capacity * log(error_rate) / denom;
    if (m < 8) m = 8;
    b->num_bits = ((uint64_t)m + 7) / 8 * 8;
    double k = ((double)b->num_bits / (double)capacity) * log(2.0);
    long kk = py_round_long(k);
    b->num_hashes = kk < 1 ? 1 : (int)kk;
    b->bits = xcalloc((size_t)(b->num_bits / 8) + 1, 1);
    b->capacity = capacity;
    b->error_rate = error_rate;
    b->added = 0;
}

void bloom_free(bloom_t *b)
{
    free(b->bits);
    b->bits = NULL;
}

void bloom_positions(const char *item, uint64_t *h1, uint64_t *h2)
{
    unsigned char d[16];
    md5_ctx c;
    md5_init(&c);
    md5_update(&c, item, strlen(item));
    md5_final(&c, d);
    uint64_t a = 0, bb = 0;
    for (int i = 7; i >= 0; i--) a = (a << 8) | d[i];        /* little endian */
    for (int i = 15; i >= 8; i--) bb = (bb << 8) | d[i];
    if (bb == 0) bb = 1;
    *h1 = a;
    *h2 = bb;
}

/* (h1 + i*h2) % m, computed exactly on every target.
 * 64-bit targets use a 128-bit accumulator; 32-bit targets (armhf) have no
 * __int128, so the product is reduced step by step instead - same value,
 * no overflow, so positions match the Python original bit for bit. */
uint64_t bloom_pos_mod(uint64_t h1, uint64_t index, uint64_t h2, uint64_t m)
{
#if defined(__SIZEOF_INT128__)
    return (uint64_t)(((unsigned __int128)h1 +
                       (unsigned __int128)index * (unsigned __int128)h2) % m);
#else
    uint64_t a = index % m;
    uint64_t b = h2 % m;
    uint64_t acc = 0;
    while (b) {
        if (b & 1u) { acc += a; if (acc >= m) acc -= m; }
        b >>= 1;
        if (b) { a <<= 1; if (a >= m) a -= m; }
    }
    uint64_t r = h1 % m;
    r += acc;
    if (r >= m) r -= m;
    return r;
#endif
}

void bloom_add(bloom_t *b, const char *item)
{
    uint64_t h1, h2;
    bloom_positions(item, &h1, &h2);
    for (int i = 0; i < b->num_hashes; i++) {
        uint64_t p = bloom_pos_mod(h1, (uint64_t)i, h2, b->num_bits);
        b->bits[p >> 3] |= (uint8_t)(1u << (p & 7));
    }
    b->added++;
}

int bloom_contains(const bloom_t *b, const char *item)
{
    uint64_t h1, h2;
    bloom_positions(item, &h1, &h2);
    for (int i = 0; i < b->num_hashes; i++) {
        uint64_t p = bloom_pos_mod(h1, (uint64_t)i, h2, b->num_bits);
        if (!(b->bits[p >> 3] & (uint8_t)(1u << (p & 7)))) return 0;
    }
    return 1;
}

double bloom_saturation(const bloom_t *b)
{
    if (!b->num_bits) return 0.0;
    static const uint8_t pc[256] = {
        B6(0), B6(1), B6(1), B6(2)
    };
    uint64_t set = 0;
    size_t bytes = (size_t)(b->num_bits / 8);
    for (size_t i = 0; i < bytes; i++) set += pc[b->bits[i]];
    return (double)set / (double)b->num_bits;
}

size_t bloom_bytes_used(const bloom_t *b) { return (size_t)(b->num_bits / 8); }
