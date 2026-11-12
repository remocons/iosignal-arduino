// Improv Wi-Fi Serial packets (https://www.improv-wifi.com/serial/), without Arduino dependencies.
// Packet: "IMPROV", version 1, type, length, data, checksum (sum of all previous bytes).
#ifndef IOSignalImprov_h
#define IOSignalImprov_h

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace improv {

enum Type : uint8_t { TYPE_STATE = 0x01, TYPE_ERROR = 0x02, TYPE_RPC = 0x03, TYPE_RESULT = 0x04 };
enum State : uint8_t { STATE_READY = 0x02, STATE_PROVISIONING = 0x03, STATE_PROVISIONED = 0x04 };
enum Error : uint8_t {
  ERROR_NONE = 0x00,
  ERROR_INVALID_RPC = 0x01,
  ERROR_UNKNOWN_RPC = 0x02,
  ERROR_UNABLE_TO_CONNECT = 0x03,
  ERROR_UNKNOWN = 0xFF
};
enum Command : uint8_t { CMD_WIFI_SETTINGS = 0x01, CMD_CURRENT_STATE = 0x02, CMD_DEVICE_INFO = 0x03, CMD_SCAN = 0x04 };

static const uint8_t VERSION = 1;
static const char HEADER[] = "IMPROV";
static const size_t HEADER_SIZE = 6;
// Header, version, type, length, 255 data bytes, checksum.
static const size_t MAX_PACKET = HEADER_SIZE + 3 + 255 + 1;

// Writes one packet into out (at least size + 10 bytes) and returns its length.
inline size_t packet(uint8_t* out, uint8_t type, const uint8_t* data, uint8_t size) {
  memcpy(out, HEADER, HEADER_SIZE);
  out[6] = VERSION;
  out[7] = type;
  out[8] = size;
  if (size) memcpy(out + 9, data, size);
  uint8_t sum = 0;
  for (size_t i = 0; i < 9u + size; i++) sum += out[i];
  out[9 + size] = sum;
  return 10u + size;
}

// RPC result data: command, total length, then each string with its length byte.
// Returns 0 when the strings do not fit in 253 bytes or in out (capacity bytes).
inline size_t resultData(uint8_t* out, size_t capacity, uint8_t command, const char* const* strings, size_t count) {
  if (capacity < 2) return 0;
  size_t at = 2;
  for (size_t i = 0; i < count; i++) {
    size_t len = strings[i] ? strlen(strings[i]) : 0;
    if (len > 255 || at + 1 + len > capacity || at + 1 + len > 255) return 0;
    out[at++] = (uint8_t)len;
    if (len) memcpy(out + at, strings[i], len);
    at += len;
  }
  out[0] = command;
  out[1] = (uint8_t)(at - 2);
  return at;
}

// Reads Improv packets from a byte stream mixed with other text. Bytes outside packets are ignored.
// Only RPC commands (client to device) with data up to Capacity bytes are kept.
template <size_t Capacity = 128>
class Parser {
 public:
  enum Result { NONE, COMMAND, BAD_PACKET };

  // Feeds one byte. COMMAND: command(), data(), size() hold a complete RPC command.
  // BAD_PACKET: a packet with the right header failed its checksum or layout.
  Result push(uint8_t b) {
    if (_at < HEADER_SIZE) {
      if (b == (uint8_t)HEADER[_at]) {
        _sum += b;
        _at++;
      } else {
        reset();
        if (b == (uint8_t)HEADER[0]) { _sum = b; _at = 1; }
      }
      return NONE;
    }
    if (_at == HEADER_SIZE) {  // version
      if (b != VERSION) {
        reset();
        if (b == (uint8_t)HEADER[0]) { _sum = b; _at = 1; }  // "IMPROVIMPROV\x01..."
        return NONE;
      }
      _sum += b; _at++;
      return NONE;
    }
    if (_at == HEADER_SIZE + 1) { _type = b; _sum += b; _at++; return NONE; }
    if (_at == HEADER_SIZE + 2) { _length = b; _sum += b; _at++; _read = 0; return NONE; }
    if (_read < _length) {
      if (_read < Capacity) _buf[_read] = b;
      _read++;
      _sum += b;
      return NONE;
    }
    // Checksum byte.
    const bool ok = b == _sum;
    const uint8_t type = _type, length = _length;
    reset();
    if (!ok) return BAD_PACKET;
    if (type != TYPE_RPC) return NONE;  // device-side packets echoed back, or other types
    if (length > Capacity || length < 2 || (size_t)_buf[1] + 2 != length) return BAD_PACKET;
    _command = _buf[0];
    _size = _buf[1];
    return COMMAND;
  }

  // True between packets: the last byte did not start or continue one.
  bool idle() const { return _at == 0; }

  uint8_t command() const { return _command; }
  const uint8_t* data() const { return _buf + 2; }
  uint8_t size() const { return _size; }

  // Splits Wi-Fi settings data into NUL-terminated SSID (<= 32) and password (<= 64).
  bool wifiSettings(char* ssid, char* pass) const {
    const uint8_t* d = data();
    if (_size < 2) return false;
    uint8_t ssidLen = d[0];
    if (ssidLen == 0 || ssidLen > 32 || 1u + ssidLen + 1u > _size) return false;
    uint8_t passLen = d[1 + ssidLen];
    if (passLen > 64 || 2u + ssidLen + passLen != _size) return false;
    memcpy(ssid, d + 1, ssidLen);
    ssid[ssidLen] = 0;
    memcpy(pass, d + 2 + ssidLen, passLen);
    pass[passLen] = 0;
    return true;
  }

 private:
  void reset() { _at = 0; _sum = 0; _type = 0; _length = 0; _read = 0; }
  uint8_t _buf[Capacity];
  size_t _at = 0;
  uint8_t _sum = 0, _type = 0, _length = 0;
  size_t _read = 0;
  uint8_t _command = 0, _size = 0;
};

}  // namespace improv

#endif
