# WordMon Studio

두 번째 ESP32에서 매일 바뀌는 영어단어 연습장을 만드는 프로젝트입니다. `C:\antigravity\bambu-monitoring`의 하드웨어 계층(디스플레이 드라이버, 터치, Wi-Fi 설정 포털)을 가져와 프린터 모니터링 코드는 모두 제거한 스켈레톤입니다.

현재 소스 버전은 **v0.3.0 발음 재생 추가판**입니다. 최신 펌웨어의 실물 음성·화면·터치 검증과 보드 업로드는 아직 하지 않았습니다.

## 발음 듣기 (v0.3.0)

- 주문한 **8Ω·1W 2415 스피커 + 1.25mm 2핀 케이블**을 CYD 보드의 `SPEAK` 단자에 연결합니다. GPIO26 DAC → 보드 내장 앰프를 사용하며 터치 GPIO25는 활성화하지 않습니다.
- 화면 왼쪽 위 **SPEAK**를 누르면 단어 발음을 들을 수 있고, 재생 중 **STOP**을 누르면 중지합니다. 자동 재생은 하지 않으며 학습 평가 기록에도 영향을 주지 않습니다.
- 기본 8개 단어는 영어 합성 음성(Microsoft Zira)을 펌웨어에 넣어 오프라인으로 재생합니다. 서버 단어장에서도 같은 철자이고 별도 음성 지정이 없으면 내장 발음을 사용합니다.
- 웹 설정의 **발음 음량**은 기본 20, 범위 0~60이며 0은 음소거입니다. 설정 저장 후 재부팅해 적용합니다.
- 추가 단어에는 `words.jsonl`의 `s` 필드로 같은 폴더의 WAV 파일명을 지정합니다. 최근 외부 발음 한 개는 LittleFS에 캐시합니다. 내장 발음도 `s`도 없는 단어는 `NO AUDIO FOR THIS WORD`라고 표시합니다.
- 생성 스크립트, 파일 규격, 연결 및 검증 방법: **[AUDIO.md](docs/AUDIO.md)**. 바로 호스팅할 예제 폴더는 `docs/audio-sample/`이며 아직 콘텐츠 서버에 배포하지 않았습니다.

## 학습 방법 (v0.2.0)

1. 영어 단어를 보고 뜻을 먼저 떠올립니다. 그림은 기본적으로 숨겨집니다.
2. 카드를 터치해 뜻과 예문을 확인합니다. 긴 내용은 답 영역을 위아래로 스크롤합니다.
3. 기억이 안 났으면 **AGAIN**, 스스로 기억했으면 **GOT IT**을 누릅니다.
4. **REVIEW**를 누르면 기한이 지난 복습 단어를 먼저 보여주고, 없으면 오늘의 미학습 단어를 보여줍니다. 모두 끝났으면 나중에 돌아오라는 안내가 뜹니다.

- AGAIN: 10분 후 다시 복습. GOT IT: 연속 성공에 따라 1일 → 3일 → 7일 → 14일 → 30일 후 복습합니다. 이는 초기 운영값이며 개인별 최적 간격을 추정하는 알고리즘은 아닙니다.
- 그림이 있는 단어에서 **HINT**로 그림을 볼 수 있습니다. 힌트를 사용한 카드는 AGAIN으로 평가합니다.
- 정답을 보기 전이나 복습 시각이 되기 전에는 평가 버튼이 비활성화됩니다. 평가 후 다음 카드는 REVIEW로 선택합니다. 단어가 바뀌면 정답을 다시 숨깁니다.
- 이력은 NVS에 저장하며 최대 **128개 고유 단어**를 지원합니다. 가득 차면 기존 기록을 지우지 않고 저장 실패를 표시합니다. 단어 철자의 64비트 해시를 키로 쓰므로 목록 순서를 바꿔도 이력이 유지되고, 같은 철자는 단어장 간 이력을 공유합니다.
- 새 단어는 하루 한 개의 순환 슬롯입니다. 날짜를 건너뛰면 그날의 단어도 건너뛰며, 무제한 새 단어 탐색 기능은 없습니다. 날짜는 한국 시간 기준 누적 일수이므로 연말에 순환이 초기화되지 않습니다.
- 오프라인에서도 캐시를 읽습니다. 전원을 완전히 끈 뒤 현재 시간을 모르면 첫 캐시 단어는 볼 수 있지만, 정확한 복습 예약을 위해 시간 동기화 전 평가·기한 복습을 제한합니다.
- 다운로드 실패 시 최소 10분 간격으로 재시도합니다. 잘린 응답, 저장 실패, 잘못된 JSON은 기존 단어장 캐시를 덮어쓰지 않습니다. 빈 줄은 무시하며, 항목당 최대 512바이트·최대 1,000항목을 검증합니다. 실제 캐시 용량은 LittleFS의 남은 공간에 제한됩니다.

**남은 작업:** 실제 스피커 출력·음량·재생 중 터치 검증, 한글 폰트 추가. 현재 Montserrat 폰트는 한글 뜻·예문을 표시하지 못하므로 기기용 콘텐츠는 영어 풀이를 사용해야 합니다. 단어장/그림 다운로드 중 UI 멈춤과 기존 HTTPS 인증서 검증 생략도 남아 있습니다. 오디오 다운로드·재생은 별도 작업에서 실행합니다.

학습 설계 참고: [Spacing, Feedback, and Testing Boost Vocabulary Learning in a Web Application](https://pmc.ncbi.nlm.nih.gov/articles/PMC8638698/).

개발용 호스트 테스트: Visual Studio C++ Build Tools가 있는 Windows에서 `scripts\test_host.cmd` 실행. 복습 간격, 날짜 경계, 통신 재시도 제한, 재부팅 복원, 저장 실패 복구, 이력 한도를 확인합니다. NVS는 테스트 대역이며 실제 플래시 내구성·전원 차단 시험을 대신하지 않습니다.

- 240×320 세로 화면에 오늘의 단어 카드 표시
- 플래시카드 방식: 앞면에는 단어, 터치하면 뒷면(뜻/예문)으로 전환. 일러스트는 HINT로 표시
- **인터넷 단어장(v0.1.0)**: GitHub 등에 올린 `words.jsonl` + 그림 PNG를 기기가 내려받아 하루에 하나씩 순환 표시. 목록·그림은 LittleFS에 캐시되어 인터넷이 끊겨도 유지
- 단어장 기본 URL은 웹 설정 페이지에서 변경 (리플래시 불필요). 비워 두면 내장 샘플 8개로 동작
- Wi-Fi 설정 AP(`WordMon-XXXX`, 비밀번호 `wordmon1`)와 웹 설정 페이지
- 설정은 ESP32 NVS에 저장

## 인터넷 단어장 꾸리기

단어장 저장소: **https://github.com/guesswhoisbackk/wordbook** (로컬 클론 `C:\antigravity\wordbook`, 2026-09-17 개설)
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
