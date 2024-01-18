# IOSignal Arduino Client

Arduino Client for [`IOSignal`](https://github.com/remocons/iosignal).


## IOSignal Server

To run the client example, you must first install iosignal-cli and then run the iosignal server.

 `iosignal-cli` [ [github](https://github.com/remocons/iosignal-cli) | [npm](https://www.npmjs.com/package/iosignal-cli) ]
 - support Mac, Linux & Windows

### CLI program install
```
$ npm i -g iosignal-cli

// If you get a permission error
$ sudo npm i -g iosignal-cli

```
### begin iosignal server

- use -L option to connect Arduino Client.
```
$ io-server -L 8888

No authentication support.
WebSocketServer listen: 7777
congServer listen: 8888

   ┌───────────────────────────────────────────┐
   │                                           │
   │   WebSocket: Browser or nodejs client     │
   │                                           │
   │   - Local:    ws://localhost:7777         │
   │   - Network:  ws://192.168.0.204:7777     │
   │                                           │
   │   CongSocket: nodejs client or CLI        │
   │                                           │
   │   - Local:    cong://localhost:8888       │
   │   - Network:  cong://192.168.0.204:8888   │
   │                                           │
   │   CongSocket: Arduino client              │
   │                                           │
   │   - host: 192.168.0.204                   │
   │   - port: 8888                            │
   │                                           │
   └───────────────────────────────────────────┘


```

- change code with you server host and port info.
```
  // from
  io.begin( &client , "io.remocon.kr", 55488);

  // to
  io.begin( &client, 192.168.0.204, 8888 );
```

## Examples

The library comes with a number of example sketches. See File > Examples > IOSignal
within the Arduino application.

## Features

### Built-in Message Transport Protocol
- pub/sub style multicast: by channel name.
- uni-cast: one to one messaging by CID.
- `CID` is a Communication Id.
- CID subscribing: subscribe one peer using CID.
- HomeChannel: group by IP address.

### Built-in Security
- Authentication
- Encryption
- E2EE
- thanks to the `Boho` [ [github](https://github.com/remocons/boho-arduino) ]


## Compatible Hardware

The library uses Stream for interacting with the
underlying network hardware. This means it Just Works with a growing number of
boards and shields, including:

### tested
 - Arduino Uno + Ethernet Shield
 - Arduino UNO R4 WiFi
 - ESP8266
 - ESP32


## iosignal repositories.
- Javascript: `iosignal` [ [github](https://github.com/remocons/iosignal) | [npm](https://www.npmjs.com/package/iosignal) ]
  - Node.js server ( WebSocket, CongSocket)
  - Node.js client ( WebSocket, CongSocket)
  - Web Browser client( WebSocket)
- Arduino client: 
  - Arduino Library Manager: `IOSignal`
  - or `iosignal-arduino` [ [github](https://github.com/remocons/iosignal-arduino) ]

- CLI program 
  - server and client
  - support mac, linux and windows.
  - `iosignal-cli` [ [github](https://github.com/remocons/iosignal-cli) | [npm](https://www.npmjs.com/package/iosignal-cli) ]
  - install: `sudo npm i -g iosignal-cli` or  `npm i -g iosignal-cli`  


## IOSignal stack

![IOSignal](./img/iosignal_stack.png)

## License

This code is released under the MIT License.