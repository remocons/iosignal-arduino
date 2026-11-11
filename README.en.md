# Arduino IOSignal Client Library

[한국어](README.md) | **English**

The Arduino IOSignal library provides a client that connects Arduino boards to an IOSignal server, with examples for each board.

<p align="center">
  <img src="docs/assets/iosignal-ecosystem.png" alt="Server, CLI, Node.js, Arduino and remote control web app connected through IOSignal" width="720" />
</p>

## IOSignal

IOSignal supports real-time communication between web browsers, Node.js and Arduino, with authentication and encrypted communication. The signaling protocol is built into the server, so no server programming is required. Arduino connects to the server over CongSocket, a TCP transport (public server `io.iosignal.net:55488`).

## Versions

From **9.0.0**, this library uses the same version number as the public [iosignal](https://github.com/remocons/iosignal) package. iosignal-cli, the remocon CLI and [remocon-arduino](https://github.com/remocons/remocon-arduino) use the same number too. Packages with the same number are meant to be used together. See [Changes by version](#changes-by-version) below.

## Installation

1. In the Arduino IDE Library Manager, search for `IOSignal` and install it together with its dependencies **Boho** (1.0.0 or later), **Crypto** and **Bounce2**.
2. Until a new version reaches the Library Manager, download the ZIP from the [public repository](https://github.com/remocons/iosignal-arduino) and add it with **Sketch > Include Library > Add .ZIP Library**.
3. Open an example for your board from **File > Examples > IOSignal**.

Supported boards: UNO with a W5100 Ethernet shield, UNO R4 WiFi, ESP8266 and ESP32 (including ESP32-C3). Each example targets specific boards; check the folder name.

## Basic usage

```cpp
#include <WiFi.h>
#include <IOSignal.h>

WiFiClient client;
IOSignal io;

void onReady() {
  Serial.println(io.cid);          // CID issued by the server
  io.subscribe("#homeButton");     // subscribe again in ready after every reconnection
}

void onMessage(char *tag, uint8_t type, uint8_t *payload, size_t size) {
  Serial.println(tag);
}

void setup() {
  Serial.begin(115200);
  // ... connect Wi-Fi ...
  io.setRxBuffer(200);             // call before begin()
  io.begin(&client, "io.iosignal.net", 55488);
  io.onReady(onReady);
  io.onMessage(onMessage);
  // io.auth("ID_KEY");            // when using authentication
}

void loop() {
  io.loop();
  // io.signal("#homeButton", "pressed");
}
```

- Call `loop()` on every iteration. After a disconnection it reconnects 3 seconds later.
- Sending: `signal(tag, ...)` for channel and CID tags, `signal2(target, topic, ...)` for a tag made by joining two strings (e.g. `signal2(cid, "@pong", ...)` → `cid@pong`), and `signal_e2e()` for end-to-end encryption.
- Diagnostics: `onError(callback)` receives error codes, and `setLog(callback)` receives human-readable notes.

## RX buffer and large messages

- `setRxBuffer(size)` sets the largest frame the device can receive. Without it, `begin()` allocates **80 bytes**.
- The buffer size is sent to the server when connecting (`CID_REQ`), so call **`setRxBuffer()` before `begin()`**. An IOSignal 9.0 server does not send this device messages larger than that.
- A frame larger than the buffer (e.g. from an older server) is skipped without closing the connection. `onError(CongPacket::OVER_SIZE)` (254) and a `setLog` note are reported once.
- Outgoing sizes follow the server's quota. The server's `QUOTA` message is ignored.

## Authentication and duplicate logins

- Set credentials with `auth("ID.KEY")` or `auth(id, key)`, and the client logs in on every connection. On an authentication failure it clears the authentication state before calling `onError`.
- If another device logs in with the same ID and the server replaces this connection (`AUTH_CLEAR`), the client clears its credentials and **stops in the `IO_STOP` state**. It does not reconnect anonymously. Check the reason with `io.lastReason` and `setLog`; call `auth()` and `begin()` to start again.
- Example: [esp32c3_duplicate_login](examples/esp32c3_duplicate_login/esp32c3_duplicate_login.ino). For the server's duplicate login policy, see [server authentication](https://iosignal.net/en/docs/server/authentication#duplicate-login).

## Documentation and related packages

[IOSignal documentation — iosignal.net](https://iosignal.net/en/) covers installation, concepts, server configuration, browser and Arduino integration, and examples. New users can start with the [five-minute quickstart](https://iosignal.net/en/docs/start/quickstart/).

| Package | Role |
| --- | --- |
| [iosignal](https://github.com/remocons/iosignal) · [npm](https://www.npmjs.com/package/iosignal) | JavaScript communication library for Node.js servers, clients and browsers |
| [iosignal-cli](https://github.com/remocons/iosignal-cli) · [npm](https://www.npmjs.com/package/iosignal-cli) | Command-line tools for running IOSignal servers and clients and testing communication |
| [iosignal-arduino](https://github.com/remocons/iosignal-arduino) | This repository. Client library that connects Arduino devices to an IOSignal server |
| [boho](https://github.com/remocons/boho) · [npm](https://www.npmjs.com/package/boho) | Shared-key authentication and encryption library for JavaScript |
| [boho-arduino](https://github.com/remocons/boho-arduino) | Arduino implementation of Boho, the authentication and encryption dependency of this library |
| [remocon-arduino](https://github.com/remocons/remocon-arduino) | Board-specific remote control examples using IOSignal |

To try it in a browser, use the [IOSignal communication lab](https://test.iosignal.net/). For device discovery and control examples, use the [Remocon web app](https://remocon.kr/). The [board workbench guide](https://iosignal.net/en/docs/examples/workbench/start) shows the connection steps.

## Boho dependency

IOSignal uses the external Arduino library **Boho 1.0.0 or later**. Boho is not bundled in `src/`. Install Boho and its dependency Crypto together with IOSignal. Keep a single Boho installation so an older copy is not used.

To develop locally before a new release reaches the Library Manager, install the updated `boho-arduino` library from a local folder or ZIP. The `depends` field follows the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/).

## Peer ping/pong example

Open the [peer_ping_pong example](examples/peer_ping_pong/peer_ping_pong.ino) on an ESP32 (including ESP32-C3) or ESP8266. Replace the Wi-Fi placeholders, compile and upload, then open the Serial Monitor at 115200 baud to see the device CID and received message tags (`[RX]`).

The device answers another device's ping immediately and then prints `[TX] @pong -> <sender-cid>`. All Arduino examples connect to `io.iosignal.net:55488`. In the web CLI, run `open wss://io.iosignal.net/ws` and then `ping <arduino-cid>`; the CLI prints `pong (<arduino-cid>)`.

The example handles only direct `@ping` TEXT messages in `onMessage()`. It immediately sends `@pong` to the sender CID with its own CID as the body, and expects the CLI's standard single NUL-terminated TEXT CID body.

It does not validate CIDs, parse serial commands or handle response waits or timeouts. No channel subscription is needed. For local tests, enable both the WebSocket (CLI) port and the CongSocket (Arduino) port on the same server.

## Changes by version

### 9.12.0

- **The version number now matches the public iosignal 9.12.0 package.** The library code (`src`) is the same as 9.6.0.
- Overlapping examples were removed. The `_channel_sender` and `_channel_receiver` examples for `esp8266`, `uno_ethernet` and `uno_r4_wifi` are replaced by the `_channel_txrx` example of the same board, `esp32_channel_txrx` by `esp8266_channel_txrx` (which now builds for ESP8266 and ESP32), and `uno_ethernet_duplicate_login` by `esp32c3_duplicate_login`.
- Parts, wiring, steps and a browser panel for each example are on [test.iosignal.net](https://test.iosignal.net/) (Korean).

### 9.6.0

- **The version number now matches the public iosignal 9.6.0 package.**
- Added `unsubscribe(tag)`. It stops receiving a tag subscribed with `subscribe()`. Several tags can be given separated by commas, and an empty string (the default) removes every subscription.
- When a write does not send every byte (a stalled or broken socket), the client closes the connection and `loop()` reconnects. Previously the client could still look connected while every later write waited for the socket timeout. With `setLog()`, it prints `[IOSignal] send failed: reconnecting.`

### 9.0.0

- **The version number now matches the public iosignal package.** This release follows 7.0.0; there is no 8.x. It supports the iosignal 9.0 server features (RX size advertisement, `QUOTA`) and still connects to 8.x servers.
- Without an RX buffer, `begin()` allocates 80 bytes. Previously, not calling `setRxBuffer()` could leave an uninitialized buffer in use.
- `CID_REQ` carries the RX buffer size. A 9.0 server does not send larger messages; older servers ignore the extra bytes. Call `setRxBuffer()` before `begin()`.
- Frames larger than the buffer are skipped instead of closing the connection. Previously the client disconnected and reconnected, which looped forever when a subscribed channel held a large retained message. `onError(CongPacket::OVER_SIZE)` (254) now reports a skip, not a disconnection. A zero-length frame still closes the connection.
- The 9.0 server's `QUOTA` (0xC6) message is recognized and ignored. The `IOSignal::MsgType::QUOTA_LEVEL` (0xC3) constant was removed.
- The `esp32cam` example page uses the `iosignal@9.0.0` browser client.

### 7.0.0

- When the server sends `AUTH_CLEAR` (e.g. replaced by a new login with the same ID), the client clears its credentials and stops in the `IO_STOP` state. 6.x cleared the credentials and reconnected anonymously.
- `setLog(callback)` and `lastReason` report the reason.
- Added the duplicate login examples `uno_ethernet_duplicate_login` and `esp32c3_duplicate_login`.

### 6.0.0

- Uses Boho 1.0.0 or later for gradual clock correction through verified encrypted server packets and for send-time and counter reuse protection. The IOSignal packet format is unchanged. Reconnect when the server rejects an expired or reused authentication challenge or a packet outside the allowed time window. Plaintext PING/PONG carries no clock sample.
- On an authentication failure, the authentication state is cleared before the error callback runs. This includes malformed `AUTH_RES` responses.

## License

[MIT License](LICENSE)
