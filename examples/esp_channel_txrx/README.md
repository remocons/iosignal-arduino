# ESP8266·ESP32 채널 송수신

버튼을 누르면 #homeButton 채널로 신호를 보내고, 같은 채널의 신호를 받으면 LED를 바꿉니다.

## 준비물

- 보드: ESP8266 D1 mini
- 보드: ESP32 개발 보드
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://lab.iosignal.net/channel/

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| 2 | LED | 수신하면 켜짐과 꺼짐이 바뀝니다. |
| 14 (ESP8266) / 12 (ESP32) | 버튼 | 누르면 GND로 연결됩니다. 내부 풀업을 씁니다. |

## 사용 순서

1. Wi-Fi는 업로드한 뒤 USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 넣습니다(데스크톱 Chrome·Edge, 보드가 저장). 같은 페이지에서 다른 IOSignal 서버(호스트:포트)와 장치 키도 넣을 수 있습니다. 스케치의 Wi-Fi 값에 실제 값을 쓰면 그 값이 부팅할 때마다 우선합니다.
2. 보드에 맞게 핀 번호를 확인합니다.
3. 컴파일하고 업로드합니다.
4. 브라우저로 https://lab.iosignal.net/channel/ 에 접속해 확인합니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 보드 | 서버 | `#homeButton` | `구독` | 접속이 끝나면 채널을 구독합니다. |
| 보드 | 서버 | `#homeButton` | `esp8266 또는 esp32` | 버튼을 누르면 보냅니다. |
| 서버 | 보드 | `#homeButton` |  | LED 상태를 바꿉니다. |

## 참고

- esp32_channel_txrx를 합쳤습니다. 보드에 따라 버튼 핀과 보내는 값이 바뀝니다.

## 컴파일

```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini examples/esp_channel_txrx
arduino-cli compile --fqbn esp32:esp32:esp32 examples/esp_channel_txrx
```
