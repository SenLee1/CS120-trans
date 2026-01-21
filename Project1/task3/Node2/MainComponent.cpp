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
  float totalSeconds = (float)recordedAudio.size() / Params::sampleRate;
  std::cout << "--- Processing " << totalSeconds << "s (No Hamming, " << Params::baudRate << " Baud) ---" << std::endl;

  if (totalSeconds < 1.0f) {
    statusLabel.setText("Error: Audio too short", juce::dontSendNotification);
    return;
  }

  auto preambleTemplate = Params::getPreamble();
  int preambleLen = (int)preambleTemplate.size();
  int maxSearch = (int)recordedAudio.size() - preambleLen - 2000;

  std::string totalBits = "";
  int chunksFound = 0;
  int expectedChunks = (Params::TOTAL_BITS + Params::CHUNK_SIZE - 1) / Params::CHUNK_SIZE;

  int scanPtr = 0;
  const int PEAK_TOLERANCE_WINDOW = 200; // 爬山算法的搜索窗

  // --- 主循环 ---
  while (scanPtr < maxSearch && chunksFound < expectedChunks) {

    // --- 1. Preamble 匹配 (Cross-Correlation) ---
    float corr = 0.0f, energy = 0.0f;
    int calcLen = std::min(preambleLen, 1000); // 降采样计算以提速
    
    for (int k = 0; k < calcLen; k += 10) {
      float s = recordedAudio[scanPtr + k];
      corr += s * preambleTemplate[k];
      energy += s * s;
    }
    if (energy < 0.00001f) energy = 0.00001f;
    float currentScore = (corr * corr) / energy;

    // --- 2. 触发与爬山 (Hill Climbing) ---
    if (currentScore > 0.1f) {
      int bestPeakPtr = scanPtr;
      float bestPeakScore = currentScore;
      int noBetterFoundCount = 0;
      int checkPtr = scanPtr + 1;

      // 动态寻找局部最高点
      while (noBetterFoundCount < PEAK_TOLERANCE_WINDOW && checkPtr < maxSearch) {
        float l_corr = 0.0f, l_energy = 0.0f;
        for (int k = 0; k < calcLen; k += 10) {
          float s = recordedAudio[checkPtr + k];
          l_corr += s * preambleTemplate[k];
          l_energy += s * s;
        }
        if (l_energy < 0.00001f) l_energy = 0.00001f;
        float checkScore = (l_corr * l_corr) / l_energy;

        if (checkScore > bestPeakScore) {
          bestPeakScore = checkScore;
          bestPeakPtr = checkPtr;
          noBetterFoundCount = 0; // 重置计数器
        } else {
          noBetterFoundCount++;
        }
        checkPtr++;
      }

      // 锁定一个包
      chunksFound++;
      std::cout << "[LOCKED] Chunk " << chunksFound << " at " << bestPeakPtr 
                << " (Score: " << bestPeakScore << ")" << std::endl;

      // --- 3. 解调逻辑 (移除 Hamming) ---
      // 这里的偏移量 = Preamble长度 + Guard(480)
      // 注意：稍微往后偏一点点(例如+10样本)避开Guard的边缘通常更安全，这里保持+480
      int readPtr = bestPeakPtr + preambleLen + 480; 
      
      std::vector<std::complex<float>> symbols;
      // 我们需要读取 DataBits + 1 个参考符号
      int symbolsToRead = Params::CHUNK_SIZE + 1;

      for (int k = 0; k < symbolsToRead; ++k) {
        if (readPtr + Params::samplesPerSymbol >= recordedAudio.size()) break;
        
        float iSum = 0.0f, qSum = 0.0f;
        // 积分 (Integrate)
        for (int s = 0; s < Params::samplesPerSymbol; ++s) {
          float val = recordedAudio[readPtr];
          // 本地载波生成 (Coherent Detection attempt)
          float t = (float)readPtr / Params::sampleRate;
          float c = std::cos(2.0f * juce::MathConstants<float>::pi * Params::carrierFreq * t);
          float q = std::sin(2.0f * juce::MathConstants<float>::pi * Params::carrierFreq * t);
          
          iSum += val * c;
          qSum += val * q;
          readPtr++;
        }
        symbols.push_back(std::complex<float>(iSum, qSum));
      }

      // 差分相干解调
      std::string chunkData = "";
      for (size_t k = 1; k < symbols.size(); ++k) {
        // Dot Product: Re(z[k] * conj(z[k-1]))
        // 结果 > 0 表示同相 (Bit 0)
        // 结果 < 0 表示反相 (Bit 1)
        float dotProd = symbols[k].real() * symbols[k - 1].real() +
                        symbols[k].imag() * symbols[k - 1].imag();
        
        chunkData += (dotProd < 0) ? '1' : '0';
      }
      totalBits += chunkData;

      // --- 4. 跳过当前包 ---
      // 计算跳过的样本数: Preamble + Guard + DataLength
      // DataLength = (500 + 1) * 48 = 24048
      // Preamble = 4800, Guard = 480
      // 总计约 29328。我们跳过 25000 以确保跳过大部分但不会错过下一个 Preamble
      scanPtr = bestPeakPtr + 25000; 

      continue;
    }

    // 没触发，快速扫描
    scanPtr += 10;
  }

  // --- 结尾处理 ---
  std::cout << "Decoded Total Bits: " << totalBits.length() << std::endl;
  // 截断多余的 (因为最后一个包可能只用了部分)
  if (totalBits.length() > Params::TOTAL_BITS)
    totalBits = totalBits.substr(0, Params::TOTAL_BITS);
  // 补齐不足的
  while (totalBits.length() < Params::TOTAL_BITS)
    totalBits += '0';

  std::ofstream outFile("OUTPUT.txt");
  outFile << totalBits;
  outFile.close();

  statusLabel.setText("Done. Saved OUTPUT.txt", juce::dontSendNotification);
}
