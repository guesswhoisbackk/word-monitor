# v0.3.0 발음 재생

## 하드웨어와 사용

사용자가 제공한 주문 사진: 2415(24×15mm) 8Ω·1W 스피커 5개, JST 표기 1.25mm 2핀 15cm 케이블 10쌍. 표준 ESP32-2432S028R CYD의 SPEAK 단자/내장 앰프를 사용하는 구성이다. 커넥터 하우징 맞물림과 실제 보드 배선은 실물에서 확인해야 한다.

스피커 두 선을 케이블 두 선에 연결·절연하고 **SPEAK 단자**에 꽂는다. 스피커 선을 GPIO26이나 보드 GND에 직접 연결하지 않는다. 전원을 끈 상태에서 배선하며, 처음에는 기본 음량 20으로 시험한다. 별도 I2S 앰프는 이 구현에 필요하지 않다.

화면 왼쪽 위 SPEAK → 단어 듣기 → 직접 따라 말하기. STOP 또는 REVIEW로 중지할 수 있다. 재생은 뜻 공개/복습 평가 상태를 바꾸지 않는다. HTTP 연결·TLS 협상 중 취소는 현재 네트워크 호출이 반환된 뒤 처리될 수 있으나 UI 루프는 계속 실행한다. 연결/읽기/TLS 협상 각각 4초 제한, 리다이렉트 최대 2회, 본문 수신 10초 제한을 둔다.

웹 설정에서 발음 음량 0~60을 저장한다. 0은 음소거, 기본 20이다. 이는 소프트웨어 진폭 비율이며 실제 음압이나 와트 단위 출력 보장은 아니다.

## 발음 파일 공급

기본 단어 resilient, diligent, curious, vivid, apple, frugal, serene, profound는 Microsoft Zira Desktop 영어 음성으로 생성하여 펌웨어에 내장했다. 정확히 같은 철자이고 `s`가 없으면 인터넷 없이 재생한다. 발음 없는 새 단어를 장치에서 즉석 TTS 합성하지는 않는다.

추가 단어는 다음처럼 같은 단어장 폴더의 파일명을 지정한다.

```json
{"w":"apple","m":"a round red fruit","e":"She ate an apple at lunch.","a":"apple.png","s":"apple-v1.wav"}
```

- 파일: RIFF/WAVE, 비압축 PCM, 모노, 8-bit unsigned 또는 16-bit signed little endian.
- 샘플링: 8,000~24,000Hz, 길이 최대 4초, 전체 파일 최대 48KiB.
- `s`는 48자 이하의 영문/숫자/밑줄/하이픈/점 파일명이며 소문자 `.wav` 확장자. 하위 폴더, `..`, 별도 URL은 허용하지 않는다.
- 기존 선택한 파일이 있으면 내장 음성보다 `s`를 우선한다. 외부 파일을 받을 수 없고 해당 파일의 캐시도 없으면 오류를 표시한다.
- 캐시는 최근 외부 파일 **한 개**. URL과 파일명을 함께 저장해 다른 단어의 음성을 잘못 재생하지 않는다. 음성을 수정했으면 `apple-v2.wav`처럼 이름도 바꿔 캐시를 갱신한다.
- 저장 공간 부족이면 발음은 RAM에서 재생하지만 캐시가 저장되지 않을 수 있다. LittleFS의 단어장/그림과 용량을 공유한다. 기존 파티션을 변경하거나 포맷하지 않는다.

Windows 영어 음성이 설치된 PC에서 생성:

```powershell
# 기본 예제 8단어 WAV + words.jsonl + 기존 그림을 docs/audio-sample에 출력
.\scripts\prepare_audio.ps1

# 원하는 단어장: 원본은 유지하며 별도 폴더로 내보내기
.\scripts\prepare_audio.ps1 -Manifest C:\path\words.jsonl -OutDir C:\path\wordbook-with-audio

# 기본 내장 발음 헤더를 다시 만들 때만 사용
.\scripts\prepare_audio.ps1 -EmbedHeader include/assets/speech_audio.hpp
```

출력 폴더 내용을 자신의 단어장 호스팅 위치에 배포하면 된다. 생성기는 게시·푸시하지 않는다. 기본 음성은 `Microsoft Zira Desktop`, `-Voice`로 설치된 다른 영어 음성을 지정할 수 있다. 4초를 넘는 문장은 거부되므로 현재는 단어 발음용이다. WAV/생성 헤더가 저장소에 포함되므로 일반 펌웨어 빌드에는 Windows 음성 엔진이 필요 없다.

## 구현 및 검증 한계

`audio_player.cpp`는 FreeRTOS 작업에서 다운로드/검증/캐시/재생을 수행한다. 작업이 끝나기 전에는 다음 작업의 요청 데이터를 바꾸지 않는다. LVGL은 Arduino 루프에서만 접근한다. 이 동안 단어장 동기화·카드 선택을 잠시 미뤄 파일/메모리 사용 충돌을 피한다.

I2S0은 16비트 스테레오 프레임에 모노 샘플을 복제해 쓰고, **I2S_DAC_CHANNEL_LEFT_EN(GPIO26)만 활성화**한다. PCM을 DAC의 상위 8비트 unsigned 형식으로 변환한다. `i2s_set_pin(..., nullptr)`은 양쪽 DAC를 켜므로 사용하지 않는다. 끝에 midpoint silence를 채워 마지막 소리가 잘리지 않도록 하고, 정지 뒤 DAC2를 128에 유지한다. 단어 양끝 10ms에 음량을 줄인다.

오디오는 파일 크기만큼 버퍼를 할당한다(HTTP 길이를 모르면 최대 48KiB). 기본 발음은 flash에서 읽어 전체 파일 RAM 복사 없이 재생한다. PNG 디코더 약 46KiB는 그림 디코딩 종료 시 해제하고 표시 중인 픽셀만 유지한다. `[audio] start/end` 로그에 힙·최대 연속 블록·작업 스택 여유를 남긴다.

호스트 테스트는 WAV 경계/잘린 파일/스테레오 및 비PCM 거부, 8/16비트 변환, 음소거, 파일명 제한과 내장 8개 파일의 유효성·비무음 여부를 검사한다. 실제 발음 품질·I2S 신호·앰프 출력·네트워크 실패·FS 캐시·UI 응답성은 보드에서 확인해야 한다.

근거: [Sunton 보드 정의의 스피커 설명](https://github.com/rzeldent/platformio-espressif32-sunton#controlling-the-speaker), [Espressif I2S 내장 DAC 문서](https://docs.espressif.com/projects/esp-idf/en/v4.3.4/esp32/api-reference/peripherals/i2s.html).
