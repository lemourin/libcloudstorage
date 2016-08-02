/*****************************************************************************
 * AmazonS3.cpp : AmazonS3 implementation
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

#include "AmazonS3.h"

#include <tinyxml2.h>
#include <iomanip>
#include <iostream>

#include <cryptopp/cryptlib.h>
#include <cryptopp/hex.h>
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>

const std::string region = "eu-central-1";

namespace cloudstorage {

namespace {

class ListDirectoryCallback : public IListDirectoryCallback {
 public:
  ListDirectoryCallback(Request<bool>::Semaphore* semaphore)
      : semaphore_(semaphore) {}

  void receivedItem(IItem::Pointer) {}
  void done(const std::vector<IItem::Pointer>&) { semaphore_->notify(); }
  void error(const std::string&) { semaphore_->notify(); }

 private:
  Request<bool>::Semaphore* semaphore_;
};

std::string escapePath(const std::string& str) {
  std::string data = HttpRequest::escape(str);
  std::string slash = HttpRequest::escape("/");
  std::string result;
  for (uint32_t i = 0; i < data.size();)
    if (data.substr(i, slash.length()) == slash) {
      result += "/";
      i += slash.length();
    } else {
      result += data[i];
      i++;
    }
  return result;
}

bool rename(Request<bool>* r, std::string dest_id, std::string source_id) {
  if (!source_id.empty() && source_id.back() != '/') {
    std::stringstream output;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto dest_data = AmazonS3::split(dest_id);
          auto request = make_unique<HttpRequest>(
              "https://" + dest_data.first + ".s3.amazonaws.com/" +
                  escapePath(dest_data.second),
              HttpRequest::Type::PUT);
          auto source_data = AmazonS3::split(source_id);
          request->setHeaderParameter(
              "x-amz-copy-source",
              escapePath("/" + source_data.first + "/" + source_data.second));
          return request;
        },
        output);
    if (!HttpRequest::isSuccess(code)) return false;
  } else {
    auto directory = r->provider()
                         ->getItemDataAsync(source_id, [](IItem::Pointer) {})
                         ->result();
    Request<bool>::Semaphore semaphore(r);
    auto children_request = r->provider()->listDirectoryAsync(
        directory, make_unique<ListDirectoryCallback>(&semaphore));
    semaphore.wait();
    if (r->is_cancelled()) {
      children_request->cancel();
      return false;
    }
    for (auto item : children_request->result())
      if (!rename(r, dest_id + "/" + item->filename(), item->id()))
        return false;
  }
  std::stringstream output;
  int code = r->sendRequest(
      [=](std::ostream&) {
        auto data = AmazonS3::split(source_id);
        return make_unique<HttpRequest>("https://" + data.first +
                                            ".s3.amazonaws.com/" +
                                            escapePath(data.second),
                                        HttpRequest::Type::DEL);
      },
      output);
  if (!HttpRequest::isSuccess(code)) {
    return false;
  }
  return true;
}

std::string host(std::string url) {
  const char* http = "https://";
  return url.substr(strlen(http),
                    url.find_first_of('/', strlen(http)) - strlen(http));
}

std::string path(std::string url) {
  int length = host(url).length();
  return url.substr(strlen("https://") + length);
}

std::string currentDate() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y%m%d");
  return ss.str();
}

std::string currentDateAndTime() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y%m%dT%H%M%SZ");
  return ss.str();
}

std::string hash(std::string message) {
  CryptoPP::SHA256 hash;
  std::string result;
  CryptoPP::StringSource(
      message, true,
      new CryptoPP::HashFilter(hash, new CryptoPP::StringSink(result)));
  return result;
}

std::string sign(std::string key, std::string plain) {
  std::string mac;
  CryptoPP::HMAC<CryptoPP::SHA256> hmac((byte*)key.c_str(), key.length());
  CryptoPP::StringSource(plain, true, new CryptoPP::HashFilter(
                                          hmac, new CryptoPP::StringSink(mac)));
  std::string result;
  CryptoPP::StringSource(mac, true, new CryptoPP::StringSink(result));
  return result;
}

std::string hex(std::string hash) {
  std::string result;
  CryptoPP::StringSource(
      hash, true,
      new CryptoPP::HexEncoder(new CryptoPP::StringSink(result), false));
  return result;
}

}  // namespace

AmazonS3::AmazonS3() : CloudProvider(make_unique<Auth>()) {}

void AmazonS3::initialize(const std::string& token,
                          ICloudProvider::ICallback::Pointer callback,
                          const ICloudProvider::Hints& hints) {
  CloudProvider::initialize(token, callback, hints);

  std::unique_lock<std::mutex> lock(auth_mutex());
  auto data = creditentialsFromString(token);
  access_id_ = data.first;
  secret_ = data.second;
}

std::string AmazonS3::token() const {
  return access_id() + Auth::SEPARATOR + secret();
}

std::string AmazonS3::name() const { return "amazons3"; }

AuthorizeRequest::Pointer AmazonS3::authorizeAsync() {
  return make_unique<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest* r) -> bool {
        if (callback()->userConsentRequired(*this) !=
            ICallback::Status::WaitForAuthorizationCode)
          return false;
        auto data = creditentialsFromString(r->getAuthorizationCode());
        std::unique_lock<std::mutex> lock(auth_mutex());
        access_id_ = data.first;
        secret_ = data.second;
        return true;
      });
}

ICloudProvider::MoveItemRequest::Pointer AmazonS3::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto r = std::make_shared<Request<bool>>(shared_from_this());
  r->set_resolver([=](Request<bool>* r) -> bool {
    bool success =
        rename(r, destination->id() + source->filename(), source->id());
    callback(success);
    return success;
  });
  return r;
}

ICloudProvider::RenameItemRequest::Pointer AmazonS3::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto r = std::make_shared<Request<bool>>(shared_from_this());
  r->set_resolver([=](Request<bool>* r) -> bool {
    std::string path = split(item->id()).second;
    if (!path.empty() && path.back() == '/') path.pop_back();
    if (path.find_first_of('/') == std::string::npos)
      path = split(item->id()).first + Auth::SEPARATOR;
    else
      path = split(item->id()).first + Auth::SEPARATOR + getPath(path) + "/";
    bool success = rename(r, path + name, item->id());
    callback(success);
    return success;
  });
  return r;
}

ICloudProvider::CreateDirectoryRequest::Pointer AmazonS3::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto r = std::make_shared<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver([=](Request<IItem::Pointer>* r) -> IItem::Pointer {
    std::stringstream output;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto data = split(parent->id());
          return make_unique<HttpRequest>(
              "https://" + data.first + ".s3.amazonaws.com/" +
                  escapePath(data.second + name + "/"),
              HttpRequest::Type::PUT);
        },
        output);
    if (!HttpRequest::isSuccess(code)) {
      callback(nullptr);
      return nullptr;
    } else {
      auto path = getPath(parent->id());
      if (!split(path).second.empty()) path += "/";
      auto item = std::make_shared<Item>(name, path + name + "/",
                                         IItem::FileType::Directory);
      callback(item);
      return item;
    }

  });
  return r;
}

ICloudProvider::DeleteItemRequest::Pointer AmazonS3::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto r = make_unique<Request<bool>>(shared_from_this());
  r->set_resolver([=](Request<bool>* r) -> bool {
    if (item->type() == IItem::FileType::Directory) {
      Request<bool>::Semaphore semaphore(r);
      auto children_request = r->provider()->listDirectoryAsync(
          item, make_unique<ListDirectoryCallback>(&semaphore));
      semaphore.wait();
      if (r->is_cancelled()) {
        children_request->cancel();
        callback(false);
        return false;
      }
      for (auto child : children_request->result()) {
        auto delete_request =
            deleteItemAsync(child, [&](bool) { semaphore.notify(); });
        semaphore.wait();
        if (r->is_cancelled()) {
          delete_request->cancel();
          callback(false);
          return false;
        }
      }
    }
    std::stringstream output;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto data = split(item->id());
          return make_unique<HttpRequest>("https://" + data.first +
                                              ".s3.amazonaws.com/" +
                                              escapePath(data.second),
                                          HttpRequest::Type::DEL);
        },
        output);
    if (!HttpRequest::isSuccess(code)) {
      callback(false);
      return false;
    }
    callback(true);
    return true;
  });
  return r;
}

ICloudProvider::GetItemDataRequest::Pointer AmazonS3::getItemDataAsync(
    const std::string& id, GetItemCallback callback) {
  auto r = make_unique<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver([=](Request<IItem::Pointer>*) -> IItem::Pointer {
    if (access_id().empty() || secret().empty()) {
      callback(nullptr);
      return nullptr;
    }
    auto data = split(id);
    auto item =
        std::make_shared<Item>(getFilename(data.first), id,
                               data.second.empty() || data.second.back() == '/'
                                   ? IItem::FileType::Directory
                                   : IItem::FileType::Unknown);
    auto request = make_unique<HttpRequest>("https://" + data.first +
                                                ".s3.amazonaws.com/" +
                                                escapePath(data.second),
                                            HttpRequest::Type::GET);
    authorizeRequest(*request);
    item->set_url(request->url() + "?" + request->parametersToString());
    callback(item);
    return item;
  });
  return r;
}

HttpRequest::Pointer AmazonS3::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id())
    return make_unique<HttpRequest>("https://s3-eu-central-1.amazonaws.com/",
                                    HttpRequest::Type::GET);
  else {
    auto data = split(item.id());
    auto request = make_unique<HttpRequest>(
        "https://" + data.first + ".s3.amazonaws.com/", HttpRequest::Type::GET);
    request->setParameter("list-type", "2");
    request->setParameter("prefix", data.second);
    request->setParameter("delimiter", "/");
    if (!page_token.empty())
      request->setParameter("continuation-token", page_token);
    return std::move(request);
  }
}

HttpRequest::Pointer AmazonS3::uploadFileRequest(const IItem& directory,
                                                 const std::string& filename,
                                                 std::ostream&,
                                                 std::ostream&) const {
  auto data = split(directory.id());
  return make_unique<HttpRequest>("https://" + data.first +
                                      ".s3.amazonaws.com/" +
                                      escapePath(data.second + filename),
                                  HttpRequest::Type::PUT);
}

HttpRequest::Pointer AmazonS3::downloadFileRequest(const IItem& item,
                                                   std::ostream&) const {
  auto data = split(item.id());
  return make_unique<HttpRequest>(
      "https://" + data.first + ".s3.amazonaws.com/" + escapePath(data.second),
      HttpRequest::Type::GET);
}

std::vector<IItem::Pointer> AmazonS3::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str(), sstream.str().size()) !=
      tinyxml2::XML_SUCCESS)
    return {};
  std::vector<IItem::Pointer> result;
  if (auto buckets = document.RootElement()->FirstChildElement("Buckets")) {
    for (auto child = buckets->FirstChild(); child;
         child = child->NextSibling()) {
      std::string name = child->FirstChildElement("Name")->GetText();
      auto item = make_unique<Item>(name, name + Auth::SEPARATOR,
                                    IItem::FileType::Directory);
      result.push_back(std::move(item));
    }
  } else {
    std::string bucket =
        document.RootElement()->FirstChildElement("Name")->GetText();
    for (auto child = document.RootElement()->FirstChildElement("Contents");
         child; child = child->NextSiblingElement("Contents")) {
      if (child->FirstChildElement("Size")->GetText() == std::string("0"))
        continue;
      std::string id = child->FirstChildElement("Key")->GetText();
      auto item =
          make_unique<Item>(getFilename(id), bucket + Auth::SEPARATOR + id,
                            IItem::FileType::Unknown);
      result.push_back(std::move(item));
    }
    for (auto child =
             document.RootElement()->FirstChildElement("CommonPrefixes");
         child; child = child->NextSiblingElement("CommonPrefixes")) {
      std::string id = child->FirstChildElement("Prefix")->GetText();
      auto item =
          make_unique<Item>(getFilename(id), bucket + Auth::SEPARATOR + id,
                            IItem::FileType::Directory);
      result.push_back(std::move(item));
    }
    if (document.RootElement()->FirstChildElement("IsTruncated")->GetText() ==
        std::string("true")) {
      next_page_token = document.RootElement()
                            ->FirstChildElement("NextContinuationToken")
                            ->GetText();
    }
  }
  return result;
}

void AmazonS3::authorizeRequest(HttpRequest& request) const {
  std::string current_date = currentDate();
  std::string time = currentDateAndTime();
  std::string scope = current_date + "/" + region + "/s3/aws4_request";
  request.setParameter("X-Amz-Algorithm", "AWS4-HMAC-SHA256");
  request.setParameter("X-Amz-Credential", access_id() + "/" + scope);
  request.setParameter("X-Amz-Date", time);
  request.setParameter("X-Amz-Expires", "86400");
  request.setHeaderParameter("host", host(request.url()));

  std::vector<std::pair<std::string, std::string>> header_parameters;
  for (auto q : request.headerParameters())
    header_parameters.push_back({q.first, q.second});
  std::sort(header_parameters.begin(), header_parameters.end());

  std::string signed_headers;
  {
    bool first = false;
    for (auto q : header_parameters) {
      if (first)
        signed_headers += ";";
      else
        first = true;
      signed_headers += q.first;
    }
    request.setParameter("X-Amz-SignedHeaders", signed_headers);
  }

  std::vector<std::pair<std::string, std::string>> query_parameters;
  for (auto q : request.parameters())
    query_parameters.push_back({q.first, q.second});
  std::sort(query_parameters.begin(), query_parameters.end());

  std::string canonical_request =
      HttpRequest::toString(request.type()) + "\n" + path(request.url()) + "\n";

  bool first = false;
  for (auto q : query_parameters) {
    if (first)
      canonical_request += "&";
    else
      first = true;
    canonical_request +=
        HttpRequest::escape(q.first) + "=" + HttpRequest::escape(q.second);
  }
  canonical_request += "\n";

  for (auto q : header_parameters)
    canonical_request += HttpRequest::escape(q.first) + ":" + q.second + "\n";
  canonical_request += "\n";
  canonical_request += signed_headers + "\n";
  canonical_request += "UNSIGNED-PAYLOAD";

  std::string string_to_sign = "AWS4-HMAC-SHA256\n" + time + "\n" + scope +
                               "\n" + hex(hash(canonical_request));
  std::string key =
      sign(sign(sign(sign("AWS4" + secret(), current_date), region), "s3"),
           "aws4_request");
  std::string signature = hex(sign(key, string_to_sign));

  request.setParameter("X-Amz-Signature", signature);

  auto params = request.parameters();
  for (auto p : params)
    request.setParameter(p.first, HttpRequest::escape(p.second));
}

bool AmazonS3::reauthorize(int code) const {
  return CloudProvider::reauthorize(code) || code == 403 ||
         access_id().empty() || secret().empty();
}

std::string AmazonS3::access_id() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return access_id_;
}

std::string AmazonS3::secret() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return secret_;
}

std::pair<std::string, std::string> AmazonS3::split(const std::string& str) {
  return creditentialsFromString(str);
}

AmazonS3::Auth::Auth() {}

std::string AmazonS3::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login";
}

HttpRequest::Pointer AmazonS3::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer AmazonS3::Auth::exchangeAuthorizationCodeResponse(
    std::istream&) const {
  return nullptr;
}

IAuth::Token::Pointer AmazonS3::Auth::refreshTokenResponse(
    std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
