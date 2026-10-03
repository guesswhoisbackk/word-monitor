# WordMon Studio

두 번째 ESP32에서 영어 단어를 떠올리고 간격을 두어 복습하는 학습기입니다. `C:\antigravity\bambu-monitoring`의 디스플레이·터치·Wi-Fi 설정 계층을 가져왔습니다.

현재 소스 버전은 **v0.4.0 순차 학습·하루 학습량 추가판**입니다. 이번 버전의 보드 업로드와 실물 검증은 아직 하지 않았습니다.

## 발음 듣기 (v0.3.0)

- 주문한 **8Ω·1W 2415 스피커 + 1.25mm 2핀 케이블**을 CYD 보드의 `SPEAK` 단자에 연결합니다. GPIO26 DAC → 보드 내장 앰프를 사용하며 터치 GPIO25는 활성화하지 않습니다.
- 화면 왼쪽 위 **SPEAK**를 누르면 단어 발음을 들을 수 있고, 재생 중 **STOP**을 누르면 중지합니다. 자동 재생은 하지 않으며 학습 평가 기록에도 영향을 주지 않습니다.
- 기본 8개 단어는 영어 합성 음성(Microsoft Zira)을 펌웨어에 넣어 오프라인으로 재생합니다. 서버 단어장에서도 같은 철자이고 별도 음성 지정이 없으면 내장 발음을 사용합니다.
- 웹 설정의 **발음 음량**은 기본 20, 범위 0~60이며 0은 음소거입니다. 설정 저장 후 재부팅해 적용합니다.
- 추가 단어에는 `words.jsonl`의 `s` 필드로 같은 폴더의 WAV 파일명을 지정합니다. 최근 외부 발음 한 개는 LittleFS에 캐시합니다. 내장 발음도 `s`도 없는 단어는 `NO AUDIO FOR THIS WORD`라고 표시합니다.
- 생성 스크립트, 파일 규격, 연결 및 검증 방법: **[AUDIO.md](docs/AUDIO.md)**. 바로 호스팅할 예제 폴더는 `docs/audio-sample/`이며 아직 콘텐츠 서버에 배포하지 않았습니다.

## 학습 방법 (v0.4.0)

1. 영어 단어를 보고 뜻을 먼저 떠올립니다. 필요하면 SPEAK로 발음을 듣고 따라 말합니다. 그림은 기본적으로 숨겨집니다.
2. 카드를 터치해 뜻과 예문을 확인합니다. 긴 내용은 답 영역을 위아래로 스크롤합니다.
3. 기억이 안 났으면 **AGAIN**, 스스로 기억했으면 **GOT IT**을 누릅니다.
4. **NEXT**를 누르면 기한이 지난 복습 단어를 먼저 보여주고, 없으면 목록에서 처음 나오는 미학습 단어를 보여줍니다. 모두 끝났으면 나중에 돌아오라는 안내가 뜹니다.

- AGAIN: 10분 후 다시 복습. GOT IT: 연속 성공에 따라 1일 → 3일 → 7일 → 14일 → 30일 후 복습합니다. 이는 초기 운영값이며 개인별 최적 간격을 추정하는 알고리즘은 아닙니다.
- 그림이 있는 단어에서 **HINT**로 그림을 볼 수 있습니다. 힌트를 사용한 카드는 AGAIN으로 평가합니다.
- 정답을 보기 전이나 복습 시각이 되기 전에는 평가 버튼이 비활성화됩니다. 평가 후 NEXT로 다음 카드를 선택합니다. 단어가 바뀌면 정답을 다시 숨깁니다.
- 하루 새 단어는 기본 **5개**, 웹 설정에서 **0~20개**로 바꿀 수 있습니다. 0은 복습만 진행합니다. 처음 평가한 단어만 셉니다(AGAIN도 포함). 재복습·단순 조회·발음 듣기는 새 단어 수를 늘리지 않습니다. `NEW 2/5`처럼 오늘 수를 표시합니다.
- 날짜로 목록을 건너뛰지 않습니다. 쉬었던 날 이후에도 다음 미학습 단어부터 시작하고, 지난날의 학습량은 누적하지 않습니다. 하루 기준은 한국 시간 자정입니다. 기본 5개와 복습 간격은 조절 가능한 시작값이며 개인별 최적값을 측정한 결과는 아닙니다.
- 이력은 LittleFS `/study-v2.bin`에 저장하며 최대 **1,000개 고유 단어**를 지원합니다. 기존 128개 NVS 이력을 자동으로 이전합니다. 목록 순서가 바뀌어도 단어 철자의 64비트 키로 이력을 유지합니다. 같은 철자는 단어장 간 이력을 공유합니다.
- 저장은 임시 파일 검증 후 교체하며, 실패하면 평가·오늘 수를 되돌립니다. 공간 부족 시 재다운로드할 수 있는 그림·발음 캐시부터 비웁니다. 손상된 이력이나 기존 파일시스템의 마운트 실패를 자동 초기화하지 않습니다.
- 오프라인에서도 캐시를 읽습니다. 전원을 완전히 끈 뒤 현재 시간을 모르면 첫 캐시 단어는 볼 수 있지만, 정확한 복습 예약을 위해 시간 동기화 전 평가·기한 복습을 제한합니다.
- 다운로드 실패 시 최소 10분 간격으로 재시도합니다. 잘린 응답, 저장 실패, 잘못된 JSON은 기존 단어장 캐시를 덮어쓰지 않습니다. 빈 줄은 무시하며, 항목당 최대 512바이트·최대 1,000항목을 검증합니다. 실제 캐시 용량은 LittleFS의 남은 공간에 제한됩니다.

**남은 작업:** 실제 스피커 출력·음량·재생 중 터치 검증, 한글 폰트 추가. 현재 Montserrat 폰트는 한글 뜻·예문을 표시하지 못하므로 기기용 콘텐츠는 영어 풀이를 사용해야 합니다. 단어장/그림 다운로드 중 UI 멈춤과 기존 HTTPS 인증서 검증 생략도 남아 있습니다. 오디오 다운로드·재생은 별도 작업에서 실행합니다.

학습 설계 참고: [Spacing, Feedback, and Testing Boost Vocabulary Learning in a Web Application](https://pmc.ncbi.nlm.nih.gov/articles/PMC8638698/).

개발용 호스트 테스트: Visual Studio C++ Build Tools가 있는 Windows에서 `scripts\test_host.cmd` 실행. 복습 우선순위, 날짜 경계, 하루 수·재부팅, 1,000개 이력, 저장 실패, 공간 부족, 기존 이력 이전을 확인합니다. 저장 장치는 테스트 대역이며 실제 플래시 내구성·전원 차단 시험을 대신하지 않습니다.

- 240×320 세로 화면에 오늘의 단어 카드 표시
- 플래시카드 방식: 앞면에는 단어, 터치하면 뒷면(뜻/예문)으로 전환. 일러스트는 HINT로 표시
- **인터넷 단어장**: GitHub 등에 올린 `words.jsonl` + 그림 PNG를 내려받아 기한 복습과 순차 학습에 사용합니다. 목록·최근 그림은 LittleFS에 캐시됩니다.
- 단어장 기본 URL은 웹 설정 페이지에서 변경 (리플래시 불필요). 비워 두면 내장 샘플 8개로 동작
- Wi-Fi 설정 AP(`WordMon-XXXX`, 비밀번호 `wordmon1`)와 웹 설정 페이지
- 설정은 ESP32 NVS에 저장

## 인터넷 단어장 꾸리기

단어장 저장소: **https://github.com/guesswhoisbackk/wordbook** (2026-10-03 확인: 단어 8개·그림 참조 1개, 로컬 `C:\antigravity\wordbook` 클론 없음).

**980개 단어·그림이 이 기기에 준비된 상태는 아닙니다.** 사용자 요청으로 다른 프로젝트의 921개 항목·그림을 [이 프로젝트의 자료 폴더](content/illustrated-vocabulary/README.md)에 가져왔습니다. 영어 이름의 중복을 묶으면 893개입니다. 기기용 변환·연결은 아직입니다. 상세 경로와 제한은 [콘텐츠 확인 기록](docs/CONTENT-AUDIT-2026-10-03.md)을 참고하세요. 1,000개 학습 기록 지원과 콘텐츠 준비는 별개입니다. 128KiB LittleFS에서 안전한 저장 여유 약 40KiB를 제외한 크기로 manifest를 제한하며, 기존 manifest와 다운로드 임시 파일이 함께 들어가야 하므로 실제 동기화 한도는 더 낮습니다.
기기 설정 페이지의 "단어장 기본 URL"에 넣을 주소: **`https://cdn.jsdelivr.net/gh/guesswhoisbackk/wordbook@main`**
(NAS/홈서버로 바꾸고 싶을 때는 Web Station·nginx 등으로 폴더를 노출하고 `http://<NAS_IP>:<포트>/<경로>`를 쓰면 된다. 평문 http도 지원.)

단어를 추가할 때는 프로젝트 루트에서:
```powershell
.\.venv\Scripts\python.exe scripts\wordbook_art.py docs\새단어.svg --word 새단어 --meaning "뜻" --example "예문" --dir C:\antigravity\wordbook
.\scripts\push_wordbook.ps1
```
SVG가 없는 단어는 `--word/--meaning/--example`만 넣으면 텍스트만 추가됩니다. 푸시하면 다음 동기화(부팅 시 또는 12시간 주기, jsDelivr 캐시로 최대 몇 시간 추가)에 반영됩니다.
그림 규격: 세로 최대 88px(권장 80×88). 그보다 크거나 64KB를 넘는 PNG는 무시됩니다.

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
