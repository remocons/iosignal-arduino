# Arduino IOSignal Client Library

[한국어](README.md) | **English**

The Arduino IOSignal library provides an Arduino client and example sources.

## IOSignal

IOSignal supports real-time communication between web browsers, Node.js, and Arduino. It also provides secure authentication and encrypted communication. The server has a built-in signaling protocol, so it can be used without additional server programming.

## Documentation and related packages

The [IOSignal documentation site — iosignal.net](https://iosignal.net/) covers installation, messaging concepts, server configuration, browser and Arduino integration, and examples. Start with the [five-minute quick start](https://iosignal.net/docs/start/quickstart/) if you are new to IOSignal. The linked documentation is in Korean.

| Package | Purpose |
| --- | --- |
| [iosignal](https://github.com/remocons/iosignal) · [npm](https://www.npmjs.com/package/iosignal) | JavaScript messaging library for Node.js servers and clients, and browser clients |
| [iosignal-cli](https://github.com/remocons/iosignal-cli) · [npm](https://www.npmjs.com/package/iosignal-cli) | Terminal tools for running IOSignal servers and clients and testing communication |
| [iosignal-arduino](https://github.com/remocons/iosignal-arduino) | This repository: a client library that connects Arduino devices to IOSignal servers |
| [boho](https://github.com/remocons/boho) · [npm](https://www.npmjs.com/package/boho) | Shared-key authentication and encryption library for JavaScript |
| [boho-arduino](https://github.com/remocons/boho-arduino) | Arduino implementation of Boho; this library's authentication and encryption dependency |
| [remocon-arduino](https://github.com/remocons/remocon-arduino) | Board-specific remote-control examples built with IOSignal |

Use the [IOSignal Communication Workbench](https://test.iosignal.net/) to experiment in your browser, or the [Remocon web app](https://remocon.kr/) with device discovery and control examples. Follow the [board tutorials](https://iosignal.net/docs/examples/workbench/start) for connection steps.

## Boho dependency

IOSignal uses the external **Boho 1.0.0 or later** Arduino library. Boho source files are no longer bundled under `src/`. Install Boho and its Crypto dependency alongside IOSignal; keep only one installed Boho library to avoid stale copies.

For local development before a release is available in Library Manager, install the updated `boho-arduino` library from its local folder or ZIP.

The `depends` entry follows the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/).

## 6.0.0 clock correction dependency

This version requires Boho 1.0.0 or later for gradual clock correction from verified server envelopes and outgoing time/counter reuse protection. IOSignal packet formats are unchanged. Reconnect when the server rejects an expired/reused challenge or a packet outside its clock window. Plain PING/PONG has no time sample.

Authentication failure also clears authorization before the error callback, including malformed `AUTH_RES` responses.

## Peer ping/pong example

Open the [peer_ping_pong example](examples/peer_ping_pong/peer_ping_pong.ino) for ESP32 (including ESP32-C3) or ESP8266. Set the Wi-Fi placeholders, compile/upload, and open Serial Monitor at 115200 baud to see the device CID and received message tags (`[RX]`).

A peer ping is answered immediately, then `[TX] @pong -> <sender-cid>` is logged. All Arduino examples connect to `io.iosignal.net:55488`. In the web CLI, run `open wss://io.iosignal.net/ws`, then `ping <arduino-cid>`; the CLI prints `pong (<arduino-cid>)`.

The example only handles a direct `@ping` TEXT message in `onMessage()` and immediately sends `@pong` to the sender CID with its own CID as payload. It assumes the standard single, null-terminated TEXT CID payload sent by the CLI.

It has no CID validation, serial command parser, response waiting or timeout. The library core and server `io.ping()` / `io.pong()` remain unchanged. No channel subscription is required. For local tests enable WebSocket (CLI) and CongSocket (Arduino) ports on the same server.
