# WordMon Studio

두 번째 ESP32에서 매일 바뀌는 영어단어 연습장을 만드는 프로젝트입니다. `C:\antigravity\bambu-monitoring`의 하드웨어 계층(디스플레이 드라이버, 터치, Wi-Fi 설정 포털)을 가져와 프린터 모니터링 코드는 모두 제거한 스켈레톤입니다.

현재 버전은 **v0.1.0 하드웨어 검증용**입니다.

- 240×320 세로 화면에 오늘의 단어 카드 표시
- 플래시카드 방식: 카드 앞면에는 단어 + 일러스트(있을 때) 또는 첫 글자, 터치하면 뒷면(뜻/예문)으로 전환
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
