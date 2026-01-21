#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <string>
#include <vector>

namespace Params {
const double sampleRate = 48000.0;
const double carrierFreq = 6000.0; // 6kHz 载波
// 【关键修改】降低波特率以增加每个bit的能量，补偿移除ECC后的稳定性
const int baudRate = 1000; 
const int samplesPerSymbol = (int)(sampleRate / baudRate); // 48 samples

// 包配置
const int CHUNK_SIZE = 500; // 每个包传输 500 bits
const int TOTAL_BITS = 10000;

// --- Preamble (0.1s) ---
// 保持不变，用于同步
static std::vector<float> getPreamble() {
  std::vector<float> p;
  int len = (int)(0.1 * sampleRate);
  for (int i = 0; i < len; ++i) {
    float t = (float)i / sampleRate;
    // Chirp 信号: 1kHz -> 6kHz
    float freq = 1000.0f + (5000.0f * t / 0.1f);
    float val = std::sin(2.0f * juce::MathConstants<float>::pi * freq * t);
    // Hanning Window 减少旁瓣
    float win = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / len));
    p.push_back(val * 0.5f * win);
  }
  return p;
}
} // namespace Params
