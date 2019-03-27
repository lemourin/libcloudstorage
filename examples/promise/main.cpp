#include <condition_variable>
#include <fstream>
#include <iostream>

#include <poll.h>
#include <unistd.h>

#include "ICloudFactory.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

using cloudstorage::util::log;

class FactoryCallback : public ICloudFactory::ICallback {
 public:
  void onCloudTokenReceived(const std::string&,
                            const EitherError<Token>&) override {}

  void onCloudCreated(std::shared_ptr<ICloudAccess> d) override {
    if (d->name() == "google") {
      d->generalData().then([](GeneralData d) {
        log("got general data here", d.username_, d.space_used_,
            d.space_total_);
      });
    } else if (d->name() == "animezone") {
      const auto path =
          "/Anime/D/Death Note/1: Odrodzenie./Death Note 1 [PL] "
          "[Openload.co].mp4";
      d->getItem(path)
          .then([d](const IItem::Pointer& item) {
            std::ofstream file("animezone.png", std::ios::binary);
            return d->generateThumbnail(
                item, ICloudAccess::streamDownloader(
                          std::make_shared<std::ofstream>(std::move(file))));
          })
          .then([] { log("animezone thumb downloaded"); })
          .error<IException>(
              [](const IException& e) { log("animezone", e.what()); });
    } else if (d->name() == "mega") {
      d->getItem("/Bit Rush _ Login Screen - League of Legends.mp4")
          .then([d](IItem::Pointer item) {
            std::ofstream file("thumbnail.png", std::ios::binary);
            return d->generateThumbnail(
                item, ICloudAccess::streamDownloader(
                          std::make_shared<std::ofstream>(std::move(file))));
          })
          .then([] { log("generated thumb"); })
          .error<IException>([](const IException& e) { log(e.what()); });
      std::stringstream input;
      input << "dupa";
      log("starting upload");
      d->uploadFile(d->root(), "test",
                    ICloudAccess::streamUploader(
                        std::make_unique<std::stringstream>(std::move(input))))
          .then([=](IItem::Pointer file) {
            log("upload done");
            auto stream = std::make_shared<std::stringstream>();
            return std::make_tuple(
                d->downloadFile(file, FullRange,
                                ICloudAccess::streamDownloader(stream)),
                stream, file);
          })
          .then([=](const std::shared_ptr<std::stringstream>& stream,
                    IItem::Pointer file) {
            log("downloaded", stream->str());
            return std::make_tuple(d->deleteItem(file), file);
          })
          .then([](IItem::Pointer file) { log("deleted", file->filename()); })
          .error<IException>(
              [](const auto& d) { log("error happened", d.code(), d.what()); });
    }
  }

  void onCloudRemoved(std::shared_ptr<ICloudAccess>) override {}

  void onEventsAdded() override {
    std::unique_lock<std::mutex> lock(mutex_);
    events_ready_ = true;
    empty_condition_.notify_one();
  }

  int exec(const std::unique_ptr<ICloudFactory>& factory) {
    struct pollfd pollfd = {STDIN_FILENO, POLLIN | POLLPRI, 0};

    while (true) {
      if (poll(&pollfd, 1, 100)) {
        char string[10];
        scanf("%9s", string);
        if (string == std::string("q")) {
          break;
        }
      }
      {
        std::unique_lock<std::mutex> lock(mutex_);
        empty_condition_.wait_for(lock, std::chrono::milliseconds(100),
                                  [=] { return events_ready_; });
        events_ready_ = false;
      }
      factory->processEvents();
    }
    return 0;
  }

 private:
  std::mutex mutex_;
  std::condition_variable empty_condition_;
  bool events_ready_ = false;
};

int main() {
  auto factory_callback = std::make_shared<FactoryCallback>();
  ICloudFactory::InitData init_data;
  init_data.base_url_ = "http://localhost:12345";
  init_data.http_ = IHttp::create();
  init_data.http_server_factory_ = IHttpServerFactory::create();
  init_data.crypto_ = ICrypto::create();
  init_data.thread_pool_ = IThreadPool::create(1);
  init_data.callback_ = factory_callback;
  auto factory = ICloudFactory::create(std::move(init_data));
  {
    std::ifstream config("config.json");
    factory->loadAccounts(config);
  }

  for (const auto& d : factory->availableProviders()) {
    log(factory->authorizationUrl(d));
  }

  int exec = factory_callback->exec(factory);

  {
    std::ofstream config("config.json");
    factory->dumpAccounts(config);
  }

  return exec;
}
