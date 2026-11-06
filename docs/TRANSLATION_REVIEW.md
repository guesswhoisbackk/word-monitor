# 예문 해석 검토 — 2026-11-06

`content/illustrated-vocabulary/examples.json`의 영어 예문 893개와 `examples_ko.json`의 한글 해석을 모델이 전체 대조하여 402개를 교정했다. 전문 번역가의 감수 완료를 의미하지 않는다. 영어 원문과 단어 키는 변경하지 않았다.

주요 교정은 사물 이름과 문맥이다. globe는 지구본, magnifying glass 예문의 bug는 벌레, whisk는 거품기로 고쳤다. 야자수의 palm을 손바닥으로 옮긴 문장, 커튼을 치는 draw, 컵 자국을 뜻하는 rings, 빨랫감을 뜻하는 darks, 손톱과 혼동된 hand saw도 바로잡았다. 번역투·부자연스러운 조사를 함께 다듬었다. 정확한 기존 해석의 말투는 모두 통일하지 않았다.

영어 자체가 모호하거나 어색한 일부 예문(marble, cereal bowl, ear 등)은 향후 원본 단어장 편집 대상으로 남긴다. 영어 원본을 수정하면 한글 해석도 함께 갱신해야 한다.

## 원본과 재생성

- 편집 원본: `content/illustrated-vocabulary/examples_ko.json`
- 생성물: `include/sentence_translations.hpp`, Paperlogy 한글 폰트 두 크기
- `scripts/build_sentence_translations.py`는 이제 특정 5개 단어의 번역을 덮어쓰지 않는다. 검토한 JSON을 그대로 사용한다.
- 생성 시 893개 영어 원문 대응, 한글 존재, 지원 글자, 문장 해시 중복을 확인했다. 글꼴은 실제 사용되는 한글 846개 음절을 포함한다.
- `tests/sentence_generator_test.py`는 검토본 보존, 재생성 일관성, 영어 변경 시 생성 중단, 불필요한 번역 API 호출 방지를 검사한다.
- 로컬 전체 대조 증거: `build/translation-audit-1106.json`. `build/`는 Git 제외다.

```powershell
$env:PYTHONUTF8='1'
.venv/Scripts/python.exe tests/sentence_generator_test.py
.venv/Scripts/python.exe scripts/build_sentence_translations.py
.venv/Scripts/python.exe scripts/make_kr_font.py
```

## 추가 내용 검토와 퀴즈 준비 — v0.11.0

단어 뜻 893개와 예문을 재검토해 뜻 17개, 영어/한글 예문 25쌍을 교정했다. `moon`은 달, `rolling pin`은 밀방망이, `mop`은 대걸레, `crayon box`는 크레용 상자, `soup`은 수프로 바로잡았다. 수식어가 누락된 뜻과 그릇 자체를 먹는 식의 문장도 정리했다. 이전 절에서 남긴 marble, cereal bowl, ear의 원문도 이번에 수정했다.

변경 목록은 `content/illustrated-vocabulary/corrections.json`에 이전/새 표현으로 기록했다. `scripts/build_quiz_content.py`는 수정한 893개 단어로 퀴즈 자료와 호환 교정 헤더를 만든다. 기기의 기존 원격 단어장도 단어 키와 이전 표현이 정확히 일치할 때 교정한 뜻/예문으로 표시한다. 사용자 정의 표현은 유지한다. 원격 저장소 자체는 변경하지 않았다. 원문을 교체한 문장은 교정된 한글 해석을 사용한다.

`vocabulary.json`과 이미지/변형 자료는 역사적 원본으로 유지한다. 실행·생성용 편집 원본은 `unique_words.json`, `examples.json`, `examples_ko.json`이다.
