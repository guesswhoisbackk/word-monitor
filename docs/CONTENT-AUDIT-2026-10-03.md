# 콘텐츠 확인 — 2026-10-03

980개 단어·그림이 이 ESP32에 연결된 상태는 확인되지 않았다.

| 위치 | 확인 결과 |
|---|---|
| `guesswhoisbackk/wordbook` main의 `words.jsonl` | 단어 8개, 그림 참조 1개, 외부 발음 참조 0개 |
| `C:\antigravity\wordbook` | 현재 PC에 폴더 없음 |
| `C:\antigravity\kids_daily_mission_app\docs\chatgpt_image_batch_001_words.json` ~ `010_words.json` | 항목 921개, 고유 ID 921개 |
| 위 프로젝트의 `assets\reusable_coloring_sets\chatgpt_batch_001` ~ `010\color` | 모든 921개 항목에 대응하는 컬러 PNG 존재, 누락 0개 |
| `C:\antigravity\point_point\experiments\word_window_app\assets\vocabulary.json` | 48개 항목 |
| `C:\antigravity\point_point\data\words_seed.json` | 911개 항목 |
| `C:\antigravity\point_point\data\all_words.js` | 3,224개 항목, image/imageUrl 참조 없음 |

921개 자료에는 한글 뜻·영어 이름·그림 생성 설명이 있다. 여러 사물의 복합 명칭도 포함되어 있어 전체를 기초 영단어 순서로 그대로 사용하는 것은 추천하지 않는다. 이번 작업에서 다른 프로젝트를 수정하거나 이 자료를 외부 서버에 배포하지 않았다.

실제 콘텐츠 연결 전 필요한 작업:

1. 사용할 원본 목록/그림 경로 확정 및 중복·철자·난이도 검수.
2. 한글 폰트를 추가하거나 화면용 영어 풀이·예문 작성. 현재 Montserrat에는 한글이 없다.
3. 그림을 기기 규격의 PNG로 변환하고 `w/m/e/a` JSONL과 일치시킴. 추가 발음은 `s`와 WAV로 연결.
4. 128KiB 파일시스템의 실제 사용량 확인. 1,000개 이력 저장은 약 16KiB, 안전한 임시 교체에는 추가 공간이 필요하다. 긴 풀이·예문 980개를 저장하는 문제는 별도로 페이지 분할/SD 카드/파티션 설계가 필요하다.

파일 개수 확인은 이미지의 의미·품질을 검수한 결과가 아니다. 본 기록은 현재 발견한 로컬 자료와 연결된 공개 저장소 상태를 구분한다.
