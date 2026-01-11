// #pragma once
// // === [新增] 修复 htonl/htons 找不到标识符的问题 ===
// #ifdef _WIN32
// #include <winsock2.h>
// #pragma comment(lib, "ws2_32.lib") // 确保链接 winsock 库
// #else
// #include <arpa/inet.h>
// #endif
// // ===============================================
//
// #include "../include/UDP.h"
// #include "../include/config.h"
// #include "../include/reader.h"
// #include "../include/utils.h" // 确保这里面定义了 TCPHeader
// #include "../include/writer.h"
// #include <JuceHeader.h>
// #include <fstream>
// #include <queue>
// #include <thread>
// #include <vector>
//
// class MainContentComponent : public juce::AudioAppComponent {
// public:
//   MainContentComponent() {
//     // === UI 设置 ===
//     titleLabel.setText("Node1 (Client)",
//                        juce::NotificationType::dontSendNotification);
//     titleLabel.setSize(200, 40);
//     titleLabel.setFont(juce::Font(24, juce::Font::FontStyleFlags::bold));
//     titleLabel.setCentrePosition(300, 40);
//     addAndMakeVisible(titleLabel);
//
//     // === 按钮设置 (Task 2) ===
//     Part1CK.setButtonText("Task 2: HTTP GET");
//     Part1CK.setSize(200, 40);
//     Part1CK.setCentrePosition(300, 140);
//
//     // 点击按钮触发 HTTP 流程 (在新线程运行，防止卡死界面)
//     Part1CK.onClick = [this]() {
//       std::thread([this]() {
//         // 目标: example.com
//         performHTTP("http://example.com");
//       }).detach();
//     };
//     addAndMakeVisible(Part1CK);
//
//     setSize(600, 300);
//     setAudioChannels(1, 1);
//
//     // 初始化网络与音频线程
//     initSockets();
//   }
//
//   ~MainContentComponent() override {
//     shutdownAudio();
//     // 停止 Reader 线程
//     if (reader) {
//       reader->signalThreadShouldExit();
//       delete reader;
//       reader = nullptr;
//     }
//     if (writer) {
//       delete writer;
//       writer = nullptr;
//     }
//     // 停止 UDP
//     if (UDP_socket)
//       delete UDP_socket;
//   }
//
// private:
//   // ============================================================
//   //  Task 2 核心逻辑: 简易 TCP/HTTP 客户端
//   // ============================================================
//
//   // TCP 状态变量
//   uint32_t my_seq = 0;
//   uint32_t server_seq = 0;
//   std::queue<FrameType> tcp_queue; // 接收队列
//   juce::CriticalSection tcp_lock;  // 线程锁
//   void performHTTP(std::string command) {
//     // [Check] 这一行日志变化了，如果没看到它说明没编译成功
//     fprintf(stderr, "\n[Node1] === Processing Command: %s ===\n",
//             command.c_str());
//
//     // 1. URL 解析
//     std::string url = command;
//     // 去掉 "start chrome " 前缀（如果有）
//     std::string cmdPrefix = "start chrome ";
//     if (url.size() >= cmdPrefix.size() &&
//         url.substr(0, cmdPrefix.size()) == cmdPrefix) {
//       url = url.substr(cmdPrefix.size());
//     }
//     // 去掉 "http://" 前缀
//     std::string httpPrefix = "http://";
//     if (url.size() >= httpPrefix.size() &&
//         url.substr(0, httpPrefix.size()) == httpPrefix) {
//       url = url.substr(httpPrefix.size());
//     }
//     // 去掉末尾斜杠
//     if (!url.empty() && url.back() == '/')
//       url.pop_back();
//
//     std::string domain = url;
//     fprintf(stderr, "[Node1] Target Domain: %s\n", domain.c_str());
//
//     // 2. IP 映射 (填入你测试通的 IP)
//     std::string targetIP;
//     if (domain == "guozhivip.com")
//       targetIP = "47.93.11.51"; // 确保这是你能 ping 通的
//     else if (domain == "example.com")
//       targetIP = "93.184.216.34";
//     else if (domain == "121.40.59.158")
//       targetIP = "121.40.59.158";
//     else {
//       // 默认 Fallback
//       targetIP = "47.93.11.51";
//       domain = "guozhivip.com";
//     }
//
//     // 3. 准备文件 (这是新加的！)
//     std::ofstream htmlFile("index.html", std::ios::binary);
//     if (!htmlFile.is_open()) {
//       fprintf(stderr, "[Node1] Error: Cannot create index.html!\n");
//     }
//
//     // 4. TCP 流程
//     int targetPort = 80;
//     my_seq = 0x12345678;
//
//     // 握手
//     sendTCP(targetIP, targetPort, TCP_SYN, "", 0);
//     if (!waitForTCPPacket(TCP_SYN | TCP_ACK)) {
//       fprintf(stderr, "[Node1] ERROR: Handshake Timeout!\n");
//       return;
//     }
//     sendTCP(targetIP, targetPort, TCP_ACK, "", 0);
//
//     // 发送请求
//     std::string httpReq =
//         "GET / HTTP/1.0\r\nHost: " + domain + "\r\nConnection:
//         close\r\n\r\n";
//     sendTCP(targetIP, targetPort, TCP_PSH | TCP_ACK, httpReq.c_str(),
//             httpReq.length());
//     fprintf(stderr, "[Node1] Sent HTTP GET Request.\n");
//
//     // 接收数据
//     bool receivedData = false;
//     fprintf(stderr, "[Node1] Downloading content to file...\n");
//
//     while (true) {
//       FrameType response = waitForAnyTCP();
//       const TCPHeader *h = (const TCPHeader *)response.body.c_str();
//
//       int headerLen = (h->dataOffset >> 4) * 4;
//       if (response.body.size() > headerLen) {
//         std::string content = response.body.substr(headerLen);
//         if (content.length() > 0) {
//           // 写入文件 (关键步骤)
//           if (htmlFile.is_open())
//             htmlFile.write(content.c_str(), content.length());
//
//           // 打印一部分到屏幕证明收到了
//           if (!receivedData)
//             fprintf(stderr, "[Node1] Writing data to index.html...\n");
//
//           server_seq += content.length();
//           sendTCP(targetIP, targetPort, TCP_ACK, "", 0);
//           receivedData = true;
//         }
//       }
//       if (h->flags & TCP_FIN) {
//         fprintf(stderr, "\n[Node1] Download Completed. Closing
//         connection.\n"); sendTCP(targetIP, targetPort, TCP_ACK | TCP_FIN, "",
//         0); break;
//       }
//     }
//
//     if (htmlFile.is_open())
//       htmlFile.close();
//
//     // 5. 调用浏览器 (Magic Step)
//     if (receivedData) {
//       fprintf(stderr, "[Node1] Opening browser to render content...\n");
//       // Windows 命令，自动用默认浏览器打开文件
//       system("start index.html");
//     } else {
//       fprintf(stderr,
//               "[Node1] No data received, strictly skipping browser
//               launch.\n");
//     }
//   } //
//   //
//
//   // 辅助: 发送 TCP 包
//   void sendTCP(std::string ip, int port, uint8_t flags, const char *data,
//                int dataLen) {
//     TCPHeader header;
//     // 伪造本地端口 12345
//     header.srcPort = htons(12345);
//     header.dstPort = htons(port);
//     // 关键: 填入序列号
//     header.seqNum = htonl(my_seq);
//     header.ackNum = htonl(server_seq);
//     header.dataOffset = (sizeof(TCPHeader) / 4) << 4;
//     header.flags = flags;
//     header.window = htons(4096);
//     header.checksum = 0; // 简化实验不校验
//     header.urgentPointer = 0;
//
//     std::string packet;
//     packet.append((char *)&header, sizeof(header));
//     if (dataLen > 0) {
//       packet.append(data, dataLen);
//     }
//
//     // 封装成 Frame 发送给 Writer
//     FrameType frame{Config::TCP, Str2IPType(ip), (PORTType)port, packet};
//
//     // 更新本地 Seq: SYN/FIN 消耗 1，数据消耗 len
//     if (flags & TCP_SYN)
//       my_seq++;
//     else if (flags & TCP_FIN)
//       my_seq++;
//     else
//       my_seq += dataLen;
//
//     if (writer)
//       writer->send(frame);
//   }
//
//   bool waitForTCPPacket(uint8_t targetFlags) {
//     for (int i = 0; i < 50; i++) {
//       if (!tcp_queue.empty()) {
//         tcp_lock.enter();
//         FrameType frame = tcp_queue.front();
//
//         // === 新增安全检查：包太小直接丢弃，防止崩溃 ===
//         if (frame.body.size() < sizeof(TCPHeader)) {
//           fprintf(stderr, "[Warning] Drop invalid frame (len=%zu)\n",
//                   frame.body.size());
//           tcp_queue.pop();
//           tcp_lock.exit();
//           continue; // 继续循环
//         }
//
//         const TCPHeader *h = (const TCPHeader *)frame.body.c_str();
//         if ((h->flags & targetFlags) == targetFlags) {
//           server_seq = ntohl(h->seqNum) + 1;
//           tcp_queue.pop();
//           tcp_lock.exit();
//           return true;
//         }
//
//         // 如果是原来的 Echo 包 (只有 SYN)，也得丢掉
//         tcp_queue.pop();
//         tcp_lock.exit();
//       }
//       std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
//     return false;
//   }
//
//   // 辅助: 等待任意 TCP 包 (阻塞直到收到)
//   FrameType waitForAnyTCP() {
//     while (true) {
//       if (!tcp_queue.empty()) {
//         tcp_lock.enter();
//         FrameType f = tcp_queue.front();
//         tcp_queue.pop();
//         tcp_lock.exit();
//         return f;
//       }
//       std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//   }
//
//   // ============================================================
//   //  底层音频/网络处理
//   // ============================================================
//
//   void initThreads() {
//     // --- 接收回调函数 ---
//     auto processFunc = [this](const FrameType &frame) {
//       // 1. 如果是 TCP 包 -> 放入队列供 performHTTP 处理
//       if (frame.type == Config::TCP) {
//         tcp_lock.enter();
//         tcp_queue.push(frame);
//         tcp_lock.exit();
//         return;
//       }
//
//       // 2. 如果是 UDP DNS 结果 (Task 1 遗留逻辑)
//       if (frame.type == Config::UDP && frame.port == 53) {
//         fprintf(stderr, "[Node1] Received DNS Response! Len=%zu\n",
//                 frame.body.size());
//         // 之前写的解析逻辑可以在这里，或者直接忽略
//       }
//     };
//
//     reader = new Reader(&directInput, &directInputLock, processFunc);
//     reader->startThread();
//     writer = new Writer(&directOutput, &directOutputLock);
//   }
//
//   void initSockets() {
//     fprintf(stderr, "[System] Node 1 Initializing Sockets...\n");
//
//     // 监听 UDP 1234 端口 (接收 Node 2 的备份包)
//     auto processUDP = [this](FrameType &frame) {
//       // [修复]：底层 UDP 类收到的包类型永远是 Config::UDP
//       // 所以这里要检查 Config::UDP，而不是 Config::TCP
//       if (frame.type == Config::UDP) {
//         // 过滤掉太小的包（比如空包或垃圾包）
//         if (frame.body.size() < sizeof(TCPHeader))
//           return;
//
//         fprintf(stderr, "[Node1] UDP Backup Received! Size=%zu\n",
//                 frame.body.size());
//
//         // [关键]：手动把类型改成 TCP，骗过 performHTTP 的检查逻辑
//         frame.type = Config::TCP;
//
//         tcp_lock.enter();
//         tcp_queue.push(frame);
//         tcp_lock.exit();
//       }
//     };
//
//     // 启动监听
//     UDP_socket = new UDP(1234, processUDP);
//     UDP_socket->startThread();
//   }
//
//   void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
//   {
//     initThreads();
//     juce::AudioDeviceManager::AudioDeviceSetup currentAudioSetup;
//     deviceManager.getAudioDeviceSetup(currentAudioSetup);
//     currentAudioSetup.bufferSize = 144;
//     deviceManager.setAudioDeviceSetup(currentAudioSetup, true);
//   }
//
//   void
//   getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill)
//   override {
//     auto *device = deviceManager.getCurrentAudioDevice();
//     auto activeInputChannels = device->getActiveInputChannels();
//     auto activeOutputChannels = device->getActiveOutputChannels();
//     auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
//     auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
//     auto buffer = bufferToFill.buffer;
//     auto bufferSize = buffer->getNumSamples();
//
//     for (auto channel = 0; channel < maxOutputChannels; ++channel) {
//       if ((!activeInputChannels[channel] || !activeOutputChannels[channel])
//       ||
//           maxInputChannels == 0) {
//         bufferToFill.buffer->clear(channel, bufferToFill.startSample,
//                                    bufferToFill.numSamples);
//       } else {
//         // Audio In -> Reader
//         const float *data = buffer->getReadPointer(channel);
//         directInputLock.enter();
//         for (int i = 0; i < bufferSize; ++i) {
//           directInput.push(data[i]);
//         }
//         directInputLock.exit();
//
//         buffer->clear();
//
//         // Audio Out <- Writer
//         float *writePosition = buffer->getWritePointer(channel);
//         directOutputLock.enter();
//         for (int i = 0; i < bufferSize; ++i) {
//           if (directOutput.empty()) {
//             writePosition[i] = 0.0f;
//           } else {
//             writePosition[i] = directOutput.front();
//             directOutput.pop();
//           }
//         }
//         directOutputLock.exit();
//       }
//     }
//   }
//
//   void releaseResources() override {}
//
// private:
//   Reader *reader{nullptr};
//   Writer *writer{nullptr};
//   std::queue<float> directInput;
//   juce::CriticalSection directInputLock;
//   std::queue<float> directOutput;
//   juce::CriticalSection directOutputLock;
//
//   juce::Label titleLabel;
//   juce::TextButton Part1CK;
//
//   GlobalConfig globalConfig{};
//   UDP *UDP_socket{nullptr};
//
//   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
// };
#pragma once

// === [Windows Socket 修复] ===
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif
// ============================

#include "../include/UDP.h"
#include "../include/config.h"
#include "../include/reader.h"
#include "../include/utils.h" // 确保这里有 TCPHeader 定义
#include "../include/writer.h"
#include <JuceHeader.h>
#include <fstream>
#include <queue>
#include <thread>
#include <vector>

class MainContentComponent : public juce::AudioAppComponent {
public:
  MainContentComponent() {
    // === 1. 标题 ===
    titleLabel.setText("Node1 (Client)",
                       juce::NotificationType::dontSendNotification);
    titleLabel.setSize(200, 40);
    titleLabel.setFont(juce::Font(24, juce::Font::FontStyleFlags::bold));
    titleLabel.setCentrePosition(300, 40);
    addAndMakeVisible(titleLabel);

    // === 2. [新增] URL 输入框和标签 ===
    // 这里的坐标决定了输入框的位置
    urlLabel.setText("Command:", juce::NotificationType::dontSendNotification);
    urlLabel.setBounds(100, 100, 80, 30); // 标签位置 (x=100, y=100)
    urlLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(urlLabel);

    // 输入框位置 (x=190, y=100)，宽度 300
    urlInput.setText("start chrome http://guozhivip.com"); // 默认文字
    urlInput.setBounds(190, 100, 300, 30);
    addAndMakeVisible(urlInput);

    // === 3. 按钮 ===
    Part1CK.setButtonText("Run Command");
    Part1CK.setSize(150, 40);
    // 按钮放在输入框下面 (y=160)
    Part1CK.setCentrePosition(300, 160);

    // === 4. 按钮点击事件 ===
    Part1CK.onClick = [this]() {
      // 获取输入框里的文字
      std::string cmd = urlInput.getText().toStdString();

      // 在新线程执行
      std::thread([this, cmd]() { performHTTP(cmd); }).detach();
    };
    addAndMakeVisible(Part1CK);

    setSize(600, 300);
    setAudioChannels(1, 1);
    initSockets();
  }

  ~MainContentComponent() override {
    shutdownAudio();
    if (reader) {
      reader->signalThreadShouldExit();
      delete reader;
      reader = nullptr;
    }
    if (writer) {
      delete writer;
      writer = nullptr;
    }
    if (UDP_socket)
      delete UDP_socket;
  }

private:
  // === 核心逻辑：带文件保存和浏览器调用的 HTTP ===
  void performHTTP(std::string command) {
    fprintf(stderr, "\n[Node1] === Processing Command: %s ===\n",
            command.c_str());

    // 1. URL 解析
    std::string url = command;
    // 去掉 "start chrome "
    std::string cmdPrefix = "start chrome ";
    if (url.size() >= cmdPrefix.size() &&
        url.substr(0, cmdPrefix.size()) == cmdPrefix) {
      url = url.substr(cmdPrefix.size());
    }
    // 去掉 "http://"
    std::string httpPrefix = "http://";
    if (url.size() >= httpPrefix.size() &&
        url.substr(0, httpPrefix.size()) == httpPrefix) {
      url = url.substr(httpPrefix.size());
    }

    // C. 【新增】去掉 "www." (防止匹配不到 IP)
    std::string wwwPrefix = "www.";
    if (url.size() >= wwwPrefix.size() &&
        url.substr(0, wwwPrefix.size()) == wwwPrefix) {
      url = url.substr(wwwPrefix.size());
    }
    // 去掉末尾斜杠
    if (!url.empty() && url.back() == '/')
      url.pop_back();

    std::string domain = url;
    fprintf(stderr, "[Node1] Target Domain: %s\n", domain.c_str());

    // 2. IP 映射 (填入你实际能 ping 通的 IP)
    std::string targetIP;
    if (domain == "guozhivip.com")
      targetIP = "47.93.11.51";
    else if (domain == "example.com")
      targetIP = "104.18.27.120";
    else if (domain == "121.40.59.158")
      targetIP = "121.40.59.158";
    else {
      // 默认 Fallback
      targetIP = "47.93.11.51";
      domain = "guozhivip.com";
    }

    // 3. 准备文件 index.html
    std::ofstream htmlFile("index.html", std::ios::binary);
    if (!htmlFile.is_open()) {
      fprintf(stderr, "[Node1] Error: Cannot create index.html!\n");
    }

    // 4. TCP 流程
    int targetPort = 80;
    my_seq = 0x12345678;

    // 握手
    sendTCP(targetIP, targetPort, TCP_SYN, "", 0);
    if (!waitForTCPPacket(TCP_SYN | TCP_ACK)) {
      fprintf(stderr, "[Node1] ERROR: Handshake Timeout!\n");
      return;
    }
    sendTCP(targetIP, targetPort, TCP_ACK, "", 0);

    // 发送请求
    std::string httpReq =
        "GET / HTTP/1.0\r\nHost: " + domain + "\r\nConnection: close\r\n\r\n";
    sendTCP(targetIP, targetPort, TCP_PSH | TCP_ACK, httpReq.c_str(),
            httpReq.length());
    fprintf(stderr, "[Node1] Sent HTTP GET Request.\n");

    // 接收数据
    bool receivedData = false;
    fprintf(stderr, "[Node1] Downloading content to file...\n");

    while (true) {
      FrameType response = waitForAnyTCP();
      const TCPHeader *h = (const TCPHeader *)response.body.c_str();

      int headerLen = (h->dataOffset >> 4) * 4;
      if (response.body.size() > headerLen) {
        std::string content = response.body.substr(headerLen);
        if (content.length() > 0) {
          // 写入文件
          if (htmlFile.is_open())
            htmlFile.write(content.c_str(), content.length());

          // 打印日志证明收到
          if (!receivedData)
            fprintf(stderr, "[Node1] Writing data to index.html...\n");

          server_seq += content.length();
          sendTCP(targetIP, targetPort, TCP_ACK, "", 0);
          receivedData = true;
        }
      }
      if (h->flags & TCP_FIN) {
        fprintf(stderr, "\n[Node1] Download Completed. Closing connection.\n");
        sendTCP(targetIP, targetPort, TCP_ACK | TCP_FIN, "", 0);
        break;
      }
    }

    if (htmlFile.is_open())
      htmlFile.close();

    // 5. 只有收到数据才调用浏览器
    if (receivedData) {
      fprintf(stderr, "[Node1] Opening browser to render content...\n");
      system("start index.html");
    } else {
      fprintf(stderr, "[Node1] No data received, skipping browser launch.\n");
    }
  }

  // === 辅助函数：发送 TCP ===
  void sendTCP(std::string ip, int port, uint8_t flags, const char *data,
               int dataLen) {
    TCPHeader header;
    header.srcPort = htons(12345);
    header.dstPort = htons(port);
    header.seqNum = htonl(my_seq);
    header.ackNum = htonl(server_seq);
    header.dataOffset = (sizeof(TCPHeader) / 4) << 4;
    header.flags = flags;
    header.window = htons(4096);
    header.checksum = 0;
    header.urgentPointer = 0;

    std::string packet;
    packet.append((char *)&header, sizeof(header));
    if (dataLen > 0)
      packet.append(data, dataLen);

    FrameType frame{Config::TCP, Str2IPType(ip), (PORTType)port, packet};

    if (flags & TCP_SYN)
      my_seq++;
    else if (flags & TCP_FIN)
      my_seq++;
    else
      my_seq += dataLen;

    if (writer)
      writer->send(frame);
  }

  // === 辅助函数：等待 TCP ===
  bool waitForTCPPacket(uint8_t targetFlags) {
    for (int i = 0; i < 50; i++) {
      if (!tcp_queue.empty()) {
        tcp_lock.enter();
        FrameType frame = tcp_queue.front();

        // 过滤小包
        if (frame.body.size() < sizeof(TCPHeader)) {
          tcp_queue.pop();
          tcp_lock.exit();
          continue;
        }

        const TCPHeader *h = (const TCPHeader *)frame.body.c_str();
        if ((h->flags & targetFlags) == targetFlags) {
          server_seq = ntohl(h->seqNum) + 1;
          tcp_queue.pop();
          tcp_lock.exit();
          return true;
        }
        tcp_queue.pop();
        tcp_lock.exit();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
  }

  FrameType waitForAnyTCP() {
    while (true) {
      if (!tcp_queue.empty()) {
        tcp_lock.enter();
        FrameType f = tcp_queue.front();
        tcp_queue.pop();
        tcp_lock.exit();
        return f;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  void initThreads() {
    auto processFunc = [this](const FrameType &frame) {
      if (frame.type == Config::TCP) {
        tcp_lock.enter();
        tcp_queue.push(frame);
        tcp_lock.exit();
        return;
      }
    };

    reader = new Reader(&directInput, &directInputLock, processFunc);
    reader->startThread();
    writer = new Writer(&directOutput, &directOutputLock);
  }

  void initSockets() {
    fprintf(stderr, "[System] Node 1 Initializing Sockets...\n");
    // UDP 1234 双保险接收逻辑
    auto processUDP = [this](FrameType &frame) {
      if (frame.type == Config::UDP) {
        if (frame.body.size() < sizeof(TCPHeader))
          return;
        // 将 UDP 备份包伪装成 TCP 包推入队列
        frame.type = Config::TCP;
        tcp_lock.enter();
        tcp_queue.push(frame);
        tcp_lock.exit();
      }
    };
    UDP_socket = new UDP(1234, processUDP);
    UDP_socket->startThread();
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

  void releaseResources() override {}

private:
  Reader *reader{nullptr};
  Writer *writer{nullptr};
  std::queue<float> directInput;
  juce::CriticalSection directInputLock;
  std::queue<float> directOutput;
  juce::CriticalSection directOutputLock;

  juce::Label titleLabel;

  // === UI 控件声明 ===
  juce::TextButton Part1CK;
  juce::TextEditor urlInput; // 输入框
  juce::Label urlLabel;      // 标签

  GlobalConfig globalConfig{};
  UDP *UDP_socket{nullptr};

  // TCP 核心变量
  uint32_t my_seq = 0;
  uint32_t server_seq = 0;
  std::queue<FrameType> tcp_queue;
  juce::CriticalSection tcp_lock;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
