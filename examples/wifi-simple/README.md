# WiFi 공유 스위치 (가장 간단한 예제)

#switch 그룹에 참여해 공유 상태를 시리얼로 출력합니다. 하드웨어 없이 통신만 확인하는 가장 간단한 예제이고, 보드 설정으로 출력 핀(LED·릴레이)과 버튼 핀을 정하면 s1을 따르는 릴레이나 s1을 토글하는 버튼 보드가 됩니다.

## 준비물

- 보드: ESP-01 (ESP8266)
- 보드: ESP8266 D1 mini (브라우저 설치는 ESP-01과 같은 ESP8266 펌웨어(1MB)를 씁니다)
- 보드: ESP32 개발 보드
- 보드: ESP32-C3 Super Mini
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://remocon.kr 또는 IOSignal Desktop 장치 제어 화면

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| out (보드 설정) | LED 또는 릴레이 | s1을 따릅니다. out_high 0이면 LOW가 켜짐(내장 LED), 1이면 HIGH가 켜짐(릴레이 모듈). 예: ESP-01 릴레이 보드 0, D1 mini 내장 LED 2, ESP32-C3 Super Mini 8 |
| button (보드 설정) | 버튼 (GND로) | 누르면 s1을 바꿔 그룹 상태 전체를 다시 씁니다(s2, s3는 유지). 내부 풀업. 예: ESP-01 GPIO0, 버튼이 RX에 있는 보드 3, D1 mini D5(14) |

## 사용 순서

1. 업로드한 뒤(또는 이 페이지의 펌웨어 설치로) USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 Wi-Fi를 넣습니다(데스크톱 Chrome·Edge, 보드가 저장). 같은 페이지에서 다른 IOSignal 서버(호스트:포트)와 장치 키도 넣을 수 있습니다.
2. LED·릴레이나 버튼을 쓰면 보드 설정 out, out_high, button을 넣습니다. 핀은 부팅할 때 읽으므로 바꾸면 보드가 재시작합니다.
3. 시리얼 모니터 115200을 엽니다.
4. 같은 공유기에 연결한 브라우저로 https://remocon.kr 을 엽니다.
5. 공유 스위치를 눌러 시리얼에 s1=1 같은 값이 나오는지, 출력 핀이 따라오는지 봅니다. 버튼을 누르면 앱의 s1도 바뀝니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 장치 | 서버 | `#switch$state` | `구독` | 공유 상태를 구독합니다. 보관된 값이 있으면 바로 받습니다. |
| 장치 | 서버 | `#search` | `구독` | 앱의 검색 요청을 받도록 구독합니다. |
| 장치 | 서버 | `#switch$name` | `공유 스위치:HOME` | 그룹 이름과 분류입니다. 구성원 모두 같은 값을 보냅니다. 분류는 앱이 그룹 카드를 묶어 보여 주는 탭 이름입니다. 이름과 분류 모두 보드 설정 name(이름:분류)으로 바꿉니다(바꾸면 바로 다시 보냄). 분류가 없으면 앱의 미분류 탭에 나옵니다. |
| 장치 | 서버 | `#switch$ui` | `t=switch3.1` | 공식 타입 switch3.1입니다(확장형: 첫 키가 t). 앱이 그 타입의 화면을 그립니다. 서버에 보관됩니다. 그룹은 확장형만 씁니다. |
| 장치 | 앱 | `#notify` | `#switch` | 그룹은 CID 대신 채널 이름으로 알립니다. |
| 앱 | 장치 | `#search` |  | 앱이 그룹을 찾습니다. |
| 장치 | 앱 | `#notify` | `#switch` | 구성원마다 답하고 앱은 하나로 합칩니다. |
| 앱 | 장치 | `#switch$state` | `s1=1;s2=0;s3=0` | 앱이 전체 문자열을 다시 씁니다. 구성원은 자기 키를 읽습니다. |
| 장치 | 그룹 | `#switch$state` | `s1=1;s2=0;s3=0` | button 핀을 누르면 s1만 바꿔 그룹 상태 전체를 씁니다. 구성원 모두와 앱이 받습니다. |

## 참고

- 기본은 값을 출력만 하고 핀은 제어하지 않습니다(out, button 모두 -1).
- 비공개 예제 esp-d1-switch-button, esp01-relay-passive, esp01-relay-passive-control, esp01-relay-passive-control-woryboard를 보드 설정으로 합쳤습니다(2026-11-16).
- ESP-01은 GPIO1(TX)을 출력이나 버튼으로 쓰면 시리얼이 멈춥니다. 보드 설정이 필요하면 다른 핀을 쓰세요.

## 컴파일

```sh
arduino-cli compile --fqbn esp8266:esp8266:generic examples/wifi-simple
arduino-cli compile --fqbn esp8266:esp8266:d1_mini examples/wifi-simple
arduino-cli compile --fqbn esp32:esp32:esp32 examples/wifi-simple
arduino-cli compile --fqbn esp32:esp32:esp32c3 examples/wifi-simple
```
