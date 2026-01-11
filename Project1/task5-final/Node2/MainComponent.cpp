#include "MainComponent.h"
#include <complex>
#include <fstream>
#include <iostream>

MainComponent::MainComponent() {
  setSize(400, 250);
  addAndMakeVisible(processButton);
  processButton.setButtonText("Stop & Decode");
  processButton.onClick = [this] {
    isRecording = false;
    processRecording();
  };
  addAndMakeVisible(statusLabel);
  statusLabel.setText("Status: Recording...", juce::dontSendNotification);
  setAudioChannels(1, 0);
}

MainComponent::~MainComponent() { shutdownAudio(); }

// 包含缺失的 GUI 函数，防止报错
void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}
void MainComponent::resized() {
  processButton.setBounds(100, 80, 200, 60);
  statusLabel.setBounds(10, 10, getWidth() - 20, 30);
}

void MainComponent::prepareToPlay(int, double) {
  recordedAudio.reserve(48000 * 60); // 预留大内存
  recordedAudio.clear();
  isRecording = true;
}

void MainComponent::releaseResources() {}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo &bufferToFill) {
  if (!isRecording)
    return;
  auto *inData =
      bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
  for (int i = 0; i < bufferToFill.numSamples; ++i)
    recordedAudio.push_back(inData[i]);
}

// --- 按照你的思路重写的逻辑 ---
void MainComponent::processRecording() {
  // 1. 基础信息打印
  float totalSeconds = (float)recordedAudio.size() / Params::sampleRate;
  std::cout << "------------------------------------------------" << std::endl;
  std::cout << "Processing " << totalSeconds << "s (Dynamic Hill Climbing)..."
            << std::endl;

  if (totalSeconds < 1.0f) {
    statusLabel.setText("Error: Audio too short", juce::dontSendNotification);
    return;
  }
  statusLabel.setText("Processing...", juce::dontSendNotification);

  auto preambleTemplate = Params::getPreamble();
  int preambleLen = (int)preambleTemplate.size();
  int maxSearch = (int)recordedAudio.size() - preambleLen - 4000;

  // --- 1. 同步 (Preamble Detection) ---
  // 使用之前的 Hill Climbing 或简单的互相关寻找最大峰值
  int bestPeakPtr = -1;
  float maxScore = 0.0f;

  // 简化版同步：直接扫全图找最大值 (为了代码清晰)
  // 实际项目中保留你的 Hill Climbing 更好
  for (int i = 0; i < maxSearch; i += 10) {
    // 简易扫描，每10点测一次，找到大致区域后再精细找
    float corr = 0.0f;
    for (int k = 0; k < 500; k += 5)
      corr += recordedAudio[i + k] * preambleTemplate[k];
    if (std::abs(corr) > maxScore) {
      maxScore = std::abs(corr);
      bestPeakPtr = i;
    }
  }
  // 精细搜索
  int startSearch = std::max(0, bestPeakPtr - 500);
  int endSearch = std::min((int)recordedAudio.size(), bestPeakPtr + 500);
  maxScore = 0.0f;
  for (int i = startSearch; i < endSearch; ++i) {
    float corr = 0.0f;
    for (int k = 0; k < preambleLen; k += 4) { // 降采样计算以加速
      if (i + k >= recordedAudio.size())
        break;
      corr += recordedAudio[i + k] * preambleTemplate[k];
    }
    if (corr > maxScore) {
      maxScore = corr;
      bestPeakPtr = i;
    }
  }

  std::cout << "Sync found at: " << bestPeakPtr << " Score: " << maxScore
            << std::endl;

  if (maxScore < 1.0f) {
    statusLabel.setText("Sync Failed", juce::dontSendNotification);
    return;
  }

  // --- 2. OFDM 解调 ---

  // 指针定位到 Reference Symbol 的 CP 开始处
  // Preamble + Guard(480)
  int currentPtr = bestPeakPtr + preambleLen + 480;

  // 准备 FFT
  juce::dsp::FFT inverseFFT(Params::fftOrder); // 接收端做 FFT (前向变换)
  std::vector<std::complex<float>> timeData(Params::fftSize);
  std::vector<std::complex<float>> freqData(Params::fftSize);

  // 存储上一个符号的相位 (用于差分)
  std::vector<float> prevPhases(Params::fftSize, 0.0f);

  // A. 处理 Reference Symbol
  // 跳过 CP
  currentPtr += Params::cpSize;

  // 读取 FFT Body
  for (int i = 0; i < Params::fftSize; ++i) {
    if (currentPtr + i < recordedAudio.size())
      timeData[i] = std::complex<float>(recordedAudio[currentPtr + i], 0.0f);
    else
      timeData[i] = 0.0f;
  }
  currentPtr += Params::fftSize; // 移动指针到下一个符号的 CP 起点

  // 执行 FFT
  inverseFFT.perform(timeData.data(), freqData.data(),
                     false); // false = forward FFT

  // 记录参考相位
  for (int k = 0; k < Params::fftSize; ++k) {
    prevPhases[k] = std::arg(freqData[k]);
  }

  // B. 处理数据符号
  std::string decodedRawBits = "";
  int totalHammingBitsExpected = (Params::TOTAL_BITS / 4) * 7 + 100; // 估算一下

  while (decodedRawBits.length() < totalHammingBitsExpected &&
         currentPtr + Params::symbolTotalSize < recordedAudio.size()) {

    // 跳过 CP
    currentPtr += Params::cpSize;

    // 读取
    for (int i = 0; i < Params::fftSize; ++i) {
      timeData[i] = std::complex<float>(recordedAudio[currentPtr + i], 0.0f);
    }
    currentPtr += Params::fftSize;

    // FFT
    inverseFFT.perform(timeData.data(), freqData.data(), false);

    // 提取有效子载波
    for (int k = Params::activeBinStart; k <= Params::activeBinEnd; ++k) {
      float currPhase = std::arg(freqData[k]);
      float diff = currPhase - prevPhases[k];

      // 归一化相位差到 -PI ~ PI
      while (diff > juce::MathConstants<float>::pi)
        diff -= 2 * juce::MathConstants<float>::pi;
      while (diff < -juce::MathConstants<float>::pi)
        diff += 2 * juce::MathConstants<float>::pi;

      // DBPSK 判决:
      // 如果相位差接近 0，则是 bit 0 (原本发送端是 phase不变)
      // 如果相位差接近 PI/-PI，则是 bit 1 (原本发送端是 phase+=PI)
      // 判决阈值：|diff| > PI/2 为 1，否则为 0
      if (std::abs(diff) > juce::MathConstants<float>::halfPi) {
        decodedRawBits += '1';
      } else {
        decodedRawBits += '0';
      }

      // 更新 prevPhase
      prevPhases[k] = currPhase;
    }
  }

  // --- 3. Hamming 解码 ---
  std::string finalData = "";
  for (size_t i = 0; i < decodedRawBits.length(); i += 7) {
    if (i + 7 > decodedRawBits.length())
      break;

    uint8_t code = 0;
    for (int b = 0; b < 7; ++b) {
      if (decodedRawBits[i + b] == '1')
        code |= (1 << b);
    }
    uint8_t nibble = Params::decodeHamming(code);
    for (int b = 0; b < 4; ++b) {
      finalData += ((nibble >> b) & 1) ? '1' : '0';
    }
  }

  // 截断并保存
  if (finalData.length() > Params::TOTAL_BITS)
    finalData = finalData.substr(0, Params::TOTAL_BITS);

  std::cout << "Decoded Bits: " << finalData.length() << std::endl;
  std::ofstream outFile("OUTPUT.txt");
  outFile << finalData;
  outFile.close();
  statusLabel.setText("Done (OFDM)", juce::dontSendNotification);
}
