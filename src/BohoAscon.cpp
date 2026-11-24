/*
 * BohoAscon.cpp — Ascon (NIST SP 800-232) for Boho
 *
 * The state is five 64-bit words. SP 800-232 loads bytes little-endian, so on
 * little-endian CPUs (AVR, ESP32, ESP8266, ARM, x86) the state bytes can be
 * XORed with data directly, without packing words.
 *
 * Author: Taeo Lee <sixgen@gmail.com>
 */

#include "BohoAscon.h"
#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "BohoAscon needs a little-endian CPU"
#endif

namespace {

union State {
  uint64_t x[5];
  uint8_t b[40];
};

inline uint64_t ror(uint64_t v, uint8_t n) { return (v >> n) | (v << (64 - n)); }

// The permutation: the last `rounds` of 12 rounds (12 or 8).
void __attribute__((noinline)) permute(State &s, uint8_t rounds)
{
  uint64_t x0 = s.x[0], x1 = s.x[1], x2 = s.x[2], x3 = s.x[3], x4 = s.x[4];
  for (uint8_t i = 12 - rounds; i < 12; ++i) {
    x2 ^= (uint8_t)(((0x0f - i) << 4) | i);
    x0 ^= x4; x4 ^= x3; x2 ^= x1;
    const uint64_t t0 = ~x0 & x1, t1 = ~x1 & x2, t2 = ~x2 & x3, t3 = ~x3 & x4, t4 = ~x4 & x0;
    x0 ^= t1; x1 ^= t2; x2 ^= t3; x3 ^= t4; x4 ^= t0;
    x1 ^= x0; x0 ^= x4; x3 ^= x2; x2 = ~x2;
    x0 ^= ror(x0, 19) ^ ror(x0, 28);
    x1 ^= ror(x1, 61) ^ ror(x1, 39);
    x2 ^= ror(x2, 1) ^ ror(x2, 6);
    x3 ^= ror(x3, 10) ^ ror(x3, 17);
    x4 ^= ror(x4, 7) ^ ror(x4, 41);
  }
  s.x[0] = x0; s.x[1] = x1; s.x[2] = x2; s.x[3] = x3; s.x[4] = x4;
}

void wipe(void *p, size_t n)
{
  volatile uint8_t *v = (volatile uint8_t *)p;
  while (n--) *v++ = 0;
}

void xorBytes(uint8_t *dst, const uint8_t *src, uint8_t n)
{
  for (uint8_t i = 0; i < n; ++i) dst[i] ^= src[i];
}

// Associated data: 16-byte blocks, always padded, then the domain bit.
void absorbAd(State &s, const uint8_t *ad, size_t adLen)
{
  if (adLen) {
    for (; adLen >= 16; ad += 16, adLen -= 16) {
      xorBytes(s.b, ad, 16);
      permute(s, 8);
    }
    xorBytes(s.b, ad, (uint8_t)adLen);
    s.b[adLen] ^= 0x01;
    permute(s, 8);
  }
  s.b[39] ^= 0x80;
}

void start(State &s, const uint8_t *key, const uint8_t *nonce, const uint8_t *ad, size_t adLen)
{
  s.x[0] = 0x00001000808c0001ULL;
  memcpy(s.b + 8, key, 16);
  memcpy(s.b + 24, nonce, 16);
  permute(s, 12);
  xorBytes(s.b + 24, key, 16);
  absorbAd(s, ad, adLen);
}

void finish(State &s, const uint8_t *key)
{
  xorBytes(s.b + 16, key, 16);
  permute(s, 12);
  xorBytes(s.b + 24, key, 16);  // tag = s.b[24..39]
}

// Absorb one padded string in 8-byte blocks, 12 rounds after each block.
void absorb8(State &s, const uint8_t *in, size_t len)
{
  for (; len >= 8; in += 8, len -= 8) {
    xorBytes(s.b, in, 8);
    permute(s, 12);
  }
  xorBytes(s.b, in, (uint8_t)len);
  s.b[len] ^= 0x01;
  permute(s, 12);
}

// One hash/XOF/CXOF run. A customization string (CXOF) goes in after its bit length.
void hashRun(uint64_t iv, uint8_t *out, size_t outLen, const uint8_t *in, size_t len,
             const uint8_t *custom = nullptr, size_t customLen = 0, bool cxof = false)
{
  State s;
  memset(&s, 0, sizeof(s));
  s.x[0] = iv;
  permute(s, 12);
  if (cxof) {
    s.x[0] ^= (uint64_t)customLen * 8;
    permute(s, 12);
    absorb8(s, custom, customLen);
  }
  absorb8(s, in, len);
  while (true) {
    const uint8_t n = outLen < 8 ? (uint8_t)outLen : 8;
    memcpy(out, s.b, n);
    out += n;
    outLen -= n;
    if (!outLen) break;
    permute(s, 12);
  }
  wipe(&s, sizeof(s));
}

}  // namespace

void ascon_aead128_encrypt(uint8_t *out, uint8_t *tag,
                           const uint8_t *in, size_t len,
                           const uint8_t *ad, size_t adLen,
                           const uint8_t *key, const uint8_t *nonce)
{
  State s;
  start(s, key, nonce, ad, adLen);
  for (; len >= 16; in += 16, out += 16, len -= 16) {
    xorBytes(s.b, in, 16);
    memcpy(out, s.b, 16);
    permute(s, 8);
  }
  xorBytes(s.b, in, (uint8_t)len);
  memcpy(out, s.b, len);
  s.b[len] ^= 0x01;
  finish(s, key);
  memcpy(tag, s.b + 24, 16);
  wipe(&s, sizeof(s));
}

bool ascon_aead128_decrypt(uint8_t *out,
                           const uint8_t *in, size_t len,
                           const uint8_t *ad, size_t adLen,
                           const uint8_t *tag, uint8_t tagLen,
                           const uint8_t *key, const uint8_t *nonce)
{
  if (tagLen < 4 || tagLen > 16) {
    memset(out, 0, len);
    return false;
  }
  State s;
  uint8_t *const first = out;
  const size_t total = len;
  start(s, key, nonce, ad, adLen);
  while (true) {
    const uint8_t n = len < 16 ? (uint8_t)len : 16;
    for (uint8_t i = 0; i < n; ++i) {
      const uint8_t c = in[i];  // read before writing: out may equal in
      out[i] = s.b[i] ^ c;
      s.b[i] = c;
    }
    in += n; out += n; len -= n;
    if (n < 16) {
      s.b[n] ^= 0x01;
      break;
    }
    permute(s, 8);
  }
  finish(s, key);
  uint8_t diff = 0;
  for (uint8_t i = 0; i < tagLen; ++i) diff |= s.b[24 + i] ^ tag[i];
  wipe(&s, sizeof(s));
  if (diff) {
    wipe(first, total);
    return false;
  }
  return true;
}

void ascon_hash256(uint8_t *digest, const void *data, size_t len)
{
  hashRun(0x0000080100cc0002ULL, digest, 32, (const uint8_t *)data, len);
}

void ascon_xof128(uint8_t *out, size_t outLen, const void *data, size_t len)
{
  if (outLen) hashRun(0x0000080000cc0003ULL, out, outLen, (const uint8_t *)data, len);
}

bool ascon_cxof128(uint8_t *out, size_t outLen, const void *data, size_t len,
                   const void *custom, size_t customLen)
{
  if (customLen > ASCON_CXOF_MAX_CUSTOM) return false;
  if (outLen) hashRun(0x0000080000cc0004ULL, out, outLen, (const uint8_t *)data, len,
                      (const uint8_t *)custom, customLen, true);
  return true;
}
