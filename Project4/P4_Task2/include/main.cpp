// #include "Node.h" // 这里的 Node.h 会根据 CMake 配置自动指向 Node1/Node.h 或
// Node2/Node.h #include <JuceHeader.h>
//
// class Application : public juce::JUCEApplication {
// public:
//   Application() = default;
//
//   const juce::String getApplicationName() override { return "Project 4"; }
//   const juce::String getApplicationVersion() override { return "1.0.0"; }
//
//   // 程序启动时调用，创建主窗口
//   void initialise(const juce::String &) override {
//     mainWindow =
//         new MainWindow(getApplicationName(), new MainContentComponent,
//         *this);
//   }
//
//   // 程序关闭时调用，清理内存
//   void shutdown() override { delete mainWindow; }
//
// private:
//   class MainWindow : public juce::DocumentWindow {
//   public:
//     MainWindow(const juce::String &name, juce::Component *c, JUCEApplication
//     &a)
//         : DocumentWindow(name, juce::Colours::darkgrey,
//                          juce::DocumentWindow::allButtons),
//           app(a) {
//       setUsingNativeTitleBar(true);
//       setContentOwned(c, true); // 将 NodeX 的界面放入窗口
//       setResizable(false, false);
//       centreWithSize(getWidth(), getHeight());
//       setVisible(true);
//     }
//
//     void closeButtonPressed() override { app.systemRequestedQuit(); }
//
//   private:
//     JUCEApplication &app;
//
//     JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
//   };
//
//   MainWindow *mainWindow{nullptr};
// };
//
// // ⚠️ 这行代码非常重要！它生成了 WinMain 入口函数，解决了你的报o错
// START_JUCE_APPLICATION(Application)

// 路径: include/main.cpp
#include "Node.h" // 关键：编译器会根据当前编译的目标（Node1或Node2）去不同的文件夹找这个文件
#include <JuceHeader.h>

class Application : public juce::JUCEApplication {
public:
  Application() = default;

  // 获取应用程序名称
  const juce::String getApplicationName() override {
    return "Project 4 Network";
  }
  // 获取版本
  const juce::String getApplicationVersion() override { return "1.0.0"; }
  // 允许运行多个实例（必须为 true，否则无法同时运行 Node1 和 Node2）
  bool moreThanOneInstanceAllowed() override { return true; }

  // 初始化：创建主窗口
  void initialise(const juce::String &) override {
    // MainContentComponent 定义在各自的 Node.h 中
    mainWindow =
        new MainWindow(getApplicationName(), new MainContentComponent, *this);
  }

  // 关闭：销毁窗口
  void shutdown() override { delete mainWindow; }

  // 系统请求退出
  void systemRequestedQuit() override { quit(); }

  void anotherInstanceStarted(const juce::String &) override {}

private:
  // 定义主窗口类
  class MainWindow : public juce::DocumentWindow {
  public:
    MainWindow(const juce::String &name, juce::Component *c, JUCEApplication &a)
        : DocumentWindow(name, juce::Colours::darkgrey,
                         juce::DocumentWindow::allButtons),
          app(a) {
      setUsingNativeTitleBar(true);
      setContentOwned(c, true); // 窗口关闭时自动析构内部的 Component
      setResizable(true, true);
      centreWithSize(getWidth(), getHeight());
      setVisible(true);
    }

    void closeButtonPressed() override { app.systemRequestedQuit(); }

  private:
    JUCEApplication &app;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

  MainWindow *mainWindow{nullptr};
};

// JUCE 宏：生成 main() 入口函数
START_JUCE_APPLICATION(Application)
