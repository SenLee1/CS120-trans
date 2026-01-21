#include "MainComponent.h"
#include <fstream>
#include <iostream>

MainComponent::MainComponent() {
  setSize(400, 200);
  addAndMakeVisible(sendButton);
  sendButton.setButtonText("Send Signal");
  sendButton.onClick = [this] {
    generateSignal();
    isTransmitting = true;
    playPosition = 0;
  };
  setAudioChannels(0, 2);
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int samplesPerBlockExpected,
                                  double sampleRate) {}
void MainComponent::releaseResources() {}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo &bufferToFill) {
  bufferToFill.clearActiveBufferRegion();
  if (!isTransmitting)
    return;

  auto *left =
      bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
  auto *right =
      bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

  for (int i = 0; i < bufferToFill.numSamples; ++i) {
    if (playPosition < transmissionSignal.size()) {
      float val = transmissionSignal[playPosition++];
      left[i] = val;
      right[i] = val;
    } else {
      isTransmitting = false;
      break;
    }
  }
}

void MainComponent::generateSignal() {
  transmissionSignal.clear();

  std::ifstream inFile("D:\\Courses\\4.1\\CN\\Project_Local\\Project1\\task3\\INPUT.txt");
  std::string allDataBits;
  if (inFile) inFile >> allDataBits;
  else allDataBits = std::string(10000, '1'); // Fallback

  // 截断或补齐
  if (allDataBits.length() > Params::TOTAL_BITS) {
    allDataBits = allDataBits.substr(0, Params::TOTAL_BITS);
  }
  while (allDataBits.length() < Params::TOTAL_BITS)
    allDataBits += '0';

  auto preamble = Params::getPreamble();
  int processedBits = 0;
  int chunkIndex = 0;

  // --- 分包循环 ---
  while (processedBits < allDataBits.length()) {
    std::string chunkBits = allDataBits.substr(processedBits, Params::CHUNK_SIZE);
    processedBits += chunkBits.length();
    chunkIndex++;

    // 1. 插入静音 Gap (防止多径效应残留)
    for (int i = 0; i < (int)(0.015 * Params::sampleRate); ++i)
      transmissionSignal.push_back(0.0f);

    // 2. 插入 Preamble
    transmissionSignal.insert(transmissionSignal.end(), preamble.begin(), preamble.end());

    // 3. Guard Interval (10ms) - 给接收端处理 Preamble 的时间
    for (int i = 0; i < 480; ++i)
      transmissionSignal.push_back(0.0f);

    // 4. DBPSK 调制 (无 Hamming)
    float currentPhase = 0.0f;

    // A. 发送参考符号 (Reference Symbol) - 相位 0
    for (int s = 0; s < Params::samplesPerSymbol; ++s) {
      float t = (float)s / Params::sampleRate;
      transmissionSignal.push_back(
          std::sin(2.0f * juce::MathConstants<float>::pi * Params::carrierFreq * t + currentPhase) * 0.7f);
    }

    // B. 数据位调制
    for (char bit : chunkBits) {
      // 差分调制：1 -> 翻转相位，0 -> 保持相位
      if (bit == '1') {
        currentPhase += juce::MathConstants<float>::pi;
      }
      
      // 生成正弦波
      for (int s = 0; s < Params::samplesPerSymbol; ++s) {
        float t = (float)s / Params::sampleRate;
        float val = std::sin(2.0f * juce::MathConstants<float>::pi * Params::carrierFreq * t + currentPhase);
        transmissionSignal.push_back(val * 0.7f);
      }
    }
  }

  // 尾部静音 (1秒足矣)
  for (int i = 0; i < 48000 * 1; ++i)
    transmissionSignal.push_back(0.0f);

  std::cout << "Generated " << chunkIndex << " chunks. Rate: " << Params::baudRate << " baud." << std::endl;
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}
void MainComponent::resized() { sendButton.setBounds(10, 10, 150, 40); }
