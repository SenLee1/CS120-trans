#pragma once
// === [新增] 修复 htonl/htons 找不到标识符的问题 ===
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib") // 确保链接 winsock 库
#else
#include <arpa/inet.h>
#endif
// ===============================================

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
  juce::StreamingSocket webSocket;
  uint32_t proxy_client_seq = 0; // 记录 Node 1 的 seq
  uint32_t proxy_server_seq = 0; // 模拟 Server 的 seq (发给 Node 1 的)
  bool is_proxy_connected = false;
  // 3. 添加辅助发送函数
  // 3. 辅助发送函数 (修复重定义问题版)
  void sendProxyTCP(IPType dstIP, int dstPort, uint8_t flags, const char *data,
                    int dataLen) {
    TCPHeader header;
    header.srcPort = htons(dstPort); // 互换端口
    header.dstPort = htons(12345);
    header.seqNum = htonl(proxy_server_seq);
    header.ackNum = htonl(proxy_client_seq);
    header.dataOffset = (sizeof(TCPHeader) / 4) << 4;
    header.flags = flags;
    header.window = htons(4096);

    // 构造 TCP 包体
    std::string packet;
    packet.append((char *)&header, sizeof(header));
    if (dataLen > 0)
      packet.append(data, dataLen);

    // === 构造发送帧 (只定义一次!) ===
    FrameType reply;
    reply.type = Config::TCP;
    reply.ip = dstIP;
    reply.port = dstPort;
    reply.body = packet;

    // 更新 Seq
    if (flags & TCP_SYN)
      proxy_server_seq++;
    else if (flags & TCP_FIN)
      proxy_server_seq++;
    else
      proxy_server_seq += dataLen;

    // 1. 发送音频 (通过 Writer)
    writer->send(reply);

    // 2. 发送 UDP 备份 (双保险)
    // 直接把 packet 内容发给 Node 1 的 UDP 端口 (1234)
    static UDP udpSender(0, [](FrameType &) {});
    // 注意：如果 Node 1 在别的电脑，这里要改 IP
    udpSender.send(packet, "127.0.0.1", 1234);
  }
  void initThreads() {
    auto processFunc = [this](const FrameType &frame) {
      // [Task 1 DNS Logic] 保持不变
      if (frame.type == Config::UDP && frame.port == 53) {
        if (frame.body.size() > 2 && ((unsigned char)frame.body[2] & 0x80))
          return;
        static UDP pythonSender(0, [](FrameType &) {});
        pythonSender.send(frame.body, "127.0.0.1", 9003);
        return;
      }

      // [Task 2 TCP Logic]
      if (frame.type == Config::TCP) {
        const TCPHeader *h = (const TCPHeader *)frame.body.c_str();
        uint8_t flags = h->flags;

        // 1. 处理 SYN (建立连接)
        if (flags & TCP_SYN) {
          fprintf(stderr,
                  "\n[Check Wireshark] Node 2 Received SYN from Node 1!\n");
          fprintf(stderr, "    Seq Num: 0x%08X\n", ntohl(h->seqNum));

          proxy_client_seq = ntohl(h->seqNum) + 1;
          proxy_server_seq = 0x87654321;
          std::string targetHost = IPType2Str(frame.ip);
          int targetPort = frame.port;

          fprintf(stderr, "\t[NAT] Connecting to Real Server %s:%d...\n",
                  targetHost.c_str(), targetPort);
          if (webSocket.connect(targetHost, targetPort)) {
            fprintf(stderr, "\t[NAT] Real Connection Established!\n");
            is_proxy_connected = true;
            sendProxyTCP(frame.ip, frame.port, TCP_SYN | TCP_ACK, "", 0);
          } else {
            fprintf(stderr, "\t[NAT] Connection Failed!\n");
          }
          return;
        }

        // 2. 处理数据 (HTTP GET)
        if ((flags & TCP_PSH) || (frame.body.size() > sizeof(TCPHeader))) {
          if (is_proxy_connected) {
            int headerLen = (h->dataOffset >> 4) * 4;
            int payloadLen = frame.body.size() - headerLen;

            if (payloadLen > 0) {
              fprintf(stderr, "\t[NAT] Forwarding %d bytes to Real Server\n",
                      payloadLen);
              webSocket.write(frame.body.c_str() + headerLen, payloadLen);

              // === [增强修复] 循环读取，死缠烂打 ===
              char buffer[4096];
              int readLen = 0;

              // 尝试 5 次读取，每次等 1 秒
              for (int i = 0; i < 5; i++) {
                fprintf(stderr,
                        "\t[NAT] Waiting for response (Attempt %d/5)...\n",
                        i + 1);
                if (webSocket.waitUntilReady(true, 1000)) {
                  readLen = webSocket.read(buffer, 4095, false);
                  if (readLen > 0) {
                    break; // 读到了！跳出循环
                  }
                }
              }
              // ===================================

              if (readLen > 0) {
                buffer[readLen] = 0;
                fprintf(stderr,
                        "\t[NAT] Got %d bytes response from Real Server\n",
                        readLen);
                sendProxyTCP(frame.ip, frame.port, TCP_PSH | TCP_ACK, buffer,
                             readLen);

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                sendProxyTCP(frame.ip, frame.port, TCP_FIN | TCP_ACK, "", 0);
                webSocket.close();
                is_proxy_connected = false;
              } else {
                // 如果 5 次都失败了，打印错误
                fprintf(stderr, "\t[NAT] ERROR: Real Server returned 0 bytes "
                                "after retries.\n");
                // 也要发 FIN 结束，不然 Node 1 会卡死
                sendProxyTCP(frame.ip, frame.port, TCP_FIN | TCP_ACK, "", 0);
              }
            }
          }
          return;
        }
        return;
      }

      // UDP Forwarding
      if (frame.type == Config::UDP) {
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
    UDP_socket =
        new UDP(globalConfig.get(Config::NODE1, Config::UDP).port, processUDP);
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
