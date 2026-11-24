/*
 * Boho.h — shared-key authentication and encrypted messages on Ascon (NIST SP 800-232)
 *
 * Part of the IOSignal Arduino library since 10.0.0 (Boho 3; the separate boho-arduino library
 * keeps the classic 1.x code and is not used). Wire formats:
 *   login     SERVER_TIME_NONCE (13B) → AUTH_REQ (33B) → AUTH_RES (17B)
 *   session   ENC / ENC_E2E: 21-byte header, Ascon-AEAD128 with an 8-byte tag
 *   pack/E2E  ENC_PACK: 29-byte header (16-byte nonce with an 8-byte sender value)
 * Keys: Ascon-CXOF128 of the key string; a new session key at every login. No SHA-256.
 * The same bytes as the JS boho 3 package (its test/fixtures/boho-vectors.json).
 *
 * Author: Taeo Lee <sixgen@gmail.com>
 */

#ifndef Boho_h
#define Boho_h

#include "Arduino.h"
#include "BohoClock.h"
#include "BohoAscon.h"

#define MetaSize_AUTH_REQ 33
#define MetaSize_AUTH_RES 17
#define MetaSize_ENC 21
#define MetaSize_ENC_PACK 29

// malloc, or PSRAM on an ESP32 that has it (IOSignal's message buffers).
void* dynamic_alloc(size_t size);

class Boho : public BohoClock
{
  public:
    enum MsgType : uint8_t {
      SERVER_TIME_NONCE = 0xB0,
      AUTH_FAIL = 0xB3,
      AUTH_REQ = 0xB4,
      AUTH_RES = 0xB5,
      ENC_PACK = 0xB9,
      ENC_E2E = 0xBA,
      ENC = 0xBB,
    };


    Boho(void);
    void clearAuth(void);
    void set_id8(const char* id);                 // at most 8 bytes
    void set_key(const char* key);
    void set_key(const void* key, size_t len);
    void set_id_key(const char* id_key);          // "id.key"

    // Login. Uses the last validated SERVER_TIME_NONCE; returns 0 if none is cached.
    int auth_req(uint8_t* out);                   // out: MetaSize_AUTH_REQ bytes
    int auth_req(uint8_t* out, const uint8_t* server_time_nonce, size_t len);
    bool verify_auth_res(const uint8_t* auth_res, size_t len);

    // Session messages after login. type is ENC (whole message) or ENC_E2E (routing
    // header only; the caller appends the ENC_PACK body). out: len + MetaSize_ENC bytes,
    // in may lie anywhere inside out. Returns the packet size, 0 on failure.
    uint32_t encrypt_session(uint8_t* out, const void* in, uint32_t len, uint8_t type = ENC);
    // Opens ENC (exact length) or the header of ENC_E2E (body after it is left alone).
    // out needs room for the declared length and may equal in. Returns true for a verified
    // message, including an empty one.
    bool decrypt_session(void* out, const uint8_t* in, uint32_t len, uint32_t& plainLen);

    // ENC_PACK with this connection's key, or with a separate E2E key string.
    // out: len + MetaSize_ENC_PACK bytes.
    uint32_t encryptPack(uint8_t* out, const void* in, uint32_t len);
    bool decryptPack(void* out, const uint8_t* in, uint32_t len, uint32_t& plainLen);
    uint32_t encrypt_e2e(uint8_t* out, const void* in, uint32_t len, const char* key);
    bool decrypt_e2e(void* out, const uint8_t* in, uint32_t len, const char* key, uint32_t& plainLen);

    bool isAuthorized = false;

  protected:
    // Tests only: a login with a given client nonce, and a given sender value. Never reuse.
    int authReqWithNonce(uint8_t* out, const uint8_t* clientNonce8);
    void setSender(const uint8_t* sender8) { memcpy(_sender, sender8, 8); hasSender = true; }

  private:
    uint32_t sealPack(uint8_t* out, const void* in, uint32_t len, const uint8_t* key);
    bool openPack(void* out, const uint8_t* in, uint32_t len, const uint8_t* key, uint32_t& plainLen);
    void freshNonce(uint8_t* nonce8);
    void makeSender(void);

    uint8_t _id8[8] = {0};
    uint8_t _key[ASCON_KEY_SIZE] = {0};
    uint8_t _sessionKey[ASCON_KEY_SIZE] = {0};
    uint8_t _serverProof[16] = {0};
    uint8_t _sender[8] = {0};
    bool hasKey = false;
    bool hasSender = false;
    bool hasPendingLogin = false;
};

#endif
