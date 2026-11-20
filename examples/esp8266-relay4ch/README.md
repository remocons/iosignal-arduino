# ESP8266 4채널 릴레이

ESP-12F 릴레이 보드의 릴레이 4개를 relay4 타입으로 제어합니다.

## 준비물

- 보드: ESP-12F 4채널 릴레이 보드
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://remocon.kr 또는 IOSignal Desktop 장치 제어 화면

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| GPIO2 | 내장 LED | L |
| GPIO13 | 릴레이 A |  |
| GPIO12 | 릴레이 B |  |
| GPIO14 | 릴레이 C |  |
| GPIO16 | 릴레이 D |  |

## 사용 순서

1. Wi-Fi는 업로드한 뒤 USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 넣습니다(데스크톱 Chrome·Edge, 보드가 저장). 같은 페이지에서 다른 IOSignal 서버(호스트:포트)와 장치 키도 넣을 수 있습니다. 스케치의 Wi-Fi 값에 실제 값을 쓰면 그 값이 부팅할 때마다 우선합니다. _secrets.h에 SECRET_WIFI_SSID, SECRET_WIFI_PASS(장치 키는 SECRET_ID_KEY)를 #define해도 됩니다.
2. USB-TTL 어댑터를 프로그래밍 포트(GND, RX, TX, 5V)에 연결하고 IO0을 GND에 연결한 채 리셋합니다.
3. arduino-cli compile --upload -p <포트> --fqbn esp8266:esp8266:generic 로 업로드합니다(Arduino IDE는 Generic ESP8266 Module).
4. 업로드 후 IO0을 떼고 리셋하면 스케치가 실행됩니다.
5. https://remocon.kr 에서 Relay4CH 카드의 스위치와 모두 켜기/끄기를 눌러 봅니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 장치 | 서버 | `#search` | `구독` | 앱의 검색 요청을 받도록 구독합니다. |
| 장치 | 서버 | `@$name` | `Relay4CH:HOME` | 이름과 분류(첫 : 뒤)입니다. 서버에 보관됩니다. 분류는 앱이 장치를 묶어 보여 주는 탭 이름입니다. 이름과 분류 모두 보드 설정 name(이름:분류)으로 바꿉니다(바꾸면 바로 다시 보냄). 분류가 없으면 앱의 미분류 탭에 나옵니다. |
| 장치 | 서버 | `@$ui` | `t=relay4.1` | 공식 타입 relay4.1입니다(확장형: 첫 키가 t). 앱이 그 타입의 화면을 그립니다. 서버에 보관됩니다. |
| 장치 | 서버 | `@$state` | `r1=0;r2=0;r3=0;r4=0` | 현재 상태입니다. 서버에 보관됩니다. |
| 장치 | 앱 | `#notify` | `<cid>` | 접속했음을 알립니다. |
| 앱 | 장치 | `#search` |  | 앱이 시작할 때, 1초 뒤, 이후 15초마다 보냅니다. |
| 장치 | 앱 | `#notify` | `<cid>` | 검색에 자기 CID로 답합니다. |
| 앱 | 서버 | `<cid>@$name, $ui, $state` | `구독` | 보관된 이름, 버튼(타입), 상태를 바로 받아 카드를 만듭니다. |
| 앱 | 장치 | `<cid>@` | `r1=1` | 앱의 스위치가 값을 지정하는 명령을 보냅니다. 장치가 받는 태그는 @입니다. |
| 장치 | 앱 | `@$state` | `r1=1;r2=0;r3=0;r4=0` | 명령을 처리한 뒤 상태를 보냅니다. 앱의 대기 표시가 풀립니다. |
| 서버 | 앱 | `<cid>@$state` | `close` | 장치가 끊기면 서버가 보내고 카드가 사라집니다. |

## 참고

- 폴더의 ESP12F_Relay_X4.pdf가 보드 자료입니다.
- SECRET_ID_KEY(id.key)를 정의하면 그 키로 인증해 CID가 고정됩니다. 제어기 예제 Remocon8266-button-controller은 이 CID로 릴레이 보드를 찾습니다.
- L, A, B, C, D 이름 명령은 상태를 모르는 IR 라우터를 위해 남겼습니다. 화면에는 나오지 않습니다. 내장 LED L은 상태에 넣지 않습니다.

## 컴파일

```sh
arduino-cli compile --fqbn esp8266:esp8266:generic examples/esp8266-relay4ch
```
