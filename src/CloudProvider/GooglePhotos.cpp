/*****************************************************************************
 * GooglePhotos.cpp
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
#include "GooglePhotos.h"

#include <json/json.h>
#include "Request/DownloadFileRequest.h"
#include "Request/UploadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

#undef CreateDirectory

using namespace std::placeholders;

namespace cloudstorage {

const auto THUMBNAIL_SIZE = 128;
const auto ALBUMS_ID = "albums";
const auto ALBUMS_NAME = "Albums";
const auto PHOTOS_ID = "photos";
const auto PHOTOS_NAME = "Photos";
const auto SHARED_ID = "shared";
const auto SHARED_NAME = "Shared with me";

ICloudProvider::GeneralDataRequest::Pointer getGeneralDataUsingOpenId(
    CloudProvider *p, GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    r->request(
        [=](util::Output) {
          return p->http()->create(
              "https://accounts.google.com/.well-known/openid-configuration");
        },
        [=](EitherError<Response> d) {
          if (d.left()) return r->done(d.left());
          try {
            auto json = util::json::from_stream(d.right()->output());
            auto endpoint = json["userinfo_endpoint"].asString();
            r->request(
                [=](util::Output) { return p->http()->create(endpoint); },
                [=](EitherError<Response> d) {
                  if (d.left()) {
                    return r->done(d.left());
                  }
                  try {
                    auto json = util::json::from_stream(d.right()->output());
                    GeneralData result = {};
                    result.username_ = json["email"].asString();
                    r->done(result);
                  } catch (const Json::Exception &e) {
                    r->done(Error{IHttpRequest::Failure, e.what()});
                  }
                });
          } catch (const Json::Exception &e) {
            r->done(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  return std::make_shared<Request<EitherError<GeneralData>>>(
             p->shared_from_this(), callback, resolver)
      ->run();
}

GooglePhotos::GooglePhotos() : CloudProvider(util::make_unique<Auth>()) {}

std::string GooglePhotos::name() const { return "gphotos"; }

std::string GooglePhotos::endpoint() const {
  return "https://photoslibrary.googleapis.com/v1";
}

IItem::Pointer GooglePhotos::rootDirectory() const {
  return util::make_unique<Item>("/", "root", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

bool GooglePhotos::reauthorize(int code,
                               const IHttpRequest::HeaderParameters &h) const {
  return code == IHttpRequest::Forbidden || CloudProvider::reauthorize(code, h);
}

ICloudProvider::OperationSet GooglePhotos::supportedOperations() const {
  return GetItem | ListDirectory | UploadFile | ListDirectoryPage |
         DownloadFile | CreateDirectory;
}

IHttpRequest::Pointer GooglePhotos::getItemDataRequest(
    const std::string &id_str, std::ostream &) const {
  if (id_str == rootDirectory()->id() || id_str == ALBUMS_ID ||
      id_str == PHOTOS_ID)
    return nullptr;
  return http()->create(endpoint() + "/mediaItems/" + id_str);
}

IItem::Pointer GooglePhotos::getItemDataResponse(std::istream &response) const {
  return toItem(util::json::from_stream(response));
}

IHttpRequest::Pointer GooglePhotos::listDirectoryRequest(
    const IItem &directory, const std::string &page_token,
    std::ostream &stream) const {
  IHttpRequest::Pointer request;
  if (directory.id() == ALBUMS_ID) {
    request = http()->create(endpoint() + "/albums");
    if (!page_token.empty()) request->setParameter("pageToken", page_token);
  } else if (directory.id() == PHOTOS_ID) {
    request = http()->create(endpoint() + "/mediaItems");
    if (!page_token.empty()) request->setParameter("pageToken", page_token);
  } else if (directory.id() == SHARED_ID) {
    request = http()->create(endpoint() + "/sharedAlbums");
    if (!page_token.empty()) request->setParameter("pageToken", page_token);
  } else {
    request = http()->create(endpoint() + "/mediaItems:search", "POST");
    request->setHeaderParameter("Content-Type", "application/json");
    Json::Value argument;
    argument["albumId"] = directory.id();
    if (!page_token.empty()) argument["pageToken"] = page_token;
    stream << util::json::to_string(argument);
  }
  return request;
}

IHttpRequest::Pointer GooglePhotos::getItemUrlRequest(const IItem &item,
                                                      std::ostream &) const {
  return http()->create(endpoint() + "/mediaItems/" + item.id());
}

IItem::List GooglePhotos::listDirectoryResponse(
    const IItem &, std::istream &stream, std::string &next_page_token) const {
  const std::string entry_names[] = {"albums", "mediaItems", "sharedAlbums"};
  auto json = util::json::from_stream(stream);
  IItem::List result;
  for (const auto &name : entry_names) {
    if (json.isMember(name)) {
      for (const auto &d : json[name]) {
        if (d.isMember("title") || d.isMember("filename"))
          result.push_back(toItem(d));
      }
    }
  }
  if (json.isMember("nextPageToken"))
    next_page_token = json["nextPageToken"].asString();
  return result;
}

std::string GooglePhotos::getItemUrlResponse(
    const IItem &, const IHttpRequest::HeaderParameters &,
    std::istream &response) const {
  auto json = util::json::from_stream(response);
  return json["baseUrl"].asString() + "=" +
         (json["mediaMetadata"].isMember("video") ? "dv" : "d");
}

IHttpRequest::Pointer GooglePhotos::createDirectoryRequest(
    const IItem &item, const std::string &name, std::ostream &stream) const {
  if (item.id() != ALBUMS_ID) return nullptr;
  auto request = http()->create(endpoint() + "/albums", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value argument;
  argument["album"]["title"] = name;
  stream << util::json::to_string(argument);
  return request;
}

ICloudProvider::DownloadFileRequest::Pointer GooglePhotos::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer GooglePhotos::getGeneralDataAsync(
    GeneralDataCallback callback) {
  return getGeneralDataUsingOpenId(this, callback);
}

ICloudProvider::DownloadFileRequest::Pointer GooglePhotos::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  class Wrapper : public IDownloadFileCallback {
   public:
    Wrapper(IDownloadFileCallback::Pointer wrapped,
            std::function<void(EitherError<void>)> done)
        : wrapped_(wrapped), done_(done) {}
    void receivedData(const char *data, uint32_t length) override {
      wrapped_->receivedData(data, length);
    }
    void progress(uint64_t total, uint64_t now) override {
      wrapped_->progress(total, now);
    }
    void done(EitherError<void> e) override { done_(e); }

   private:
    IDownloadFileCallback::Pointer wrapped_;
    std::function<void(EitherError<void>)> done_;
  };
  auto resolver = [=](Request<EitherError<void>>::Pointer r) {
    r->request(
        [=](util::Output stream) {
          return getItemDataRequest(item->id(), *stream);
        },
        [=](EitherError<Response> d) {
          if (d.left()) return r->done(d.left());
          try {
            auto json = util::json::from_stream(d.right()->output());
            auto thumbnail_url = json["baseUrl"].asString() + "=w" +
                                 std::to_string(THUMBNAIL_SIZE) + "-h" +
                                 std::to_string(THUMBNAIL_SIZE) + "-c";
            r->make_subrequest<GooglePhotos>(
                &GooglePhotos::downloadFromUrl, item, thumbnail_url,
                std::make_shared<Wrapper>(
                    callback, [=](EitherError<void> e) { r->done(e); }));
          } catch (const std::exception &e) {
            r->done(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  return std::make_shared<Request<EitherError<void>>>(
             shared_from_this(),
             [=](EitherError<void> e) { callback->done(e); }, resolver)
      ->run();
}

ICloudProvider::ListDirectoryPageRequest::Pointer
GooglePhotos::listDirectoryPageAsync(IItem::Pointer item,
                                     const std::string &page_token,
                                     ListDirectoryPageCallback complete) {
  auto resolver = [=](Request<EitherError<PageData>>::Pointer r) {
    if (item->id() == rootDirectory()->id()) {
      PageData data;
      data.items_.push_back(util::make_unique<Item>(
          PHOTOS_NAME, PHOTOS_ID, IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory));
      data.items_.push_back(util::make_unique<Item>(
          ALBUMS_NAME, ALBUMS_ID, IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory));
      data.items_.push_back(util::make_unique<Item>(
          SHARED_NAME, SHARED_ID, IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory));
      return r->done(data);
    }
    try {
      r->request(
          [=](util::Output input) {
            return listDirectoryRequest(*item, page_token, *input);
          },
          [=](EitherError<Response> e) {
            if (e.left()) return r->done(e.left());
            PageData result;
            result.items_ = listDirectoryResponse(*item, e.right()->output(),
                                                  result.next_token_);
            r->done(result);
          });
    } catch (const Json::Exception &e) {
      r->done(Error{IHttpRequest::Failure, e.what()});
    }
  };
  return std::make_shared<Request<EitherError<PageData>>>(shared_from_this(),
                                                          complete, resolver)
      ->run();
}

ICloudProvider::UploadFileRequest::Pointer GooglePhotos::uploadFileAsync(
    IItem::Pointer directory, const std::string &filename,
    IUploadFileCallback::Pointer cb) {
  auto create_batch = [=](Request<EitherError<IItem>>::Pointer r,
                          const std::string &id) {
    r->request(
        [=](util::Output stream) {
          auto request =
              http()->create(endpoint() + "/mediaItems:batchCreate", "POST");
          request->setHeaderParameter("Content-Type", "application/json");
          Json::Value argument;
          if (directory->id() != PHOTOS_ID) {
            argument["albumId"] = directory->id();
          }
          Json::Value entry;
          entry["simpleMediaItem"]["uploadToken"] = id;
          Json::Value array;
          array.append(entry);
          argument["newMediaItems"] = array;
          *stream << util::json::to_string(argument);
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          try {
            auto json = util::json::from_stream(
                e.right()->output())["newMediaItemResults"][0];
            if (json["status"]["code"].asInt() != 0) {
              r->done(Error{IHttpRequest::Failure,
                            json["status"]["message"].asString()});
            } else
              r->done(toItem(json["mediaItem"]));
          } catch (const Json::Exception &e) {
            r->done(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  auto upload = [=](Request<EitherError<IItem>>::Pointer r,
                    const std::string &url) {
    auto wrapper = std::make_shared<UploadStreamWrapper>(
        std::bind(&IUploadFileCallback::putData, cb.get(), _1, _2, _3),
        cb->size());
    r->send(
        [=](util::Output) {
          auto request = http()->create(url, "POST");
          request->setHeaderParameter("X-Goog-Upload-Command",
                                      "upload, finalize");
          request->setHeaderParameter("X-Goog-Upload-Offset", "0");
          wrapper->reset();
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          std::string id;
          e.right()->output() >> id;
          create_batch(r, id);
        },
        [=] { return std::make_shared<std::iostream>(wrapper.get()); },
        std::make_shared<std::stringstream>(), nullptr,
        std::bind(&IUploadFileCallback::progress, cb.get(), _1, _2), true);
  };
  auto resolve = [=](Request<EitherError<IItem>>::Pointer r) {
    if (directory->id() == ALBUMS_ID ||
        directory->id() == rootDirectory()->id()) {
      return r->done(Error{IHttpRequest::ServiceUnavailable,
                           util::Error::COULD_NOT_UPLOAD_HERE});
    }
    r->make_subrequest<GooglePhotos>(&GooglePhotos::getUploadUrl, directory,
                                     filename, cb->size(),
                                     [=](EitherError<std::string> e) {
                                       if (e.left()) return r->done(e.left());
                                       upload(r, *e.right());
                                     });
  };
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), [=](EitherError<IItem> e) { cb->done(e); },
             resolve)
      ->run();
}

IItem::Pointer GooglePhotos::toItem(const Json::Value &json) const {
  if (json.isMember("title")) {
    return util::make_unique<Item>(
        json["title"].asString(), json["id"].asString(), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory);
  } else {
    auto filename = json["filename"].asString();
    if (json["mediaMetadata"].isMember("video") &&
        json["mediaMetadata"]["video"]["status"] != "READY") {
      filename = "[" + json["mediaMetadata"]["video"]["status"].asString() +
                 "] " + filename;
    }
    return util::make_unique<Item>(
        filename, json["id"].asString(), IItem::UnknownSize,
        util::parse_time(json["mediaMetadata"]["creationTime"].asString()),
        json["mediaMetadata"].isMember("photo")
            ? IItem::FileType::Image
            : (json["mediaMetadata"].isMember("video")
                   ? IItem::FileType::Video
                   : IItem::FileType::Unknown));
  }
}

ICloudProvider::DownloadFileRequest::Pointer GooglePhotos::downloadFromUrl(
    IItem::Pointer item, const std::string &url,
    IDownloadFileCallback::Pointer callback) {
  return std::make_shared<cloudstorage::DownloadFileRequest>(
             shared_from_this(), item, callback, FullRange,
             [=](const IItem &, std::ostream &) { return http()->create(url); })
      ->run();
}

IRequest<EitherError<std::string>>::Pointer GooglePhotos::getUploadUrl(
    IItem::Pointer, const std::string &filename, uint64_t size,
    std::function<void(EitherError<std::string>)> cb) {
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<std::string>>::Pointer r) {
               r->request(
                   [=](util::Output) {
                     auto extension =
                         filename.substr(filename.find_last_of(".") + 1);
                     auto request =
                         http()->create(endpoint() + "/uploads", "POST");
                     request->setHeaderParameter("X-Goog-Upload-Command",
                                                 "start");
                     request->setHeaderParameter("X-Goog-Upload-Content-Type",
                                                 util::to_mime_type(extension));
                     request->setHeaderParameter("X-Goog-Upload-Protocol",
                                                 "resumable");
                     request->setHeaderParameter("X-Goog-Upload-Raw-Size",
                                                 std::to_string(size));
                     request->setHeaderParameter("X-Goog-File-Name", filename);
                     return request;
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) return r->done(e.left());
                     const auto &headers = e.right()->headers();
                     auto it = headers.find("x-goog-upload-url");
                     if (it == headers.end()) {
                       r->done(Error{IHttpRequest::ServiceUnavailable,
                                     util::Error::UNKNOWN_RESPONSE_RECEIVED});
                     } else {
                       r->done(it->second);
                     }
                   });
             })
      ->run();
}

std::string GooglePhotos::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/photoslibrary+openid%20email"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
