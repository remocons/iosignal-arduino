
#include "IOSignal.h"
#include <Arduino.h>
#include <string.h>
#include "Client.h"

IOSignal::IOSignal()
{
  state = IO_CLOSED;
  setTime(0);
  packetLength.u32 = 0;
  cong = CongPacket();
}


void IOSignal::loop()
{
  refreshTime();

  if( !this->client->connected()){
      state = IO_CLOSED;
      clear();
      delay(3000); 
      if(this->client->connect( _host , _port) ){
        state = IO_OPEN;
      }
      return;
  }

  if ((getUnixTime() - lastTxRxTime) > pingPeriod)
  {
    ping();
    lastTxRxTime = getUnixTime();
  }

  cong.run();
  if(cong._state >= 250){
    if (errorCallback) {
        errorCallback(cong._state);
    }
    close( cong._state);
    this->client->stop();
    state = IO_CLOSED;
    return;
  } 
  
  if (!cong.ready()) return; 

  lastTxRxTime = getUnixTime();

  uint8_t *tmpbuf = NULL;
  uint32_t len = cong._payloadLength;
  uint8_t *message = (uint8_t *)cong._buffer;

  if (message[0] == Boho::MsgType::ENC_488)
  {
    tmpbuf = (uint8_t *)dynamic_alloc(len);

    if (tmpbuf == NULL)
    {
      cong.clear();
      return;
    }

    uint32_t decLen = decrypt_488(tmpbuf, (uint8_t *)cong._buffer, len);
    if (decLen)
    {
      message = tmpbuf;
      len = decLen;
    }
    else
    {
      if (tmpbuf != NULL) free(tmpbuf);
      cong.clear(); 
      return;
    }
  }
  else if (message[0] == Boho::MsgType::ENC_E2E)
  {
    /*
    !! End to End Encryption Message decryption.
    ** payload is enc_pack.
    ** application should decrypt payload with e2e key. use decrypt_e2e()
    */

    // Serial.println(F(">> E2"));
    memcpy(packetLength.buf, cong._buffer + 1, 4);
    int encHeaderSize = packetLength.u32;
    tmpbuf = (uint8_t *)dynamic_alloc(encHeaderSize);

    if (tmpbuf == NULL)
    {
      cong.clear();
      return;
    }

    uint32_t decLen = decrypt_488(tmpbuf, (uint8_t *)cong._buffer, len);
    if (decLen)
    {
      memcpy(cong._buffer + 21, tmpbuf, encHeaderSize);
      message = cong._buffer + 21;
      len = len - 21;
    }
    else
    {
      if (tmpbuf != NULL) free(tmpbuf);
      cong.clear();
      return;  
    }
  }

  uint8_t hd = message[0];

  switch (hd)
  {
  case IOSignal::MsgType::PING:
  {
    pong();
    break;
  }

  case IOSignal::MsgType::PONG:
  {
    break;
  }

  case IOSignal::MsgType::SIGNAL_E2E:
  case IOSignal::MsgType::SIGNAL:
  {
    int tagLen = message[1];
    int payloadLen = len - (tagLen + 3);
    uint8_t payloadType = message[2 + tagLen];

    char *tagStr = (char *)message + 2;
    message[2 + tagLen] = 0; // add null string. end of tagStr string.
    // If tagStr includes '@' charactor. it's uni-cast message.
    char *directMessageTopic = strchr(tagStr, '@');
    if (directMessageTopic)
    {
      if (messageCallback)
        messageCallback(directMessageTopic, payloadType, message + 3 + tagLen, payloadLen);
    }
    else
    {
      if (messageCallback)
        messageCallback(tagStr, payloadType, message + 3 + tagLen, payloadLen);
    }
    break;
  }

  case IOSignal::MsgType::CID_RES:
  {
    if (len >= 1 && len < MAX_CID_LEN)
    {
      memcpy(cid, message + 1, len - 1);
      cid[len] = 0;
      state = IO_READY;
    }
    else
    {
      // Serial.println(F("-- ERR: CID_RES SIZE LIMIT"));
      break;
    }

    if (readyCallback)
      readyCallback();
    break;
  }

  case Boho::MsgType::SERVER_TIME_NONCE: // SERVER_READY
  {
    state = IO_SERVER_READY;
    if (useAuth)
    {
      uint8_t authPack[MetaSize_SERVER_TIME_NONCE];
      size_t auth_len = auth_req(authPack, message, len);
      send(authPack, auth_len);
    }
    else
    {
      _buffer[0] = IOSignal::MsgType::CID_REQ;
      send(_buffer, 1);
    }
    break;
  }

  case IOSignal::MsgType::AUTH_CLEAR:
  {
    useAuth = false;
    clearAuth(); // Boho::clearAuth()
    if (tmpbuf != NULL) free(tmpbuf);
    close(IOSignal::MsgType::AUTH_CLEAR);
    if (errorCallback) {
        errorCallback( IO_AUTH_CLEAR);
    }
    return;
  }

  case IOSignal::MsgType::SERVER_REDIRECT:
  {
    if( len == 7){  // 1 MsgType, ip4, port2 
      state = IO_REDIRECTING;
      close(IOSignal::MsgType::SERVER_REDIRECT);
  
      char ipString[16];
      sprintf(ipString,  "%d.%d.%d.%d\0", message[1], message[2], message[3], message[4]);
      uint16_t port = (message[5] << 8 ) + message[6];
      
      if( this->client->connect( ipString , port ) ){
        state = IO_OPEN;
      }
      if (tmpbuf != NULL) free(tmpbuf);
      return;

    }else{
      break;
    }
  }

  case Boho::MsgType::AUTH_RES:
  {
    state = IO_AUTH_RES;
    if (verify_auth_res(message, len))
    {
      // Serial.println(F(">> AUTH_RES"));
      _buffer[0] = IOSignal::MsgType::CID_REQ;
      send(_buffer, 1);
    }
    else
    {
      if (errorCallback) {
          errorCallback( IO_AUTH_FAIL);
      }
      // Serial.println(F(">> WRONG SERVER AUTH_RES"));
    }
    break;
  }

  case Boho::MsgType::AUTH_FAIL:
    state = IO_AUTH_FAIL;
    // Serial.println(F(">> AUTH_FAIL"));
    if (errorCallback) {
        errorCallback( IO_AUTH_FAIL);
    }
    break;

  default:
    // printHex("<unkonwn:", message , len );
    break;
  }

  if (tmpbuf != NULL) free(tmpbuf);
  cong.clear();
  return;
}


void IOSignal::begin(Client *client, const char *_host, uint16_t _port )
{
  this->client = client;
  cong.init(client);
  this->_host = _host;
  this->_port = _port;
}


void IOSignal::setRxBuffer(size_t size)
{
  // Serial.println(F("-- setRxBuffer: "));
  if (_rx_buffer != nullptr) { // Free previous buffer if any
      free(_rx_buffer);
  }
  _rx_buffer = (uint8_t *)dynamic_alloc(size);

  if (_rx_buffer == NULL)
  {
    // Serial.println(F("\n-- NO RX_BUFFER!"));
    return;
  }
  cong.setBufferSize(_rx_buffer, size);
}

void IOSignal::write(const uint8_t *buffer, uint32_t size)
{
  this->client->write(buffer, size);
}

void IOSignal::send(const uint8_t *buffer, uint32_t size)
{
  cong.send(buffer, size);
  lastTxRxTime = getUnixTime();
}

void IOSignal::send_enc_mode(const uint8_t *buf, uint32_t bufSize)
{
  // useEncryption
  if (encMode == IOSignal::ENC_MODE::YES ||
      encMode == IOSignal::ENC_MODE::AUTO && isAuthorized)
  {
    uint8_t *enc_buf = (uint8_t *)dynamic_alloc(bufSize + 25);
    if (enc_buf == NULL)
      return;

    int packSize = encrypt_488(enc_buf, buf, bufSize);
    send(enc_buf, packSize);
    free(enc_buf);
    // ('<<  [ENC_488]')
  }
  else
  {
    // ('<<  [PLAIN]' )
    send(buf, bufSize);
  }
}

void IOSignal::ping()
{
  _buffer[0] = IOSignal::MsgType::PING;
  send(_buffer, 1);
}

void IOSignal::pong()
{
  _buffer[0] = IOSignal::MsgType::PONG;
  send(_buffer, 1);
}


void IOSignal::login(const char *auth_id, const char *auth_key)
{
  // instance manual login.
  set_id8(auth_id);
  set_key(auth_key);
  size_t size = auth_req(_buffer);
  send(_buffer, size);
}

void IOSignal::auth(const char *auth_id, const char *auth_key)
{
  // automatic
  // send auth_req when sever ready, re_auth when reconnect
  set_id8(auth_id);
  set_key(auth_key);
  useAuth = true;
}

void IOSignal::auth(const char *id_key)
{
  set_id_key( id_key );
  useAuth = true;
}


void IOSignal::subscribe(const char *tag)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;
  bool useDefaultBuffer = (2 + tagLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
    uint8_t *buf = (uint8_t *)dynamic_alloc(2 + tagLen);
    if (buf == NULL)
      return;
  }
  buf[0] = IOSignal::MsgType::SUBSCRIBE;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  send_enc_mode(buf, 2 + tagLen);

  if (!useDefaultBuffer)
    free(buf);
}

void IOSignal::signal2(const char *target, const char *topic, const char *data)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data);
}

void IOSignal::signal2(const char *target, const char *topic, const char *data1, const char *data2)
{
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data1, data2);
}

// signal : no payload
void IOSignal::signal(const char *tag)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;

  bool useDefaultBuffer = (2 + tagLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
    uint8_t *buf = (uint8_t *)dynamic_alloc(2 + tagLen);
    if (buf == NULL)
      return;
  }

  buf[0] = IOSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = IOSignal::PAYLOAD_TYPE::EMPTY;
  send_enc_mode(buf, 3 + tagLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : string payload.
void IOSignal::signal(const char *tag, const char *data)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint32_t dataLen = strlen(data) + 1; // put null charactor
  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
    buf = (uint8_t *)dynamic_alloc(3 + tagLen + dataLen);
    if (buf == NULL)
      return;
  }

  buf[0] = IOSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = IOSignal::PAYLOAD_TYPE::TEXT;
  memcpy(buf + 3 + tagLen, data, dataLen);
  send_enc_mode(buf, 3 + tagLen + dataLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : two string payload.  PAYLOAD_TYPE::MJSON (JSON array )
void IOSignal::signal(const char *tag, const char *data1, const char *data2)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  int data1Len = strlen(data1);
  int data2Len = strlen(data2);
  uint32_t dataLen = data1Len + data2Len + 7; // ["data1","data2"]

  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
    buf = (uint8_t *)dynamic_alloc(3 + tagLen + dataLen);    
    if (buf == NULL)
      return;
  }

  buf[0] = IOSignal::MsgType::SIGNAL;
  buf[1] = tagLen;
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = IOSignal::PAYLOAD_TYPE::MJSON;

  memcpy(buf + 3 + tagLen, "[\"", 2);
  memcpy(buf + 3 + tagLen + 2, data1, data1Len);
  memcpy(buf + 3 + tagLen + 2 + data1Len, "\",\"", 3);
  memcpy(buf + 3 + tagLen + 2 + data1Len + 3, data2, data2Len);
  memcpy(buf + 3 + tagLen + 2 + data1Len + 3 + data2Len, "\"]", 2);

  send_enc_mode(buf, 3 + tagLen + dataLen);

  if (!useDefaultBuffer)
    free(buf);
}

// siganl : binary payload.
void IOSignal::signal(const char *tag, const uint8_t *data, uint32_t dataLen)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag); // no count null char
  if (tagLen > 255)
    return;
  uint8_t *buf = NULL;

  bool useDefaultBuffer = (3 + tagLen + dataLen) <= DEFAULT_TX_BUF_SIZE;
  if (useDefaultBuffer)
  {
    buf = _buffer;
  }
  else
  {
    buf = (uint8_t *)dynamic_alloc(3 + tagLen + dataLen);
    if (buf == NULL)
      return;
  }

  buf[0] = IOSignal::MsgType::SIGNAL;
  buf[1] = tagLen; //  ch str len
  strcpy((char *)buf + 2, tag);
  buf[2 + tagLen] = IOSignal::PAYLOAD_TYPE::BINARY;
 

  memcpy(buf + 3 + tagLen, data, dataLen);
  send_enc_mode(buf, (3 + tagLen + dataLen));

  if (!useDefaultBuffer)
    free(buf);
}

void IOSignal::signal2(const char *target, const char *topic, const uint8_t *data, uint32_t dataLen)
{
  if ( state != IO_READY ) return;
  int targetLen = strlen(target);
  int topicLen = strlen(topic);
  char tag[targetLen + topicLen + 1];
  memcpy(tag, target, targetLen);
  memcpy(tag + targetLen, topic, topicLen);
  tag[targetLen + topicLen] = 0;
  signal(tag, data, dataLen);
}

void IOSignal::signal_e2e(const char *tag, const uint8_t *data, uint32_t dataLen, const char *dataKey)
{
  if ( state != IO_READY ) return;
  int tagLen = strlen(tag);
  if (tagLen > 255)
    return;

  uint8_t *buf = NULL;
  int dataOffset = 0;

  if (encMode == IOSignal::ENC_MODE::YES ||
      encMode == IOSignal::ENC_MODE::AUTO && isAuthorized)
  {
    // enc_e2e(21) + sig_e2e(3+tagLen+ enc_pack_payload)

    // encMode on:
    // buf size: enc_e2e header(21) + IOSignalHeader( 3+tagLen) |  encpack(25) + dataLen
    dataOffset = 21 + 3 + tagLen;
    buf = (uint8_t *)dynamic_alloc(dataOffset + 25 + dataLen);
    if (buf == NULL)
      return;

    uint8_t sigMsgHeader[3 + tagLen];
    sigMsgHeader[0] = IOSignal::MsgType::SIGNAL_E2E; // for end receiver.
    sigMsgHeader[1] = tagLen;
    strcpy((char *)sigMsgHeader + 2, tag);
    sigMsgHeader[2 + tagLen] = IOSignal::PAYLOAD_TYPE::BINARY;

    // encrypt sigHeader only. exclude payload(already encrypted)
    encrypt_488(buf, sigMsgHeader, 3 + tagLen);
    // now fills 21+3+tagLen.
    buf[0] = Boho::MsgType::ENC_E2E; // change type   .. for server
  }
  else
  {
    // encMode off:   IOSignalMsg Signal Header( 3+tagLen) | encpack(25) + dataLen
    dataOffset = 3 + tagLen;
    buf = (uint8_t *)dynamic_alloc(dataOffset + 25 + dataLen);
    if (buf == NULL)
      return;

    // IOSignal Header
    buf[0] = IOSignal::MsgType::SIGNAL_E2E;
    buf[1] = tagLen; //  ch str len
    strcpy((char *)buf + 2, tag);
    buf[2 + tagLen] = IOSignal::PAYLOAD_TYPE::BINARY;
  }

  uint32_t e2ePackSize = encrypt_e2e(buf + dataOffset, data, dataLen, dataKey);
  send(buf, dataOffset + e2ePackSize);
  free(buf);
}


void IOSignal::onMessage(void (*messageCallback)(char *, uint8_t, uint8_t *, size_t))
{
  this->messageCallback = messageCallback;
}

void IOSignal::onReady(void (*readyCallback)(void))
{
  this->readyCallback = readyCallback;
}

void IOSignal::onError(void (*errorCallback)(uint8_t))
{
  this->errorCallback = errorCallback;
}



void IOSignal::close(uint8_t reason)
{
  _buffer[0] = IOSignal::MsgType::CLOSE;
  _buffer[1] = reason;
  send(_buffer, 2);
  this->client->flush();
  clear();
  state = IO_CLOSED;
}

void IOSignal::clear(void)
{
  cong.drop();
  cong.clear();
  isAuthorized = false;
}


IOSignal::~IOSignal()
{
  if (_rx_buffer != nullptr) {
    free(_rx_buffer);
  }
}