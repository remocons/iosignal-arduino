# UNO 이더넷 채널 송수신

버튼을 누르면 #homeButton 채널로 신호를 보내고, 같은 채널의 신호를 받으면 LED를 바꿉니다.

## 준비물

- 보드: Arduino UNO R3 + Ethernet Shield
- 네트워크: 유선 이더넷
- 앱: https://lab.iosignal.net/channel/

## 배선

| 보드 핀 | 연결 | 비고 |
| --- | --- | --- |
| 3 | LED | 수신하면 켜짐과 꺼짐이 바뀝니다. |
| 2 | 버튼 | 누르면 GND로 연결됩니다. 내부 풀업을 씁니다. |

## 사용 순서

1. 보드에 맞게 핀 번호를 확인합니다.
2. 여러 보드를 함께 쓰면 MAC 주소를 서로 다르게 바꿉니다.
3. 컴파일하고 업로드합니다.
4. 브라우저로 https://lab.iosignal.net/channel/ 에 접속해 확인합니다.

## 시그널 절차

| 보내는 쪽 | 받는 쪽 | 태그 | 값 | 설명 |
| --- | --- | --- | --- | --- |
| 보드 | 서버 | `#homeButton` | `구독` | 접속이 끝나면 채널을 구독합니다. |
| 보드 | 서버 | `#homeButton` | `uno3-eth` | 버튼을 누르면 보냅니다. |
| 서버 | 보드 | `#homeButton` |  | LED 상태를 바꿉니다. |

## 컴파일

```sh
arduino-cli compile --fqbn arduino:avr:uno examples/uno_ethernet_channel_txrx
```
