#include "DesktopUtility.h"

#ifdef USE_DESKTOP_UTILITY

#include <QDesktopServices>
#include <QUrl>
#include <cstdlib>

#ifdef __unix__

#include <spawn.h>
#include <sys/wait.h>

#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "Utility/Utility.h"

#ifdef __unix__
extern char **environ;
#endif

namespace {

#ifdef __unix__

int run_xdg_screensaver(const std::vector<std::string> &args) {
  std::vector<char *> argv = {strdup("xdg-screensaver")};
  for (auto &&arg : args) argv.push_back(strdup(arg.c_str()));
  argv.push_back(nullptr);
  pid_t pid;
  auto status = posix_spawnp(&pid, "xdg-screensaver", nullptr, nullptr,
                             argv.data(), environ);
  for (auto &&arg : argv) free(arg);
  if (status == 0) {
    if (waitpid(pid, &status, 0) != -1) {
      return 0;
    } else {
      return -1;
    }
  } else {
    return status;
  }
}

#endif
}  // namespace

DesktopUtility::DesktopUtility() : screensaver_enabled_(true), running_(true) {
#ifdef __unix__
  thread_ = std::thread([=] {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait_for(lock, std::chrono::seconds(15),
                          [=] { return !running_; });
      if (!running_) break;
      if (!screensaver_enabled_) run_xdg_screensaver({"reset"});
    }
  });
#endif
}

DesktopUtility::~DesktopUtility() {
#ifdef __unix__
  {
    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    condition_.notify_one();
  }
  thread_.join();
#endif
}

bool DesktopUtility::mobile() const { return false; }

QString DesktopUtility::name() const { return "desktop"; }

bool DesktopUtility::openWebPage(QString url) {
  return QDesktopServices::openUrl(url);
}

void DesktopUtility::closeWebPage() {}

void DesktopUtility::landscapeOrientation() {}

void DesktopUtility::defaultOrientation() {}

void DesktopUtility::showPlayerNotification(bool, QString, QString) {}

void DesktopUtility::hidePlayerNotification() {}

void DesktopUtility::enableKeepScreenOn() {
  screensaver_enabled_ = false;
#ifdef _WIN32
  auto result =
      SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                              ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED);
  cloudstorage::util::log(result);
#endif
}

void DesktopUtility::disableKeepScreenOn() {
  screensaver_enabled_ = false;
#ifdef _WIN32
  SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

void DesktopUtility::showAd() {}

void DesktopUtility::hideAd() {}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<DesktopUtility>();
}

#endif  // USE_DESKTOP_UTILITY
