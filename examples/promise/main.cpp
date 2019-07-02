#include <condition_variable>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ICloudFactory.h"

using namespace cloudstorage;

class FactoryCallback : public ICloudFactory::ICallback {
 public:
  void onCloudCreated(const std::shared_ptr<ICloudAccess>& d) override {
    if (d->name() == "google") {
      std::cout << "getting general data\n";
      d->generalData().then([](GeneralData d) {
        std::cout << "got general data here " << d.username_ << " "
                  << d.space_used_ << " " << d.space_total_ << "\n";
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
          .then([] { std::cout << "animezone thumb downloaded\n"; })
          .error<IException>([](const IException& e) {
            std::cout << "animezone " << e.what();
          });
    } else if (d->name() == "mega") {
      d->getItem("/Bit Rush _ Login Screen - League of Legends.mp4")
          .then([d](IItem::Pointer item) {
            std::ofstream file("thumbnail.png", std::ios::binary);
            return d->generateThumbnail(
                item, ICloudAccess::streamDownloader(
                          std::make_shared<std::ofstream>(std::move(file))));
          })
          .then([] { std::cout << "generated thumb\n"; })
          .error<IException>(
              [](const IException& e) { std::cout << e.what() << "\n"; });
      std::stringstream input;
      input << "example content";
      std::cout << "starting upload\n";
      d->uploadFile(d->root(), "test",
                    ICloudAccess::streamUploader(
                        std::make_unique<std::stringstream>(std::move(input))))
          .then([=](IItem::Pointer file) {
            std::cout << "upload done\n";
            auto stream = std::make_shared<std::stringstream>();
            return std::make_tuple(
                d->downloadFile(file, FullRange,
                                ICloudAccess::streamDownloader(stream)),
                stream, file);
          })
          .then([=](const std::shared_ptr<std::stringstream>& stream,
                    IItem::Pointer file) {
            std::cout << "downloaded " << stream->str() << "\n";
            return std::make_tuple(d->deleteItem(file), file);
          })
          .then([](IItem::Pointer file) {
            std::cout << "deleted " << file->filename() << "\n";
          })
          .error<IException>([](const auto& d) {
            std::cout << "error happened " << d.code() << " " << d.what()
                      << "\n";
          });
    }
  }
};

int main(int argc, char** argv) {
  auto factory_callback = std::make_shared<FactoryCallback>();
  auto factory = ICloudFactory::create(factory_callback);

  if (argc == 2 && argv[1] == std::string("--list")) {
    std::cout << "Available providers:\n";
    for (const auto& d : factory->availableProviders()) {
      std::cout << d << " " << factory->authorizationUrl(d) << "\n";
    }
  }

  factory->loadAccounts(std::ifstream("config.json"));
  int exec = factory->exec();
  factory->dumpAccounts(std::ofstream("config.json"));

  return exec;
}
