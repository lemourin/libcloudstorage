#include <condition_variable>
#include <fstream>
#include <iostream>

#include "ICloudFactory.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

using cloudstorage::util::log;

class FactoryCallback : public ICloudFactory::ICallback {
 public:
  void onCloudCreated(std::shared_ptr<ICloudAccess> d) override {
    if (d->name() == "google") {
      log("getting general data");
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
};

int main(int argc, char** argv) {
  auto factory_callback = std::make_shared<FactoryCallback>();
  auto factory = ICloudFactory::create(factory_callback);

  if (argc == 2 && argv[1] == std::string("--list")) {
    log("Available providers:");
    for (const auto& d : factory->availableProviders()) {
      log(d, factory->authorizationUrl(d));
    }
  }

  factory->loadAccounts(std::ifstream("config.json"));
  int exec = factory->exec();
  factory->dumpAccounts(std::ofstream("config.json"));

  return exec;
}
