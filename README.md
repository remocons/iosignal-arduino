# Arduino IOSignal Client library

The Arduino iosignal library provides an Arduino client and example sources.

[kr]Arduino iosignal 라이브러리는 아두이노 client 및  예제소스를 제공합니다.

## iosignal
iosignal supports real-time communication between web browsers, node.js, and arduino. It also provides secure authentication and encrypted communication. The signaling protocol is built-in, so the server can be used without programming.

[Kr] iosignal 은 웹브라우저, node.js , arduino 간의 실시간 통신을 지원합니다. 또한 보안 인증과 암호통신 기능도 제공됩니다. 시그널링 프로토콜이 내장된 서버는 프로그래밍 없이도 사용 가능합니다.


## Boho dependency

IOSignal uses the external **Boho 1.0.0 or later** Arduino library. Boho source
files are no longer bundled under `src/`. Install Boho and its Crypto dependency
alongside IOSignal; keep only one installed Boho library to avoid stale copies.
For local development before a release is available in Library Manager, install
the updated `boho-arduino` library from its local folder or ZIP.

The `depends` entry follows the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/).


## 6.0.0 clock correction dependency

This version requires Boho 1.0.0 for gradual clock correction from verified
server envelopes and outgoing time/counter reuse protection. IOSignal packet
formats are unchanged. Reconnect when the server rejects an expired/reused
challenge or a packet outside its clock window. Plain PING/PONG has no time sample.

이 버전은 서버 암호문 기반 시계 보정과 송신 시간·카운터 재사용 방어를 위해
Boho 1.0.0 이상을 사용합니다. 패킷 형식은 유지하며 서버가 challenge 만료·재사용
또는 허용 시간 차이 초과로 종료하면 재연결합니다.

Authentication failure also clears authorization before the error callback,
including malformed AUTH_RES responses.

## Peer ping/pong example

Open `examples/peer_ping_pong/peer_ping_pong.ino` for ESP32 (including ESP32-C3)
or ESP8266. Set the WiFi placeholders, compile/upload, and open Serial Monitor
at 115200 baud to see the device CID. From a CLI on the same server, run
`ping <arduino-cid>`; the CLI prints `pong (<arduino-cid>)`.

The example only handles a direct `@ping` TEXT message in `onMessage()` and
immediately sends `@pong` to the sender CID with its own CID as payload.
It assumes the standard single, null-terminated TEXT CID payload sent by the CLI.
It has no CID validation, serial command parser, response waiting or timeout.
The library core and server `io.ping()` / `io.pong()` remain unchanged.
No channel subscription is required. For local tests enable WebSocket (CLI) and
CongSocket (Arduino) ports on the same server.
