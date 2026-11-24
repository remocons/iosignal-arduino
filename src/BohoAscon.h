/*
 * BohoAscon.h — Ascon (NIST SP 800-232) for Boho
 *
 * Ascon-AEAD128, Ascon-Hash256, Ascon-XOF128 and Ascon-CXOF128 on one 320-bit permutation.
 * A size-first portable implementation for 8-bit AVR (Uno) up to ESP32.
 * Used by Boho (Boho.h). Usable on its own as well.
 *
 * Author: Taeo Lee <sixgen@gmail.com>
 */

#ifndef BohoAscon_h
#define BohoAscon_h

#include <stddef.h>
#include <stdint.h>

#define ASCON_KEY_SIZE 16
#define ASCON_NONCE_SIZE 16
#define ASCON_TAG_SIZE 16
#define ASCON_HASH_SIZE 32

// Encrypts len bytes of in to out (out may equal in) and writes a 16-byte tag.
// The (key, nonce) pair must never be used for two different messages.
void ascon_aead128_encrypt(uint8_t *out, uint8_t *tag,
                           const uint8_t *in, size_t len,
                           const uint8_t *ad, size_t adLen,
                           const uint8_t *key, const uint8_t *nonce);

// Decrypts len bytes and checks the first tagLen (4..16) bytes of the tag in
// constant time. On failure out is zeroed and false is returned. out may equal in.
bool ascon_aead128_decrypt(uint8_t *out,
                           const uint8_t *in, size_t len,
                           const uint8_t *ad, size_t adLen,
                           const uint8_t *tag, uint8_t tagLen,
                           const uint8_t *key, const uint8_t *nonce);

// Ascon-Hash256: 32-byte digest.
void ascon_hash256(uint8_t *digest, const void *data, size_t len);

// Ascon-XOF128: outLen bytes of output.
void ascon_xof128(uint8_t *out, size_t outLen, const void *data, size_t len);

// Ascon-CXOF128: XOF with a customization string of at most 256 bytes.
// Returns false (and writes nothing) when the customization string is too long.
#define ASCON_CXOF_MAX_CUSTOM 256
bool ascon_cxof128(uint8_t *out, size_t outLen, const void *data, size_t len,
                   const void *custom, size_t customLen);

#endif
