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
#include <algorithm>
#include <iomanip>

using namespace std::placeholders;

namespace cloudstorage {

namespace {

std::string escapePath(IHttp* http, const std::string& str) {
  std::string data = util::Url::escape(str);
  std::string slash = util::Url::escape("/");
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

EitherError<void> rename(Request<EitherError<void>>* r, std::string dest_id,
                         std::string source_id, std::string region,
                         IHttp* http) {
  if (!source_id.empty() && source_id.back() != '/') {
    std::stringstream output;
    Error error;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto dest_data = AmazonS3::split(dest_id);
          auto request = http->create("https://" + dest_data.first + ".s3." +
                                          region + ".amazonaws.com/" +
                                          escapePath(http, dest_data.second),
                                      "PUT");
          auto source_data = AmazonS3::split(source_id);
          request->setHeaderParameter(
              "x-amz-copy-source",
              escapePath(http,
                         "/" + source_data.first + "/" + source_data.second));
          return request;
        },
        output, &error);
    if (!IHttpRequest::isSuccess(code)) return error;
  } else {
    auto directory_request = static_cast<ICloudProvider*>(r->provider().get())
                                 ->getItemDataAsync(source_id);
    r->subrequest(directory_request);
    auto directory = directory_request->result();
    if (directory.left()) return directory.left();
    auto children_request = static_cast<ICloudProvider*>(r->provider().get())
                                ->listDirectoryAsync(directory.right());
    r->subrequest(children_request);
    if (children_request->result().left())
      return children_request->result().left();
    for (auto item : *children_request->result().right()) {
      auto p =
          rename(r, dest_id + "/" + item->filename(), item->id(), region, http);
      if (p.left()) return p;
    }
  }
  std::stringstream output;
  Error error;
  int code = r->sendRequest(
      [=](std::ostream&) {
        auto data = AmazonS3::split(source_id);
        return http->create("https://" + data.first + ".s3." + region +
                                ".amazonaws.com/" +
                                escapePath(http, data.second),
                            "DELETE");
      },
      output, &error);
  if (!IHttpRequest::isSuccess(code)) return error;
  return nullptr;
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

}  // namespace

AmazonS3::AmazonS3() : CloudProvider(util::make_unique<Auth>()) {}

void AmazonS3::initialize(InitData&& init_data) {
  unpackCredentials(init_data.token_);
  CloudProvider::initialize(std::move(init_data));
}

std::string AmazonS3::token() const {
  return access_id() + "@" + region() + Auth::SEPARATOR + secret();
}

std::string AmazonS3::name() const { return "amazons3"; }

std::string AmazonS3::endpoint() const {
  return "https://s3-" + region() + ".amazonaws.com";
}

AuthorizeRequest::Pointer AmazonS3::authorizeAsync() {
  return util::make_unique<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest* r) -> EitherError<void> {
        if (auth_callback()->userConsentRequired(*this) !=
            IAuthCallback::Status::WaitForAuthorizationCode)
          return Error{IHttpRequest::Failure, "not waiting for code"};
        auto code = r->getAuthorizationCode();
        if (code.left()) return code.left();
        return unpackCredentials(*code.right())
                   ? EitherError<void>(nullptr)
                   : EitherError<void>(
                         Error{IHttpRequest::Failure, "invalid code"});
      });
}

ICloudProvider::MoveItemRequest::Pointer AmazonS3::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<void>>* r) -> EitherError<void> {
    auto p = rename(r, destination->id() + source->filename(), source->id(),
                    region_, http());
    callback(p);
    return p;
  });
  return std::move(r);
}

ICloudProvider::RenameItemRequest::Pointer AmazonS3::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<void>>* r) -> EitherError<void> {
    std::string path = split(item->id()).second;
    if (!path.empty() && path.back() == '/') path.pop_back();
    if (path.find_first_of('/') == std::string::npos)
      path = split(item->id()).first + Auth::SEPARATOR;
    else
      path = split(item->id()).first + Auth::SEPARATOR + getPath(path) + "/";
    auto p = rename(r, path + name, item->id(), region_, http());
    callback(p);
    return p;
  });
  return std::move(r);
}

ICloudProvider::CreateDirectoryRequest::Pointer AmazonS3::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto r = util::make_unique<Request<EitherError<IItem>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<IItem>>* r) -> EitherError<IItem> {
    std::stringstream output;
    Error error;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto data = split(parent->id());
          return http()->create(
              "https://" + data.first + ".s3." + region_ + ".amazonaws.com/" +
                  escapePath(http(), data.second + name + "/"),
              "PUT");
        },
        output, &error);
    if (!IHttpRequest::isSuccess(code)) {
      callback(error);
      return error;
    } else {
      auto path = getPath(parent->id());
      if (!split(path).second.empty()) path += "/";
      auto item = std::make_shared<Item>(name, path + name + "/",
                                         IItem::FileType::Directory);
      callback(EitherError<IItem>(item));
      return EitherError<IItem>(item);
    }
  });
  return std::move(r);
}

ICloudProvider::DeleteItemRequest::Pointer AmazonS3::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<void>>* r) -> EitherError<void> {
    if (item->type() == IItem::FileType::Directory) {
      auto children_request = static_cast<ICloudProvider*>(r->provider().get())
                                  ->listDirectoryAsync(item);
      r->subrequest(children_request);
      if (children_request->result().left()) {
        Error e = *children_request->result().left();
        callback(e);
        return e;
      }
      for (auto child : *children_request->result().right()) {
        auto delete_request =
            static_cast<ICloudProvider*>(this)->deleteItemAsync(child);
        r->subrequest(delete_request);
        if (delete_request->result().left()) {
          Error e = *delete_request->result().left();
          callback(e);
          return e;
        }
      }
    }
    std::stringstream output;
    Error error;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto data = split(item->id());
          return http()->create("https://" + data.first + ".s3." + region_ +
                                    ".amazonaws.com/" +
                                    escapePath(http(), data.second),
                                "DELETE");
        },
        output, &error);
    if (!IHttpRequest::isSuccess(code)) {
      callback(error);
      return error;
    }
    callback(nullptr);
    return nullptr;
  });
  return std::move(r);
}

ICloudProvider::GetItemDataRequest::Pointer AmazonS3::getItemDataAsync(
    const std::string& id, GetItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<IItem>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<IItem>>* r) -> EitherError<IItem> {
    if (access_id().empty() || secret().empty()) r->reauthorize();
    auto data = split(id);
    auto item = std::make_shared<Item>(
        getFilename(data.second), id,
        (data.second.empty() || data.second.back() == '/')
            ? IItem::FileType::Directory
            : IItem::FileType::Unknown);
    if (item->type() != IItem::FileType::Directory)
      item->set_url(getUrl(*item));
    callback(EitherError<IItem>(item));
    return EitherError<IItem>(item);
  });
  return std::move(r);
}

IHttpRequest::Pointer AmazonS3::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id())
    return http()->create(endpoint() + "/", "GET");
  else {
    auto data = split(item.id());
    auto request = http()->create(
        "https://" + data.first + ".s3." + region_ + ".amazonaws.com/", "GET");
    request->setParameter("list-type", "2");
    request->setParameter("prefix", data.second);
    request->setParameter("delimiter", "/");
    if (!page_token.empty())
      request->setParameter("continuation-token", page_token);
    return request;
  }
}

IHttpRequest::Pointer AmazonS3::uploadFileRequest(const IItem& directory,
                                                  const std::string& filename,
                                                  std::ostream&,
                                                  std::ostream&) const {
  auto data = split(directory.id());
  return http()->create("https://" + data.first + ".s3." + region_ +
                            ".amazonaws.com/" +
                            escapePath(http(), data.second + filename),
                        "PUT");
}

IHttpRequest::Pointer AmazonS3::downloadFileRequest(const IItem& item,
                                                    std::ostream&) const {
  auto data = split(item.id());
  return http()->create("https://" + data.first + ".s3." + region_ +
                            ".amazonaws.com/" + escapePath(http(), data.second),
                        "GET");
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
      auto item = util::make_unique<Item>(name, name + Auth::SEPARATOR,
                                          IItem::FileType::Directory);
      result.push_back(std::move(item));
    }
  } else if (document.RootElement()->FirstChildElement("Name")) {
    std::string bucket =
        document.RootElement()->FirstChildElement("Name")->GetText();
    for (auto child = document.RootElement()->FirstChildElement("Contents");
         child; child = child->NextSiblingElement("Contents")) {
      if (child->FirstChildElement("Size")->GetText() == std::string("0"))
        continue;
      std::string id = child->FirstChildElement("Key")->GetText();
      auto item = util::make_unique<Item>(getFilename(id),
                                          bucket + Auth::SEPARATOR + id,
                                          IItem::FileType::Unknown);
      item->set_url(getUrl(*item));
      result.push_back(std::move(item));
    }
    for (auto child =
             document.RootElement()->FirstChildElement("CommonPrefixes");
         child; child = child->NextSiblingElement("CommonPrefixes")) {
      std::string id = child->FirstChildElement("Prefix")->GetText();
      auto item = util::make_unique<Item>(getFilename(id),
                                          bucket + Auth::SEPARATOR + id,
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

void AmazonS3::authorizeRequest(IHttpRequest& request) const {
  if (!crypto()) throw std::runtime_error("no crypto functions provided");
  std::string current_date = currentDate();
  std::string time = currentDateAndTime();
  std::string scope = current_date + "/" + region_ + "/s3/aws4_request";
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
      request.method() + "\n" + path(request.url()) + "\n";

  bool first = false;
  for (auto q : query_parameters) {
    if (first)
      canonical_request += "&";
    else
      first = true;
    canonical_request +=
        util::Url::escape(q.first) + "=" + util::Url::escape(q.second);
  }
  canonical_request += "\n";

  for (auto q : header_parameters)
    canonical_request += util::Url::escape(q.first) + ":" + q.second + "\n";
  canonical_request += "\n";
  canonical_request += signed_headers + "\n";
  canonical_request += "UNSIGNED-PAYLOAD";

  auto hash = std::bind(&ICrypto::sha256, crypto(), _1);
  auto sign = std::bind(&ICrypto::hmac_sha256, crypto(), _1, _2);
  auto hex = std::bind(&ICrypto::hex, crypto(), _1);

  std::string string_to_sign = "AWS4-HMAC-SHA256\n" + time + "\n" + scope +
                               "\n" + hex(hash(canonical_request));
  std::string key =
      sign(sign(sign(sign("AWS4" + secret(), current_date), region_), "s3"),
           "aws4_request");
  std::string signature = hex(sign(key, string_to_sign));

  request.setParameter("X-Amz-Signature", signature);

  auto params = request.parameters();
  for (auto p : params) request.setParameter(p.first,
          util::Url::escape(p.second));
}

bool AmazonS3::reauthorize(int code) const {
  return CloudProvider::reauthorize(code) || code == IHttpRequest::Forbidden ||
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

std::string AmazonS3::region() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return region_;
}

std::pair<std::string, std::string> AmazonS3::split(const std::string& str) {
  return credentialsFromString(str);
}

bool AmazonS3::unpackCredentials(const std::string& code) {
  std::lock_guard<std::mutex> lock(auth_mutex());
  auto separator = code.find_first_of(Auth::SEPARATOR);
  auto at_position = code.find_last_of('@', separator);
  if (at_position == std::string::npos || separator == std::string::npos)
    return false;
  access_id_ = code.substr(0, at_position);
  region_ = code.substr(at_position + 1, separator - at_position - 1);
  secret_ = code.substr(separator + strlen(Auth::SEPARATOR));
  return true;
}

std::string AmazonS3::getUrl(const Item& item) const {
  auto data = split(item.id());
  auto request =
      http()->create("https://" + data.first + ".s3." + region_ +
                         ".amazonaws.com/" + escapePath(http(), data.second),
                     "GET");
  authorizeRequest(*request);
  std::string parameters;
  for (const auto& p : request->parameters())
    parameters += p.first + "=" + p.second + "&";
  return request->url() + "?" + parameters;
}

AmazonS3::Auth::Auth() {}

std::string AmazonS3::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state();
}

IHttpRequest::Pointer AmazonS3::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer AmazonS3::Auth::refreshTokenRequest(std::ostream&) const {
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
