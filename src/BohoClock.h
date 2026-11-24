/*
 * BohoClock.h — clock, server challenge and nonce source of Boho
 *
 * The client clock follows the server: the challenge sets it, authenticated
 * server packets steer it by at most 10%, and outgoing (time, counter) tuples
 * strictly increase. Both protocol versions put this tuple on the wire.
 *
 * Author: Taeo Lee <sixgen@gmail.com>
 */

#ifndef BohoClock_h
#define BohoClock_h

#include "Arduino.h"
#include <string.h>

#define MetaSize_SERVER_TIME_NONCE 13

union u32buf4{  uint32_t u32;  uint8_t buf[4]; };  // Union: uint32 & 4bytes buffer
union u16buf2{  uint16_t u16;  uint8_t buf[2]; };  // Union: uint16 & 2bytes buffer

class BohoClock
{
  public:
    BohoClock(void);

    void setTime( uint32_t utc, uint16_t ms = 0);
    void setClientTimeToServerTime( const uint8_t* server_time_nonce , size_t len);
    // Authenticated server packets steer this clock by at most 10%, without
    // stepping backward. Packet layouts and explicit setTime() remain unchanged.
    void refreshTime( void );
    uint32_t getUnixTime();
    uint16_t getMilTime();
    // Boards without hardware random numbers (Uno): a value that differs at every boot, for example a
    // counter kept in EEPROM, makes the login nonce differ from every earlier boot. Call it before the
    // first login. ESP32 and ESP8266 use the chip's random numbers and ignore it.
    void setNonceSeed( uint32_t seed ) { nonceSource.seed = seed; }

  protected:
    // Next outgoing tuple: sec4 | ms2 | counter2, strictly after the last one.
    // Returns false (and sets sendClockValid false) when the 32-bit seconds are exhausted.
    bool nextSendClock( uint8_t *clock8 );
    void observeServerClock(uint32_t seconds, uint16_t milliseconds,
                            uint16_t sequence, uint32_t receivedAt);
    void clearClock( void );
    // Stop steering by server packets until the next login is verified.
    void resetServerClock( void );
    void stirNonce( void );

    bool serverClockActive = false;
    bool sendClockValid = true;
    bool hasChallenge = false;
    uint8_t serverChallenge[MetaSize_SERVER_TIME_NONCE] = {0};
    union u32buf4 secTime;
    union u16buf2 milTime;
    union u16buf2 counter;
    uint32_t lastTime;
    // What the nonce is hashed from on boards without hardware random numbers.
    struct { uint32_t timing; uint32_t seed; } nonceSource = {0, 0}; // refreshTime() and auth_req(), setNonceSeed()

  private:
    bool hasServerSample = false;
    uint64_t lastServerTimeMs = 0;
    uint16_t lastServerCounter = 0;
    int64_t clockCorrectionMs = 0;
    uint8_t correctionRemainder = 0;
    bool hasSendClock = false;
    uint64_t lastSendTimeMs = 0;
    uint16_t lastSendCounter = 0;
};

inline BohoClock::BohoClock()
{
  secTime.u32 = 0;
  milTime.u16 = 0;
  counter.u16 = 0;
  lastTime = millis();
}

inline void BohoClock::clearClock(void)
{
  hasChallenge = false;
  memset(serverChallenge, 0, sizeof(serverChallenge));
  serverClockActive = false;
  hasServerSample = false;
  clockCorrectionMs = 0;
  correctionRemainder = 0;
  hasSendClock = false;
  sendClockValid = true;
}

inline void BohoClock::resetServerClock(void)
{
  serverClockActive = false;
  hasServerSample = false;
  clockCorrectionMs = 0;
  correctionRemainder = 0;
}

inline void BohoClock::refreshTime( void)
{
  const uint32_t now = millis();
  const uint32_t delta = now - lastTime; // unsigned subtraction includes rollover
  lastTime = now;
#if !defined(ESP32) && !defined(ESP8266)
  // No hardware random numbers: gather the timing of every call (see the login nonce).
  stirNonce();
#endif
  ++counter.u16;
  // Accumulate fractional correction across frequent (even 1 ms) loop calls.
  // Slewing by at most 10% preserves monotonic time and avoids timeout jumps.
  uint64_t advance = delta;
  if (serverClockActive && clockCorrectionMs) {
    const uint8_t fraction = correctionRemainder + delta % 10;
    const uint32_t budget = delta / 10 + fraction / 10;
    correctionRemainder = fraction % 10;
    const uint64_t remaining = clockCorrectionMs < 0 ? -clockCorrectionMs : clockCorrectionMs;
    const uint32_t applied = remaining < budget ? (uint32_t)remaining : budget;
    if (clockCorrectionMs < 0) {
      advance -= applied;
      clockCorrectionMs += applied;
    } else {
      advance += applied;
      clockCorrectionMs -= applied;
    }
    if (!clockCorrectionMs) correctionRemainder = 0;
  }
  secTime.u32 += advance / 1000;
  const uint16_t remainder = milTime.u16 + advance % 1000;
  secTime.u32 += remainder / 1000;
  milTime.u16 = remainder % 1000;
}

inline uint32_t BohoClock::getUnixTime()
{
  return secTime.u32;
}

inline uint16_t BohoClock::getMilTime()
{
  return milTime.u16;
}

inline bool BohoClock::nextSendClock(uint8_t *clock8)
{
  refreshTime();
  uint64_t timeMs = (uint64_t)secTime.u32 * 1000 + milTime.u16;
  uint16_t sequence = counter.u16;
  // Clock correction, explicit setTime(), loop-driven counter wrap and E2E
  // interleaving must never reuse an outgoing clock/counter combination.
  if (hasSendClock && (timeMs < lastSendTimeMs ||
      (timeMs == lastSendTimeMs && sequence <= lastSendCounter))) {
    timeMs = lastSendTimeMs;
    sequence = lastSendCounter + 1;
    if (!sequence) ++timeMs;
  }
  sendClockValid = timeMs <= (uint64_t)0xffffffffUL * 1000 + 999;
  if (!sendClockValid) return false;
  hasSendClock = true;
  lastSendTimeMs = timeMs;
  lastSendCounter = sequence;
  counter.u16 = sequence;
  u32buf4 seconds;
  u16buf2 milliseconds;
  seconds.u32 = timeMs / 1000;
  milliseconds.u16 = timeMs % 1000;
  memcpy(clock8, seconds.buf, 4);
  memcpy(clock8 + 4, milliseconds.buf, 2);
  memcpy(clock8 + 6, counter.buf, 2);
  return true;
}

inline void BohoClock::setTime( uint32_t utc , uint16_t millis )
{
  secTime.u32 = utc + millis / 1000;
  milTime.u16 = millis % 1000;
  lastTime = ::millis();
  clockCorrectionMs = 0;
  correctionRemainder = 0;
}

inline void BohoClock::setClientTimeToServerTime( const uint8_t* server_time_nonce , size_t inputLen )
{
  if (!server_time_nonce || inputLen != MetaSize_SERVER_TIME_NONCE ||
      server_time_nonce[0] != 0xB0) return;  // SERVER_TIME_NONCE
  u16buf2 ms;
  memcpy(ms.buf, server_time_nonce + 5, 2);
  if (ms.u16 >= 1000) return;
  if (server_time_nonce != serverChallenge) memcpy(serverChallenge, server_time_nonce, sizeof(serverChallenge));
  hasChallenge = true;
  resetServerClock();
  memcpy(secTime.buf, server_time_nonce + 1, 4);
  milTime = ms;
  lastTime = millis();
}

inline void BohoClock::observeServerClock(uint32_t seconds, uint16_t milliseconds,
                                   uint16_t sequence, uint32_t receivedAt)
{
  if (!serverClockActive) return;
  const uint64_t serverMs = (uint64_t)seconds * 1000 + milliseconds;
  // Duplicate or older envelopes may still be handled by the caller, but must
  // not steer the clock again. The server JS sender has a monotonic wire clock.
  if (hasServerSample && (serverMs < lastServerTimeMs ||
      (serverMs == lastServerTimeMs && sequence <= lastServerCounter))) return;
  refreshTime();
  hasServerSample = true;
  lastServerTimeMs = serverMs;
  lastServerCounter = sequence;
  // Account for local decryption time; unknown network delay is not estimated.
  const uint64_t target = serverMs + (uint32_t)(lastTime - receivedAt);
  clockCorrectionMs = (int64_t)target - ((int64_t)secTime.u32 * 1000 + milTime.u16);
  if (!clockCorrectionMs) correctionRemainder = 0;
}

// One function for both callers: an Uno has no flash for two copies of a 32-bit sum.
inline void __attribute__((noinline)) BohoClock::stirNonce( void )
{
  nonceSource.timing += micros();
}

#endif
