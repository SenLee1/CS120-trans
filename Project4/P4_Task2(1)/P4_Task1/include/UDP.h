
#ifndef UDP_H
#define UDP_H

#include "config.h"
#include "utils.h"
#include <JuceHeader.h>

class UDP : public Thread {
  public:
    UDP() = delete;

    UDP(const UDP &) = delete;

    UDP(const UDP &&) = delete;

    explicit UDP(int listen_port, ProcessorType processFunc) : Thread("UDP"), port(listen_port), UDP_Socket(new juce::DatagramSocket(false)), process(std::move(processFunc)) {
        if (!UDP_Socket->bindToPort(listen_port)) {
            fprintf(stderr, "\t\tPort %d in use!\n", listen_port);
            exit(listen_port);
        }
        fprintf(stderr, "\t\tUDP Thread Start\n");
    }

    ~UDP() override {
        this->signalThreadShouldExit();
        this->waitForThreadToExit(1000);
        UDP_Socket->shutdown();
        delete UDP_Socket;
    }

    void run() override {
        while (!threadShouldExit()) {
            // char buffer[50];  <-- 太小了！删掉
            char buffer[1024]; // <-- 改成 1024，足够大

            juce::String sender_ip;
            int sender_port;
            UDP_Socket->waitUntilReady(true, 10000);

            // int len = UDP_Socket->read(buffer, 41, ...); <-- 太小了！删掉
            // 改成读取 buffer 的最大容量减 1
            int len = UDP_Socket->read(buffer, 1023, false, sender_ip, sender_port);

            if (len == 0)
                continue;

            // 下面这行日志打印 DNS 包可能会乱码（因为 DNS 包中间有 0），但没关系
            // buffer[len] = 0; // 这行其实可以保留作为字符串结尾
            fprintf(stderr, "\t\t%s:%d %d bytes content\n", sender_ip.toStdString().c_str(), sender_port, len); // 稍微改一下日志，别打印 binary content 了

            FrameType frame{Config::UDP, Str2IPType(sender_ip.toStdString()), (PORTType)sender_port, std::string(buffer, len)};
            process(frame);
        }
    }

    void send(const std::string &buffer, const std::string &ip, int target_port) {
        UDP_Socket->waitUntilReady(false, 10000);
        UDP_Socket->write(ip, target_port, buffer.c_str(), static_cast<int>(buffer.size()));
        fprintf(stderr, "\t\tUDP sent\n");
    }

  private:
    int port;
    juce::DatagramSocket *UDP_Socket;
    ProcessorType process;
};

#endif // UDP_H
