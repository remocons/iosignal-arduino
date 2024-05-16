# IOSignal Arduino Client

Arduino Client for [`IOSignal`](https://github.com/remocons/iosignal).


## IOSignal Server

To run the client example, you must first install iosignal-cli and then run the iosignal server.

 `iosignal-cli` [ [github](https://github.com/remocons/iosignal-cli) | [npm](https://www.npmjs.com/package/iosignal-cli) ]
 - support Mac, Linux & Windows


### CLI program install
```sh
# global install.
$ npm install -g iosignal-cli

# If you encounter a permissions error, use sudo.
$ sudo npm install -g iosignal-cli

```
## IOSignal server

- use `io-server` command.
- `ios` is a shortened name for io-server.

- use -L option to connect Arduino Client.
- use -l option to connect Browser Client.

```sh
 % io-server -l 7777 -L 55488
opening WebSocket Server: 7777
opening CongSocket Server: 55488

   ┌─────────────────────────────────────────────┐
   │                                             │
   │   Serving                                   │
   │                                             │
   │   IOSignal Over WebSocket                   │
   │                                             │
   │    Web Browser & Node.js                    │
   │    - Local:    ws://localhost:7777          │
   │    - Network:  ws://192.168.0.213:7777      │
   │                                             │
   │   IOSignal Over CongSocket                  │
   │                                             │
   │    Node.js                                  │
   │    - Local:    cong://localhost:55488       │
   │    - Network:  cong://192.168.0.213:55488   │
   │                                             │
   │    Arduino                                  │
   │    - host: 192.168.0.213                    │
   │    - port: 55488                            │
   │                                             │
   └─────────────────────────────────────────────┘

```

## Examples

The library comes with a number of example sketches. See File > Examples > IOSignal
within the Arduino application.

1. Prepare the server.
- You can run your own server by installing iosignal-cli.
- Connect to the server using the server IP address and port number like below.
```c
io.begin( &client , "192.168.0.204", 55488);
```
- If you don't have a server, you can use the public server for trial.
- Connect to it using the following code
```c
io.begin( &client , "io.iosignal.net", 55488);
```
2. Communicate with your web browser and Arduino

- Connect to [http://test.iosignal.net](http://test.iosignal.net) using a web browser.
- Connect to the same server as your Arduino.
- Press the Send button on the web app to send a signal to the Arduino.
- Press the button on the Arduino to send signals and receive them on the web browser.

[Korean] 
1. 서버를 준비합니다.
- iosignal-cli 를 설치하여 직접 서버를 운영할 수 있습니다.
- 아래와 같이 서버 ip주소와  port번호를 사용하여 접속합니다.
```c
io.begin( &client , "192.168.0.204", 55488);
```
- 서버가 없다면 체험용 공개 서버를 사용할 수 있습니다.
- 아래 코드를 사용하여 접속합니다.
```c
io.begin( &client , "io.iosignal.net", 55488);
```
2. 웹브라우저와 아두이노 통신하기

- 웹브라우저로 [http://test.iosignal.net](http://test.iosignal.net) 에 접속합니다.
- 아두이노와 동일한 서버에 접속합니다.
- 웹앱에서 전송 버튼을 눌러서 아두이노에 시그널을 송신합니다.
- 아두이노의 버튼을 눌러서 시그널을 보내고 웹브라우저에서 수신합니다.



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

- CLI program 
  - server and client
  - support mac, linux and windows.
  - `iosignal-cli` [ [github](https://github.com/remocons/iosignal-cli) | [npm](https://www.npmjs.com/package/iosignal-cli) ]
  - install: `sudo npm i -g iosignal-cli` or  `npm i -g iosignal-cli`  

- Javascript: `iosignal` [ [github](https://github.com/remocons/iosignal) | [npm](https://www.npmjs.com/package/iosignal) ]
  - Node.js server ( WebSocket, CongSocket)
  - Node.js client ( WebSocket, CongSocket)
  - Web Browser client( WebSocket)

- Arduino client: 
  - Arduino Library Manager: `IOSignal`
  - or `iosignal-arduino` [ [github](https://github.com/remocons/iosignal-arduino) ]

- Arduino example working with remocon web app
  - Arduino Library Manager: `Remocon`
  - or `Remocon` [ [github](https://github.com/remocons/remocon-arduino) ]


## IOSignal stack

![IOSignal](./img/iosignal_stack.png)

## License

This code is released under the MIT License.