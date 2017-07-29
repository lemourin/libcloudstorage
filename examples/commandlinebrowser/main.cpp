#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ICloudProvider.h"
#include "ICloudStorage.h"

using cloudstorage::ICloudProvider;
using cloudstorage::ICloudStorage;
using cloudstorage::IItem;

const std::string HELP_MESSAGE =
    "ls: list directory\n"
    "cd: change directory\n"
    "url: get url to the file\n"
    "help: this message\n";

class Callback : public cloudstorage::ICloudProvider::ICallback {
 public:
  Callback(std::string drive_file) : drive_file_(drive_file) {}

  Status userConsentRequired(
      const cloudstorage::ICloudProvider& provider) override {
    std::cout << "Required consent at url: \n";
    std::cout << provider.authorizeLibraryUrl() << "\n";
    return Status::WaitForAuthorizationCode;
  }

  void accepted(const cloudstorage::ICloudProvider& provider) override {
    std::fstream file(drive_file_, std::fstream::out);
    file << provider.token();
  }

  void declined(const cloudstorage::ICloudProvider&) override {
    std::cout << "You denied access ;_;\n";
  }

  void error(const cloudstorage::ICloudProvider&,
             cloudstorage::Error e) override {
    std::cout << "Error: " << e.description_ << "\n";
  }

 private:
  std::string drive_file_;
};

IItem::Pointer getChild(ICloudProvider::Pointer provider, IItem::Pointer item,
                        const std::string& filename) {
  for (auto i : *provider->listDirectoryAsync(item)->result().right())
    if (i->filename() == filename) return i;
  return nullptr;
}

int main(int, char**) {
  std::cout << HELP_MESSAGE;
  std::string command_line;
  ICloudProvider::Pointer current_provider;
  IItem::Pointer current_directory;
  std::vector<IItem::Pointer> directory_stack;
  std::string prompt = "/";
  while (std::cout << prompt << ": " && std::getline(std::cin, command_line)) {
    std::stringstream line(command_line);
    std::string command;
    line >> command >> std::ws;
    if (command == "ls") {
      if (current_directory == nullptr) {
        std::cout << "Select a provider: \n";
        for (auto p : ICloudStorage::create()->providers())
          std::cout << p->name() << "\n";
      } else {
        for (auto item :
             *current_provider->listDirectoryAsync(current_directory)
                  ->result()
                  .right()) {
          std::cout << item->filename() << "\n";
        }
      }
    } else if (command == "cd") {
      if (current_provider == nullptr) {
        std::string provider_name;
        line >> provider_name;
        auto provider = ICloudStorage::create()->provider(provider_name);
        if (!provider)
          std::cout << "No provider with name " << provider_name << "\n";
        else {
          std::string filename = provider_name + ".txt";
          std::string token;
          std::fstream(filename, std::fstream::in) >> token;
          provider->initialize(
              {token,
               std::unique_ptr<Callback>(new Callback(filename)),
               nullptr,
               nullptr,
               nullptr,
               nullptr,
               {}});
          prompt += provider_name + "/";
          current_provider = provider;
          current_directory = provider->rootDirectory();
          directory_stack.push_back(current_directory);
        }
      } else {
        std::string destination;
        std::getline(line, destination);
        if (destination != "..") {
          auto item =
              getChild(current_provider, current_directory, destination);
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
    } else if (command == "url") {
      if (!current_provider)
        std::cout << "No provider set\n";
      else {
        std::string file;
        std::getline(line, file);
        auto item = getChild(current_provider, current_directory, file);
        if (item) {
          auto data =
              current_provider->getItemDataAsync(item->id())->result().right();
          if (data) std::cout << "Url: " << data->url() << "\n";
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
