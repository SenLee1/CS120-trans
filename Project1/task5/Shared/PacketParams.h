#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace Params {
const double sampleRate = 48000.0;

// --- OFDM 参数配置 ---
const int numCarriers = 2;    // 2个子载波并行传输
const int ofdmBaudRate = 500; // 每个载波仅 125 Baud (非常慢，抗干扰)
const int freqSpacing = 500;  // 间隔必须等于波特率以保持正交性
const double baseFreq = 4000.0; // 起始频率

// 计算符号长度
const int symbolLen = (int)(sampleRate / ofdmBaudRate); // 96 samples
const int cpLen = (int)(sampleRate * 0.002); // 5ms 循环前缀 (CP) 吸收回声
const int totalSymbolLen = symbolLen + cpLen;

// 获取 2 个频率
static std::array<double, numCarriers> getCarriers() {
  std::array<double, numCarriers> freqs;
  for (int i = 0; i < numCarriers; ++i) {
    freqs[i] = baseFreq + i * freqSpacing;
  }
  return freqs;
}

// 包大小：因为每次传 8 bits，我们设为 800 bits (100个 OFDM 符号)
const int CHUNK_SIZE = 500;
const int TOTAL_BITS = 10000;

// --- Hamming(7,4) ---
static uint8_t encodeHamming(uint8_t nibble) {
  static const uint8_t table[16] = {0x00, 0x0B, 0x16, 0x1D, 0x2C, 0x27,
                                    0x3A, 0x31, 0x4B, 0x40, 0x5D, 0x56,
                                    0x67, 0x6C, 0x71, 0x7A};
  return table[nibble & 0x0F];
}

static uint8_t decodeHamming(uint8_t byte) {
  int minErr = 100;
  uint8_t bestVal = 0;
  static const uint8_t codes[16] = {0x00, 0x0B, 0x16, 0x1D, 0x2C, 0x27,
                                    0x3A, 0x31, 0x4B, 0x40, 0x5D, 0x56,
                                    0x67, 0x6C, 0x71, 0x7A};
  for (int i = 0; i < 16; ++i) {
    uint8_t x = codes[i] ^ (byte & 0x7F);
    int errs = 0;
    for (int b = 0; b < 7; ++b)
      if ((x >> b) & 1)
        errs++;
    if (errs < minErr) {
      minErr = errs;
      bestVal = i;
    }
  }
  return bestVal;
}

// --- Preamble (0.1s) ---
static std::vector<float> getPreamble() {
  std::vector<float> p;
  int len = (int)(0.1 * sampleRate);
  for (int i = 0; i < len; ++i) {
    float t = (float)i / sampleRate;
    float freq = 1000.0f + (5000.0f * t / 0.1f);
    float val = std::sin(2.0f * juce::MathConstants<float>::pi * freq * t);
    float win =
        0.5f *
        (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / len));
    p.push_back(val * 0.5f * win);
  }
  return p;
}
} // namespace Params
