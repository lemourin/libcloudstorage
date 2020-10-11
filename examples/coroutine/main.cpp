#include <fstream>
#include <iostream>
#include <sstream>

#include "ICloudFactory.h"

#ifdef __cpp_lib_coroutine

using namespace cloudstorage;

Promise<> co_main(ICloudAccess& d) noexcept {
  try {
    GeneralData data = co_await d.generalData();
    std::cerr << data.username_ << "\n";

    std::string page_token;
    while (true) {
      auto page_data = co_await d.listDirectoryPage(d.root(), page_token);

      for (const auto& d : page_data.items_) {
        std::cerr << d->filename() << "\n";
      }

      page_token = page_data.next_token_;
      if (page_token.empty()) {
        break;
      }
    }
  } catch (const IException& e) {
    std::cerr << "Exception: " << e.code() << " " << e.what() << "\n";
  }
}

class FactoryCallback : public ICloudFactory::ICallback {
 public:
  void onCloudCreated(const std::shared_ptr<ICloudAccess>& d) override {
    co_main(*d);
  }
};

int main(int argc, char** argv) {
  auto factory = ICloudFactory::create(std::make_shared<FactoryCallback>());

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

#else
int main() { return 0; }
#endif
