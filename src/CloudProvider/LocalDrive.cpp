/*****************************************************************************
 * LocalDrive.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "LocalDrive.h"

#ifdef WITH_LOCALDRIVE

#include <json/json.h>
#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <codecvt>
#include "Utility/Item.h"
#include "Utility/Utility.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = boost::filesystem;
using error_code = boost::system::error_code;

namespace cloudstorage {

const size_t BUFFER_SIZE = 1024;
const auto POLL_INTERVAL = std::chrono::seconds(1);

namespace {

bool is_hidden(const fs::directory_entry &e) {
#ifdef _WIN32
  return GetFileAttributesW(e.path().c_str()) &
         (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
#else
  return e.path().filename().c_str()[0] == '.' ||
         e.path().filename() == "lost+found";
#endif
}

std::string to_string(const fs::path &path) {
  return path.string(std::codecvt_utf8<wchar_t>());
}

fs::path from_string(const std::string &string) {
  return fs::path(string, std::codecvt_utf8<wchar_t>());
}

void list_directory(
    const LocalDrive &p, IItem::Pointer item,
    std::function<void(EitherError<IItem::List>)> done,
    std::function<void(IItem::Pointer)> received_item = [](IItem::Pointer) {}) {
  IItem::List result;
  error_code ec;
  auto directory = fs::directory_iterator(p.path(item), ec);
  if (ec) return done(Error{ec.value(), ec.message()});
  for (auto &&e : directory) {
    auto timestamp = std::chrono::system_clock::from_time_t(
        fs::last_write_time(e.path(), ec));
    if (ec) timestamp = IItem::UnknownTimeStamp;
    auto size = fs::file_size(e.path(), ec);
    if (ec) size = IItem::UnknownSize;
    bool is_directory = fs::is_directory(e.path(), ec);
    if (ec) is_directory = false;
    if (!is_hidden(e)) {
      auto item = std::make_shared<Item>(
          to_string(e.path().filename()), to_string(e.path()), size, timestamp,
          is_directory ? IItem::FileType::Directory : IItem::FileType::Unknown);
      received_item(item);
      result.push_back(item);
    }
  }
  done(result);
}

}  // namespace

LocalDrive::LocalDrive() : CloudProvider(util::make_unique<Auth>()) {}

std::string LocalDrive::name() const { return "local"; }

std::string LocalDrive::endpoint() const { return ""; }

std::string LocalDrive::token() const {
  auto lock = auth_lock();
  Json::Value json;
  json["path"] = path_;
  return credentialsToString(json);
}

void LocalDrive::initialize(ICloudProvider::InitData &&data) {
  if (data.token_.empty())
    data.token_ = credentialsToString(Json::Value(Json::objectValue));
  unpackCredentials(data.token_);
  CloudProvider::initialize(std::move(data));
}

AuthorizeRequest::Pointer LocalDrive::authorizeAsync() {
  return std::make_shared<SimpleAuthorization>(shared_from_this());
}

LocalDrive::ListDirectoryPageRequest::Pointer
LocalDrive::listDirectoryPageAsync(IItem::Pointer item, const std::string &,
                                   ListDirectoryPageCallback callback) {
  return request<EitherError<PageData>>(
      [=](EitherError<PageData> e) { callback(e); },
      [=, this](Request<EitherError<PageData>>::Pointer r) {
        list_directory(*this, item, [=](EitherError<IItem::List> e) {
          if (e.left())
            r->done(e.left());
          else
            r->done(PageData{*e.right(), ""});
        });
      });
}

LocalDrive::GetItemUrlRequest::Pointer LocalDrive::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  return request<EitherError<std::string>>(
      [=](EitherError<std::string> e) { callback(e); },
      [=, this](Request<EitherError<std::string>>::Pointer r) {
#ifdef _WIN32
        auto p = path(item);
        for (auto &c : p)
          if (c == '\\') c = '/';
        r->done("file:///" + p);
#else
        r->done("file://" + path(item));
#endif
      });
}

LocalDrive::DownloadFileRequest::Pointer LocalDrive::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback,
    Range drange) {
  return request<EitherError<void>>(
      [=](EitherError<void> e) { callback->done(e); },
      [=, this](Request<EitherError<void>>::Pointer r) {
        fs::ifstream stream(from_string(path(item)), std::ios::binary);
        std::vector<char> buffer(BUFFER_SIZE);
        stream.seekg(0, std::ios::end);
        auto range =
            Range{drange.start_,
                  drange.size_ == Range::Full
                      ? static_cast<size_t>(stream.tellg()) - drange.start_
                      : drange.size_};
        stream.seekg(range.start_);
        size_t bytes_read = 0;
        while (bytes_read < range.size_) {
          if (r->is_cancelled())
            return r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
          while (r->is_paused()) {
            std::this_thread::sleep_for(POLL_INTERVAL);
          }
          if (!stream.read(
                  buffer.data(),
                  std::min<size_t>(BUFFER_SIZE, range.size_ - bytes_read)))
            return r->done(
                Error{IHttpRequest::Failure, util::Error::COULD_NOT_READ_FILE});
          callback->receivedData(buffer.data(),
                                 static_cast<uint32_t>(stream.gcount()));
          bytes_read += stream.gcount();
          callback->progress(range.size_, bytes_read);
        }
        r->done(nullptr);
      });
}

LocalDrive::UploadFileRequest::Pointer LocalDrive::uploadFileAsync(
    IItem::Pointer parent, const std::string &name,
    IUploadFileCallback::Pointer callback) {
  return request<EitherError<IItem>>(
      [=](EitherError<IItem> e) { callback->done(e); },
      [=, this](Request<EitherError<IItem>>::Pointer r) {
        auto path = from_string(this->path(parent)) / name;
        size_t bytes_read = 0, size = callback->size();
        std::vector<char> buffer(BUFFER_SIZE);
        fs::ofstream stream(path, std::ios::binary);
        while (bytes_read < size) {
          if (r->is_cancelled())
            return r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
          while (r->is_paused()) {
            std::this_thread::sleep_for(POLL_INTERVAL);
          }
          auto cnt = callback->putData(buffer.data(), BUFFER_SIZE, bytes_read);
          bytes_read += cnt;
          if (!stream.write(buffer.data(), cnt))
            return r->done(Error{IHttpRequest::Failure, "couldn't write file"});
          callback->progress(size, bytes_read);
        }
        r->done(std::static_pointer_cast<IItem>(std::make_shared<Item>(
            name, to_string(path), size, std::chrono::system_clock::now(),
            IItem::FileType::Unknown)));
      });
}

LocalDrive::DeleteItemRequest::Pointer LocalDrive::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  return request<EitherError<void>>(
      [=](EitherError<void> e) { callback(e); },
      [=, this](Request<EitherError<void>>::Pointer r) {
        error_code error;
        fs::remove_all(path(item), error);
        if (error)
          r->done(Error{error.value(), error.message()});
        else
          r->done(nullptr);
      });
}

LocalDrive::CreateDirectoryRequest::Pointer LocalDrive::createDirectoryAsync(
    IItem::Pointer parent, const std::string &name,
    CreateDirectoryCallback callback) {
  return request<EitherError<IItem>>(
      [=](EitherError<IItem> e) { callback(e); },
      [=, this](Request<EitherError<IItem>>::Pointer r) {
        error_code error;
        auto path = fs::path(this->path(parent)) / name;
        fs::create_directory(path, error);
        if (error)
          r->done(Error{error.value(), error.message()});
        else
          r->done(std::static_pointer_cast<IItem>(std::make_shared<Item>(
              name, to_string(path), IItem::UnknownSize,
              std::chrono::system_clock::now(), IItem::FileType::Directory)));
      });
}

LocalDrive::MoveItemRequest::Pointer LocalDrive::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  return request<EitherError<IItem>>(
      [=](EitherError<IItem> e) { callback(e); },
      [=, this](Request<EitherError<IItem>>::Pointer r) {
        fs::path path(this->path(source));
        fs::path new_path(fs::path(this->path(destination)) / path.filename());
        error_code error;
        fs::rename(path, new_path, error);
        if (error)
          r->done(Error{error.value(), error.message()});
        else
          r->done(std::static_pointer_cast<IItem>(std::make_shared<Item>(
              source->filename(), to_string(new_path), source->size(),
              source->timestamp(), source->type())));
      });
}

LocalDrive::RenameItemRequest::Pointer LocalDrive::renameItemAsync(
    IItem::Pointer item, const std::string &name, RenameItemCallback callback) {
  return request<EitherError<IItem>>(
      [=](EitherError<IItem> e) { callback(e); },
      [=, this](Request<EitherError<IItem>>::Pointer r) {
        fs::path path(this->path(item));
        fs::path new_path(path.parent_path() / name);
        error_code error;
        fs::rename(path, new_path, error);
        if (error)
          r->done(Error{error.value(), error.message()});
        else
          r->done(std::static_pointer_cast<IItem>(
              std::make_shared<Item>(name, to_string(new_path), item->size(),
                                     item->timestamp(), item->type())));
      });
}

LocalDrive::GetItemDataRequest::Pointer LocalDrive::getItemDataAsync(
    const std::string &id, GetItemDataCallback callback) {
  return request<EitherError<IItem>>(
      [=](EitherError<IItem> e) { callback(e); },
      [=](Request<EitherError<IItem>>::Pointer r) {
        fs::path path(from_string(id));
        error_code error;
        fs::status(path, error);
        if (error) return r->done(Error{error.value(), error.message()});
        auto size = fs::file_size(path, error);
        if (error) size = IItem::UnknownSize;
        auto timestamp = std::chrono::system_clock::from_time_t(
            fs::last_write_time(path, error));
        if (error) timestamp = IItem::UnknownTimeStamp;
        bool directory = fs::is_directory(path, error);
        if (error) directory = false;
        r->done(std::static_pointer_cast<IItem>(std::make_shared<Item>(
            to_string(path.filename()), to_string(path), size, timestamp,
            directory ? IItem::FileType::Directory
                      : IItem::FileType::Unknown)));
      });
}

LocalDrive::GeneralDataRequest::Pointer LocalDrive::getGeneralDataAsync(
    GeneralDataCallback callback) {
  return request<EitherError<GeneralData>>(
      [=](EitherError<GeneralData> e) { callback(e); },
      [=, this](Request<EitherError<GeneralData>>::Pointer r) {
        fs::path path(this->path());
        error_code error;
        auto space = fs::space(path, error);
        if (error) return r->done(Error{error.value(), error.message()});
        GeneralData data;
        data.space_used_ = space.capacity - space.available;
        data.space_total_ = space.capacity;
        data.username_ = this->path();
        r->done(data);
      });
}

LocalDrive::GetItemUrlRequest::Pointer LocalDrive::getFileDaemonUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  return getItemUrlAsync(item, callback);
}

bool LocalDrive::unpackCredentials(const std::string &code) {
  auto lock = auth_lock();
  try {
    auto json = credentialsFromString(code);
    path_ = json["path"].asString();
    return true;
  } catch (const Json::Exception &) {
    return false;
  }
}

std::string LocalDrive::path() const {
  auto lock = auth_lock();
  return path_;
}

std::string LocalDrive::path(IItem::Pointer item) const {
  return item->id() == rootDirectory()->id() ? path_ : item->id();
}

std::string LocalDrive::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state() +
         "&hint=" + util::Url::escape(util::home_directory());
}

template <class T>
void LocalDrive::ensureInitialized(typename Request<T>::Pointer r,
                                   std::function<void()> on_success) {
  auto f = [=, this](EitherError<void> e) {
    if (e.left())
      r->done(e.left());
    else
      thread_pool()->schedule(on_success);
  };
  if (path().empty())
    r->reauthorize(f);
  else
    f(nullptr);
}

template <class ReturnValue>
typename Request<ReturnValue>::Wrapper::Pointer LocalDrive::request(
    typename Request<ReturnValue>::Callback callback,
    typename Request<ReturnValue>::Resolver resolver) {
  return std::make_shared<Request<ReturnValue>>(
             shared_from_this(), callback,
             [=, this](typename Request<ReturnValue>::Pointer r) {
               ensureInitialized<ReturnValue>(r, [=]() { resolver(r); });
             })
      ->run();
}

}  // namespace cloudstorage

#endif  // WITH_LOCALDRIVE
