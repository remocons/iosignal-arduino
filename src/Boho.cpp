/*
 * Boho.cpp — shared-key authentication and encrypted messages on Ascon (NIST SP 800-232)
 *
 * Author: Taeo Lee <sixgen@gmail.com>
 */

#if defined(ESP32)
  #include "esp_random.h"
  #include "esp_heap_caps.h"
  #include "esp32-hal-psram.h"
#endif

#include "Boho.h"
#include <stdlib.h>
#if defined(__AVR__)
  #include <avr/pgmspace.h>
#endif

namespace {

// Customization strings of Ascon-CXOF128 (design section 2), kept in flash on AVR.
#if defined(__AVR__)
  const char KEY_CUSTOM[] PROGMEM = "boho/v2/key";
  const char SESSION_CUSTOM[] PROGMEM = "boho/v2/session";
  #define BOHO_COPY_CUSTOM(dst, src) memcpy_P((dst), (src), sizeof(src) - 1)
#else
  const char KEY_CUSTOM[] = "boho/v2/key";
  const char SESSION_CUSTOM[] = "boho/v2/session";
  #define BOHO_COPY_CUSTOM(dst, src) memcpy((dst), (src), sizeof(src) - 1)
#endif

const uint8_t DIR_CLIENT_TO_SERVER = 1;
const uint8_t DIR_SERVER_TO_CLIENT = 2;
const uint8_t TAG_SIZE = 8;

void wipe(void *p, size_t n)
{
  volatile uint8_t *v = (volatile uint8_t *)p;
  while (n--) *v++ = 0;
}

bool sameBytes(const uint8_t *a, const uint8_t *b, uint8_t n)
{
  uint8_t diff = 0;
  for (uint8_t i = 0; i < n; ++i) diff |= a[i] ^ b[i];
  return diff == 0;
}

uint32_t readU32(const uint8_t *p)
{
  u32buf4 v;
  memcpy(v.buf, p, 4);
  return v.u32;
}

uint16_t readU16(const uint8_t *p)
{
  u16buf2 v;
  memcpy(v.buf, p, 2);
  return v.u16;
}

// Key string → 16-byte key: Ascon-CXOF128(Z = "boho/v2/key").
void deriveKey(uint8_t *key, const void *text, size_t len)
{
  char custom[sizeof(KEY_CUSTOM) - 1];
  BOHO_COPY_CUSTOM(custom, KEY_CUSTOM);
  ascon_cxof128(key, ASCON_KEY_SIZE, text, len, custom, sizeof(custom));
}

// The 16-byte session nonce: the header's clock (bytes 5..12) and the direction.
void sessionNonce(uint8_t *nonce, const uint8_t *header, uint8_t dir)
{
  memset(nonce, 0, ASCON_NONCE_SIZE);
  memcpy(nonce, header + 5, 8);
  nonce[8] = dir;
}

}  // namespace

Boho::Boho() {}

void Boho::clearAuth(void)
{
  wipe(_id8, sizeof(_id8));
  wipe(_key, sizeof(_key));
  wipe(_sessionKey, sizeof(_sessionKey));
  wipe(_serverProof, sizeof(_serverProof));
  isAuthorized = false;
  hasKey = false;
  hasPendingLogin = false;
  clearClock();
}

void Boho::set_id8(const char *id)
{
  memset(_id8, 0, sizeof(_id8));
  if (!id) return;
  size_t len = 0;
  while (len < sizeof(_id8) && id[len]) ++len;
  memcpy(_id8, id, len);
}

void Boho::set_key(const char *key)
{
  set_key(key, key ? strlen(key) : 0);
}

// A new key ends the session and any login in progress.
void Boho::set_key(const void *key, size_t len)
{
  isAuthorized = false;
  hasPendingLogin = false;
  wipe(_sessionKey, sizeof(_sessionKey));
  if (!key || !len) {
    wipe(_key, sizeof(_key));
    hasKey = false;
    return;
  }
  deriveKey(_key, key, len);
  hasKey = true;
}

void Boho::set_id_key(const char *id_key)
{
  if (!id_key) { clearAuth(); return; }
  const char *dot = strchr(id_key, '.');
  if (!dot || dot == id_key || dot - id_key > 8 || !dot[1]) {
    clearAuth();
    return;
  }
  char id[9] = {0};
  memcpy(id, id_key, dot - id_key);
  set_id8(id);
  set_key(dot + 1);
}

int Boho::auth_req(uint8_t *out)
{
  if (!hasChallenge) return 0;
  return auth_req(out, serverChallenge, sizeof(serverChallenge));
}

int Boho::auth_req(uint8_t *out, const uint8_t *challenge, size_t len)
{
  if (!hasKey || !out || !challenge || len != MetaSize_SERVER_TIME_NONCE ||
      challenge[0] != SERVER_TIME_NONCE || readU16(challenge + 5) >= 1000) return 0;
  if (challenge != serverChallenge) {
    setClientTimeToServerTime(challenge, len);
    memcpy(counter.buf, challenge + 7, 2);
  }
  resetServerClock();
  uint8_t nonce[8];
  freshNonce(nonce);
  const int size = authReqWithNonce(out, nonce);
  wipe(nonce, sizeof(nonce));
  return size;
}

// AUTH_REQ = type | id8 | clientNonce8 | clientProof16.
// CXOF128(Z = "boho/v2/session", key | challenge | id8 | clientNonce) gives
// sessionKey16 | clientProof16 | serverProof16.
int Boho::authReqWithNonce(uint8_t *out, const uint8_t *clientNonce8)
{
  if (!hasKey || !hasChallenge || !out || !clientNonce8) return 0;
  uint8_t input[ASCON_KEY_SIZE + MetaSize_SERVER_TIME_NONCE + 16];
  memcpy(input, _key, ASCON_KEY_SIZE);
  memcpy(input + 16, serverChallenge, MetaSize_SERVER_TIME_NONCE);
  memcpy(input + 29, _id8, 8);
  memcpy(input + 37, clientNonce8, 8);
  char custom[sizeof(SESSION_CUSTOM) - 1];
  BOHO_COPY_CUSTOM(custom, SESSION_CUSTOM);
  uint8_t derived[48];
  ascon_cxof128(derived, sizeof(derived), input, sizeof(input), custom, sizeof(custom));
  memcpy(_sessionKey, derived, 16);
  memcpy(_serverProof, derived + 32, 16);
  out[0] = AUTH_REQ;
  memcpy(out + 1, _id8, 8);
  memcpy(out + 9, clientNonce8, 8);
  memcpy(out + 17, derived + 16, 16);
  wipe(input, sizeof(input));
  wipe(derived, sizeof(derived));
  isAuthorized = false;
  hasPendingLogin = true;
  return MetaSize_AUTH_REQ;
}

bool Boho::verify_auth_res(const uint8_t *res, size_t len)
{
  if (!hasKey || !hasPendingLogin || !res || len != MetaSize_AUTH_RES || res[0] != AUTH_RES) return false;
  if (!sameBytes(res + 1, _serverProof, 16)) return false;
  wipe(_serverProof, sizeof(_serverProof));
  hasPendingLogin = false;
  isAuthorized = true;
  serverClockActive = true;
  return true;
}

uint32_t Boho::encrypt_session(uint8_t *out, const void *in, uint32_t len, uint8_t type)
{
  if (!isAuthorized || !hasKey || !out || (!in && len) || (type != ENC && type != ENC_E2E) ||
      len > (uint32_t)((size_t)-1) - MetaSize_ENC) return 0;
  uint8_t header[13];
  header[0] = type;
  u32buf4 size;
  size.u32 = len;
  memcpy(header + 1, size.buf, 4);
  if (!nextSendClock(header + 5)) return 0;
  uint8_t nonce[ASCON_NONCE_SIZE], tag[ASCON_TAG_SIZE];
  sessionNonce(nonce, header, DIR_CLIENT_TO_SERVER);
  if (len) memmove(out + MetaSize_ENC, in, len);
  ascon_aead128_encrypt(out + MetaSize_ENC, tag, out + MetaSize_ENC, len, header, sizeof(header), _sessionKey, nonce);
  memcpy(out, header, sizeof(header));
  memcpy(out + 13, tag, TAG_SIZE);
  return len + MetaSize_ENC;
}

bool Boho::decrypt_session(void *out, const uint8_t *in, uint32_t len, uint32_t &plainLen)
{
  plainLen = 0;
  if (!isAuthorized || !hasKey || !in || len < MetaSize_ENC || len > (uint32_t)((size_t)-1) ||
      (in[0] != ENC && in[0] != ENC_E2E)) return false;
  const uint32_t declared = readU32(in + 1);
  const uint32_t room = len - MetaSize_ENC;
  if (declared > room || (in[0] == ENC && declared != room) || (!out && declared)) return false;
  if (readU16(in + 9) >= 1000) return false;
  const uint32_t receivedAt = millis();
  uint8_t header[MetaSize_ENC];  // copied: out may equal in
  memcpy(header, in, sizeof(header));
  uint8_t nonce[ASCON_NONCE_SIZE];
  sessionNonce(nonce, header, DIR_SERVER_TO_CLIENT);
  if (!ascon_aead128_decrypt((uint8_t *)out, in + MetaSize_ENC, declared, header, 13,
                             header + 13, TAG_SIZE, _sessionKey, nonce)) return false;
  observeServerClock(readU32(header + 5), readU16(header + 9), readU16(header + 11), receivedAt);
  plainLen = declared;
  return true;
}

uint32_t Boho::encryptPack(uint8_t *out, const void *in, uint32_t len)
{
  if (!hasKey) return 0;
  return sealPack(out, in, len, _key);
}

bool Boho::decryptPack(void *out, const uint8_t *in, uint32_t len, uint32_t &plainLen)
{
  plainLen = 0;
  if (!hasKey) return false;
  return openPack(out, in, len, _key, plainLen);
}

uint32_t Boho::encrypt_e2e(uint8_t *out, const void *in, uint32_t len, const char *key)
{
  if (!key || !*key) return 0;
  uint8_t k[ASCON_KEY_SIZE];
  deriveKey(k, key, strlen(key));
  const uint32_t size = sealPack(out, in, len, k);
  wipe(k, sizeof(k));
  return size;
}

bool Boho::decrypt_e2e(void *out, const uint8_t *in, uint32_t len, const char *key, uint32_t &plainLen)
{
  plainLen = 0;
  if (!key || !*key) return false;
  uint8_t k[ASCON_KEY_SIZE];
  deriveKey(k, key, strlen(key));
  const bool ok = openPack(out, in, len, k, plainLen);
  wipe(k, sizeof(k));
  return ok;
}

// ENC_PACK = type | len4 | nonce16 (sec4 | ms2 | counter2 | sender8) | tag8 | ciphertext.
uint32_t Boho::sealPack(uint8_t *out, const void *in, uint32_t len, const uint8_t *key)
{
  if (!out || (!in && len) || len > (uint32_t)((size_t)-1) - MetaSize_ENC_PACK) return 0;
  uint8_t header[21];
  header[0] = ENC_PACK;
  u32buf4 size;
  size.u32 = len;
  memcpy(header + 1, size.buf, 4);
  if (!nextSendClock(header + 5)) return 0;
  if (!hasSender) makeSender();
  memcpy(header + 13, _sender, 8);
  uint8_t tag[ASCON_TAG_SIZE];
  if (len) memmove(out + MetaSize_ENC_PACK, in, len);
  ascon_aead128_encrypt(out + MetaSize_ENC_PACK, tag, out + MetaSize_ENC_PACK, len, header, 5, key, header + 5);
  memcpy(out, header, sizeof(header));
  memcpy(out + 21, tag, TAG_SIZE);
  return len + MetaSize_ENC_PACK;
}

bool Boho::openPack(void *out, const uint8_t *in, uint32_t len, const uint8_t *key, uint32_t &plainLen)
{
  plainLen = 0;
  if (!in || len < MetaSize_ENC_PACK || len > (uint32_t)((size_t)-1) || in[0] != ENC_PACK) return false;
  const uint32_t declared = readU32(in + 1);
  if (declared != len - MetaSize_ENC_PACK || (!out && declared) || readU16(in + 9) >= 1000) return false;
  uint8_t header[MetaSize_ENC_PACK];  // copied: out may equal in
  memcpy(header, in, sizeof(header));
  if (!ascon_aead128_decrypt((uint8_t *)out, in + MetaSize_ENC_PACK, declared, header, 5,
                             header + 21, TAG_SIZE, key, header + 5)) return false;
  plainLen = declared;
  return true;
}

// The client nonce of a login. ESP32, ESP8266: the chip's random numbers. Other boards (Uno):
// the timing gathered since boot and the setNonceSeed() value, hashed with the key and the
// server's challenge, so it cannot be worked out or steered without the key.
void Boho::freshNonce(uint8_t *nonce8)
{
#if defined(ESP32)
  for (uint8_t i = 0; i < 8; i += 4) { const uint32_t r = esp_random(); memcpy(nonce8 + i, &r, 4); }
#elif defined(ESP8266)
  for (uint8_t i = 0; i < 8; i += 4) { const uint32_t r = RANDOM_REG32 ^ micros(); memcpy(nonce8 + i, &r, 4); }
#else
  stirNonce();
  uint8_t input[ASCON_KEY_SIZE + MetaSize_SERVER_TIME_NONCE + sizeof(nonceSource)];
  memcpy(input, _key, ASCON_KEY_SIZE);
  memcpy(input + 16, serverChallenge, MetaSize_SERVER_TIME_NONCE);
  memcpy(input + 29, &nonceSource, sizeof(nonceSource));
  ascon_xof128(nonce8, 8, input, sizeof(input));
  wipe(input, sizeof(input));
#endif
}

// The sender value of ENC_PACK, fixed for this boot. Boards without random numbers mix the
// device ID and key, so devices sharing an E2E key still differ.
void Boho::makeSender(void)
{
#if defined(ESP32)
  for (uint8_t i = 0; i < 8; i += 4) { const uint32_t r = esp_random(); memcpy(_sender + i, &r, 4); }
#elif defined(ESP8266)
  for (uint8_t i = 0; i < 8; i += 4) { const uint32_t r = RANDOM_REG32 ^ micros(); memcpy(_sender + i, &r, 4); }
#else
  stirNonce();
  uint8_t input[8 + ASCON_KEY_SIZE + sizeof(nonceSource)];
  memcpy(input, _id8, 8);
  memcpy(input + 8, _key, ASCON_KEY_SIZE);
  memcpy(input + 24, &nonceSource, sizeof(nonceSource));
  ascon_xof128(_sender, 8, input, sizeof(input));
  wipe(input, sizeof(input));
#endif
  hasSender = true;
}

void* dynamic_alloc(size_t size)
{
#if defined(ESP32)
  if (psramFound()) return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  return malloc(size);
}
