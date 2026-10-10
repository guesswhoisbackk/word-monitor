# WordMon Studio

두 번째 ESP32에서 영어 단어를 떠올리고 간격을 두어 복습하는 학습기입니다. `C:\antigravity\bambu-monitoring`의 디스플레이·터치·Wi-Fi 설정 계층을 가져왔습니다.

현재 소스 버전은 **v0.5.3 893단어 그림 카드·자동 진행판**입니다. 2026-10-10에 실기기(COM6)에 적용해 부팅·동기화·그림 표시(사용자 확인 완료)까지 확인했고, SPEAK 스피커 출력은 사용자 확인이 남아 있습니다.

## 893단어 그림 단어장과 한글 표시 (v0.5.0~v0.5.1)

- `content/illustrated-vocabulary/`의 893개 묶음 단어를 기기용으로 변환해 공개 단어장 서버에 배포했습니다. `scripts\build_illustrated_content.py`가 words.jsonl과 그림을 함께 생성합니다.
- **모든 단어에 예문과 발음이 있습니다.** 예문은 `content/illustrated-vocabulary/examples.json`(직접 작성, ASCII 전용)에서, 발음은 `scripts\prepare_audio.ps1`이 Microsoft Zira 음성으로 생성한 11,025Hz 8비트 모노 WAV(평균 약 19KB)입니다. manifest의 `e`·`s` 필드로 연결됩니다.
- **그림은 PNG가 아니라 WMR1 형식**(헤더 8바이트 + 카드 배경색으로 미리 합성한 RGB565 픽셀)로 배포합니다. 기기는 파일을 픽셀 버퍼로 바로 읽으므로 PNG/zlib 디코더와 46KiB 힙 할당이 필요 없습니다. TLS 사용 후 힙이 단편화해 PNG 디코딩이 실패하는 문제를 제거합니다.
- **외부 발음도 스트리밍 재생입니다(v0.5.1).** WAV를 통째로 RAM에 담지 않고 LittleFS에서 512바이트 블록으로 읽어 재생하고, 다운로드도 파일로 스트리밍한 뒤 헤더만 검증합니다. 전체 파일 버퍼 방식은 TLS 핸드셰이크와 메모리를 다투어 부팅 동기화를 실패하게 하므로 폐기했습니다.
- 한글 뜻을 표시합니다. `scripts\make_kr_font.py`가 맑은 고딕 16px에서 현재 단어장의 526개 음절 + ASCII 서브셋을 뽑아 `src/font_kr_16.c`를 생성합니다. 새 한글 텍스트에 서브셋 밖 음절이 있으면 빈칸으로 보이니 스크립트를 다시 실행한 뒤 재빌드해야 합니다.
- 파티션 테이블을 `partitions_wordmon.csv`로 바꿔 쓰지 않던 두 번째 OTA 슬롯(1.875MiB)을 회수했습니다. 앱 파티션 2.25MiB(한글 폰트 여유), LittleFS **1.625MiB**(128KiB에서 확대). NVS 위치는 그대로라 설정·학습 이력이 유지됩니다.
- **파티션 테이블을 처음 적용할 때는 새 LittleFS 영역(`0x250000`, `0x1A0000`)을 반드시 지우고** 앱(`0x10000`)과 파티션 테이블(`0x8000`)을 기록해야 합니다. 지우지 않으면 펌웨어가 기존 데이터가 있는 파일시스템을 보호하려고 마운트 실패로 남습니다.

## 발음 듣기 (v0.3.0)

- 주문한 **8Ω·1W 2415 스피커 + 1.25mm 2핀 케이블**을 CYD 보드의 `SPEAK` 단자에 연결합니다. GPIO26 DAC → 보드 내장 앰프를 사용하며 터치 GPIO25는 활성화하지 않습니다.
- 화면 왼쪽 위 **SPEAK**를 누르면 단어 발음을 들을 수 있고, 재생 중 **STOP**을 누르면 중지합니다. 자동 재생은 하지 않으며 학습 평가 기록에도 영향을 주지 않습니다.
- 기본 8개 단어는 영어 합성 음성(Microsoft Zira)을 펌웨어에 넣어 오프라인으로 재생합니다. 서버 단어장에서도 같은 철자이고 별도 음성 지정이 없으면 내장 발음을 사용합니다.
- 웹 설정의 **발음 음량**은 기본 20, 범위 0~60이며 0은 음소거입니다. 설정 저장 후 재부팅해 적용합니다.
- 추가 단어에는 `words.jsonl`의 `s` 필드로 같은 폴더의 WAV 파일명을 지정합니다. 최근 외부 발음 한 개는 LittleFS에 캐시합니다. 내장 발음도 `s`도 없는 단어는 `NO AUDIO FOR THIS WORD`라고 표시합니다.
- 생성 스크립트, 파일 규격, 연결 및 검증 방법: **[AUDIO.md](docs/AUDIO.md)**. 바로 호스팅할 예제 폴더는 `docs/audio-sample/`이며 아직 콘텐츠 서버에 배포하지 않았습니다.

## 학습 방법 (v0.4.0)

1. 영어 단어와 그림을 보고 뜻을 먼저 떠올립니다. 필요하면 SPEAK로 발음을 듣고 따라 말합니다.
2. 카드를 터치해 뜻과 예문을 확인합니다. 긴 내용은 답 영역을 위아래로 스크롤합니다.
3. 기억이 안 났으면 **AGAIN**, 스스로 기억했으면 **GOT IT**을 누릅니다.
4. **NEXT**를 누르면 기한이 지난 복습 단어를 먼저 보여주고, 없으면 목록에서 처음 나오는 미학습 단어를 보여줍니다. 모두 끝났으면 나중에 돌아오라는 안내가 뜹니다.

- AGAIN: 10분 후 다시 복습. GOT IT: 연속 성공에 따라 1일 → 3일 → 7일 → 14일 → 30일 후 복습합니다. 이는 초기 운영값이며 개인별 최적 간격을 추정하는 알고리즘은 아닙니다.
- **그림은 카드 앞면에 항상 표시됩니다**(v0.5.2). 확인이 필요하면 카드를 탭해 뜻·예문을 보면 됩니다.
- **평가(AGAIN/GOT IT)는 카드의 어느 면에서든 누를 수 있고, 평가하면 자동으로 다음 카드로 진행합니다**(v0.5.3). 복습 시각이 되기 전·하루 새 단어 수를 다 쓴 카드는 평가가 잠기고, 기한이 지난 복습 단어가 있으면 NEXT·자동 진행 모두 그 단어를 먼저 보여줍니다.
- 하루 새 단어는 기본 **5개**, 웹 설정에서 **0~20개**로 바꿀 수 있습니다. 0은 복습만 진행합니다. 처음 평가한 단어만 셉니다(AGAIN도 포함). 재복습·단순 조회·발음 듣기는 새 단어 수를 늘리지 않습니다. `NEW 2/5`처럼 오늘 수를 표시합니다.
- 날짜로 목록을 건너뛰지 않습니다. 쉬었던 날 이후에도 다음 미학습 단어부터 시작하고, 지난날의 학습량은 누적하지 않습니다. 하루 기준은 한국 시간 자정입니다. 기본 5개와 복습 간격은 조절 가능한 시작값이며 개인별 최적값을 측정한 결과는 아닙니다.
- 이력은 LittleFS `/study-v2.bin`에 저장하며 최대 **1,000개 고유 단어**를 지원합니다. 기존 128개 NVS 이력을 자동으로 이전합니다. 목록 순서가 바뀌어도 단어 철자의 64비트 키로 이력을 유지합니다. 같은 철자는 단어장 간 이력을 공유합니다.
- 저장은 임시 파일 검증 후 교체하며, 실패하면 평가·오늘 수를 되돌립니다. 공간 부족 시 재다운로드할 수 있는 그림·발음 캐시부터 비웁니다. 손상된 이력이나 기존 파일시스템의 마운트 실패를 자동 초기화하지 않습니다.
- 오프라인에서도 캐시를 읽습니다. 전원을 완전히 끈 뒤 현재 시간을 모르면 첫 캐시 단어는 볼 수 있지만, 정확한 복습 예약을 위해 시간 동기화 전 평가·기한 복습을 제한합니다.
- 다운로드 실패 시 최소 10분 간격으로 재시도합니다. 잘린 응답, 저장 실패, 잘못된 JSON은 기존 단어장 캐시를 덮어쓰지 않습니다. 빈 줄은 무시하며, 항목당 최대 512바이트·최대 1,000항목을 검증합니다. 실제 캐시 용량은 LittleFS의 남은 공간에 제한됩니다.

**남은 작업:** 실물 화면에서 한글·그림·예문 육안 확인과 스피커 발음 검증. 단어 뜻·난이도·학습 순서 검수(현재는 원본 배치 순서). 단어장/그림/발음 다운로드 중 UI 멈춤과 HTTPS 인증서 검증 생략도 남아 있습니다.

학습 설계 참고: [Spacing, Feedback, and Testing Boost Vocabulary Learning in a Web Application](https://pmc.ncbi.nlm.nih.gov/articles/PMC8638698/).

개발용 호스트 테스트: Visual Studio C++ Build Tools가 있는 Windows에서 `scripts\test_host.cmd` 실행. 복습 우선순위, 날짜 경계, 하루 수·재부팅, 1,000개 이력, 저장 실패, 공간 부족, 기존 이력 이전을 확인합니다. 저장 장치는 테스트 대역이며 실제 플래시 내구성·전원 차단 시험을 대신하지 않습니다.

- 240×320 세로 화면에 오늘의 단어 카드 표시
- 플래시카드 방식: 앞면에는 단어와 그림, 터치하면 뒷면(뜻/예문)으로 전환. 평가하면 자동으로 다음 카드
- **인터넷 단어장**: GitHub 등에 올린 `words.jsonl` + 그림 PNG를 내려받아 기한 복습과 순차 학습에 사용합니다. 목록·최근 그림은 LittleFS에 캐시됩니다.
- 단어장 기본 URL은 웹 설정 페이지에서 변경 (리플래시 불필요). 비워 두면 내장 샘플 8개로 동작
- Wi-Fi 설정 AP(`WordMon-XXXX`, 비밀번호 `wordmon1`)와 웹 설정 페이지
- 설정은 ESP32 NVS에 저장

## 인터넷 단어장 꾸리기

단어장 저장소: **https://github.com/guesswhoisbackk/wordbook** — 2026-10-10 확인: **단어 893개(뜻·예문·발음 참조 포함) + WMR1 그림 893개 + 발음 WAV 893개 배포 완료**(커밋 `ced945b`, 약 30MB). 로컬 클론은 `C:\antigravity\wordbook`, 배포는 `scripts\push_wordbook.ps1`.

그림 단어 자료의 원본과 변환 산출물은 [이 프로젝트의 자료 폴더](content/illustrated-vocabulary/README.md)에 있다. 원본 921개 중 영어 이름 중복을 묶은 893개를 기기용으로 변환했으며, 상세 기록은 [콘텐츠 확인 기록](docs/CONTENT-AUDIT-2026-10-03.md)을 참고. 학습 순서는 아직 원본 배치 순서이지 검수된 난이도 순서가 아니다.
기기 설정 페이지의 "단어장 기본 URL"에 넣을 주소: **`https://cdn.jsdelivr.net/gh/guesswhoisbackk/wordbook@main`**
(NAS/홈서버로 바꾸고 싶을 때는 Web Station·nginx 등으로 폴더를 노출하고 `http://<NAS_IP>:<포트>/<경로>`를 쓰면 된다. 평문 http도 지원.)

자료 일괄 재변환(그림·예문·발음 참조 manifest 재생성)은 프로젝트 루트에서:
```powershell
C:\antigravity\bambu-monitoring\.venv\Scripts\python.exe scripts\build_illustrated_content.py
# 발음 WAV 재생성(예문 수정·단어 추가 시): words.jsonl을 받아 같은 폴더에 WAV 출력
powershell -File scripts\prepare_audio.ps1 -Manifest build\illustrated-device\words.jsonl -OutDir build\illustrated-audio
.\scripts\push_wordbook.ps1
```
그림 규격: **WMR1 형식**(매직 4바이트 + 가로·세로 uint16 리틀엔디언 + 카드색 합성 RGB565 픽셀). 최대 120×88, 64KiB. 변환 스크립트가 원본 PNG에서 자동 생성하며, 배경 합성 색은 `src/word_ui.cpp`의 카드색과 맞춰야 한다.

자세한 계획과 다음 작업은 `docs/HANDOFF.md`을, 실제 보드 시험 순서는 `docs/HARDWARE_TEST.md`을 보세요.

## 빌드

PowerShell에서:

```powershell
cd "C:\antigravity\word monitor"
python -m venv .venv
.\.venv\Scripts\pip install platformio
.\scripts\build.ps1
```

`.venv`가 없어도 PATH에 `pio`가 있으면 그걸 사용합니다. 완성된 파일은 `build` 폴더에 생성됩니다.

- `WordMonitor-vX.Y.Z-cyd-*-merged.bin`: Chrome/Edge 웹 플래셔에서 주소 `0x0`으로 기록
- `WordMonitor-vX.Y.Z-cyd-*-firmware.bin`: OTA/개발용 앱 영역 파일

화면이 하얗거나 깨지면 다른 패널 프로필 BIN으로 다시 시험합니다.

## 폴더 안내

- `src/word_ui.cpp`: 단어 카드 화면과 터치
- `src/wordbook.cpp`: 인터넷 단어장 (다운로드·캐시·PNG 디코드·일별 선택)
- `include/assets/`: 펌웨어 내장 일러스트 에셋 (생성된 헤더)
- `scripts/svg_to_header.py`: SVG → LVGL 이미지 헤더 변환기 (펌웨어 내장용)
- `scripts/wordbook_art.py`: SVG → 단어장 PNG + words.jsonl 등록 (인터넷 단어장용)
- `docs/wordbook-sample/`: 인터넷 단어장 샘플 (GitHub에 올리는 형태)
- `src/web_portal.cpp`: Wi-Fi 설정 웹페이지
- `src/settings_store.cpp`: 설정 저장
- `include/display_driver.hpp`: ST7789/ILI9341 화면 드라이버
- `include/app_config.hpp`: 버전, 핀, 화면 크기, 터치 보정값, 시간대
- `platformio.ini`: 개발 환경과 라이브러리 버전
- `scripts/build.ps1`: 두 프로필 빌드와 통합 BIN 생성

## 주의

설정 페이지는 로컬 HTTP이므로 공용 네트워크에서는 사용하지 말고, 신뢰하는 가정·작업실 LAN에서만 사용하세요.
