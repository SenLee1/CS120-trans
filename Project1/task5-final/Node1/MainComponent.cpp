#include "MainComponent.h"
#include <complex>
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

  // 1. 读取文件并处理比特
  std::ifstream inFile(
      "D:\\Courses\\4.1\\CN\\Project_Local\\Project1\\task4\\INPUT.txt");
  std::string rawDataBits;
  if (inFile)
    inFile >> rawDataBits;
  else
    rawDataBits = std::string(10000, '1');

  if (rawDataBits.length() > Params::TOTAL_BITS)
    rawDataBits = rawDataBits.substr(0, Params::TOTAL_BITS);
  while (rawDataBits.length() < Params::TOTAL_BITS)
    rawDataBits += '0';

  // 2. 对所有比特进行 Hamming 编码 -> 得到 encodedBits
  std::vector<int> encodedBits;
  // 补齐到 4 的倍数
  std::string bitsToProcess = rawDataBits;
  while (bitsToProcess.length() % 4 != 0)
    bitsToProcess += '0';

  for (size_t i = 0; i < bitsToProcess.length(); i += 4) {
    uint8_t nibble = 0;
    for (int b = 0; b < 4; ++b) {
      if (bitsToProcess[i + b] == '1')
        nibble |= (1 << b);
    }
    uint8_t code = Params::encodeHamming(nibble);
    for (int b = 0; b < 7; ++b)
      encodedBits.push_back((code >> b) & 1);
  }

  // 3. 准备 OFDM 生成
  auto preamble = Params::getPreamble();
  // 创建 FFT 对象 (Order 9 = 512 size)
  juce::dsp::FFT forwardFFT(Params::fftOrder);

  // 用于保存上一个符号的相位 (Differential BPSK)，初始化为 0
  std::vector<float> lastPhases(Params::fftSize, 0.0f);

  // 频域 Buffer (复数，但在 JUCE FFT 中通常处理为 float 数组，大小为 2*size)
  // 这里我们使用 performRealOnlyInverseTransform 的话，需要遵循特定格式，
  // 但为了手动控制共轭对称，我们最好用 complex->complex IFFT 然后取实部。
  // JUCE 的 perform (complex->complex) 需要输入 vector<complex<float>>.
  std::vector<std::complex<float>> freqBins(Params::fftSize);
  std::vector<std::complex<float>> timeDomain(Params::fftSize);

  // --- 开始组包 ---
  // 为了简单，我们把所有数据编码成一个巨大的连续 OFDM 符号流，前面加一次
  // Preamble 或者你可以维持 Chunk 结构。这里演示单次发送，适应大数据流。

  // A. 插入静音和 Preamble
  for (int i = 0; i < 4800; ++i)
    transmissionSignal.push_back(0.0f);
  transmissionSignal.insert(transmissionSignal.end(), preamble.begin(),
                            preamble.end());
  for (int i = 0; i < 480; ++i)
    transmissionSignal.push_back(0.0f); // Guard

  // B. 插入 Reference Symbol (全 1，用于建立相位基准)
  // 频域设为 1.0 (相位0)，时域生成后加入
  // 这里我们重置 lastPhases 为
  // 0，并发送一个代表“无变化”的符号，或者直接逻辑上处理
  // 简单做法：发送一个固定的已知 OFDM 符号作为参考
  // 我们选择在 active bins 上发送 1+0j，其他为 0
  std::fill(freqBins.begin(), freqBins.end(), std::complex<float>(0, 0));
  for (int k = Params::activeBinStart; k <= Params::activeBinEnd; ++k) {
    freqBins[k] = std::complex<float>(1.0f, 0.0f);
    freqBins[Params::fftSize - k] =
        std::complex<float>(1.0f, 0.0f); // 共轭对称(实部为1,虚部0,共轭也是1)
  }
  // IFFT
  forwardFFT.perform(freqBins.data(), timeDomain.data(),
                     true); // true for inverse
  // 添加 CP 并存入 transmissionSignal
  for (int i = Params::fftSize - Params::cpSize; i < Params::fftSize; ++i)
    transmissionSignal.push_back(timeDomain[i].real()); // CP
  for (int i = 0; i < Params::fftSize; ++i)
    transmissionSignal.push_back(timeDomain[i].real()); // Body

  // C. 处理数据比特
  int bitPtr = 0;
  while (bitPtr < encodedBits.size()) {
    std::fill(freqBins.begin(), freqBins.end(), std::complex<float>(0, 0));

    // 填充有效子载波
    for (int k = Params::activeBinStart; k <= Params::activeBinEnd; ++k) {
      int bit = 0;
      if (bitPtr < encodedBits.size()) {
        bit = encodedBits[bitPtr++];
      }

      // Differential BPSK:
      // 如果 bit = 1，相位翻转 (加 PI)；如果 bit = 0，相位不变。
      if (bit == 1) {
        lastPhases[k] += juce::MathConstants<float>::pi;
      }
      // 限制相位在 -PI 到 PI 虽非必须但好习惯，此处直接用 sin/cos 即可

      // 生成复数符号
      float phase = lastPhases[k];
      std::complex<float> sym(std::cos(phase), std::sin(phase));

      // 赋值给正频率 Bin k
      freqBins[k] = sym;
      // 赋值给负频率 Bin N-k (共轭)
      freqBins[Params::fftSize - k] = std::conj(sym);
    }

    // IFFT
    forwardFFT.perform(freqBins.data(), timeDomain.data(), true);

    // 添加 CP (Cyclic Prefix)
    // 复制尾部最后 cpSize 个样本放到头部
    for (int i = Params::fftSize - Params::cpSize; i < Params::fftSize; ++i) {
      transmissionSignal.push_back(timeDomain[i].real());
    }
    // 添加 OFDM Body
    for (int i = 0; i < Params::fftSize; ++i) {
      transmissionSignal.push_back(timeDomain[i].real());
    }
  }

  // 尾部静音
  for (int i = 0; i < 48000; ++i)
    transmissionSignal.push_back(0.0f);

  std::cout << "Generated OFDM Signal. Total Size: "
            << transmissionSignal.size() << std::endl;
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}
void MainComponent::resized() { sendButton.setBounds(10, 10, 150, 40); }
