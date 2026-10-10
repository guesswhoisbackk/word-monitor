# WordMon Studio 인수인계 문서

## 다음 세션용 최신 요약 — 2026-10-10 저녁, v0.5.0 893단어 적용

**프로젝트:** `C:\antigravity\word monitor\word-monitor`  
**원격:** https://github.com/guesswhoisbackk/word-monitor (`main`)  
**소스 버전:** v0.5.0  
**이번 세션 전 커밋:** `265a5e2`

사용자가 "가져온 893개 자료를 기기에 활용해 변환·연결·검증"을 요청했고 기기를 연결해 뒀다. **변환·서버 배포·기기 플래시·시리얼 로그 검증을 완료했다. 화면 육안 확인(한글·그림·터치)과 스피커 검증은 사용자 몫으로 남아 있다.**

### 이번 세션에서 한 일

1. **파티션 재구성** — 쓰지 않던 두 번째 OTA 슬롯(1.875MiB)을 회수해 `partitions_wordmon.csv`로 앱 2.25MiB + LittleFS **1.625MiB**(기존 128KiB)를 만들었다. NVS·앱 오프셋은 그대로라 기존 플래시 워크플로(앱 `0x10000`)와 호환된다. 최초 적용 시 `esptool erase_region 0x250000 0x1A0000`로 새 FS 영역을 지운 뒤 포맷 유도했다(이동 전 기기에는 학습 이력이 없었고, NVS 이전 이력도 비어 있었다 — `reviews-v1 NOT_FOUND` 확인).
2. **한글 폰트** — `scripts/make_kr_font.py`가 자료의 뜻에 쓰이는 **526개 음절 + ASCII**를 맑은 고딕 16px 4bpp로 추출해 `src/font_kr_16.c` 생성(npx lv_font_conv, Node 필요). `word_ui.cpp`의 뜻 라벨이 이 폰트로 한글을 표시한다. 새 한글 텍스트에 서브셋 밖 음절이 있으면 빈칸으로 보인다 — 스크립트 재실행·재빌드 필요.
3. **그림 PNG→WMR1 전환** — 실기기 로그에서 TLS 세션 후 힙 단편화로 PNG 디코더(46KiB 연속) 할당이 반복 실패하는 것을 확인했다(AUDIO.md의 오디오 48KiB 예산과도 충돌). 그래서 **PNG 디코딩을 기기에서 아예 제거**하고, 변환 시점에 카드색(0x0D1B2D)으로 합성한 RGB565 원시 파일(WMR1: 매직+가로·세로 uint16le+픽셀)을 배포한다. `Wordbook::loadArt`가 픽셀 버퍼로 바로 읽는다. 아트 픽셀 버퍼(21KB)는 Wi-Fi 전에 미리 할당한다. PNGdec는 링크에서 빠져 Flash 약 28KB 절감.
4. **콘텐츠 배포** — `scripts/build_illustrated_content.py` + `scripts/wmr.py`(공용 WMR 모듈, `wordbook_art.py`도 WMR 출력으로 갱신)로 893단어 변환: manifest 52,000바이트, 그림 총 약 14.3MB(평균 16KB). wordbook 저장소에 푸시(커밋 `678d9aa`), purge.jsdelivr.net 갱신 완료.
5. **기기 적용·검증** — COM6(CH340)으로 파티션 테이블(`0x8000`) + v0.5.0 앱(`0x10000`) + FS 영역 erase 후 부팅 로그 확인: `WordMon Studio v0.5.0`, LittleFS 포맷·마운트, **`manifest synced: 893 words`**, `card #0: pencil (art)`, WMR 헤더 검증 동작(구형 PNG 캐시를 정상 거부 후 새 그림으로 교체·이전 캐시 자동 삭제). 재부팅 후에도 893단어·그림·Wi-Fi 설정 유지 확인. 호스트 테스트 3종 통과, 두 패널 빌드(RAM 36.3%, ILI9341 Flash 78.5%)와 v0.5.0 BIN 4종 생성 완료.

### 현재 알려진 이슈

- **jsDelivr `@main` 분기 별칭 캐시는 푸시 후 수십 분~12시간 걸릴 수 있다.** 이번 세션에서도 약 40분 뒤 purge로 갱신됐고, 갱신 전에는 기기가 구 manifest를 계속 받았다. 배포 직후 기기에 새 내용이 안 보이면 `curl https://cdn.jsdelivr.net/gh/guesswhoisbackk/wordbook@main/words.jsonl | head -1`로 CDN 상태를 먼저 확인한다(커밋 고정 URL은 즉시 반영된다).
- 부팅 초반 `No core dump partition found` 에러는 coredump 파티션 제거로 인한 정상 로그다.
- manifest 다운로드가 가끔 일시 실패한다(관측 2회, 모두 다음 시도/재부팅으로 회복). jsDelivr 에지·TLS 일시 오류로 보인다.

### 남은 작업 (우선순위 순)

1. **사용자 실물 확인** — 화면에서 한글 뜻 표시, HINT 그림(카드색 합성), 카드 터치·NEXT·평가, SPEAK/STOP(스피커 물리 연결 상태 재확인). `docs/HARDWARE_TEST.md` v0.5.0 절 참고.
2. **예문(`e`)·외부 발음(`s`) 생성** — 현재 manifest는 뜻+그림만 있다. 예문은 893개 일괄 작성 필요, 발음은 `scripts/prepare_audio.ps1` 확장 후 wordbook에 WAV 배포(리포 용량 약 20MB 추가 예상, jsDelivr 50MB 한도 내).
3. 오디오 48KiB 버퍼도 TLS 후 단편화 힙에서 실패할 수 있다(미검증). 외부 WAV 배포 전 부팅 시 선할당 또는 스트리밍 재생 검토.
4. 단어 뜻·난이도·학습 순서 검수(현재는 원본 배치 순서).
5. 동기화 중 UI 블로킹, HTTPS 인증서 검증 생략은 종전대로 남아 있다.

### 다음 세션 시작 순서

1. CDN·기기는 2026-10-10 검증 완료(`@main`이 `w_pencil.wmr` 제공, 기기 `card #0: pencil (art)`). 시작 시 `git log -1`로 원격 상태만 확인한다.
2. 사용자와 함께 화면·터치·스피커 육안 검증(HARDWARE_TEST v0.5.0 절) — 한글 뜻 표시, HINT 그림, NEXT·평가, SPEAK/STOP.
3. 그 다음은 예문·발음 생성 작업으로 이어간다.

```powershell
Set-Location 'C:\antigravity\word monitor\word-monitor'
git status -sb
cmd /c scripts\test_host.cmd
$env:PATH = 'C:\antigravity\bambu-monitoring\.venv\Scripts;' + $env:PATH
.\scripts\build.ps1 -Environment all
# 기기 로그: .venv python scripts\read_log.py COM6 45
```

보드용 앱: `build/WordMonitor-v0.5.0-cyd-ili9341-firmware.bin`(앱 `0x10000`). merged는 `0x0`용. **파티션 테이블은 이미 기기에 적용됨** — 다른 보드에 처음 적용할 때만 `0x8000` 기록 + FS 영역 erase가 필요하다.

---

## 이전 인계 — 2026-10-10 아침 (v0.4.0 상태)

**프로젝트:** `C:\antigravity\word monitor\word-monitor`  
**원격:** https://github.com/guesswhoisbackk/word-monitor (`main`)  
**소스 버전:** v0.4.0  
**이번 인계 작성 전 최신 커밋:** `265a5e2` — 그림 단어 자료 반입. 앞선 `e76ae00`은 순차 학습 구현, `8c8d28f`는 발음 재생 구현이다.

사용자는 ESP32 영어 단어 학습기를 만들고 있으며, 학습에 좋은 방식을 추천해서 구현하도록 요청했다. 이후 다른 프로젝트의 그림 단어 자료를 여기서도 사용하고 싶다고 했고 자료 반입까지 완료했다. **현재 요청은 세션 인계 문서 작성이다. 이번 인계에서 추가 펌웨어 구현·콘텐츠 배포·기기 업로드를 하지 않는다.**

### 완료한 기능

- 기한이 가장 오래 지난 복습부터 고르고, 없으면 목록의 첫 미학습 단어를 선택한다. 날짜로 카드 번호를 정하지 않는다.
- 새 단어 기본 하루 5개, 웹 설정 0~20개. 첫 평가만 오늘 수에 포함하며 AGAIN도 포함한다. 재복습·조회·발음 듣기는 수를 늘리지 않는다. KST 자정에 초기화하고 쉬었던 날의 학습량은 누적하지 않는다.
- 뜻을 먼저 떠올리고 카드를 터치해 답·예문 확인 → AGAIN/GOT IT → NEXT로 진행한다. 그림은 HINT로 표시하며 힌트를 사용한 카드는 GOT IT 평가를 막는다.
- AGAIN은 10분 후, 성공 복습은 1/3/7/14/30일 후 예약한다. 초기 운영값이며 개인별 최적 간격 추정은 아니다.
- 최대 1,000개 학습 이력을 LittleFS `/study-v2.bin`에 저장한다. 이전 128개 NVS 이력을 이전하고, 재부팅 후 기록과 하루 수를 복원한다. 임시 파일 검증·원자 교체, 저장 실패 시 메모리 복원, 캐시 공간 회수, 손상된 파일시스템 자동 초기화 방지를 구현했다.
- SPEAK/STOP, GPIO26 DAC → CYD 내장 앰프, 음량 기본20/범위0~60. 기본 8개 영어 발음은 내장하고 외부 WAV는 manifest의 `s` 필드로 연결한다. 터치용 GPIO25는 오디오에 사용하지 않는다.

### 가져온 자료와 경로

오늘 확인: **원본 항목 921개·PNG 921개·원본 목록 10개, 그림 누락 0개**. 중복 영어 이름을 묶은 목록은 **893개**다. 980개로 표현하지 않는다.

| 프로젝트 내 경로 | 내용 |
|---|---|
| `content/illustrated-vocabulary/vocabulary.json` | 원본 항목 전체: ID, 영어 이름, 한글 뜻, 그림 제작 설명, 경로, 파일 크기, SHA-256 |
| `content/illustrated-vocabulary/unique_words.json` | 영어 이름 기준 893개 묶음. 다른 원본 그림은 `variants`에 보존 |
| `content/illustrated-vocabulary/images/` | 컬러 PNG 원본 921개, 약 125.8MiB |
| `content/illustrated-vocabulary/sources/` | 원본 목록 JSON 10개 |
| `content/illustrated-vocabulary/README.md` | 자료 구조·출처·기기 연결 제한 |

출처는 `C:\antigravity\kids_daily_mission_app`이다. 원본 목록은 그 프로젝트의 `docs\chatgpt_image_batch_001_words.json` ~ `010_words.json`, 그림은 `assets\reusable_coloring_sets\chatgpt_batch_001` ~ `010\color`다. 원본 프로젝트는 변경하지 않았다. 2026-10-03 반입 당시 모든 PNG의 SHA-256이 원본과 일치함을 검증했고 GitHub에 푸시했다. 오늘은 목록 개수와 파일 존재를 재확인했다.

### 아직 적용하지 않은 것

**자료는 저장소로 가져왔지만 ESP32용 변환·연결은 아직이다. v0.4.0도 이 세션에서 기기에 업로드하지 않았다.**

- PNG 원본은 기기 규격으로 축소·변환해야 한다. 현재 이미지 한도와 형식은 `include/app_config.hpp`, `src/wordbook.cpp`를 확인한다.
- 현재 Montserrat 폰트에 한글이 없다. 한글 뜻을 쓰려면 폰트 대응이 필요하며 영어 풀이를 사용하는 방법도 있다. `subject`는 그림 제작 설명이므로 학습용 정의·예문으로 그대로 쓰지 않는다.
- 단어·뜻·난이도·중복의 의미 검수, 예문·추가 발음 생성은 남아 있다. 현재 자료 순서는 원본 배치 순서이지 검수된 학습 순서가 아니다.
- LittleFS가 **128KiB**다. 1,000개 이력 지원과 대규모 콘텐츠 저장은 별개다. 긴 풀이·예문 목록, 다운로드 임시 파일, 그림·음성 캐시, 이력 교체 여유를 함께 계산해야 한다. 페이지별 목록 다운로드/SD 카드/파티션 조정 등을 비교한 뒤 정한다. 파티션 변경으로 기존 이력을 잃지 않도록 이전 방법도 필요하다.
- 공개 콘텐츠 저장소는 `guesswhoisbackk/wordbook`, 설정 주소는 `https://cdn.jsdelivr.net/gh/guesswhoisbackk/wordbook@main`이다. **마지막 확인(2026-10-03)**은 단어 8개·그림 참조 1개였다. 오늘 외부 콘텐츠나 기기 상태는 재검사하지 않았다.
- 마지막 보드 확인은 ILI9341 CYD, CH340 COM6, v0.3.0 부팅과 Wi-Fi/8단어 동기화였다. 현재 포트·설치 버전은 다시 확인해야 한다. 사용자는 8Ω·1W 2415 스피커와 1.25mm 2핀 케이블을 구매해 연결했다고 했다. 실제 소리·음량·재생 중 터치의 검증 결과는 확정하지 않는다.

### 다음 세션 시작 순서

1. 이 최신 요약 → `README.md` → `docs/STUDY-DESIGN.md` → 자료 폴더 README를 읽고 Git 상태를 확인한다. 아래 오래된 절의 버전·버튼명·연결 상태보다 이 요약을 우선한다.
2. 사용자가 자료 활용 작업을 이어가도록 요청하면, **자료 893개 묶음을 기기용 학습 콘텐츠로 변환하고 연결**하는 작업에서 시작한다. 우선 한글 표시 방식·그림 규격·전체 목록 용량을 확인한다. 자료 복사를 다시 하거나 980개 원본을 찾는 작업을 반복하지 않는다.
3. 전체 이력·원본 자료를 보존하면서 작은 검증용 묶음으로 실제 화면/음성/학습 기록을 확인한 뒤 전체 콘텐츠에 적용한다. 원본 PNG·JSON을 직접 덮어쓰지 말고 변환 산출물을 분리한다.
4. 실제 적용할 때 장치 포트를 재확인하고 **ILI9341 앱만 `0x10000`**에 기록하는 업데이트 방식을 사용한다. merged BIN은 `0x0`용이며 혼동하지 않는다. 기기 적용과 서버 배포를 한 것으로 추정하지 않는다.
5. `docs/HARDWARE_TEST.md`의 v0.4.0 항목으로 터치·발음·10분 복습·재부팅·자정·오프라인·동기화·저장 공간을 확인한다.

### 검증·빌드 환경

2026-10-03 v0.4.0 호스트 테스트, 두 패널 빌드, 코드 리뷰는 통과했다. 오늘 문서 작성 때문에 펌웨어를 다시 빌드하지 않았다. 실물 전원 차단·런타임 힙·새 버전 UI/오디오 검증은 남아 있다.

```powershell
Set-Location 'C:\antigravity\word monitor\word-monitor'
git status -sb
git log -3 --oneline
cmd /c scripts\test_host.cmd
$env:PATH = 'C:\antigravity\bambu-monitoring\.venv\Scripts;' + $env:PATH
.\scripts\build.ps1 -Environment all
```

오늘 확인한 로컬 산출물: `build/WordMonitor-v0.4.0-cyd-ili9341-firmware.bin`(앱), 같은 접두어의 `merged.bin` 및 ST7789 두 파일이 존재한다. `build/`와 `.pio/`는 Git 제외이므로 다른 PC에서는 다시 생성한다. `bambu-monitoring`의 venv 도구를 사용하지만 그 프로젝트의 코드·설정은 수정하지 않는다.

다음 세션에 붙여 넣을 시작 문장:

> `C:\antigravity\word monitor\word-monitor\docs\HANDOFF.md`의 2026-10-10 최신 요약을 읽고 이어서 작업해줘. 가져온 921쌍 원본과 중복을 묶은 893개 목록을 ESP32 학습기에 활용하려고 해. 원본을 보존하면서 기기용 그림·한글 뜻 또는 영어 풀이·예문·발음과 저장 용량을 확인하고, 변환·연결·검증을 진행해줘. 완료 여부는 소스·콘텐츠 서버·실기기를 각각 구분해서 알려줘.

---

아래는 이전 세션의 상세 기록이다.

## 후속 자료 반입 — 2026-10-03

사용자가 `kids_daily_mission_app` 자료를 여기서도 사용하도록 가져와 달라고 요청했다. `content/illustrated-vocabulary/`에 원본 목록 10개, 항목 921개와 컬러 PNG 921개를 복사했다. 모든 PNG는 원본과 SHA-256 일치 확인. 총 그림 131,898,968바이트(약 125.8MiB). 원본 프로젝트는 수정하지 않았다.

`vocabulary.json`에 모든 원본 항목·그림 경로·해시를 저장했다. `unique_words.json`은 같은 영어 이름을 묶은 893개 목록이며, 중복 그림은 variants에 남겨 두었다. 980개 자료로 표현하지 않는다. 원본 그림 크기·한글·저장 공간에 대한 기기 대응은 아직이며, 이번 자료 반입에서 펌웨어와 공개 wordbook 서버를 변경하거나 기기를 플래시하지 않았다. 다음 작업은 이 자료를 기기용 콘텐츠로 변환·검수하고 저장/호스팅 방식을 정하는 것이다.

## 최신 인계 — 2026-10-03, v0.4.0 순차 학습

사용자가 가장 공부하기 좋은 방식을 추천해 목표 모드로 구현하도록 요청했다. 날짜 나머지로 카드를 고르던 방식을 **기한 복습 우선 → 목록의 첫 미학습 단어**로 바꿨다. 기본 하루 새 단어 5개, 웹 설정 0~20개, 최초 평가(AGAIN 포함)만 하루 수에 포함한다. 조회·발음·재복습은 수를 늘리지 않는다. KST 자정마다 새로 계산하며 쉬었던 날의 단어를 건너뛰거나 학습량을 누적하지 않는다. 화면은 `NEW 2/5`, 평가 후 `NEXT`로 진행한다.

### 기록과 저장 공간

- `StudyStore`: 힙에 최대 1,000개 기록, LittleFS `/study-v2.bin`에 저장. 기존 NVS `wm-study/reviews-v1` 128개 기록을 복습 단계·시각 그대로 이전한다. 기존 NVS는 복구용으로 남지만 새 평가의 기준은 v2 파일이다.
- 저장은 임시 `/study-v2.tmp` 쓰기 → 크기·내용 체크섬 검증 → 원자 교체다. 실패하면 메모리의 평가와 하루 수를 되돌린다. 손상된 기록은 초기화하지 않고 저장을 중지한다.
- 마운트 실패 시 기존 비어 있지 않은 파일시스템을 포맷하지 않는다. 처음 설치의 완전히 지워진 파티션만 초기화한다.
- 공간 부족 시 `/voice.wav`, `/voice.key`, `/wb/a_*` 캐시를 제거한다. manifest와 학습 기록은 보존한다. 최대 두 스냅샷+메타데이터 약 40KiB를 제외한 크기로 신규 manifest를 제한한다. 128KiB FS에서 긴 풀이·예문 980개를 동기화하는 용량 문제는 해결한 것으로 간주하지 않는다.
- 전원 차단으로 남은 `/voice.tmp`, `/wb/words.tmp`는 부팅 시 다운로드 작업이 시작되기 전에 제거한다. 오프라인에서 임시 파일이 저장 공간을 계속 차지하는 경우도 회귀 테스트했다.
- 단어 철자의 64비트 키를 사용해 목록 재정렬에 영향을 받지 않는다. 동기화에서 삭제된 활성 카드는 새 목록 카드로 교체한다. 한글 폰트는 여전히 없다.

### 검증과 현재 보드

- `scripts/test_host.cmd`: 복습 우선순위, 순차 선택, 날짜/자정 경계, 하루 수와 재부팅, 1,000개 기록, 쓰기/이름 변경 실패, 희소한 구형 이력 이전, 공간 부족 시 캐시 정리/실패 복구, 기존 오디오 검사 통과.
- 두 프로필 빌드 및 firmware/merged BIN 생성 성공. ILI9341 정적 RAM 118,952바이트(36.3%), 앱 Flash 1,816,225바이트(92.4%). 로그 `build/build-v040-final.log`.
- 코드 리뷰에서 발견한 저장 공간·삭제된 활성 단어·희소 이력 이전·남은 다운로드 임시 파일 문제를 수정하고 후속 검토에서 중요한 지적이 없음을 확인했다. 실제 저장 장치 전원 차단, 런타임 힙, 물리 UI·음성은 호스트 테스트로 검증한 것이 아니다.
- 이번 v0.4.0 작업에서 기기에 업로드하지 않았다. 앞선 연결 확인에서 CH340 **COM6**, 부팅 **v0.3.0**, Wi-Fi 및 외부 단어 8개 동기화를 확인했다. Win32_SerialPort가 COM6을 빠뜨리므로 Get-PnpDevice/PlatformIO로도 포트를 확인한다. 화면에 `diligent`가 나오던 것은 예전 KST 누적 일수 `% 8` 선택 결과였다.
- 실제 보드용 `build/WordMonitor-v0.4.0-cyd-ili9341-firmware.bin`은 앱 `0x10000`용이다. merged 파일은 `0x0`용이며 서로 혼동하지 않는다. 파티션/NVS/FS 보존을 위해 앱 업데이트 방식으로 진행한다. `build/`는 Git 제외이므로 다른 PC에서는 다시 빌드한다.

### 콘텐츠 상태와 다음 작업

**980개 단어·그림이 현재 기기에 준비된 상태는 아니다.** 연결된 공개 wordbook에는 단어 8개·그림 참조 1개다. 로컬 `kids_daily_mission_app`에서 항목 921개와 대응 컬러 그림 921개를 확인했다. 다른 프로젝트의 911/3,224개 목록이나 48개 실험용 단어장과 혼동하지 않는다. 자세한 경로는 `CONTENT-AUDIT-2026-10-03.md`에 있다. 사용자에게 별도 980개 원본의 경로를 질문했으며 이번 작업에서는 원본 변환·외부 서버 배포를 진행하지 않았다.

다음 세션은 이 절 → `README.md` → `STUDY-DESIGN.md` → 콘텐츠 확인 기록을 읽고 Git 상태를 확인한다. 기기 적용으로 이어갈 때 COM 포트를 재확인하고 ILI9341 앱만 업로드한 뒤 `HARDWARE_TEST.md`의 v0.4.0 항목을 검사한다. 이후 원본 콘텐츠·난이도 순서·영어 풀이/한글 폰트·980개 캐시 용량을 확정한다.

```powershell
Set-Location 'C:\antigravity\word monitor\word-monitor'
cmd /c scripts\test_host.cmd
$env:PATH = 'C:\antigravity\bambu-monitoring\.venv\Scripts;' + $env:PATH
.\scripts\build.ps1 -Environment all
```

이하 절은 **과거 기록**이다. 최신 버전·연결·콘텐츠·다음 작업은 위 절을 우선한다.

## 과거 인계 — v0.3.0 발음 재생

사용자가 **2415 8Ω·1W 스피커 + 1.25mm 2핀 케이블** 주문 사진을 제공하고 구현을 요청했다. SPEAK/STOP 버튼, 웹 음량 0~60(기본20), GPIO26 내장 DAC 단독 출력, 기본 8단어 오프라인 영어 발음, `s` 필드의 외부 WAV와 최근 한 개 캐시를 추가했다. 상세 연결·파일 생성·메모리·실기기 검증 제한은 `docs/AUDIO.md`를 먼저 읽는다.

오디오 작업 중 LVGL/웹 포털은 계속 돌고 단어장 처리는 대기한다. REVIEW는 재생을 취소한 뒤 진행한다. PNG 디코더는 더 이상 영구 유지하지 않고 디코드 직후 해제한다. 추가 발음 샘플 폴더 `docs/audio-sample/`과 생성 스크립트 `scripts/prepare_audio.ps1`도 포함한다. 외부 단어장 서버에는 이번 샘플을 배포하지 않았다. 기존 8단어는 서버 변경 없이 내장 음성을 사용한다.

검증: `scripts/test_host.cmd`의 복습·저장·오디오 검사 모두 통과, `scripts/build.ps1 -Environment all`의 두 패널 빌드/통합 BIN 생성 통과. ILI9341 정적 RAM 120,992바이트(36.9%), 앱 Flash 1,812,893바이트(92.2%). 발음 생성 파일 전체 형식/비무음 검사와 별도 코드 검토도 완료했다. 최종 로그는 로컬 `build/audio-final-build.log`, 보드용 앱은 `build/WordMonitor-v0.3.0-cyd-ili9341-firmware.bin`(`0x10000`)이다. build 폴더는 Git에 포함하지 않으므로 다른 PC에서는 다시 생성한다.

**실기기 업로드·실제 소리·재생 중 터치는 아직 미검증**이다. 다음 작업은 스피커가 도착하면 SPEAK 단자에 연결, ILI9341 앱 펌웨어 적용, HARDWARE_TEST.md의 v0.3.0 항목 실행이다. 특히 GPIO25 터치 정상 유지, 그림이 있는 상태의 첫 오디오 다운로드 힙, STOP/REVIEW, 오프라인 캐시를 확인한다. 한글 폰트는 계속 미구현이다. 아래 v0.2.0의 '스피커 정보 미확인/발음 미구현'은 과거 상태이다.

## 최신 인계 — 2026-09-18, v0.2.0

학습 평가(AGAIN/GOT IT), 10분 재학습 및 1/3/7/14/30일 복습 예약, 128단어 NVS 이력, REVIEW 기한순 선택, HINT 그림, 답 영역 스크롤을 추가했다. 캐시 오프라인 선택, 자정 갱신, 동기화 재시도 간격, 다운로드 완전성/JSON 검증도 수정했다. 자세한 변경과 제한은 `docs/REVIEW-2026-09-18.md`와 README를 참조.

현재 새 버전은 **구현·두 패널 빌드·호스트 테스트 완료, 실기기 업로드 전**이다. 보드의 마지막 확인 버전은 v0.1.1이다. 사용자는 스피커를 주문했고 곧 도착한다고 했다. 제품명/링크/앰프/연결 방식은 질문했으나 아직 답을 받지 못했으며, 발음 재생과 한글 폰트는 미구현이다. 사용자가 이 인계 문서와 구현 변경의 커밋·푸시를 요청했다. 소스·테스트·문서를 한 커밋으로 인계하며 실제 원격 상태는 `git fetch origin` 후 확인한다.

호스트 회귀 테스트: `scripts\test_host.cmd` (Visual Studio C++ 도구 필요). 두 패널 빌드 및 BIN 생성은 기존 `scripts\build.ps1` 사용. 현재 PC에서는 `C:\antigravity\bambu-monitoring\.venv\Scripts`를 해당 PowerShell 프로세스의 PATH 앞에 넣으면 `pio`와 `python`을 찾는다.

### 다음 세션 시작 순서

1. 이 최신 인계 절 → `README.md` → `docs/REVIEW-2026-09-18.md`를 읽고 `git status --short`, `git log -3 --oneline`으로 작업 사본을 확인한다.
2. 스피커 제품 정보가 도착했으면 모델과 연결을 확인한다. 현재 보드는 ESP32-2432S028R, **ILI9341 확정**이다. 오디오 핀이나 출력 방식을 추측하여 활성화하지 않는다.
3. 보드 연결 후 v0.2.0을 적용하는 작업으로 이어가면 COM 포트를 다시 확인하고 ILI9341 앱만 `0x10000`에 기록한다. 현재 인계 세션에서는 업로드하지 않았다. `docs/HARDWARE_TEST.md`의 **v0.2.0 추가 검증** 절로 터치·스크롤·10분 복습·재부팅 이력·오프라인·동기화를 확인한다.
4. 스피커에 맞는 발음 버튼/재생 및 오디오 공급·캐시를 구현한다. 다음 우선순위는 한글 폰트, 실제 학습 콘텐츠, 128단어 이력 한도 확대다. 퀴즈·손글씨·S3 이전은 후순위 후보이며 이번에 구현하지 않았다.

### 검증과 산출물

- 정책/복습 우선순위 테스트 및 NVS 저장 대역 테스트 통과. 실제 NVS 전원 차단·UI·오디오 검증을 대신하지 않는다.
- `cyd-ili9341`, `cyd-st7789` 최종 빌드와 firmware/merged BIN 생성 모두 성공.
- ILI9341 정적 RAM: 120,872바이트(36.9%), 앱 Flash: 1,652,165바이트(84.0%). 런타임 힙 여유는 미측정.
- 별도 코드 검토 완료. 동기화 후 같은 날 뜻/그림이 갱신되지 않던 문제를 추가 수정하고 후속 검토 완료.
- 실제 보드용 로컬 파일: `build/WordMonitor-v0.2.0-cyd-ili9341-firmware.bin` (앱, `0x10000`), `build/WordMonitor-v0.2.0-cyd-ili9341-merged.bin` (통합, `0x0`). 두 파일을 혼동하지 않는다.
- `build/`, `.pio/`, 로그는 Git 제외 대상이다. 다른 PC에서 소스만 받으면 다시 빌드해야 한다. 최종 빌드 로그는 현재 PC의 `build/final-build.log`에 있다.

```powershell
Set-Location 'C:\antigravity\word monitor\word-monitor'
cmd /c scripts\test_host.cmd
$env:PATH = 'C:\antigravity\bambu-monitoring\.venv\Scripts;' + $env:PATH
.\scripts\build.ps1 -Environment all
```

### 구현 위치와 유지할 동작

- `include/study_policy.hpp`: 순수 C++ 날짜·복습 간격·복습 우선순위·동기화 간격. 일자는 KST 누적 일수이며 과거의 `tm_yday` 방식이 아니다.
- `src/study_store.cpp`: NVS `wm-study` / `reviews-v1`, 128개 기록. 단어 철자의 64비트 해시 키로 순서 변경에 영향받지 않는다. 같은 철자는 단어장 간 이력을 공유한다. 저장 실패 시 메모리 변경을 되돌리고, 가득 차면 기존 이력을 삭제하지 않는다.
- `src/word_ui.cpp`: 뜻 확인 전/기한 전 중복 평가 방지, 힌트 사용 시 GOT IT 금지, 답 영역 세로 스크롤, 단어 변경 시 답 숨김. 평가 뒤 REVIEW를 눌러 다음 카드를 선택한다.
- `src/wordbook.cpp`: 기한 지난 단어 우선, 없으면 오늘 슬롯의 미학습 단어. 성공한 동기화에서는 현재 단어를 철자로 찾아 최신 내용/그림을 다시 읽고, 삭제되면 오늘 슬롯으로 전환한다.
- 새 단어는 하루 한 슬롯이므로 접속하지 않은 날의 단어를 자동 보충하지 않는다. 전원 재인가 후 시간이 없으면 캐시 첫 단어는 보이지만 평가·기한 복습은 NTP 동기화가 필요하다.
- 한글 폰트 부재, 다운로드 중 UI 블로킹, HTTPS 인증서 검증 생략은 남아 있다. 기능 완료로 오해하지 않는다.

---

아래는 **2026-09-17의 과거 기록**이다. 버전, 날짜별 카드 번호, 다음 작업 순서가 최신 절과 다르면 위 내용을 우선한다.

## 프로젝트

- 로컬 작업 사본: `C:\antigravity\word monitor\word-monitor` (git 관리 중)
- 원격: https://github.com/guesswhoisbackk/word-monitor (공개, main)
- 단어장 콘텐츠 저장소: https://github.com/guesswhoisbackk/wordbook (공개) — 기기가 jsDelivr CDN으로 읽음. 내용 수정 후에는 purge.jsdelivr.net으로 CDN 캐시 갱신 필요
- 앱: WordMon Studio v0.1.1 (하드웨어 검증용 + 인터넷 단어장)
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

- 대상 보드: 사용자의 **ESP32-2432S028R(CYD)**, USB 시리얼 CH340 → **집 PC에서는 COM6** (이전 작업장에서는 COM4였음 — PC마다 다르니 장치 관리자로 확인)
- **패널 확정: ILI9341.** ST7789 프로필은 화면 전체가 하얗게 됨(사용자 확인). 이후 플래싱은 항상 `cyd-ili9341` 빌드 사용. ST7789 프로필은 다른 보드용으로만 유지.
- 터치·Wi-Fi·단어장 동기화는 **2026-09-17 저녁 실물 검증 완료** (아래 세션 기록 참조).

### 플래싱/로그 명령 (2026-09-17 저녁 실제로 쓴 것)

```powershell
# 빌드 — 이 프로젝트 로컬 .venv는 없고 bambu-monitoring의 venv를 사용
C:\antigravity\bambu-monitoring\.venv\Scripts\pio.exe run --project-dir "C:\antigravity\word monitor\word-monitor" -e cyd-ili9341

# 플래시 — 앱만 0x10000에 기록(부트로더·파티션·NVS·LittleFS 캐시 보존)
C:\antigravity\bambu-monitoring\.venv\Scripts\python.exe "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32 --port COM6 --baud 460800 write_flash 0x10000 "C:\antigravity\word monitor\word-monitor\.pio\build\cyd-ili9341\firmware.bin"

# 부팅 로그 보기 (리셋 후 30초). noreset를 4번째 인자로 주면 리셋 없이 관찰
C:\antigravity\bambu-monitoring\.venv\Scripts\python.exe "C:\antigravity\word monitor\word-monitor\scripts\read_log.py" COM6 30
```

주의: 이 보드는 **시리얼 포트를 여는 것만으로 리셋**(RTS 글리치)된다. 조용히 관찰하려면 noreset 모드.

## 2026-09-17 저녁 세션 — 첫 실기기 단어장 검증 완료 (v0.1.1)

집 PC에서 사용자 설정 완료 후 **전체 파이프라인 검증 성공**: Wi-Fi 연결 → jsDelivr manifest 동기화(8단어) → 카드 선택 → apple 그림 PNG 다운로드·캐시·RGB565 디코드까지 로그 확인(`[wb] card #3: apple (art)`).

- 집 PC에서는 보드가 **COM6**으로 잡힌다(COM4는 이전 위치 기준).
- 사용자가 포털에 단어장 URL을 `guesswhoisbakk`로 오타 입력해 동기화 실패했었음 → 포털에서 재저장으로 해결. STA 모드에서도 `http://<IP>/` 포턠로 설정 수정 가능(빈 Wi-Fi 비밀번호 제출 시 기존 값 유지).
- v0.1.0의 버그 3개를 수정해 v0.1.1로 플래시:
  1. **첫 동기화가 부팅 10분 후에야 시도됨** — `syncDue()`가 `lastAttemptMs_`(0 초기화) 기준으로 대기. `everAttempted_` 플래그 추가로 캐시 없는 첫 부팅에 즉시 시도.
  2. **`/wb` 디렉터리를 만드는 코드가 없어 다운로드 저장 실패** — `begin()`에서 `LittleFS.mkdir(kWordbookDir)` 추가.
  3. **PNGdec 드로잉 콜백이 `return 0`이라 첫 줄만 그리고 `PNG_QUIT_EARLY`로 중단** — PNGdec 규약상 계속 그리려면 0이 아닌 값 반환. `pngLineDraw`가 `return 1`로 수정. `decodeArt`에 단계별 실패 로그도 추가.
- 단어 카드 index는 **0-based `tm_yday`** 기준(`yday % 단어수`). 오전 문서의 "apple index 4 (yday 260 % 8)" 계산은 1-based day를 써서 한 칸 어긋났었음 — wordbook 저장소에서 apple을 index 3으로 이동(커밋 0b4e2e4, jsDelivr purge 완료). 2026-09-17 yday=259 → 259%8=3 → apple. 다음 날(yday 260)은 index 4 = vivid부터 순환.
- **터치(카드 뒤집기) 포함 사용자가 실물 확인 완료** — 화면 apple 카드+그림, 터치 동작 모두 확인(2026-09-17).
- 당시 기기 최종 상태: **v0.1.1 구동 중**, 가정 Wi-Fi 연결, 단어장 8단어 동기화 + apple 그림 캐시됨. 설정값은 NVS에 저장되어 재부팅해도 유지됨.

### 다음 세션 작업

1. **HARDWARE_TEST.md 7장(안정성)** — 남은 유일한 미검증 장. 12시간 주기 재동기화, 인터넷 끊김 시 `WB CACHED` 동작, 아트 캐시 evict(내일 vivid로 바뀔 때 apple 그림 삭제 확인)
2. 이후 로드맵 순서: UI 확장(복습 카드·퀴즈·손글씨 캔버스, 아래 3번) → 한글 폰트(4번)

## 2026-09-17 세션 결과 — 집에 가서 할 일 (완료됨, 위 저녁 세션 기록 참조)

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

## 현재 구현 상태 (v0.1.1)

- [x] 두 프로필 컴파일 통과 (2026-09-17, RAM 36.2% / Flash 83.7%. v0.0.2 대비 Flash +11.8%: PNGdec+zlib, HTTPClient/TLS, wordbook 모듈)
- [x] 플래싱용 통합 BIN 생성 완료 — `build\WordMonitor-v0.1.0-cyd-{st7789,ili9341}-{merged,firmware}.bin`
- [x] 화면+터치+LVGL 초기화 (원본에서 검증된 코드 경로 그대로)
- [x] Wi-Fi STA 연결, 15초 실패 시 `WordMon-XXXX` AP(비밀번호 `wordmon1`) + DNS 포털
- [x] 웹 설정 페이지: Wi-Fi 검색/저장, 화면 밝기, **단어장 기본 URL** → NVS 저장 후 재부팅
- [x] 플래시카드 카드: 앞면(단어 + 일러스트, 없으면 첫 글자 + TAP TO REVEAL) ↔ 터치로 뒷면(뜻/예문)
- [x] **인터넷 단어장 (방식 B, 사용자 선택)**: `src/wordbook.cpp`가 `{URL}/words.jsonl`(JSON Lines, 한 줄 = {"w","m","e","a"})을 내려받아 LittleFS 캐시. 카드 선택은 `yday % 단어수`. 그림은 `{URL}/{a}`를 내려받아 PNGdec로 RGB565 디코드(카드색 배경 합성) 후 표시. 부팅 시 + 12시간 주기 동기화, 실패 시 10분 재시도, 오프라인이면 캐시로 운영(`WB CACHED`), 캐시도 없으면 내장 샘플 8개. 아트 캐시는 **현재 카드 그림 1개만 유지**(evictOtherArt) — 128KB LittleFS가 며칠 만에 찰 수 있어서.
- [x] 내장 에셋 파이프라인: `scripts/svg_to_header.py` (SVG→ARGB8888 헤더, `include/assets/`), `scripts/wordbook_art.py` (SVG→PNG + `words.jsonl` 등록, `docs/wordbook-sample/`). SVG 래스터라이저는 Windows DLL 문제로 cairosvg/svglib 대신 Edge 헤드리스 사용.
- [x] **실물 보드 검증 1~4장 + 6장(단어장)** — 2026-09-17 완료. 7장(안정성)만 남음
- [ ] 진짜 단어장 콘텐츠 (아래 로드맵)

### 메모리 예산 (CYD 4MB, 실측)

- 정적 RAM 118.7KB(예산 ~125KB라 여유 없음) → **정적 대형 버퍼 금지**. PNGdec `PNG` 객체 하나가 45.7KB(32KB zlib 윈도우 포함)라 부팅 후 첫 디코드 때 힙에 1회 할당(`new`, 해제 없음), 아트 픽셀도 최대치(120×88×2=21KB)로 1회 할당 후 매일 재사용 — 일일 할당/해제가 없어 파편화 걱정 없음.
- 힙 여유 ~180KB. 동시 최대 소비: PNG 45.7 + 아트 21 + TLS 핸드셰이크 ~40 + WiFi 런타임 ~50 → 실측 필요(하드웨어 검증 항목).
- Flash 앱 83.7% (321KB 여유). `huge_app.csv` 전환 시 3MB로 확장 가능하나 OTA 슬롯 상실.
- 단어장 용량 상한: manifest ~100B/단어 + 그림 1개 → 128KB FS에서 manifest만 약 1,000단어. 1,000을 넘기면 파티션 재배치 필요.

## 개발 환경

빌드는 `bambu-monitoring`의 PlatformIO venv로 한다(이 프로젝트에 로컬 venv 없음):

```powershell
C:\antigravity\bambu-monitoring\.venv\Scripts\pio.exe run --project-dir "C:\antigravity\word monitor\word-monitor" -e cyd-ili9341
```

- 산출물: `.pio\build\cyd-ili9341\firmware.bin` → esptool로 `0x10000`에 기록(위 플래시 명령). 두 프로필 통합 BIN이 필요하면 `scripts\build.ps1`(웹 플래셔용, 주소 `0x0`)
- 시리얼: 115200 baud, `scripts\read_log.py`로 리셋+캡처. 부팅 로그에 패널 이름·AP 정보·IP가 나온다.

## 단어장 로드맵 (다음 세션 작업 순서)

1. ~~**실기기 패널·터치 확인**~~ — **완료(2026-09-17)**
2. ~~**인터넷 단어장 실증**~~ — **완료(2026-09-17)**. wordbook 저장소(8단어+apple.png) 구축됨. 이후 개선 후보: 내려받기 중 UI 멈춤 방지(현재 부팅/자정 동기화는 수 초 블로킹), 인증서 검증 활성화(현재 `setInsecure`, 읽기 전용 공개 콘텐츠라 위험은 낮음), 그림 사전 내려받기(내일 그림 미리 받아 자정 즉시 전환).
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
- 커밋은 원격(main)에 즉시 푸시해 다음 세션이 로컬 상태와 무관하게 이어받을 수 있게 한다.

## 문제 발생 시 수집할 정보

- 보드 앞·뒷면 사진과 모델 표기
- 사용한 BIN 파일명
- 화면 사진 또는 짧은 영상
- 115200 baud 시리얼 로그
- 화면 하단에 표시된 IP 또는 AP 이름
