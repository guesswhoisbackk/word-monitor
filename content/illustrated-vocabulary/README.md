# 그림 단어 자료

2026-10-03 사용자의 요청으로 `kids_daily_mission_app`의 자료를 이 프로젝트에 복사했다. 원본 프로젝트는 수정하지 않았다.

| 파일/폴더 | 내용 |
|---|---|
| `vocabulary.json` | 원본 921개 항목: ID, 영어 이름, 한글 뜻, 그림 설명, 배치, 그림 경로, 파일 크기, SHA-256 |
| `unique_words.json` | 영어 이름의 대소문자·공백을 정리해 묶은 893개 항목. 같은 이름의 다른 원본은 `variants`에 보존 |
| `images/` | 컬러 PNG 원본 921개, 총 131,898,968바이트(약 125.8MiB) |
| `sources/` | 가져온 원본 목록 JSON 10개 |

원본 목록: `C:\antigravity\kids_daily_mission_app\docs\chatgpt_image_batch_001_words.json` ~ `010_words.json`.

원본 그림: 같은 프로젝트의 `assets\reusable_coloring_sets\chatgpt_batch_001` ~ `010\color`.

가져온 모든 그림의 SHA-256을 원본과 비교해 일치함을 확인했다. 항목 ID는 921개 모두 고유하며 그림 누락은 없다. 같은 영어 이름의 항목 28쌍은 원본을 지우지 않고 별도 목록에서 묶었다. 이 묶음은 뜻·난이도·그림 품질을 검수한 결과는 아니다.

## ESP32 연결 상태

**2026-10-10 기기용 변환·배포·적용 완료.** 893개 묶음 목록을 `scripts\build_illustrated_content.py`로 변환해 공개 단어장(guesswhoisbackk/wordbook, 커밋 `678d9aa`)에 배포했다. 그림은 원본 PNG를 카드 배경색으로 합성한 WMR1(RGB565)로 변환하며 최대 120×88, 평균 약 16KB다. 뜻은 한글 그대로 사용하고 펌웨어에 한글 폰트 서브셋(`scripts\make_kr_font.py`, 맑은 고딕 16px 526음절+ASCII)을 추가했다. 변환 산출물은 `build\illustrated-device\`에 별도로 생성하며 이 폴더의 원본은 수정하지 않는다.

아직 남은 것: 예문(`e`)·외부 발음(`s`) 생성, 단어 뜻·난이도·학습 순서 검수(현재 순서는 원본 배치 순서), 실물 화면 육안 확인. `subject`는 이미지 제작 설명이므로 학습용 정의·예문으로 쓰지 않는다.
