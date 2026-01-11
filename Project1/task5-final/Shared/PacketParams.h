// Shared/PacketParams.h
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <complex>
#include <vector>

namespace Params {
const double sampleRate = 48000.0;

// --- OFDM Parameters ---
// 频域参数
const int fftOrder = 9;                       // 2^9 = 512
const int fftSize = 1 << fftOrder;            // 512 points
const int cpSize = 128;                       // Cyclic Prefix 长度
const int symbolTotalSize = fftSize + cpSize; // 一个完整 OFDM 符号的时域长度

// 子载波分配 (Bin Index)
// 频率分辨率 = 48000 / 512 ≈ 93.75 Hz
// Start: 16 * 93.75 = 1500 Hz
// End: 128 * 93.75 = 12000 Hz
const int activeBinStart = 16;
const int activeBinEnd = 128;
const int numCarriers =
    activeBinEnd - activeBinStart + 1; // 单个 OFDM 符号携带的比特数

// 原始数据参数
const int TOTAL_BITS = 10000;
const int CHUNK_SIZE = 1000; // 为了适应 OFDM 的高吞吐，可以加大分包

// --- Hamming(7,4) 保持不变 ---
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

// --- Preamble (Chirp) 保持不变，用于同步 ---
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
