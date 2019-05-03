/*****************************************************************************
 * LocalDriveWinRT.cpp
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#include "LocalDriveWinRT.h"

#ifdef WINRT

#include "Request/Request.h"

#include <codecvt>
#include <locale>
#include <memory>
#include <string>

#include <winrt/base.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace cloudstorage {

const uint64_t BUFFER_SIZE = 1024;

namespace {

#define ADD_FOLDER(vector, folder)         \
  try {                                    \
    vector.push_back(folder());            \
  } catch (const winrt::hresult_error &) { \
  }

IAsyncOperation<IVector<IStorageItem>> root_directory() {
  static std::vector<IStorageItem> vector;
  static std::atomic_bool initialized = false;
  if (!initialized.exchange(true)) {
    ADD_FOLDER(vector, KnownFolders::AppCaptures);
    ADD_FOLDER(vector, KnownFolders::CameraRoll);
    ADD_FOLDER(vector, KnownFolders::DocumentsLibrary);
    ADD_FOLDER(vector, KnownFolders::HomeGroup);
    ADD_FOLDER(vector, KnownFolders::MediaServerDevices);
    ADD_FOLDER(vector, KnownFolders::MusicLibrary);
    ADD_FOLDER(vector, KnownFolders::Objects3D);
    ADD_FOLDER(vector, KnownFolders::PicturesLibrary);
    ADD_FOLDER(vector, KnownFolders::SavedPictures);
    ADD_FOLDER(vector, KnownFolders::VideosLibrary);
  }
  auto result = winrt::single_threaded_vector<IStorageItem>();
  for (const auto &v : vector) result.Append(v);
  co_return result;
}

std::string get_first_part(const std::string &path) {
  auto idx = path.find_first_of('\\', 1);
  return path.substr(1, idx - 1);
}

std::string get_rest(const std::string &path) {
  auto idx = path.find_first_of('\\', 1);
  return idx == std::string::npos ? "" : path.substr(idx);
}

std::string parent_path(const std::string &path) {
  if (path.empty()) return "";
  if (path.back() == '\\') {
    auto d = std::string(path.begin(), path.begin() + path.size() - 1);
    return d.substr(0, d.find_last_of('\\') + 1);
  } else {
    return path.substr(0, path.find_last_of('\\') + 1);
  }
}

IItem::Pointer to_item(const StorageFolder &f, const std::string &path) {
  return util::make_unique<Item>(
      to_string(f.Name()), path + to_string(f.Name()) + "\\",
      IItem::UnknownSize,
      std::chrono::system_clock::time_point(std::chrono::seconds(
          f.DateCreated().time_since_epoch().count() / 10000000 -
          11644473600LL)),
      IItem::FileType::Directory);
}

IItem::Pointer to_item(const StorageFile &f, const std::string &path,
                       uint64_t size) {
  return util::make_unique<Item>(
      to_string(f.Name()), path + to_string(f.Name()), size,
      std::chrono::system_clock::time_point(std::chrono::seconds(
          f.DateCreated().time_since_epoch().count() / 10000000 -
          11644473600LL)),
      IItem::FileType::Unknown);
}

IAsyncOperation<IStorageItem> get_item(const std::string &path) {
  auto current = path;
  auto list = co_await root_directory();
  while (true) {
    auto first = winrt::to_hstring(get_first_part(current));
    auto it = std::find_if(begin(list), end(list), [first](const auto &d) {
      return d.Name() == first;
    });
    if (it == end(list)) {
      winrt::throw_hresult(TYPE_E_ELEMENTNOTFOUND);
    }
    auto current_item = *it;
    current = get_rest(current);
    if (current.empty() || current == "\\") {
      co_return current_item;
    }
    if (current_item.IsOfType(StorageItemTypes::Folder)) {
      auto new_list = co_await current_item.as<StorageFolder>().GetItemsAsync();
      list.Clear();
      for (auto &&d : new_list) {
        list.Append(std::move(d));
      }
    }
  }
}

IAsyncOperation<StorageFolder> get_folder(const std::string &path) {
  auto item = co_await get_item(path);
  return item.as<StorageFolder>();
}

IAsyncOperation<StorageFile> get_file(const std::string &path) {
  auto item = co_await get_item(path);
  return item.as<StorageFile>();
}

}  // namespace

LocalDriveWinRT::LocalDriveWinRT() : CloudProvider(util::make_unique<Auth>()) {}

std::string LocalDriveWinRT::name() const { return "localwinrt"; }

std::string LocalDriveWinRT::endpoint() const { return ""; }

IItem::Pointer LocalDriveWinRT::rootDirectory() const {
  return util::make_unique<Item>("root", "\\", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

AuthorizeRequest::Pointer LocalDriveWinRT::authorizeAsync() {
  return std::make_shared<SimpleAuthorization>(shared_from_this());
}

bool LocalDriveWinRT::unpackCredentials(const std::string &) { return true; }

ICloudProvider::GeneralDataRequest::Pointer
LocalDriveWinRT::getGeneralDataAsync(GeneralDataCallback callback) {
  auto request = std::make_shared<Request<EitherError<GeneralData>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<GeneralData>>::Pointer r) {
        GeneralData data = {};
        r->done(data);
      });
  return request->run();
}

ICloudProvider::ListDirectoryPageRequest::Pointer
LocalDriveWinRT::listDirectoryPageAsync(IItem::Pointer item,
                                        const std::string &,
                                        ListDirectoryPageCallback callback) {
  auto request = std::make_shared<Request<EitherError<PageData>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<PageData>>::Pointer r) -> IAsyncAction {
        try {
          auto current_path = item->id();
          if (current_path == rootDirectory()->id()) {
            PageData page;
            auto root = co_await root_directory();
            for (const auto &d : root)
              page.items_.push_back(
                  to_item(d.as<StorageFolder>(), current_path));
            co_return r->done(page);
          }
          auto folder = co_await get_folder(current_path);
          auto items = co_await folder.GetItemsAsync();
          auto result = std::make_shared<PageData>();
          for (const auto &d : items) {
            if (d.IsOfType(StorageItemTypes::File)) {
              auto file = d.as<StorageFile>();
              auto props = co_await file.GetBasicPropertiesAsync();
              result->items_.push_back(
                  to_item(file, current_path, props.Size()));
            } else if (d.IsOfType(StorageItemTypes::Folder)) {
              auto folder = d.as<StorageFolder>();
              result->items_.push_back(to_item(folder, current_path));
            }
          }
          r->done(result);
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, to_string(e.message())});
        } catch (const std::exception &e) {
          r->done(Error{IHttpRequest::Failure, e.what()});
        }
      });
  return request->run();
}

ICloudProvider::GetItemDataRequest::Pointer LocalDriveWinRT::getItemDataAsync(
    const std::string &id, GetItemDataCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) -> IAsyncAction {
        if (id.empty())
          co_return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        if (rootDirectory()->id() == id) {
          co_return r->done(rootDirectory());
        }
        try {
          auto path = id;
          if (path.back() == '\\') {
            auto directory = co_await get_folder(path);
            r->done(to_item(directory, parent_path(path)));
          } else {
            auto file = co_await get_file(path);
            auto props = co_await file.GetBasicPropertiesAsync();
            r->done(to_item(file, parent_path(path), props.Size()));
          }
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::DownloadFileRequest::Pointer LocalDriveWinRT::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback, Range range) {
  if (range.size_ == Range::Full) range.size_ = item->size() - range.start_;
  auto request = std::make_shared<Request<EitherError<void>>>(
      shared_from_this(), [=](EitherError<void> e) { callback->done(e); },
      [=](Request<EitherError<void>>::Pointer r) -> IAsyncAction {
        try {
          auto cb = callback;
          auto start = range.start_;
          auto size = range.size_;
          auto path = item->id();
          auto file = co_await get_file(path);
          auto stream = co_await file.OpenReadAsync();
          auto input = stream.GetInputStreamAt(start);
          auto data_reader = Windows::Storage::Streams::DataReader(input);
          uint64_t downloaded = 0;
          while (downloaded < size && !r->is_cancelled()) {
            auto chunk_size =
                std::min<uint64_t>(size - downloaded, BUFFER_SIZE);
            auto bytes = co_await data_reader.LoadAsync(
                static_cast<uint32_t>(chunk_size));
            std::vector<uint8_t> data(bytes);
            data_reader.ReadBytes(data);
            cb->receivedData(reinterpret_cast<char *>(data.data()),
                             static_cast<uint32_t>(data.size()));
            cb->progress(size, downloaded += bytes);
          }
          if (r->is_cancelled())
            r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
          else
            r->done(nullptr);
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer
LocalDriveWinRT::createDirectoryAsync(IItem::Pointer parent,
                                      const std::string &name,
                                      CreateDirectoryCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) -> IAsyncAction {
        try {
          auto path = parent->id();
          auto filename = winrt::to_hstring(name);
          auto parent_folder = co_await get_folder(path);
          auto folder = co_await parent_folder.CreateFolderAsync(
              filename, CreationCollisionOption::FailIfExists);
          r->done(to_item(folder, path));
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::DeleteItemRequest::Pointer LocalDriveWinRT::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<void>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<void>>::Pointer r) -> IAsyncAction {
        if (item->id().empty())
          co_return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        try {
          auto path = item->id();
          if (path.back() == '\\') {
            auto directory = co_await get_folder(path);
            co_await directory.DeleteAsync(
                StorageDeleteOption::PermanentDelete);
            r->done(nullptr);
          } else {
            auto file = co_await get_file(path);
            co_await file.DeleteAsync(StorageDeleteOption::PermanentDelete);
            r->done(nullptr);
          }
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::RenameItemRequest::Pointer LocalDriveWinRT::renameItemAsync(
    IItem::Pointer item, const std::string &name, RenameItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) -> IAsyncAction {
        if (item->id().empty())
          co_return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        try {
          auto path = item->id();
          auto new_name = winrt::to_hstring(name);
          if (path.back() == '\\') {
            auto directory = co_await get_folder(path);
            co_await directory.RenameAsync(new_name);
            r->done(to_item(directory, parent_path(path)));
          } else {
            auto size = item->size();
            auto file = co_await get_file(path);
            co_await file.RenameAsync(new_name);
            r->done(to_item(file, parent_path(path), size));
          }
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::MoveItemRequest::Pointer LocalDriveWinRT::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) -> IAsyncAction {
        try {
          auto destination_path = destination->id();
          auto source_path = source->id();
          auto source_filename = winrt::to_hstring(source->filename());
          auto source_size = source->size();
          auto destination_folder = co_await get_folder(destination_path);
          if (source_path.empty())
            co_return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          if (source_path.back() == '\\') {
            r->done(Error{IHttpRequest::Failure, util::Error::UNIMPLEMENTED});
          } else {
            auto source_file = co_await get_file(source_path);
            co_await source_file.MoveAsync(
                destination_folder, source_filename,
                NameCollisionOption::ReplaceExisting);
            r->done(to_item(source_file, destination_path, source_size));
          }
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

ICloudProvider::GetItemUrlRequest::Pointer LocalDriveWinRT::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  return getFileDaemonUrlAsync(item, callback);
}

ICloudProvider::UploadFileRequest::Pointer LocalDriveWinRT::uploadFileAsync(
    IItem::Pointer parent, const std::string &name,
    IUploadFileCallback::Pointer callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), [=](EitherError<IItem> e) { callback->done(e); },
      [=](Request<EitherError<IItem>>::Pointer r) -> IAsyncAction {
        auto cb = callback;
        auto path = parent->id();
        auto filename = winrt::to_hstring(name);
        try {
          auto directory = co_await get_folder(path);
          auto file = co_await directory.CreateFileAsync(filename);
          try {
            auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
            auto output = stream.GetOutputStreamAt(0);
            auto writer = Windows::Storage::Streams::DataWriter(output);
            uint64_t uploaded = 0;
            auto size = cb->size();
            while (uploaded < size && !r->is_cancelled()) {
              auto chunk_size =
                  std::min<uint64_t>(size - uploaded, BUFFER_SIZE);
              std::vector<uint8_t> buffer(chunk_size);
              auto current_chunk =
                  cb->putData(reinterpret_cast<char *>(buffer.data()),
                              static_cast<uint32_t>(buffer.size()), uploaded);
              writer.WriteBytes(winrt::array_view<const uint8_t>(
                  buffer.data(), buffer.data() + current_chunk));
              co_await writer.StoreAsync();
              cb->progress(size, uploaded += current_chunk);
            }
            co_await stream.FlushAsync();
            if (r->is_cancelled())
              r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
            else
              r->done(to_item(file, path, size));
          } catch (const winrt::hresult_error &) {
            [file]() -> IAsyncAction { co_await file.DeleteAsync(); }();
            throw;
          }
        } catch (const winrt::hresult_error &e) {
          r->done(Error{IHttpRequest::Failure, winrt::to_string(e.message())});
        }
      });
  return request->run();
}

std::string LocalDriveWinRT::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state();
}

IHttpRequest::Pointer LocalDriveWinRT::Auth::exchangeAuthorizationCodeRequest(
    std::ostream &) const {
  return nullptr;
}

IHttpRequest::Pointer LocalDriveWinRT::Auth::refreshTokenRequest(
    std::ostream &) const {
  return nullptr;
}

IAuth::Token::Pointer LocalDriveWinRT::Auth::exchangeAuthorizationCodeResponse(
    std::istream &) const {
  return nullptr;
}

IAuth::Token::Pointer LocalDriveWinRT::Auth::refreshTokenResponse(
    std::istream &) const {
  return nullptr;
}

}  // namespace cloudstorage

#endif
