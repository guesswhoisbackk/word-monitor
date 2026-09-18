#include <cassert>
#include <cstdio>
#include <vector>
#include "audio_policy.hpp"
#include "assets/speech_audio.hpp"
using namespace wordmon;
std::vector<uint8_t> wav() {
  return {'R','I','F','F',40,0,0,0,'W','A','V','E',
          'f','m','t',' ',16,0,0,0,1,0,1,0,0x11,0x2b,0,0,
          0x11,0x2b,0,0,1,0,8,0,'d','a','t','a',4,0,0,0,0,128,255,128};
}
int main() {
  WavInfo info{};
  auto data = wav();
  assert(parseWav(data.data(), data.size(), info));
  assert(info.offset == 44 && info.bytes == 4 && info.rate == 11025 && info.bits == 8);
  for (size_t size = 0; size < data.size(); ++size) assert(!parseWav(data.data(), size, info));
  data[22] = 2; assert(!parseWav(data.data(), data.size(), info));
  data = wav(); data[20] = 3; assert(!parseWav(data.data(), data.size(), info));
  data = wav(); data[40] = 255; assert(!parseWav(data.data(), data.size(), info));
  data = wav(); data[32] = 2; data[34] = 16; data[28] = 0x22; data[29] = 0x56;
  assert(parseWav(data.data(), data.size(), info));
  data[40] = 3; assert(!parseWav(data.data(), data.size(), info));
  assert(dacSample(0, 8, 100) == 0);
  assert(dacSample(128, 8, 100) == 32768);
  assert(dacSample(255, 8, 100) == 65280);
  assert(dacSample(0x8000, 16, 100) == 0);
  assert(dacSample(0, 16, 100) == 32768);
  assert(dacSample(0x7fff, 16, 100) == 65280);
  assert(dacSample(0, 8, 0) == 32768);
  assert(validAudioName("apple.wav"));
  assert(!validAudioName("../apple.wav"));
  assert(!validAudioName("https://other/a.wav"));
  assert(!validAudioName("audio/apple.wav"));
  assert(!validAudioName("a.mp3"));
  for (const auto& clip : speech::clips) {
    assert(parseWav(clip.data, clip.size, info));
    assert(info.bits == 8 && info.bytes > info.rate / 2);
    uint8_t lowest = 255, highest = 0;
    for (size_t i = 0; i < info.bytes; ++i) {
      const uint8_t sample = clip.data[info.offset + i];
      if (sample < lowest) lowest = sample;
      if (sample > highest) highest = sample;
    }
    assert(highest - lowest > 32); // Detect empty/silent generated assets.
  }
  assert(speech::find("apple") && !speech::find("not-in-pack"));
  puts("audio parser and DAC conversion: all tests passed");
}
