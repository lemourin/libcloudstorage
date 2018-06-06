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

using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;
using namespace Platform;
using namespace concurrency;

#define ADD_FOLDER(vector, folder)  \
  try {                             \
    vector->Append(folder);         \
  } catch (Platform::Exception ^) { \
  }

namespace cloudstorage {

const uint64_t BUFFER_SIZE = 1024;

namespace {
Vector<StorageFolder ^> ^ root_directory() {
  auto vector = ref new Vector<StorageFolder ^>();
  ADD_FOLDER(vector, KnownFolders::AppCaptures);
  ADD_FOLDER(vector, KnownFolders::CameraRoll);
  ADD_FOLDER(vector, KnownFolders::DocumentsLibrary);
  ADD_FOLDER(vector, KnownFolders::HomeGroup);
  ADD_FOLDER(vector, KnownFolders::MediaServerDevices);
  ADD_FOLDER(vector, KnownFolders::MusicLibrary);
  ADD_FOLDER(vector, KnownFolders::Objects3D);
  ADD_FOLDER(vector, KnownFolders::PicturesLibrary);
  ADD_FOLDER(vector, KnownFolders::Playlists);
  ADD_FOLDER(vector, KnownFolders::RecordedCalls);
  ADD_FOLDER(vector, KnownFolders::RemovableDevices);
  ADD_FOLDER(vector, KnownFolders::SavedPictures);
  ADD_FOLDER(vector, KnownFolders::VideosLibrary);
  return vector;
}

}  // namespace

std::string to_string(const std::wstring &str) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(str);
}

std::wstring from_string(const std::string &str) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
}

std::string get_first_part(const std::string &path) {
  auto idx = path.find_first_of('/', 1);
  return path.substr(1, idx - 1);
}

std::string get_rest(const std::string &path) {
  auto idx = path.find_first_of('/', 1);
  return path.substr(idx);
}

std::string parent_path(const std::string &path) {
  if (path.empty()) return "";
  if (path.back() == '/') {
    auto d = std::string(path.begin(), path.begin() + path.size() - 1);
    return d.substr(0, d.find_last_of('/') + 1);
  } else {
    return path.substr(0, path.find_last_of('/') + 1);
  }
}

void get_directory(const std::string &path, StorageFolder ^ current,
                   std::function<void(StorageFolder ^)> callback) {
  if (path == "/") return callback(current);
  create_task(current->GetFoldersAsync())
      .then([=](IVectorView<StorageFolder ^> ^ folders) {
        auto fragment = get_first_part(path);
        for (auto i = 0u; i < folders->Size; i++) {
          auto f = folders->GetAt(i);
          if (to_string(f->Name->Data()) == fragment) {
            return get_directory(get_rest(path), f, callback);
          }
        }
        callback(nullptr);
      });
}

void get_directory(const std::string &path,
                   std::function<void(StorageFolder ^)> callback) {
  auto root = root_directory();
  auto fragment = get_first_part(path);
  for (auto i = 0u; i < root->Size; i++) {
    auto f = root->GetAt(i);
    if (to_string(f->Name->Data()) == fragment) {
      return get_directory(get_rest(path), f, callback);
    }
  }
  callback(nullptr);
}

void get_file(const std::string &path,
              std::function<void(StorageFile ^)> callback) {
  auto last = path.find_last_of('/');
  auto directory_path = path.substr(0, last + 1);
  auto filename = path.substr(last + 1);
  get_directory(directory_path, [=](StorageFolder ^ folder) {
    if (folder == nullptr) return callback(nullptr);
    create_task(folder->GetFilesAsync())
        .then([=](IVectorView<StorageFile ^> ^ files) {
          for (auto i = 0u; i < files->Size; i++) {
            auto f = files->GetAt(i);
            if (to_string(f->Name->Data()) == filename) return callback(f);
          }
          callback(nullptr);
        });
  });
}

void get_file_list(LocalDriveWinRT *p, const std::string &path,
                   IVectorView<StorageFile ^> ^ files, int idx,
                   std::shared_ptr<PageData> result,
                   std::function<void(std::shared_ptr<PageData>)> callback) {
  if (idx >= files->Size) {
    callback(result);
  } else {
    auto file = files->GetAt(idx);
    create_task(file->GetBasicPropertiesAsync())
        .then([=](FileProperties::BasicProperties ^ props) {
          result->items_.push_back(p->toItem(file, path, props->Size));
          get_file_list(p, path, files, idx + 1, result, callback);
        });
  }
}

void read_file(Request<EitherError<void>>::Pointer r, DataReader ^ data_reader,
               uint64_t size, uint64_t total_size,
               IDownloadFileCallback::Pointer callback) {
  if (size == 0) return r->done(nullptr);
  if (r->is_cancelled())
    return r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
  auto chunk_size = std::min<uint64_t>(size, BUFFER_SIZE);
  create_task(data_reader->LoadAsync(chunk_size))
      .then([=](unsigned int byte_count) {
        auto bytes = ref new Array<byte>(byte_count);
        data_reader->ReadBytes(bytes);
        std::vector<char> buffer(byte_count);
        for (auto i = 0u; i < bytes->Length; i++) buffer[i] = bytes[i];
        callback->receivedData(buffer.data(), buffer.size());
        callback->progress(total_size - (size - byte_count), total_size);
        read_file(r, data_reader, size - byte_count, total_size, callback);
      });
}

void write_file(Request<EitherError<IItem>>::Pointer r, const std::string &path,
                StorageFile ^ file, IOutputStream ^ output_stream,
                DataWriter ^ data_writer, uint64_t size, uint64_t total_size,
                IUploadFileCallback::Pointer callback) {
  if (size == 0) {
    create_task(output_stream->FlushAsync()).then([=](task<bool> d) {
      try {
        d.get();
        r->done(static_cast<LocalDriveWinRT *>(r->provider().get())
                    ->toItem(file, path, callback->size()));
      } catch (Exception ^ e) {
        r->done(Error{IHttpRequest::Failure, to_string(e->Message->Data())});
      }
    });
    return;
  }
  if (r->is_cancelled())
    return r->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
  auto chunk_size = std::min<uint64_t>(size, BUFFER_SIZE);
  std::vector<char> buffer(chunk_size);
  auto current_chunk =
      callback->putData(buffer.data(), chunk_size, total_size - size);
  auto bytes = ref new Array<byte>(current_chunk);
  for (auto i = 0u; i < current_chunk; i++) bytes[i] = buffer[i];
  data_writer->WriteBytes(bytes);
  create_task(data_writer->StoreAsync()).then([=](unsigned int) {
    callback->progress(total_size - (size - current_chunk), total_size);
    write_file(r, path, file, output_stream, data_writer, size - current_chunk,
               total_size, callback);
  });
}

LocalDriveWinRT::LocalDriveWinRT() : CloudProvider(util::make_unique<Auth>()) {}

std::string LocalDriveWinRT::name() const { return "localwinrt"; }

std::string LocalDriveWinRT::endpoint() const { return ""; }

IItem::Pointer LocalDriveWinRT::rootDirectory() const {
  return util::make_unique<Item>("/", "/", IItem::UnknownSize,
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
      [=](Request<EitherError<PageData>>::Pointer r) {
        if (item->id() == rootDirectory()->id()) {
          PageData page;
          auto vector = root_directory();
          for (auto i = 0u; i < vector->Size; i++) {
            auto f = vector->GetAt(i);
            page.items_.push_back(toItem(f, item->id()));
          }
          r->done(page);
        } else {
          get_directory(item->id(), [=](StorageFolder ^ folder) {
            if (folder == nullptr)
              return r->done(
                  Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
            auto result = std::make_shared<PageData>();
            create_task(folder->GetFoldersAsync())
                .then([=](IVectorView<StorageFolder ^> ^ folders) {
                  for (auto i = 0u; i < folders->Size; i++) {
                    auto folder = folders->GetAt(i);
                    result->items_.push_back(toItem(folder, item->id()));
                  }
                  return folder->GetFilesAsync();
                })
                .then([=](IVectorView<StorageFile ^> ^ files) {
                  get_file_list(
                      this, item->id(), files, 0, result,
                      [=](std::shared_ptr<PageData> result) {
                        IItem::List filtered;
                        std::unordered_set<std::string> present;
                        for (auto &&d : result->items_) {
                          if (present.find(d->id()) == present.end()) {
                            present.insert(d->id());
                            filtered.push_back(d);
                          }
                        }
                        result->items_ = filtered;
                        r->done(*result);
                      });
                });
          });
        }
      });
  return request->run();
}

ICloudProvider::GetItemDataRequest::Pointer LocalDriveWinRT::getItemDataAsync(
    const std::string &id, GetItemDataCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) {
        if (id.empty())
          return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        if (rootDirectory()->id() == id) {
          r->done(rootDirectory());
        } else {
          if (id.back() == '/')
            get_directory(id, [=](StorageFolder ^ folder) {
              if (!folder)
                r->done(
                    Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
              else {
                r->done(toItem(folder, parent_path(id)));
              }
            });
          else {
            get_file(id, [=](StorageFile ^ file) {
              if (!file)
                r->done(
                    Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
              else {
                create_task(file->GetBasicPropertiesAsync())
                    .then([=](FileProperties::BasicProperties ^ props) {
                      r->done(toItem(file, parent_path(id), props->Size));
                    });
              }
            });
          }
        }
      });
  return request->run();
}

ICloudProvider::DownloadFileRequest::Pointer LocalDriveWinRT::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback, Range range) {
  if (range.size_ == Range::Full) range.size_ = item->size() - range.start_;
  auto request = std::make_shared<Request<EitherError<void>>>(
      shared_from_this(), [=](EitherError<void> e) { callback->done(e); },
      [=](Request<EitherError<void>>::Pointer r) {
        get_file(item->id(), [=](StorageFile ^ file) {
          if (file == nullptr)
            return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          create_task(file->OpenAsync(FileAccessMode::Read))
              .then([=](IRandomAccessStream ^ stream) {
                auto input = stream->GetInputStreamAt(range.start_);
                auto data_reader = ref new DataReader(input);
                read_file(r, data_reader, range.size_, range.size_, callback);
              });
        });
      });
  return request->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer
LocalDriveWinRT::createDirectoryAsync(IItem::Pointer parent,
                                      const std::string &name,
                                      CreateDirectoryCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) {
        get_directory(parent->id(), [=](StorageFolder ^ folder) {
          if (folder == nullptr)
            return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          create_task(folder->CreateFolderAsync(
                          ref new String(from_string(name).data()),
                          CreationCollisionOption::FailIfExists))
              .then([=](task<StorageFolder ^> folder) {
                try {
                  r->done(toItem(folder.get(), parent->id()));
                } catch (Platform::Exception ^ e) {
                  r->done(Error{IHttpRequest::Failure,
                                to_string(e->Message->Data())});
                }
              });
        });
      });
  return request->run();
}

ICloudProvider::DeleteItemRequest::Pointer LocalDriveWinRT::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<void>>>(
      shared_from_this(), callback, [=](Request<EitherError<void>>::Pointer r) {
        if (item->id().empty())
          return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        if (item->id().back() == '/') {
          get_directory(item->id(), [=](StorageFolder ^ folder) {
            if (folder == nullptr)
              return r->done(
                  Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
            create_task(
                folder->DeleteAsync(StorageDeleteOption::PermanentDelete))
                .then([=] { r->done(nullptr); });
          });
        } else {
          get_file(item->id(), [=](StorageFile ^ file) {
            if (file == nullptr)
              return r->done(
                  Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
            create_task(file->DeleteAsync(StorageDeleteOption::PermanentDelete))
                .then([=] { r->done(nullptr); });
          });
        }
      });
  return request->run();
}

ICloudProvider::RenameItemRequest::Pointer LocalDriveWinRT::renameItemAsync(
    IItem::Pointer item, const std::string &name, RenameItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) {
        if (item->id().empty())
          return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        if (item->id().back() == '/') {
          get_directory(item->id(), [=](StorageFolder ^ folder) {
            if (folder == nullptr)
              return r->done(
                  Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
            create_task(
                folder->RenameAsync(ref new String(from_string(name).data())))
                .then([=](task<void> d) {
                  try {
                    d.get();
                    r->done(toItem(folder, parent_path(item->id())));
                  } catch (Exception ^ e) {
                    r->done(Error{IHttpRequest::Failure,
                                  to_string(e->Message->Data())});
                  }
                });
          });
        } else {
          get_file(item->id(), [=](StorageFile ^ file) {
            if (file == nullptr)
              return r->done(
                  Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
            create_task(
                file->RenameAsync(ref new String(from_string(name).data())))
                .then([=](task<void> d) {
                  try {
                    d.get();
                    r->done(
                        toItem(file, parent_path(item->id()), item->size()));
                  } catch (Exception ^ e) {
                    r->done(Error{IHttpRequest::Failure,
                                  to_string(e->Message->Data())});
                  }
                });
          });
        }
      });
  return request->run();
}

ICloudProvider::MoveItemRequest::Pointer LocalDriveWinRT::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto request = std::make_shared<Request<EitherError<IItem>>>(
      shared_from_this(), callback,
      [=](Request<EitherError<IItem>>::Pointer r) {
        get_directory(destination->id(), [=](StorageFolder ^
                                             destination_folder) {
          if (destination_folder == nullptr)
            return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          if (source->id().empty())
            return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          if (source->id().back() == '/') {
            r->done(Error{IHttpRequest::Failure, util::Error::UNIMPLEMENTED});
          } else {
            get_file(source->id(), [=](StorageFile ^ file) {
              if (file == nullptr)
                return r->done(
                    Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
              create_task(
                  file->MoveAsync(
                      destination_folder,
                      ref new String(from_string(source->filename()).data()),
                      NameCollisionOption::ReplaceExisting))
                  .then([=](task<void> d) {
                    try {
                      d.get();
                      r->done(toItem(file, parent_path(source->id()),
                                     source->size()));
                    } catch (Exception ^ e) {
                      r->done(Error{IHttpRequest::Failure,
                                    to_string(e->Message->Data())});
                    }
                  });
            });
          }
        });
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
      [=](Request<EitherError<IItem>>::Pointer r) {
        get_directory(parent->id(), [=](StorageFolder ^ folder) {
          if (folder == nullptr)
            return r->done(
                Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
          create_task(
              folder->CreateFileAsync(ref new String(from_string(name).data())))
              .then([=](task<StorageFile ^> d) {
                try {
                  create_task(d.get()->OpenAsync(FileAccessMode::ReadWrite))
                      .then([=](IRandomAccessStream ^ stream) {
                        auto output = stream->GetOutputStreamAt(0);
                        auto size = callback->size();
                        write_file(r, parent->id(), d.get(), output,
                                   ref new DataWriter(output), size, size,
                                   callback);
                      });
                } catch (Exception ^ e) {
                  r->done(Error{IHttpRequest::Failure,
                                to_string(e->Message->Data())});
                }
              });
        });
      });
  return request->run();
}

IItem::Pointer LocalDriveWinRT::toItem(Windows::Storage::StorageFolder ^ f,
                                       const std::string &path) const {
  return util::make_unique<Item>(
      to_string(f->Name->Data()), path + to_string(f->Name->Data()) + "/",
      IItem::UnknownSize,
      std::chrono::system_clock::time_point(std::chrono::seconds(
          f->DateCreated.UniversalTime / 10000000 - 11644473600LL)),
      IItem::FileType::Directory);
}

IItem::Pointer LocalDriveWinRT::toItem(Windows::Storage::StorageFile ^ f,
                                       const std::string &path,
                                       uint64_t size) const {
  return util::make_unique<Item>(
      to_string(f->Name->Data()), path + to_string(f->Name->Data()), size,
      std::chrono::system_clock::time_point(std::chrono::seconds(
          f->DateCreated.UniversalTime / 10000000 - 11644473600LL)),
      IItem::FileType::Unknown);
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
