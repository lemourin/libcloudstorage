#include <json/json.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "ICloudProvider.h"
#include "ICloudStorage.h"
#include "Utility/Utility.h"

using cloudstorage::ICloudProvider;
using cloudstorage::ICloudStorage;
using cloudstorage::IItem;

const std::string HELP_MESSAGE =
    "ls: list directory\n"
    "cd: change directory\n"
    "info: get file info\n"
    "help: this message\n";

#ifdef TOKEN_FILE
const std::string token_file = TOKEN_FILE;
#else
const std::string token_file = ".libcloudstorage-token.json";
#endif  // TOKEN_FILE

class Callback : public cloudstorage::ICloudProvider::IAuthCallback {
 public:
  Status userConsentRequired(
      const cloudstorage::ICloudProvider& provider) override {
    std::cout << "Required consent at url: \n";
    std::cout << provider.authorizeLibraryUrl() << "\n";
    return Status::WaitForAuthorizationCode;
  }

  void done(const cloudstorage::ICloudProvider& provider,
            cloudstorage::EitherError<void> e) override {
    if (e.left()) {
      std::cout << "authorization error " << e.left()->code_ << ": "
                << e.left()->description_ << "\n";
    } else {
      Json::Value json;
      try {
        json = cloudstorage::util::json::from_stream(std::ifstream(token_file));
      } catch (const Json::Exception&) {
      }
      json[provider.name()] = provider.token();
      std::ofstream(token_file) << json;
    }
  }
};

IItem::Pointer getChild(ICloudProvider* provider, IItem::Pointer item,
                        const std::string& filename) {
  auto lst = provider->listDirectorySimpleAsync(item)->result().right();
  if (!lst) return nullptr;
  for (auto i : *lst)
    if (i->filename() == filename) return i;
  return nullptr;
}

std::string read_token(const std::string& provider_name) {
  try {
    auto json =
        cloudstorage::util::json::from_stream(std::ifstream(token_file));
    return json[provider_name].asString();
  } catch (const Json::Exception&) {
    return "";
  }
}

int main(int, char**) {
  std::cout << HELP_MESSAGE;
  std::string command_line;
  ICloudProvider::Pointer current_provider;
  IItem::Pointer current_directory;
  IItem::List directory_stack;
  std::string prompt = "/";
  while (std::cout << prompt << ": " && std::getline(std::cin, command_line)) {
    std::stringstream line(command_line);
    std::string command;
    line >> command >> std::ws;
    if (command == "ls") {
      if (current_directory == nullptr) {
        std::cout << "Select a provider: \n";
        for (auto p : ICloudStorage::create()->providers())
          std::cout << p << "\n";
      } else {
        auto lst = current_provider->listDirectorySimpleAsync(current_directory)
                       ->result()
                       .right();
        if (lst)
          for (auto item : *lst) {
            std::cout << item->filename() << "\n";
          }
      }
    } else if (command == "cd") {
      if (current_provider == nullptr) {
        std::string provider_name;
        line >> provider_name;
        std::string token = read_token(provider_name);
        ICloudProvider::InitData data;
        data.token_ = token;
        data.permission_ = ICloudProvider::Permission::ReadWrite;
        data.callback_ = std::unique_ptr<Callback>(new Callback());
        auto provider =
            ICloudStorage::create()->provider(provider_name, std::move(data));
        if (provider) {
          prompt += provider_name + "/";
          current_directory = provider->rootDirectory();
          current_provider = std::move(provider);
          directory_stack.push_back(current_directory);
        } else {
          std::cout << "Provider " << provider_name << " unavailable.\n";
        }
      } else {
        std::string destination;
        std::getline(line, destination);
        if (destination != "..") {
          auto item =
              getChild(current_provider.get(), current_directory, destination);
          if (item) {
            if (item->type() == IItem::FileType::Directory) {
              current_directory = item;
              prompt += item->filename() + "/";
              directory_stack.push_back(item);
            } else
              std::cout << destination << " not a directory\n";
          } else
            std::cout << destination << " not found\n";
        } else {
          if (directory_stack.size() == 1) {
            current_provider = nullptr;
            current_directory = nullptr;
            directory_stack.clear();
            prompt = "/";
          } else {
            directory_stack.pop_back();
            current_directory = directory_stack.back();
            prompt = prompt.substr(
                0, prompt.find_last_of('/', prompt.length() - 2) + 1);
          }
        }
      }
    } else if (command == "info") {
      if (!current_provider)
        std::cout << "No provider set\n";
      else {
        std::string file;
        std::getline(line, file);
        auto item = getChild(current_provider.get(), current_directory, file);
        if (item) {
          auto data =
              current_provider->getItemDataAsync(item->id())->result().right();
          if (data) {
            if (data->timestamp() != IItem::UnknownTimeStamp) {
              auto time = cloudstorage::util::gmtime(
                  std::chrono::duration_cast<std::chrono::seconds>(
                      data->timestamp().time_since_epoch())
                      .count());
              std::cout << "timestamp: " << std::put_time(&time, "%c") << "\n";
            }
            if (data->size() != IItem::UnknownSize)
              std::cout << "size: " << data->size() << "\n";
            auto url_request =
                current_provider->getItemUrlAsync(item)->result();
            if (auto url = url_request.right())
              std::cout << "url: " << *url << "\n";
          }
        }
      }
    } else if (command == "help") {
      std::cout << HELP_MESSAGE;
    } else {
      std::cout << "Unknown command: " << command_line << "\n";
    }
  }

  return 0;
}
