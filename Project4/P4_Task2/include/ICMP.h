// 保存到 include/ICMP.h
#ifndef ICMP_H
#define ICMP_H

#include "config.h"
// #include "icmp_header.h"       // 如果你有这些头文件，保留；如果没有，请注释掉
// #include "icmp_request_send.h" // 同上
// #include "ipv4_header.h"       // 同上
#include "utils.h"
#include <JuceHeader.h>
#include <sstream>
#include <utility>

// 定义本地通信端口 (localhost)
constexpr int CPP_PORT = 9001; // C++ 监听端口 (接收来自Python的数据)
constexpr int PY_PORT = 9002;  // Python 监听端口 (接收来自C++的数据)

class ICMP : public juce::Thread {
  public:
    ICMP() = delete;

    // 构造函数
    explicit ICMP(std::string self, ICMPProcessorType processFunc)
        : Thread("ICMP"), self_ip(std::move(self)), process(std::move(processFunc)) {

        // 初始化 UDP Socket 用于接收 Python 的数据
        socket = std::make_unique<juce::DatagramSocket>(false);
        if (!socket->bindToPort(CPP_PORT)) {
            // 如果绑定失败，可能是端口被占用
            DBG("ICMP Error: Could not bind to port " + juce::String(CPP_PORT));
        }
        fprintf(stderr, "\t\t[ICMP Bridge] Thread Start (Listening on UDP %d)\n", CPP_PORT);
    }

    ~ICMP() override {
        signalThreadShouldExit();
        if (socket)
            socket->shutdown();
        waitForThreadToExit(1000);
    }

    void run() override {
        char buffer[2048];
        while (!threadShouldExit()) {
            // 等待数据，超时1秒以便检查退出标志
            if (!socket->waitUntilReady(true, 1000))
                continue;

            int len = socket->read(buffer, sizeof(buffer) - 1, false);
            if (len <= 0)
                continue;
            buffer[len] = 0; // 确保字符串结束符

            std::string data(buffer);
            // 简单的数据完整性检查
            if (data.empty())
                continue;

            std::stringstream ss(data);
            ICMPFrameType frame;
            std::string dst_ip_addr;

            // 解析协议: IP(src) IP(dst) TYPE ID SEQ PAYLOAD
            // 对应 Python 发送的格式
            ss >> frame.ip >> dst_ip_addr >> frame.type >> frame.identifier >> frame.seq >> frame.payload;

            if (frame.ip.empty())
                continue;

            // 只有发给本机的包才处理 (Router模式下可能需要放宽这个限制)
            if (dst_ip_addr != self_ip) {
                // 在路由模式下，我们可能也对转发感兴趣，但这里先根据原逻辑过滤
                // 如果你需要抓包调试，可以注释掉这行
                // continue;
            }

            // 回调处理
            std::cerr << "[ICMP Recv] " << frame.ip << " -> " << dst_ip_addr << " Type:" << frame.type << std::endl;
            process(frame);
        }
    }

    static void send(const ICMPFrameType &frame) {
        // 创建一个临时 Socket 发送给 Python
        static juce::DatagramSocket senderSocket(false);

        std::stringstream ss;
        // 序列化格式: TYPE IP(dest) ID SEQ PAYLOAD
        // 注意：这里的 IP 是目标 IP，Python脚本会用它作为发送目的地
        ss << frame.type << ' ' << frame.ip << ' ' << frame.identifier << ' ' << frame.seq << ' ' << frame.payload;

        std::string msg = ss.str();
        // 发送到 localhost:9002
        senderSocket.write("127.0.0.1", PY_PORT, msg.c_str(), (int)msg.size());

        fprintf(stderr, "\t\t[ICMP Send] Relayed to Python Bridge\n");
    }

  private:
    std::string self_ip;
    ICMPProcessorType process;
    std::unique_ptr<juce::DatagramSocket> socket;
};

#endif // ICMP_H