
#include "config.h"

using namespace std::chrono_literals;

GlobalConfig::GlobalConfig() {
  // 把 "./config.txt" 改成你的绝对路径，注意用双斜杠
  std::ifstream configFile(
      "D:\\Courses\\4.1\\CN\\Project_Local\\Project4\\P4_Task1\\config.txt",
      std::ios::in);
  while (!configFile.is_open()) {
    fprintf(stderr, "Failed to open config.txt! Retry after 1s.\n");
    std::this_thread::sleep_for(1000ms);
  }
  while (!configFile.eof()) {
    std::string ip, node, type;
    int port;
    for (int i = 0; i < 4; ++i) {
      configFile >> ip >> port >> node >> type;
      Config config;
      config.ip = ip;
      config.port = port;
      if (node == "NODE1") {
        config.node = Config::Node::NODE1;
      } else if (node == "NODE2") {
        config.node = Config::Node::NODE2;
      } else if (node == "NODE3") {
        config.node = Config::Node::NODE3;
      } else {
        NOT_REACHED
      }
      if (type == "UDP") {
        config.type = Config::Type::UDP;
      } else if (type == "TCP") {
        config.type = Config::Type::TCP;
      } else {
        NOT_REACHED
      }
      _config.push_back(config);
    }
  }
}

Config GlobalConfig::get(Config::Node node, Config::Type type) {
  for (auto _i : _config) {
    if (_i.node == node && _i.type == type) {
      return _i;
    }
  }
  NOT_REACHED
}
