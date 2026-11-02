#ifndef CongPacket_h
#define CongPacket_h

union U32_BUF {  
  uint32_t u32;
  uint16_t u16;  
  uint8_t buf[4]; 
};  // Union: uint32, uint16 and 4bytes buffer

class CongPacket
{
  public:
  enum MsgType : uint8_t{
    TYPE_LEN1 = 1,
    TYPE_LEN2 = 2,
    TYPE_LEN3 = 3,
    TYPE_LEN4 = 4
  };
  
  enum ReadingState: uint8_t {
    HEAD = 0,
    LEN1 = 1,
    LEN2 = 2,
    LEN3 = 3,
    LEN4 = 4,
    BODY = 5,
    READY = 6,
    SKIP = 7,  // discarding the body of a frame larger than the RX buffer
    // From 0xFA(250) connection will be disconnected. 
    UNKNOWN_TYPE = 253,
    OVER_SIZE = 254,
    OVER_FLOW = 255
  };

  CongPacket();
  void init( Stream * _client);
  void run();
  void drop();
  void fill_head();
  void fill_len( uint8_t lenBytes );
  void fill_body();
  void fill_skip();
  bool ready();
  void clear();
  void setBufferSize(uint8_t * buf, size_t size);
  void send(const uint8_t* buffer, uint32_t size );
  Stream *client = nullptr;
  uint8_t * _buffer = nullptr;
  size_t _bufferSize = 0;
  size_t _bufferIndex = 0;
  size_t _payloadLength = 0;
  bool _ready {false};
  // Set when a frame larger than the RX buffer starts being skipped; the owner reports and resets it.
  bool _skipped {false};
  uint8_t _state;
  union U32_BUF packetLength;
};

#endif
