
#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

class Config {
  public:
    enum Node { NODE1 = 1,
                NODE2 = 2,
                NODE3 = 3 };
    enum Type { UDP = 1,
                TCP = 2,
                PING = 3,
                PONG = 4 };

    std::string ip;
    int port;
    Node node;
    Type type;
};

class GlobalConfig {
  public:
    GlobalConfig();
    Config get(Config::Node node, Config::Type type);

  private:
    std::vector<Config> _config;
};

#endif // CONFIG_H
