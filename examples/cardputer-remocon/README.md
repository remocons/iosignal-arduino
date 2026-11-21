# Cardputer remocon (주머니 리모컨)

M5Stack Cardputer(ADV 포함)가 앱처럼 #search를 보내 같은 공유기의 장치를 찾고, 분류 탭별 목록으로 보여 줍니다. 장치를 열면 공식 타입의 조작(스위치, 슬라이더, 버튼, 선택)이나 버튼 목록을 키보드로 쓰고, 장치의 그림($view 또는 타입의 기본 그림)과 상태를 컬러 화면에 보여 줍니다. 어떤 명령이든 직접 입력해 보낼 수도 있습니다.

## 준비물

- 보드: M5Stack Cardputer / Cardputer ADV (StampS3, ESP32-S3) (라이브러리 M5Cardputer 1.1.1 (M5Unified 0.3.0, M5GFX 0.3.0). 화면은 가로가 되는 회전을 스스로 고릅니다)
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://remocon.kr 장치 카드의 scan 버튼으로 다시 찾습니다
- 연동: 같은 공유기의 IOSignal 장치: #search에 #notify로 답하는 장치(다른 예제 보드, IOSignal Desktop 가상 장치, bayo.tv 등)를 찾아 제어합니다

## 사용 순서

1. Arduino 라이브러리 매니저에서 M5Cardputer를 설치합니다(M5Unified, M5GFX가 함께 설치됨).
2. 보드는 M5Cardputer, USB CDC On Boot는 Enabled, Partition Scheme은 Minimal SPIFFS (1.9MB APP with OTA)로 골라 업로드합니다.
3. Wi-Fi는 업로드한 뒤 USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 넣습니다(데스크톱 Chrome·Edge, 보드가 저장). 같은 페이지에서 다른 IOSignal 서버, 장치 키와 보드 설정 name도 넣을 수 있습니다.
4. 배선은 없습니다. 화면, 키보드와 G0 버튼(다시 찾기)은 Cardputer에 들어 있습니다.
5. 목록: `,` `/`(←→) 분류 탭, `;` `.`(↑↓) 이동, Enter 열기, ` 또는 Backspace 다시 찾기.
6. 장치: `;` `.` 조작 고르기, Enter(또는 Space) 누르기, `,` `/` 슬라이더·선택 값 바꾸기(손을 떼고 0.3초 뒤 보냄), 숫자 1~9 그 조작 바로 누르기, ` 또는 Backspace 목록으로.
7. 마지막 조작 '> 명령 입력'에서 명령을 직접 쓰고 Enter로 보냅니다. Tab은 취소입니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 장치 | 같은 공유기의 장치 | `#search` |  | 접속할 때, ` 키, G0 버튼, 앱 카드의 scan 버튼에서 보냅니다. |
| 다른 장치 | 장치 | `#notify` | `<cid>` | 검색에 자기 CID로 답합니다. 최대 24대까지 목록에 넣습니다. |
| 장치 | 서버 | `<cid>@$name, $ui, $state` | `구독` | 찾은 장치마다 구독합니다. 분류 탭은 $name의 첫 : 뒤에서 만들고, 없으면 미분류입니다. |
| 장치 | 서버 | `<cid>@$view` | `구독` | 연 장치의 $ui에 v=1이 있을 때만 구독합니다. 없으면 공식 타입의 기본 그림을 씁니다. |
| 장치 | 다른 장치 | `<cid>@` | `on, off, s1=1, level=70 …` | 조작이나 직접 입력한 명령입니다. 장치가 $state로 답하면 '보냄...' 표시가 풀리고, 3초 안에 답이 없으면 '응답 없음'. |
| 서버 | 장치 | `<cid>@$state` | `close` | 장치가 끊기면 목록에 'x 끊김'으로 표시합니다. |
| 장치 | 서버 | `@$name` | `Cardputer Remocon:HOME` | 이 보드의 이름과 분류입니다(보드 설정 name). |
| 장치 | 서버 | `@$ui` | `scan` | 앱 카드의 버튼입니다. 누르면 다시 찾습니다. |
| 장치 | 서버 | `@$state` | `devices=3;open=<cid>` | 찾은 장치 수와 열어 본 장치입니다. |

## 참고

- 조작은 장치 규칙의 화면 고르기를 따릅니다: 공식 타입(t=) → b= 버튼 → 예전 버튼 목록. 공식 타입 표는 스케치 안에 있고(_iosignal/devices/types와 같은 조작, 색상은 미리 정한 색 선택), 장치가 직접 정한 화면($panel, p=1)은 읽지 않으므로 그런 장치는 타입이나 b= 버튼으로 보입니다.
- 그림은 bulb, fan, relays, thermo, blind, text를 그립니다. 모르는 kind는 그리지 않습니다. 그림이 없는 장치는 상태를 key value 줄로 보여 줍니다.
- 값이 두 개 이상인 상태(bayo.tv 등)는 첫 값만 보여 줍니다.
- 한글은 M5GFX의 efontKR_12 글꼴로 그립니다. 이 글꼴 때문에 프로그램이 1.2MB를 넘어 Minimal SPIFFS 분할이 필요합니다.
- 키보드 방향키는 Cardputer 키의 아래 글자(; . , /)이며 Fn 없이도 움직입니다. 명령 입력 중에는 그 글자를 입력합니다(; 는 규칙상 명령에 쓸 수 없어 막음).
- 2026-11-22 Cardputer ADV에서 확인: 저장한 Wi-Fi로 io.iosignal.net 접속, #search로 같은 공유기 장치 6대를 찾아 분류 탭 목록에 표시, 화면이 가로로 바르게 나옴(ADV는 회전 0을 자동으로 고름), 스위치(plug1 s1·s2, relay4ch·r4b r1~r4), 버튼 목록(?xyzs b1, ?b27b play·pause·next), media-remote.1 선택(playToggle)으로 실제 장치 제어, 사용자가 화면과 장치 동작을 확인.
- 2026-11-22 remocon.kr에서 확인: Cardputer Remocon 카드가 HOME 분류에 scan 버튼과 함께 나오고, 보드 전원을 끄면 약 1분 뒤 카드가 사라짐(전원을 갑자기 끊으면 서버가 끊김을 알아채는 데 시간이 걸림).

## 컴파일

```sh
arduino-cli compile --fqbn esp32:esp32:m5stack_cardputer:CDCOnBoot=cdc,PartitionScheme=min_spiffs examples/cardputer-remocon
```
