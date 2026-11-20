# 버튼 2개 제어기

버튼 2개로 다른 장치(4채널 릴레이, 전등, 그룹 채널)를 제어합니다. 대상과 버튼마다 하는 일을 보드 설정으로 정해 펌웨어를 고치지 않고 바꿉니다.

## 준비물

- 보드: ESP8266 D1 mini
- 보드: ESP32 개발 보드
- 보드: ESP32-C3
- 보드: ESP32-S3
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://remocon.kr (제어기 카드의 b1, b2 버튼), 대상 장치
- 연동: esp8266-relay4ch

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| 버튼 1 | 푸시 버튼 → GND | D1 mini D5(GPIO14), ESP32 GPIO13, ESP32-C3 GPIO9(BOOT), ESP32-S3 GPIO0(BOOT) |
| 버튼 2 | 푸시 버튼 → GND | D1 mini D6(GPIO12), ESP32 GPIO14, ESP32-C3 GPIO10, ESP32-S3 GPIO4 |

## 사용 순서

1. 제어할 장치(예: esp8266-relay4ch)를 먼저 켜고 CID를 확인합니다.
2. 이 스케치를 올리고 USB로 연결해 https://arduino.iosignal.net/wifi/ 에서 Wi-Fi를 넣습니다. 같은 페이지의 스케치 설정 target에 대상 CID를 넣습니다.
3. b1, b2를 비워 두면 기본 동작입니다. 대상 채널이 3개 이상이면 버튼 1은 모두 켜기, 버튼 2는 모두 끄기, 채널이 2개면 채널 1·2 토글입니다.
4. 버튼마다 다른 일을 시키려면 b1, b2에 'r1'(릴레이 1 토글), 'r2=1'(켜기), 'all'(모두 토글), 'lamp1:toggle'(다른 장치), '#lab:s2'(그룹 채널)처럼 넣습니다. remocon pub "<제어기 cid>@" "b2=r3" 로도 바꿀 수 있습니다.
5. remocon.kr의 Button Control 카드에서 b1, b2를 눌러도 버튼을 누른 것과 같습니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 장치 | 서버 | `@$name` | `Button Control:HOME` | 이름과 분류(첫 : 뒤)입니다. 서버에 보관됩니다. 분류는 앱이 장치를 묶어 보여 주는 탭 이름입니다. 이름과 분류 모두 보드 설정 name(이름:분류)으로 바꿉니다(바꾸면 바로 다시 보냄). 분류가 없으면 앱의 미분류 탭에 나옵니다. |
| 장치 | 서버 | `@$ui` | `b1,b2` | 버튼 목록입니다. 앱은 누른 버튼 이름을 그대로 명령으로 보냅니다. 서버에 보관됩니다. |
| 장치 | 서버 | `<대상>@$state` | `구독` | 대상과, 설정이 따로 지정한 장치마다 구독합니다(그룹은 #ch$state). 토글은 마지막 상태의 반대를 보냅니다. |
| 장치 | 대상 | `<대상>@` | `r1=1` | 버튼을 누르면 설정대로 보냅니다. all은 all=1/all=0 명령 하나로 보냅니다. 상태를 받기 전에는 토글을 보내지 않습니다. |
| 장치 | 서버 | `#lab$state` | `s1=1;s2=1` | 그룹 대상이면 공유 상태 전체를 바꿔 씁니다. |
| 앱 | 장치 | `<cid>@` | `b1 / b2=r3 / target=?k3vd` | 누르기, 버튼 설정 바꾸기, 대상 바꾸기. 처리 뒤 상태로 답합니다. |
| 장치 | 서버 | `@$state` | `target=?k3vd;link=1;b1=;b2=r3` | link는 대상 상태를 알고 있는지입니다. 잘못된 설정은 ?로 보입니다. |

## 참고

- 버튼 동작 형식과 기본값은 IOSignal 라이브러리의 IOSignalBinding.h 맨 위에 있습니다. 보드 설정은 최대 8개라 버튼은 7개까지(target 포함) 둘 수 있고, 이 예제는 group, target, b1, b2를 씁니다.
- 실물 버튼 동작은 아직 확인하지 않았습니다(draft).

## 컴파일

```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini examples/button-controller
arduino-cli compile --fqbn esp32:esp32:esp32 examples/button-controller
arduino-cli compile --fqbn esp32:esp32:esp32c3 examples/button-controller
arduino-cli compile --fqbn esp32:esp32:esp32s3 examples/button-controller
```
