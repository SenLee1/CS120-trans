#pragma once

#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h"
#include "../include/writer.h"
#include <JuceHeader.h>
#include <fstream>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class MainContentComponent : public juce::AudioAppComponent {
public:
  MainContentComponent() {
    titleLabel.setText("Node1 (Client)",
                       juce::NotificationType::dontSendNotification);
    titleLabel.setSize(200, 40);
    titleLabel.setFont(juce::Font(24, juce::Font::FontStyleFlags::bold));
    titleLabel.setJustificationType(
        juce::Justification(juce::Justification::Flags::centred));
    titleLabel.setCentrePosition(300, 40);
    addAndMakeVisible(titleLabel);

    // Task 1 DNS Button
    Part3.setButtonText("Task 1: DNS Lookup");
    Part3.setSize(150, 40);
    Part3.setCentrePosition(300, 140);
    Part3.onClick = [this] {
      std::ifstream fIn("configPING.txt");
      if (!fIn.is_open()) {
        fprintf(stderr, "[Node1] Error: Cannot open configPING.txt\n");
        return;
      }

      std::string targetStr;
      fIn >> targetStr; // Read domain like "google.com"

      // Check if it looks like a domain (has letters)
      bool isDomain = false;
      for (char c : targetStr) {
        if (isalpha(c))
          isDomain = true;
      }

      if (isDomain) {
        fprintf(stderr, "[Node1] Resolving Domain: %s\n", targetStr.c_str());

        // Build DNS Query Packet (ID=1234)
        std::string dnsPacket = buildDNSQuery(targetStr, 0x1234);

        // Send to Node 2 via AtherNet (Port 53 is key)
        // The IP here (1.1.1.1) is just a placeholder, Node 2 will intercept
        // based on Port 53
        FrameType frame{Config::UDP, Str2IPType("1.1.1.1"), 53, dnsPacket};
        writer->send(frame);
      } else {
        fprintf(stderr, "[Node1] Input is not a domain string.\n");
      }
    };
    addAndMakeVisible(Part3);

    setSize(600, 300);
    setAudioChannels(1, 1);
  }

  ~MainContentComponent() override {
    shutdownAudio();
    releaseResources();
  }

private:
  // Helper to byte-swap 16-bit integer (Host to Network Short)
  uint16_t my_htons(uint16_t v) { return (v >> 8) | (v << 8); }

  // Helper to build raw DNS packet
  std::string buildDNSQuery(std::string domain, uint16_t id) {
    std::string packet;

    // --- Header (12 bytes) ---
    uint16_t transactionID = my_htons(id);
    uint16_t flags = my_htons(0x0100); // Standard Query, Recursion Desired
    uint16_t qdcount = my_htons(1);    // 1 Question
    uint16_t ancount = 0;
    uint16_t nscount = 0;
    uint16_t arcount = 0;

    packet.append((char *)&transactionID, 2);
    packet.append((char *)&flags, 2);
    packet.append((char *)&qdcount, 2);
    packet.append((char *)&ancount, 2);
    packet.append((char *)&nscount, 2);
    packet.append((char *)&arcount, 2);

    // --- Question Section ---
    // Format: [len]label[len]label...[0]
    size_t start = 0, end = 0;
    while ((end = domain.find('.', start)) != std::string::npos) {
      packet += (char)(end - start);
      packet += domain.substr(start, end - start);
      start = end + 1;
    }
    packet += (char)(domain.length() - start);
    packet += domain.substr(start);
    packet += (char)0; // End of name

    // Type A (1), Class IN (1)
    uint16_t qtype = my_htons(1);
    uint16_t qclass = my_htons(1);
    packet.append((char *)&qtype, 2);
    packet.append((char *)&qclass, 2);

    return packet;
  }

  void initThreads() {
    auto processFunc = [this](FrameType &frame) {
      // Check for DNS Response (UDP Source Port 53)
      if (frame.type == Config::UDP && frame.port == 53) {

        // === [新增过滤器] 防止听到自己的回音 ===

        // 1. 长度检查：DNS 回复包通常比 40 字节长 (Header 12 + Query ~16 +
        // Answer ~16)
        if (frame.body.size() < 40) {
          // fprintf(stderr, "[Node1] Ignored Echo/Query packet (Len=%zu)\n",
          // frame.body.size());
          return;
        }

        // 2. 标志位检查：DNS Header 的第3个字节 (索引2) 的最高位是 QR
        // 0x80 = 10000000 二进制
        unsigned char flag1 = (unsigned char)frame.body[2];
        bool isResponse = (flag1 & 0x80) != 0;

        if (!isResponse) {
          return; // 这是个查询包，忽略
        }

        // ===================================

        fprintf(stderr, "[Node1] Received DNS Response! Len=%zu\n",
                frame.body.size());

        // Simple parsing to extract IP (Type A)
        // In a simple response, the last 4 bytes are often the IP address.
        if (frame.body.size() > 16) {
          const unsigned char *ptr = (const unsigned char *)frame.body.c_str();
          size_t len = frame.body.length();

          // Print the last 4 bytes as IP
          fprintf(stderr, "[Node1] *** RESOLVED IP: %d.%d.%d.%d ***\n",
                  ptr[len - 4], ptr[len - 3], ptr[len - 2], ptr[len - 1]);
        }
      } else if (frame.type == Config::UDP) {
        fprintf(stderr, "[Node1] Recv UDP Msg: %s\n", frame.body.c_str());
      }
    };
    reader = new Reader(&directInput, &directInputLock, processFunc);
    reader->startThread();
    writer = new Writer(&directOutput, &directOutputLock);
  }

  void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
    initThreads();
    juce::AudioDeviceManager::AudioDeviceSetup currentAudioSetup;
    deviceManager.getAudioDeviceSetup(currentAudioSetup);
    currentAudioSetup.bufferSize = 144;
    deviceManager.setAudioDeviceSetup(currentAudioSetup, true);
  }

  void
  getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
    auto *device = deviceManager.getCurrentAudioDevice();
    auto activeInputChannels = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();
    auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
    auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
    auto buffer = bufferToFill.buffer;
    auto bufferSize = buffer->getNumSamples();
    for (auto channel = 0; channel < maxOutputChannels; ++channel) {
      if ((!activeInputChannels[channel] || !activeOutputChannels[channel]) ||
          maxInputChannels == 0) {
        bufferToFill.buffer->clear(channel, bufferToFill.startSample,
                                   bufferToFill.numSamples);
      } else {
        const float *data = buffer->getReadPointer(channel);
        directInputLock.enter();
        for (int i = 0; i < bufferSize; ++i) {
          directInput.push(data[i]);
        }
        directInputLock.exit();
        buffer->clear();
        float *writePosition = buffer->getWritePointer(channel);
        directOutputLock.enter();
        for (int i = 0; i < bufferSize; ++i) {
          if (directOutput.empty()) {
            writePosition[i] = 0.0f;
          } else {
            writePosition[i] = directOutput.front();
            directOutput.pop();
          }
        }
        directOutputLock.exit();
      }
    }
  }

  void releaseResources() override {
    if (reader) {
      reader->signalThreadShouldExit();
      delete reader;
      reader = nullptr;
    }
    if (writer) {
      delete writer;
      writer = nullptr;
    }
  }

private:
  Reader *reader{nullptr};
  Writer *writer{nullptr};
  std::queue<float> directInput;
  juce::CriticalSection directInputLock;
  std::queue<float> directOutput;
  juce::CriticalSection directOutputLock;

  juce::Label titleLabel;
  juce::TextButton Part3;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
