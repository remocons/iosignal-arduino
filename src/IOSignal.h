
#ifndef IOSignal_h
#define IOSignal_h

#include <Arduino.h>
#include <CongPacket.h>
#include <Boho.h>
#include "Client.h"

#define DEFAULT_TX_BUF_SIZE 50
#define MAX_CID_LEN 12

// USE_PSRAM: If you need large size memory on ESP boards which has PSRAM.
// #define USE_PSRAM

// # define IO_OPENING       0
# define IO_OPEN          1
// # define IO_CLOSING       2
# define IO_CLOSED        3
# define IO_SERVER_READY  4
# define IO_AUTH_FAIL     5
# define IO_AUTH_READY    6
# define IO_READY         7
# define IO_REDIRECTING   8
  
// enum class STATES
// {
//   OPENING,      // 0
//   OPEN,         // 1
//   CLOSING,      // 2
//   CLOSED,       // 3
//   SERVER_READY, // 4
//   AUTH_FAIL,    // 5
//   AUTH_READY,   // 6
//   READY,        // 7
//   REDIRECTING   // 8
// };

class IOSignal final : public Boho
{
public:
  IOSignal(void);
  ~IOSignal();

  enum MsgType : uint8_t
  {
    /* ###############
     * Not recommended 0~127dec.
     * 0~31: control code
     * 32~126: ascii charactor
     * 127: DEL
     */

    ADMIN_REQ = 0xA0,
    ADMIN_RES = 0xA1,

    // B. Boho is using Bx.
    // AUTH_REQ = 0xB0,
    // AUTH_NONCE,
    // AUTH_HMAC,
    // AUTH_ACK,
    // AUTH_FAIL,
    // AUTH_EXT,
    // ENC_PACK,
    // ENC_E2E,
    // ENC_488,

    // C. IOSignal status contorl.
    SERVER_READY = 0xC0,
    CID_REQ = 0xC1,
    CID_RES = 0xC2,
    QUOTA_LEVEL = 0xC3,
    SERVER_CLEAR_AUTH = 0xC4,
    SERVER_REDIRECT = 0xC5,
    // ..
    LOOP = 0xCB,
    ECHO = 0xCC,
    PING = 0xCD,
    PONG = 0xCE,
    CLOSE = 0xCF,
    // ~CF

    // D. IOSignal data signaling
    SIGNAL = 0xD0,
    SIGNAL_REQ = 0xD1,
    SIGNAL_E2E = 0xD2,
    SUBSCRIBE = 0xD3,
    SUBSCRIBE_REQ = 0xD4,
    UNSUBSCRIBE = 0xD5,
    SERVER_SIGNAL = 0xD6,
    // ..
    IAM = 0xD9,
    IAM_RES = 0xDA,
    //..
    SET = 0xDB, // setting server database.
    RESPONSE_CODE = 0xDC,
    RESPONSE_MBP = 0xDD,
    REQUEST = 0xDE,
    RESPONSE = 0xDF,
    // ~DF

    // F. Framing Flow control related framing protocol.(CongPacket)
    FLOW_MODE = 0xF0,
    WAIT = 0xF1,
    RESUME = 0xF2,
 
    SERVER_REQ_CLOSE = 0xFA,
    TIME_OUT = 0xFD,
    OVER_SIZE = 0xFE,
    OVER_FLOW = 0xFF
  };

  enum PAYLOAD_TYPE : uint8_t
  {
    EMPTY = 0,
    TEXT = 1,
    BINARY = 2,
    OBJECT = 3, // one stringify able object. no buffer.
    MJSON = 4,  // multiple stringify able obejct.  JSON string. with top levle array , no buffer
    MBA = 5     // can contains buffer.
  };

  enum ENC_MODE : uint8_t
  {
    NO = 0,
    YES = 1,
    AUTO = 2
  };


  void loop();
  void begin(Client *client, const char *_host, uint16_t _port);
  void setRxBuffer(size_t size);
  void write(const uint8_t *buffer, uint32_t size);
  void send(const uint8_t *buffer, uint32_t size);
  void send_enc_mode(const uint8_t *buffer, uint32_t size);
  void ping();
  void pong();
  void login(const char *auth_id, const char *auth_key);
  void auth(const char *auth_id, const char *auth_key);
  void auth(const char *auth_id_key);
  uint8_t useAuth;
  void set(const char *setString);
  void subscribe(const char *tag);
  void signal(const char *tag);
  void signal(const char *tag, const char *data);
  void signal(const char *tag, const char *data1, const char *data2);
  void signal(const char *tag, const uint8_t *data, uint32_t dataLen);
  void signal_e2e(const char *tag, const uint8_t *data, uint32_t dataLen, const char *dataKey);
  void signal2(const char *target, const char *topic, const char *data);
  void signal2(const char *target, const char *topic, const char *data1, const char *data2);
  void signal2(const char *target, const char *topic, const uint8_t *data, uint32_t dataLen);
  void signal2_e2e(const char *target, const char *topic, const uint8_t *data, uint32_t dataLen, const char *dataKey);
  void onMessage(void (*messageCallback)(char *, uint8_t, uint8_t *, size_t));
  void onReady(void (*readyCallback)(void));
  void close(uint8_t reason);
  void clear(void);

  uint8_t _buffer[DEFAULT_TX_BUF_SIZE];
  union u32buf4 packetLength;
  char cid[MAX_CID_LEN + 1];
  uint32_t lastTxRxTime = 0; // seconds
  uint32_t pingPeriod = 50;  // seconds
  uint8_t encMode = IOSignal::ENC_MODE::AUTO;
  uint8_t state;

  CongPacket cong;
  Client *client;
  const char *_host;
  uint16_t _port = 55488L;

private:
  void (*messageCallback)(char *, uint8_t, uint8_t *, size_t);
  void (*readyCallback)(void);
};

#endif
