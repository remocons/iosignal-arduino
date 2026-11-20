# ESP32-C3 중복 로그인 확인

같은 ID로 다른 곳에서 로그인했을 때 이 보드가 어떻게 동작하는지 시리얼로 확인합니다.

## 준비물

- 보드: ESP32-C3 Super Mini
- 네트워크: Wi-Fi 2.4GHz 공유기, 직접 운영하는 IOSignal 서버(장치 키를 등록한 서버)
- 앱: 없음
- 연동: 같은 ID로 로그인하는 다른 접속: 이 예제를 올린 다른 보드 또는 iosignal-cli의 io-client

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| 8 | 내장 LED | LOW에서 켜집니다. |
| 9 | BOOT 버튼 | 누르면 다시 로그인합니다. |

## 사용 순서

1. 이 예제는 직접 운영하는 IOSignal 서버와 그 서버에 등록한 장치 키(id.key)가 필요합니다. 스케치의 ID_KEY("did.key")는 가짜 값이고, 기본 서버 io.iosignal.net은 장치 키를 발급하지 않습니다.
2. USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 Wi-Fi, 서버(호스트:포트), 자신의 서버에 등록한 장치 키(id.key)를 넣습니다(보드가 저장). 스케치의 WIFI_SSID, WIFI_PASSWORD, ID_KEY, 서버 주소에 써도 되고, USB로 넣은 값이 우선합니다.
3. 보드는 ESP32C3 Dev Module, USB CDC On Boot는 Enabled로 설정합니다.
4. 업로드 후 시리얼 모니터 115200에서 READY cid를 확인합니다.
5. 같은 장치 키로 다른 곳에서 로그인합니다.
6. 연결이 멈추면 BOOT 버튼, 시리얼 r 입력 또는 리셋으로 다시 로그인합니다.

## 참고

- keepNew 서버는 나중 로그인이 이 보드를 교체하고, keepOld 서버는 나중 로그인을 거부합니다.
- 서버와 장치 키는 보드 설정(USB)으로 넣으면 펌웨어를 다시 빌드하지 않아도 됩니다. 사이트의 펌웨어에는 장치 키가 들어 있지 않습니다.

## 컴파일

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c3 examples/esp32c3_duplicate_login
```
