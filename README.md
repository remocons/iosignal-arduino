# Arduino IOSignal Client library

The Arduino iosignal library provides an Arduino client and example sources.

[kr]Arduino iosignal 라이브러리는 아두이노 client 및  예제소스를 제공합니다.

## iosignal
iosignal supports real-time communication between web browsers, node.js, and arduino. It also provides secure authentication and encrypted communication. The signaling protocol is built-in, so the server can be used without programming.

[Kr] iosignal 은 웹브라우저, node.js , arduino 간의 실시간 통신을 지원합니다. 또한 보안 인증과 암호통신 기능도 제공됩니다. 시그널링 프로토콜이 내장된 서버는 프로그래밍 없이도 사용 가능합니다.


## Boho dependency

IOSignal uses the external **Boho 0.8.0 or later** Arduino library. Boho source
files are no longer bundled under `src/`. Install Boho and its Crypto dependency
alongside IOSignal; keep only one installed Boho library to avoid stale copies.
For local development before a release is available in Library Manager, install
the updated `boho-arduino` library from its local folder or ZIP.

The `depends` entry follows the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/).
