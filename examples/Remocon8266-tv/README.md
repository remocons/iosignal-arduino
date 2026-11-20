# Remocon8266 tv (bayo.tv 리모컨과 OLED 상태)

Remocon8266 보드의 버튼 4개로 bayo.tv 화면을 CID로 제어하고, 화면의 재생 상태·음량·음소거·셔플과 영상 제목(한글)을 OLED에 보여 줍니다. 대상 화면(target)과 버튼 명령(b1~b4)은 보드 설정이고, target이 비면 같은 공유기의 bayo.tv를 찾습니다.

## 준비물

- 보드: Remocon8266 보드 (D1 mini 기반) (폴더의 회로도 PDF 참고)
- 네트워크: Wi-Fi 2.4GHz 공유기
- 앱: https://remocon.kr 장치 카드(버튼 목록: playToggle, left, right, down, vol+, vol-, mute)가 이 보드를 거쳐 화면을 제어합니다
- 연동: bayo.tv: 제어할 화면입니다. 브라우저에서 https://bayo.tv 를 열어 둡니다.

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| GPIO16 (D0) | SW1 위: b1 (기본 playToggle) | 외부 풀업 |
| GPIO0 (D3) | SW2 왼쪽: b2 (기본 left: 이전 챕터 또는 영상) | 내부 풀업 |
| GPIO14 (D5) | SW3 아래: b3 (기본 down, 다음 카테고리) | 내부 풀업 |
| GPIO13 (D7) | SW4 오른쪽: b4 (기본 right: 다음 챕터 또는 영상) | 내부 풀업 |
| GPIO4, GPIO5 | SSD1306 OLED (SDA, SCL) | I2C |

## 사용 순서

1. Wi-Fi는 업로드한 뒤 USB로 연결해 https://arduino.iosignal.net/wifi/ (보드 설정)에서 넣습니다(데스크톱 Chrome·Edge, 보드가 저장). 같은 페이지에서 다른 IOSignal 서버(호스트:포트), 장치 키와 보드 설정도 넣을 수 있습니다. 스케치의 Wi-Fi 값에 실제 값을 쓰면 그 값이 부팅할 때마다 우선합니다.
2. 같은 공유기의 PC나 TV 브라우저에서 https://bayo.tv 를 열어 둡니다.
3. 스케치를 업로드합니다. OLED에 'Finding bayo.tv...'가 나온 뒤, 화면을 찾으면 재생 상태와 제목이 나옵니다.
4. SW1(재생/일시정지), SW2(이전 챕터·영상), SW3(다음 카테고리), SW4(다음 챕터·영상)를 눌러 봅니다. OLED 아래 줄에 보낸 명령이 나오고, 화면이 상태를 보내면 위쪽이 바뀝니다.
5. 다른 화면을 쓰려면 보드 설정 target에 그 화면의 CID를 넣거나 remocon pub "<이 보드 cid>@" "target=<화면 cid>"를 보냅니다. 비우면 다시 찾습니다.
6. 버튼 명령은 보드 설정 b1~b4로 바꿉니다(예: vol+, vol-, mute, shuffle, 1).

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 장치 | 서버 | `#search` | `구독` | 앱의 검색 요청을 받도록 구독합니다. |
| 장치 | 서버 | `@$name` | `Remocon8266 TV:HOME` | 이름과 분류입니다. 서버에 보관됩니다. 보드 설정 name으로 바꿉니다. |
| 장치 | 서버 | `@$ui` | `playToggle,left,right,down,vol+,vol-,mute` | 앱 카드의 버튼 목록입니다. 누르면 이 보드가 같은 명령을 화면에 보냅니다. |
| 장치 | 서버 | `#search` |  | target이 비었으면 같은 공유기의 장치를 찾습니다. |
| 장치 | 서버 | `<cid>@$name` | `구독` | #notify로 답한 장치의 이름을 읽어 bayo.tv로 시작하는 첫 장치를 화면으로 고릅니다. |
| 장치 | 서버 | `<tv cid>@$state` | `구독` | 화면의 보관된 상태를 바로 받고, 바뀔 때마다 받습니다. |
| 서버 | 장치 | `<tv cid>@$state` | `close` | 화면이 끊기면(새로고침하면 CID가 바뀜) target이 빈 경우 다시 찾고, 찾을 때까지 5초마다 #search를 보냅니다. |
| bayo.tv | 장치 | `<tv cid>@$state` | `state=playing, {title, chapterTitle, chapterCount, chapters.index, volume, muted, shuffle, …}` | 값 두 개(MJSON)입니다. 첫 값의 state와 객체의 제목, 음량, 음소거, 셔플을 OLED에 그립니다. 챕터 영상이면 chapterTitle(지금 챕터 제목)과 번호(chapters.index + 1 / chapterCount)를 보여 줍니다. |
| 장치 | bayo.tv | `<tv cid>@` | `playToggle` | 버튼을 누르면 그 버튼의 명령(b1~b4)을 보냅니다. 화면을 아직 모르면 홈채널 #screen으로 보냅니다. |
| 장치 | 서버 | `@$state` | `tv=<tv cid>;link=1;state=playing;vol=70` | 이 보드 자신의 상태(앱 카드용)입니다. |
| 앱 | 장치 | `<cid>@` | `target=<tv cid>` | 다른 화면을 고릅니다(보드 설정 target에 저장). b1=vol+처럼 버튼 명령도 바꿉니다. |

## 참고

- bayo.tv 명령: play, pause, playToggle, left·right(챕터가 있으면 챕터, 없으면 영상 이동), prev·next(영상 이동), chapterPrev·chapterNext(챕터만), up·down(카테고리), vol+, vol-, vol, mute, shuffle, 1~3(미리 지정한 영상). 자세한 표는 bayo.tv 저장소 README에 있습니다.
- #search는 같은 공인 IP(같은 공유기)에만 전달됩니다. 다른 곳의 화면은 target에 CID를 넣어 씁니다.
- 제목은 u8g2의 unifont 한글 폰트(16px, KS X 1001 한글 2,350자와 ASCII)로 두 줄씩, 길면 3초마다 다음 두 줄을 보여 줍니다. 제목은 159바이트(한글 약 53자)까지 받고 12줄(6페이지)까지 나눕니다. 폰트에 없는 글자(이모지, 꾸밈 문자, 드문 음절)는 그리지 않습니다. 음량·재생 상태만 바뀌면 보던 페이지를 유지합니다.
- 상태는 JSON 라이브러리 없이 필요한 값만 찾아 읽습니다. 수신 버퍼는 1024바이트입니다.
- 2026-11-17 Remocon8266 보드와 bayo.tv에서 확인: 화면 자동 찾기, 상태 수신(link=1), 앱 경로로 playToggle·vol+·vol- 전달, SW4/SW2(right/left)로 챕터 3→4→5→4→3 이동(bayo.tv 4989886부터 left/right가 챕터 우선), 화면 새로고침(CID 변경) 뒤 다시 찾기. 처음 판에서 긴 한글 제목을 줄로 나누다 Soft WDT로 재부팅되던 문제를 고쳤습니다.
- bayo.tv가 음량을 바꾼 직후 이전 음량을 상태로 보내는 경우가 있어 OLED 음량이 한 단계 늦게 보일 수 있습니다(bayo.tv 쪽 문제).
- 보드 설정은 이름으로 칩에 저장되어 다른 스케치에서 남긴 target이 그대로 쓰일 수 있습니다. 자동 찾기를 쓰려면 보드 설정 페이지에서 target을 비우거나 target= 를 보냅니다.
- 2026-11-17 사용자가 OLED(재생 표시, 음량, 한글 제목)와 버튼 4개를 확인했습니다.
- 2026-11-17 긴 제목에서 폰트에 없는 글자(폭 0)가 줄 맨 앞에 오면 빈 줄이 생겨 빈 페이지가 나오던 문제를 고쳤습니다. 이모지·작은 대문자가 든 실제 제목으로 4페이지가 모두 보이는 것을 사용자가 OLED로 확인했습니다.
- 챕터가 있는 영상에서는 제목 자리에 지금 챕터의 제목, 첫 줄에 4/41 같은 챕터 번호가 나옵니다(bayo.tv d52a147부터 chapterTitle과 chapterCount를 보냄). 예전 bayo.tv에서는 영상 제목을 보여 줍니다.
- 2026-11-17 다른 Remocon8266 보드(익명 접속)에서 확인: 챕터 영상에서 right/left로 챕터 10→11→12→11→10을 옮길 때 OLED 제목이 각 챕터 제목으로 바뀌었습니다(시리얼 TITLE 기록).

## 컴파일

```sh
arduino-cli compile --fqbn esp8266:esp8266:d1_mini examples/Remocon8266-tv
```
