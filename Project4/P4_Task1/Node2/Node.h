#pragma once

#include "../include/UDP.h"
#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h"
#include "../include/writer.h"
#include <JuceHeader.h>
#include <fstream>
#include <queue>
#include <thread>

class MainContentComponent : public juce::AudioAppComponent {
public:
  MainContentComponent() {
    titleLabel.setText("Node2 (DNS NAT)",
                       juce::NotificationType::dontSendNotification);
    titleLabel.setSize(200, 40);
    titleLabel.setFont(juce::Font(24, juce::Font::FontStyleFlags::bold));
    titleLabel.setCentrePosition(300, 40);
    addAndMakeVisible(titleLabel);

    // Dummy button
    Part1CK.setButtonText("Running...");
    Part1CK.setSize(120, 40);
    Part1CK.setCentrePosition(300, 140);
    addAndMakeVisible(Part1CK);

    setSize(600, 300);
    setAudioChannels(1, 1);

    // Start network sockets
    initSockets();
  }

  ~MainContentComponent() override {
    shutdownAudio();
    // Stop reader thread first
    if (reader) {
      reader->signalThreadShouldExit();
      delete reader;
      reader = nullptr;
    }
    if (writer) {
      delete writer;
      writer = nullptr;
    }
    // Stop UDP sockets
    if (UDP_socket)
      delete UDP_socket;
    if (DNS_result_socket)
      delete DNS_result_socket;
  }

private:
  void initThreads() {
    // --- AtherNet (Audio) -> Ethernet/Python ---
    auto processFunc = [this](const FrameType &frame) {
      // [Task 1 Core Logic]
      // If UDP and Port 53, it is a DNS Query from Node 1
      if (frame.type == Config::UDP && frame.port == 53) {
        // === [新增] 检查这是查询还是回复？ ===
        // DNS Header 第3个字节 (idx 2) 的最高位是 QR。0=Query, 1=Response
        if (frame.body.size() > 2) {
          unsigned char flag = (unsigned char)frame.body[2];
          bool isQuery = (flag & 0x80) == 0;

          if (!isQuery) {
            fprintf(stderr,
                    "\t0[NAT] Intercepted DNS Query -> Forwarding to Python\n");
            // 这是我自己发出的回复包，被麦克风听到了，忽略它！
            return;
          }
        }
        // ===================================
        fprintf(stderr,
                "\t1[NAT] Intercepted DNS Query -> Forwarding to Python\n");

        // Send raw DNS body to Python (localhost:9003)
        // Using a temporary UDP sender
        static UDP pythonSender(0, [](FrameType &) {});
        pythonSender.send(frame.body, "127.0.0.1", 9003);
        return;
      }

      // [Task 2 Logic] Normal Forwarding
      if (frame.type == Config::UDP) {
        fprintf(stderr, "\t[NAT] Forwarding UDP to %s\n",
                IPType2Str(frame.ip).c_str());
        if (UDP_socket)
          UDP_socket->send(frame.body, IPType2Str(frame.ip), frame.port);
      }
    };

    reader = new Reader(&directInput, &directInputLock, processFunc);
    reader->startThread();
    writer = new Writer(&directOutput, &directOutputLock);
  }

  void initSockets() {
    fprintf(stderr, "[System] Initializing Sockets...\n");

    // 1. Regular UDP Listener (from Internet/Node3)
    // Uses Port defined in config for NODE1 (acting as Gateway)
    auto processUDP = [this](FrameType &frame) {
      fprintf(stderr, "\t[NAT] Recv UDP from Net -> Forwarding to Node1\n");
      writer->send(frame);
    };
    UDP_socket = new UDP(53, processUDP);
    UDP_socket->startThread();

    // 2. DNS Result Listener (from Python on Port 9004)
    auto processDNS = [this](FrameType &frame) {
      fprintf(stderr, "[Check A] Node2 UDP Recv from Python! Size=%zu\n",
              frame.body.size());
      // fprintf(stderr,
      // "\t[NAT] Recv DNS Result from Python -> Forwarding to Node1\n");

      // Construct a reply frame mimicking a real server
      FrameType replyFrame = frame;
      replyFrame.type = Config::UDP;
      replyFrame.ip = Str2IPType("1.1.1.1"); // Fake source IP
      replyFrame.port = 53;                  // Fake source Port
      replyFrame.body = frame.body;          // The DNS Answer

      fprintf(stderr, "[Check B] Node2 sending to Audio Writer...\n");
      writer->send(replyFrame);
    };
    // Listen on 9004 locally
    DNS_result_socket = new UDP(9004, processDNS);

    fprintf(stderr, "\tHere\n");
    DNS_result_socket->startThread();
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
        // Audio In -> Reader
        const float *data = buffer->getReadPointer(channel);
        directInputLock.enter();
        for (int i = 0; i < bufferSize; ++i) {
          directInput.push(data[i]);
        }
        directInputLock.exit();

        buffer->clear();

        // Audio Out <- Writer
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
    // Resources are cleaned in destructor for simplicity here
  }

private:
  Reader *reader{nullptr};
  Writer *writer{nullptr};
  std::queue<float> directInput;
  juce::CriticalSection directInputLock;
  std::queue<float> directOutput;
  juce::CriticalSection directOutputLock;

  juce::Label titleLabel;
  juce::TextButton Part1CK;

  GlobalConfig globalConfig{};
  UDP *UDP_socket{nullptr};
  UDP *DNS_result_socket{nullptr}; // Listener for Python results

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
