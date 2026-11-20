// E2E session keys for esp32cam-session: X25519, checked with the board's e2e_key (HMAC-SHA256),
// HKDF-SHA256 to the session key. The same steps are in remocon.kr (src/lib/control/e2eSession.js).
//
//   receiver -> <cam>@e2e-hello      [1]                         ask for a challenge
//   camera   -> @e2e-challenge       [1][reason][nonce 16][holder 8]
//   receiver -> <cam>@e2e-hello      [1][A 32][HMAC(K, "e2e-hello" 1 nonce A)]
//   camera   -> @e2e-ready           [1][B 32][HMAC(K, "e2e-ready" 1 nonce A B)]
//   both:  session key = HKDF-SHA256(ikm = X25519, salt = K, info = "iosignal-e2e-session/1" nonce A B), 32 bytes,
//          written as 64 lowercase hex characters and given to signal_e2e() / decrypt_e2e() as the key.
//
// K is the e2e_key text. A and B are public keys of secrets that live only in RAM and are wiped after use, so a
// recorded exchange and a key read from flash later do not give the session key (forward secrecy).
// holder is the first 8 bytes of SHA-256(A) of the receiver that has the session, zeros when nobody has it.
// For REFUSED and BUSY it is that of the hello being answered instead, so only its sender takes it.
#pragma once
#include <string.h>
#include <Curve25519.h>
#include <SHA256.h>
#include <HKDF.h>

namespace e2esession {

const uint8_t VERSION = 1;
const size_t NONCE = 16, PUB = 32, MAC = 32, HOLDER = 8;
const size_t REQUEST_SIZE = 1;
const size_t HELLO_SIZE = 1 + PUB + MAC;               // 65
const size_t CHALLENGE_SIZE = 2 + NONCE + HOLDER;      // 26
const size_t READY_SIZE = 1 + PUB + MAC;               // 65

enum Reason : uint8_t {
  START = 0,    // nobody has the session (boot, new key, nobody answered a rotation)
  REQUEST = 1,  // answer to a request
  ROTATE = 2,   // the session expired: the holder should send a new hello
  REFUSED = 3,  // the hello (holder = its A) did not match the key
  BUSY = 4      // the hello (holder = its A) signed the previous nonce: another receiver was first
};

inline void wipe(void* p, size_t n) {
  volatile uint8_t* v = (volatile uint8_t*)p;
  while (n--) *v++ = 0;
}

inline bool same(const uint8_t* a, const uint8_t* b, size_t n) {  // constant time
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= a[i] ^ b[i];
  return d == 0;
}

// HMAC-SHA256(K, label 1 nonce A [B])
inline void mac(uint8_t out[MAC], const char* key, const char* label, const uint8_t nonce[NONCE],
                const uint8_t a[PUB], const uint8_t* b) {
  SHA256 h;
  const size_t keyLen = strlen(key);
  h.resetHMAC(key, keyLen);
  h.update(label, strlen(label));
  h.update(&VERSION, 1);
  h.update(nonce, NONCE);
  h.update(a, PUB);
  if (b) h.update(b, PUB);
  h.finalizeHMAC(key, keyLen, out, MAC);
}

inline bool helloValid(const char* key, const uint8_t nonce[NONCE], const uint8_t hello[HELLO_SIZE]) {
  uint8_t expected[MAC];
  mac(expected, key, "e2e-hello", nonce, hello + 1, nullptr);
  const bool ok = hello[0] == VERSION && same(expected, hello + 1 + PUB, MAC);
  wipe(expected, sizeof(expected));
  return ok;
}

inline void holderOf(uint8_t out[HOLDER], const uint8_t a[PUB]) {
  SHA256 h;
  uint8_t full[32];
  h.update(a, PUB);
  h.finalize(full, sizeof(full));
  memcpy(out, full, HOLDER);
}

inline void toHex(char out[65], const uint8_t bytes[32]) {
  static const char HEX_DIGITS[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 32; i++) {
    out[i * 2] = HEX_DIGITS[bytes[i] >> 4];
    out[i * 2 + 1] = HEX_DIGITS[bytes[i] & 15];
  }
  out[64] = 0;
}

// The camera's side of a valid hello. secret: 32 random bytes (wiped here). Writes the ready message and the
// session key (hex). False for a public key that gives no shared secret; nothing is written then.
inline bool accept(const char* key, const uint8_t nonce[NONCE], const uint8_t a[PUB], uint8_t secret[32],
                   uint8_t ready[READY_SIZE], char sessionKey[65]) {
  secret[0] &= 0xF8;
  secret[31] = (secret[31] & 0x7F) | 0x40;
  uint8_t b[PUB], shared[32];
  Curve25519::eval(b, secret, 0);
  const bool valid = Curve25519::eval(shared, secret, a);
  wipe(secret, 32);
  uint8_t any = 0;
  for (uint8_t i = 0; i < 32; i++) any |= shared[i];
  if (!valid || !any) { wipe(shared, sizeof(shared)); return false; }

  static const char INFO_LABEL[] = "iosignal-e2e-session/1";
  const size_t labelLen = sizeof(INFO_LABEL) - 1;
  uint8_t info[sizeof(INFO_LABEL) - 1 + NONCE + PUB + PUB];
  memcpy(info, INFO_LABEL, labelLen);
  memcpy(info + labelLen, nonce, NONCE);
  memcpy(info + labelLen + NONCE, a, PUB);
  memcpy(info + labelLen + NONCE + PUB, b, PUB);
  uint8_t k[32];
  hkdf<SHA256>(k, sizeof(k), shared, sizeof(shared), key, strlen(key), info, sizeof(info));
  toHex(sessionKey, k);
  wipe(k, sizeof(k));
  wipe(shared, sizeof(shared));

  ready[0] = VERSION;
  memcpy(ready + 1, b, PUB);
  mac(ready + 1 + PUB, key, "e2e-ready", nonce, a, b);
  return true;
}

inline void challenge(uint8_t out[CHALLENGE_SIZE], Reason reason, const uint8_t nonce[NONCE], const uint8_t holder[HOLDER]) {
  out[0] = VERSION;
  out[1] = reason;
  memcpy(out + 2, nonce, NONCE);
  memcpy(out + 2 + NONCE, holder, HOLDER);
}

}  // namespace e2esession
