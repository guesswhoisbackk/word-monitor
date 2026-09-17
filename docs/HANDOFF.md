# WordMon Studio 인수인계 문서

작성일: 2026-09-17

## 프로젝트

- 저장소: `C:\antigravity\word monitor` (아직 git 초기화 전 — 시작할 때 `git init` 후 첫 커밋 권장)
- 앱: WordMon Studio v0.1.0 (하드웨어 검증용 + 인터넷 단어장)
- 목적: 사용자의 **두 번째 ESP32**에서 하루에 하나씩 자동으로 바뀌는 영어단어 연습장 만들기
- `C:\antigravity\bambu-monitoring`(Bambu 프린터 모니터)과 완전히 별개 프로젝트. 프린터/MQTT 코드는 가져오지 않았고 하드웨어 계층만 복사했다. 서로 영향을 주지 않는다.
- 인수인계 시 먼저 `git status`와 최근 커밋을 확인하고 기존 변경을 덮어쓰지 않는다.

## 원본에서 가져온 것

출처: `C:\antigravity\bambu-monitoring` commit `3f443e2` (2026-09-17 기준 main)

| 파일 | 손본 내용 |
|---|---|
| `include/display_driver.hpp` | 네임스페이스 `printmon`→`wordmon`, 패널 매크로 `PRINTMON_PANEL_*`→`WORDMON_PANEL_*` |
| `include/app_config.hpp` | MQTT/프린터 상수 제거, 앱 이름·버전 교체, NTP/타임존 상수 추가 |
| `include/models.hpp` | 프린터 구조체 제거, `AppSettings`(Wi-Fi+밝기)만 남김 |
| `src/settings_store.cpp` | 프린터 직렬화 제거, Wi-Fi/밝기만 NVS에 저장 |
| `src/web_portal.cpp` | 프린터 등록 폼 10개 제거 → Wi-Fi 검색/저장 + 밝기만. 문구는 단어장용으로 교체 |
| `platformio.ini` | PubSubClient(MQTT) 제거, 패널 매크로 이름 교체. LVGL 9.5 / LovyanGFX 1.2.25 / ArduinoJson 7.4.2 / XPT2046는 동일 |
| `scripts/build.ps1` | 출력 파일명 `WordMonitor-*`으로 교체, `.venv` 없으면 PATH의 `pio`로 폴백 |
| `src/ui.cpp` | **복사하지 않음** — LVGL+터치 초기화 패턴만 발췌해 `src/word_ui.cpp`로 새로 작성 |

새로 작성한 파일: `src/word_ui.cpp`, `include/word_ui.hpp`, `src/main.cpp`, `README.md`, `docs/HARDWARE_TEST.md`, 이 문서.

원본의 검증된 자산(실기기에서 화면·터치·Wi-Fi가 확인된 코드 경로): LovyanGFX 패널 초기화, LVGL 부분 프레임버퍼, XPT2046 터치 좌표 변환, 청크 스트리밍 웹 포털, NVS 설정 저장. 전부 그대로 계승했다.

## 하드웨어 — 2026-09-17 실물 확인 완료

- 대상 보드: 사용자의 **ESP32-2432S028R(CYD)**, USB 시리얼 CH340 → **COM4** (Windows)
- **패널 확정: ILI9341.** ST7789 프로필은 화면 전체가 하얗게 됨(사용자 확인). 이후 플래싱은 항상 `cyd-ili9341` 빌드 사용. ST7789 프로필은 다른 보드용으로만 유지.
- 터치·Wi-Fi·단어장 동기화는 **아직 미검증** (사용자가 집이 아니라서 설정 연기).

### 플래싱/로그 명령 (이번에 실제로 쓴 것)

```powershell
# 플래시 (통합 BIN을 0x0에)
.\.venv\Scripts\python.exe "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32 --port COM4 --baud 460800 write_flash 0x0 build\WordMonitor-vX.Y.Z-cyd-ili9341-merged.bin

# 부팅 로그 15초 보기 (리셋 후 읽기)
.\.venv\Scripts\python.exe -c "import serial,time; s=serial.Serial('COM4',115200,timeout=1); s.dtr=False; s.rts=True; time.sleep(0.1); s.rts=False; e=time.time()+15
while time.time()<e:
 l=s.readline()
 print(l.decode('utf-8','replace'),end='') if l else None"
```

웹 플래셔 없이 위 명령으로 충분하다. 최초 1회 `erase_flash`도 했음(NVS 초기화).

## 2026-09-17 저녁 세션 — 첫 실기기 단어장 검증 완료 (v0.1.1)

집 PC에서 사용자 설정 완료 후 **전체 파이프라인 검증 성공**: Wi-Fi 연결 → jsDelivr manifest 동기화(8단어) → 카드 선택 → apple 그림 PNG 다운로드·캐시·RGB565 디코드까지 로그 확인(`[wb] card #3: apple (art)`).

- 집 PC에서는 보드가 **COM6**으로 잡힌다(COM4는 이전 위치 기준).
- 사용자가 포털에 단어장 URL을 `guesswhoisbakk`로 오타 입력해 동기화 실패했었음 → 포털에서 재저장으로 해결. STA 모드에서도 `http://<IP>/` 포턠로 설정 수정 가능(빈 Wi-Fi 비밀번호 제출 시 기존 값 유지).
- v0.1.0의 버그 3개를 수정해 v0.1.1로 플래시:
  1. **첫 동기화가 부팅 10분 후에야 시도됨** — `syncDue()`가 `lastAttemptMs_`(0 초기화) 기준으로 대기. `everAttempted_` 플래그 추가로 캐시 없는 첫 부팅에 즉시 시도.
  2. **`/wb` 디렉터리를 만드는 코드가 없어 다운로드 저장 실패** — `begin()`에서 `LittleFS.mkdir(kWordbookDir)` 추가.
  3. **PNGdec 드로잉 콜백이 `return 0`이라 첫 줄만 그리고 `PNG_QUIT_EARLY`로 중단** — PNGdec 규약상 계속 그리려면 0이 아닌 값 반환. `pngLineDraw`가 `return 1`로 수정. `decodeArt`에 단계별 실패 로그도 추가.
- 단어 카드 index는 **0-based `tm_yday`** 기준(`yday % 단어수`). 오전 문서의 "apple index 4 (yday 260 % 8)" 계산은 1-based day를 써서 한 칸 어긋났었음 — wordbook 저장소에서 apple을 index 3으로 이동(커밋 0b4e2e4, jsDelivr purge 완료). 2026-09-17 yday=259 → 259%8=3 → apple. 다음 날(yday 260)은 index 4 = vivid부터 순환.
- **터치(카드 뒤집기)만 아직 실물 미확인.**

## 2026-09-17 세션 결과 — 집에 가서 할 일 (완료됨, 아래 저녁 세션 참조)

현재 기기 상태: v0.1.0 ILI9341 펌웨어 구동 중, 화면 정상(사용자 확인), **설정 모드(AP) 대기 중**.

1. 휴대폰 와이파이에서 **`WordMon-2DF4`** 연결 (비밀번호 `wordmon1`) — SSID 접미사는 기기마다 다름, 부팅 로그에 나옴
2. 브라우저 `192.168.4.1` → 집 Wi-Fi SSID/비밀번호 + 단어장 URL `https://cdn.jsdelivr.net/gh/guesswhoisbackk/wordbook@main` 입력 후 저장 → 자동 재부팅
3. 1~2분 뒤 확인:
   - 화면 하단 `Wi-Fi <IP> - WB 8` (단어장 8단어 동기화됨)
   - **오늘 카드 = apple + 빨간 사과 그림**이 떠야 한다. 이 테스트를 위해 2026-09-17 기준 words.jsonl에서 apple을 index 4(yday 260 % 8)로 옮겨뒀다(커밋 a1b98cf, jsDelivr purge 완료). 다음 날부터는 frugal부터 순환.
   - 카드 터치 → 뜻/예문 표시
4. 시리얼 로그에 `[wb] manifest synced: 8 words`, `card #4: apple (art)` 나오는지 확인
5. 이후 HARDWARE_TEST.md 6~7장(오프라인 캐시, 안정성) 마무리

문제가 생기면: 부팅 로그의 `[wb]`/`[wifi]` 줄이 판단 재료. `WB OFF` = 다운로드 실패(URL/인터넷 확인), `WB CACHED` = 캐시로만 운영 중.

## 현재 구현 상태 (v0.1.0)

- [x] 두 프로필 컴파일 통과 (2026-09-17, RAM 36.2% / Flash 83.7%. v0.0.2 대비 Flash +11.8%: PNGdec+zlib, HTTPClient/TLS, wordbook 모듈)
- [x] 플래싱용 통합 BIN 생성 완료 — `build\WordMonitor-v0.1.0-cyd-{st7789,ili9341}-{merged,firmware}.bin`
- [x] 화면+터치+LVGL 초기화 (원본에서 검증된 코드 경로 그대로)
- [x] Wi-Fi STA 연결, 15초 실패 시 `WordMon-XXXX` AP(비밀번호 `wordmon1`) + DNS 포털
- [x] 웹 설정 페이지: Wi-Fi 검색/저장, 화면 밝기, **단어장 기본 URL** → NVS 저장 후 재부팅
- [x] 플래시카드 카드: 앞면(단어 + 일러스트, 없으면 첫 글자 + TAP TO REVEAL) ↔ 터치로 뒷면(뜻/예문)
- [x] **인터넷 단어장 (방식 B, 사용자 선택)**: `src/wordbook.cpp`가 `{URL}/words.jsonl`(JSON Lines, 한 줄 = {"w","m","e","a"})을 내려받아 LittleFS 캐시. 카드 선택은 `yday % 단어수`. 그림은 `{URL}/{a}`를 내려받아 PNGdec로 RGB565 디코드(카드색 배경 합성) 후 표시. 부팅 시 + 12시간 주기 동기화, 실패 시 10분 재시도, 오프라인이면 캐시로 운영(`WB CACHED`), 캐시도 없으면 내장 샘플 8개. 아트 캐시는 **현재 카드 그림 1개만 유지**(evictOtherArt) — 128KB LittleFS가 며칠 만에 찰 수 있어서.
- [x] 내장 에셋 파이프라인: `scripts/svg_to_header.py` (SVG→ARGB8888 헤더, `include/assets/`), `scripts/wordbook_art.py` (SVG→PNG + `words.jsonl` 등록, `docs/wordbook-sample/`). SVG 래스터라이저는 Windows DLL 문제로 cairosvg/svglib 대신 Edge 헤드리스 사용.
- [ ] **실물 보드 검증** — `docs/HARDWARE_TEST.md` 순서대로 (6장이 단어장 검증)
- [ ] 진짜 단어장 콘텐츠 (아래 로드맵)

### 메모리 예산 (CYD 4MB, 실측)

- 정적 RAM 118.7KB(예산 ~125KB라 여유 없음) → **정적 대형 버퍼 금지**. PNGdec `PNG` 객체 하나가 45.7KB(32KB zlib 윈도우 포함)라 부팅 후 첫 디코드 때 힙에 1회 할당(`new`, 해제 없음), 아트 픽셀도 최대치(120×88×2=21KB)로 1회 할당 후 매일 재사용 — 일일 할당/해제가 없어 파편화 걱정 없음.
- 힙 여유 ~180KB. 동시 최대 소비: PNG 45.7 + 아트 21 + TLS 핸드셰이크 ~40 + WiFi 런타임 ~50 → 실측 필요(하드웨어 검증 항목).
- Flash 앱 83.7% (321KB 여유). `huge_app.csv` 전환 시 3MB로 확장 가능하나 OTA 슬롯 상실.
- 단어장 용량 상한: manifest ~100B/단어 + 그림 1개 → 128KB FS에서 manifest만 약 1,000단어. 1,000을 넘기면 파티션 재배치 필요.

## 개발 환경

PowerShell:

```powershell
cd "C:\antigravity\word monitor"
python -m venv .venv
.\.venv\Scripts\pip install platformio
.\scripts\build.ps1                          # 두 프로필 모두
.\scripts\build.ps1 -Environment cyd-ili9341 # 한쪽만
```

- `.venv`가 없어도 PATH에 `pio`가 있으면 `build.ps1`가 그걸 쓴다.
- 원본 프로젝트의 PlatformIO로도 바로 빌드 가능:
  `C:\antigravity\bambu-monitoring\.venv\Scripts\pio.exe run --project-dir "C:\antigravity\word monitor" -e cyd-ili9341`
- 산출물: `build\WordMonitor-v0.0.1-cyd-*-merged.bin` → Chrome/Edge 웹 플래셔(esptool-web)에서 주소 `0x0`으로 기록
- 시리얼 모니터: 115200 baud. 부팅 로그에 패널 이름·AP 정보·IP가 나온다.

## 단어장 로드맵 (다음 세션 작업 순서)

1. **실기기 패널·터치 확인** — HARDWARE_TEST.md 1~4장. 패널 확정이 모든 이후 작업의 전제다.
2. **인터넷 단어장 실증** — v0.1.0에서 방식 B(외부 호스팅)를 구현했다. HARDWARE_TEST.md 6장 순서로 실물에서 검증. 사용자가 GitHub 저장소를 만들어 샘플(`docs/wordbook-sample/`)을 올리는 것이 첫 단계. 이후 개선 후보: 내려받기 중 UI 멈춤 방지(현재 부팅/자정 동기화는 수 초 블로킹), 인증서 검증 활성화(현재 `setInsecure`, 읽기 전용 공개 콘텐츠라 위험은 낮음), 그림 사전 내려받기(내일 그림 미리 받아 자정 즉시 전환).
3. **UI 확장**: 오늘 단어 + 지난 며칠 복습 카드, 퀴즈 모드(뜻 보고 단어 고르기), 손으로 쓰기 캔버스(`lv_canvas` + 터치, 스펠링 연습). 라벨이 한 줄인 것도 이 단계에서 여러 줄(`LV_LABEL_LONG_WRAP` + 높이 계산)로 개선. 단어 데이터는 이제 words.jsonl이 단일 소스.
4. **한글 뜻 표시**: words.jsonl의 `m`/`e`에 한글을 넣으면 현재 화면에서 깨진다(Montserrat는 ASCII 전용). NotoSansKR 서브셋(실제 사용 음절만 추리면 수백 KB)을 lv_font_conv로 만들어 연결. 웹 포털의 한글은 브라우저가 그리므로 문제없음.
5. **시간 관련 확인**: 타임존은 `include/app_config.hpp`의 `kTimezone`(`KST-9`). Wi-Fi 일시 실패로 부팅하면 캐시된 어제 카드 또는 내장 샘플이 뜨는 것이 설계된 동작.
6. **(선택) 야간 자동 밝기/화면 끄기**: NTP 시간이 있으니 시간대별 밝기 조절이 가능. `display_.setBrightness()`로 즉시 적용된다.
7. **(선택) S3 이전**: 사용자가 그림 몇백 장을 준비 중. 16MB S3 보드로 옮기면 파티션/메모리 제약이 사라진다. `platformio.ini` 보드 교체 + `display_driver.hpp` 패널/정전식 터치 교체가 주된 작업.

## 검증 명령

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Environment all
```

하드웨어 없이 할 수 있는 검증은 컴파일뿐이다. 실기기 검증은 `docs/HARDWARE_TEST.md` 참조.

## 주의 (원본 프로젝트 정책 승계)

- 설정 포털은 로컬 HTTP다. 공용 네트워크에서 설정하지 말고 신뢰하는 가정·작업실 LAN에서만 사용.
- 공개 저장소 문서에는 Wi-Fi 이름, Wi-Fi 비밀번호, 내부 IP를 기록하지 않는다.
- 이 폴더는 아직 git 관리가 아니다. 작업 시작 전 `git init` + 첫 커밋으로 현재 상태를 보존한다.

## 문제 발생 시 수집할 정보

- 보드 앞·뒷면 사진과 모델 표기
- 사용한 BIN 파일명
- 화면 사진 또는 짧은 영상
- 115200 baud 시리얼 로그
- 화면 하단에 표시된 IP 또는 AP 이름
