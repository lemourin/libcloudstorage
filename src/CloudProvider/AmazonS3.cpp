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

void rename(Request<EitherError<void>>::Pointer r, IHttp* http,
            std::string region, std::string dest_id, std::string source_id,
            std::function<void(EitherError<void>)> complete);

std::string escapePath(const std::string& str) {
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

void remove(Request<EitherError<void>>::Pointer r,
            std::shared_ptr<std::vector<IItem::Pointer>> lst,
            std::function<void(EitherError<void>)> complete) {
  if (lst->empty()) return complete(nullptr);
  auto item = lst->back();
  lst->pop_back();
  r->subrequest(r->provider()->deleteItemAsync(item, [=](EitherError<void> e) {
    if (e.left())
      complete(e.left());
    else
      remove(r, lst, complete);
  }));
}

void rename(Request<EitherError<void>>::Pointer r, IHttp* http,
            std::string region,
            std::shared_ptr<std::vector<IItem::Pointer>> lst,
            std::string dest_id, std::string source_id,
            std::function<void(EitherError<void>)> complete) {
  if (lst->empty()) return complete(nullptr);
  auto item = lst->back();
  lst->pop_back();
  rename(r, http, region, lst, dest_id, source_id, [=](EitherError<void> e) {
    if (e.left())
      complete(e.left());
    else
      rename(r, http, region, dest_id + "/" + item->filename(), item->id(),
             complete);
  });
}

void rename(Request<EitherError<void>>::Pointer r, IHttp* http,
            std::string region, std::string dest_id, std::string source_id,
            std::function<void(EitherError<void>)> complete) {
  auto finalize = [=]() {
    auto output = std::make_shared<std::stringstream>();
    r->sendRequest(
        [=](util::Output) {
          auto data = AmazonS3::split(source_id);
          return http->create("https://" + data.first + ".s3." + region +
                                  ".amazonaws.com/" + escapePath(data.second),
                              "DELETE");
        },
        [=](EitherError<util::Output> e) {
          if (e.left()) {
            complete(e.left());
            r->done(e.left());
          } else {
            complete(nullptr);
            r->done(nullptr);
          }
        },
        output);
  };
  if (!source_id.empty() && source_id.back() != '/') {
    auto output = std::make_shared<std::stringstream>();
    r->sendRequest(
        [=](util::Output) {
          auto dest_data = AmazonS3::split(dest_id);
          auto request =
              http->create("https://" + dest_data.first + ".s3." + region +
                               ".amazonaws.com/" + escapePath(dest_data.second),
                           "PUT");
          auto source_data = AmazonS3::split(source_id);
          request->setHeaderParameter(
              "x-amz-copy-source",
              escapePath("/" + source_data.first + "/" + source_data.second));
          return request;
        },
        [=](EitherError<util::Output> e) {
          if (e.left()) {
            complete(e.left());
            r->done(e.left());
          } else {
            finalize();
          }
        },
        output);
  } else {
    r->subrequest(
        r->provider()->getItemDataAsync(source_id, [=](EitherError<IItem> e) {
          if (e.left()) {
            complete(e.left());
            return r->done(e.left());
          }
          r->subrequest(r->provider()->listDirectoryAsync(
              e.right(), [=](EitherError<std::vector<IItem::Pointer>> e) {
                if (e.left()) {
                  complete(e.left());
                  return r->done(e.left());
                }
                rename(r, http, region, e.right(), dest_id, source_id,
                       [=](EitherError<void> e) {
                         if (e.left()) {
                           complete(e.left());
                           return r->done(e.left());
                         }
                         finalize();
                       });
              }));
        }));
  }
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
  auto time =
      util::gmtime(std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count());
  std::stringstream ss;
  ss << std::put_time(&time, "%Y%m%d");
  return ss.str();
}

std::string currentDateAndTime() {
  auto time =
      util::gmtime(std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count());
  std::stringstream ss;
  ss << std::put_time(&time, "%Y%m%dT%H%M%SZ");
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
  return std::make_shared<SimpleAuthorization>(shared_from_this());
}

ICloudProvider::MoveItemRequest::Pointer AmazonS3::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Pointer r) {
    rename(r, http(), region(), destination->id() + source->filename(),
           source->id(), callback);
  });
  return r->run();
}

ICloudProvider::RenameItemRequest::Pointer AmazonS3::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Pointer r) {
    std::string path = split(item->id()).second;
    if (!path.empty() && path.back() == '/') path.pop_back();
    if (path.find_first_of('/') == std::string::npos)
      path = split(item->id()).first + Auth::SEPARATOR;
    else
      path = split(item->id()).first + Auth::SEPARATOR + getPath(path) + "/";
    rename(r, http(), region(), path + name, item->id(), callback);
  });
  return r->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer AmazonS3::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set([=](Request<EitherError<IItem>>::Pointer r) {
    auto output = std::make_shared<std::stringstream>();
    r->sendRequest(
        [=](util::Output) {
          auto data = split(parent->id());
          return http()->create("https://" + data.first + ".s3." + region() +
                                    ".amazonaws.com/" +
                                    escapePath(data.second + name + "/"),
                                "PUT");
        },
        [=](EitherError<util::Output> e) {
          if (e.left()) {
            callback(e.left());
            r->done(e.left());
          } else {
            auto path = getPath(parent->id());
            if (!split(path).second.empty()) path += "/";
            auto item = std::make_shared<Item>(name, path + name + "/", 0,
                                               IItem::UnknownTimeStamp,
                                               IItem::FileType::Directory);
            callback(EitherError<IItem>(item));
            r->done(EitherError<IItem>(item));
          }
        },
        output);
  });
  return r->run();
}

ICloudProvider::DeleteItemRequest::Pointer AmazonS3::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Pointer r) {
    auto release = [=] {
      auto output = std::make_shared<std::stringstream>();
      r->sendRequest(
          [=](util::Output) {
            auto data = split(item->id());
            return http()->create("https://" + data.first + ".s3." + region() +
                                      ".amazonaws.com/" +
                                      escapePath(data.second),
                                  "DELETE");
          },
          [=](EitherError<util::Output> e) {
            if (e.left()) {
              callback(e.left());
              r->done(e.left());
            } else {
              callback(nullptr);
              r->done(nullptr);
            }
          },
          output);
    };
    if (item->type() == IItem::FileType::Directory) {
      r->subrequest(r->provider()->listDirectoryAsync(
          item, [=](EitherError<std::vector<IItem::Pointer>> e) {
            if (e.left()) {
              callback(e.left());
              return r->done(e.left());
            }
            remove(r, e.right(), [=](EitherError<void> e) {
              if (e.left()) {
                callback(e.left());
                return r->done(e.left());
              }
              release();
            });
          }));
    } else
      release();
  });
  return r->run();
}

ICloudProvider::GetItemDataRequest::Pointer AmazonS3::getItemDataAsync(
    const std::string& id, GetItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set([=](Request<EitherError<IItem>>::Pointer r) {
    auto data = split(id);
    if (data.second.empty()) {
      auto item = std::make_shared<Item>(data.first, id, IItem::UnknownSize,
                                         IItem::UnknownTimeStamp,
                                         IItem::FileType::Directory);
      callback(EitherError<IItem>(item));
      return r->done(EitherError<IItem>(item));
    }
    auto factory = [=](util::Output) {
      auto request = http()->create(
          "https://" + data.first + ".s3." + region() + ".amazonaws.com/",
          "GET");
      request->setParameter("list-type", "2");
      request->setParameter("prefix", data.second);
      request->setParameter("delimiter", "/");
      return request;
    };
    auto output = std::make_shared<std::stringstream>();
    r->sendRequest(
        factory,
        [=](EitherError<util::Output> e) {
          if (e.left()) {
            callback(e.left());
            return r->done(e.left());
          }
          std::stringstream sstream;
          sstream << output->rdbuf();
          tinyxml2::XMLDocument document;
          if (document.Parse(sstream.str().c_str(), sstream.str().size()) !=
              tinyxml2::XML_SUCCESS) {
            Error e{IHttpRequest::Failure, "invalid xml"};
            callback(e);
            return r->done(e);
          }
          auto node = document.RootElement();
          auto size = IItem::UnknownSize;
          auto timestamp = IItem::UnknownTimeStamp;
          if (auto contents_element = node->FirstChildElement("Contents")) {
            if (auto size_element = contents_element->FirstChildElement("Size"))
              if (auto text = size_element->GetText()) size = std::atoll(text);
            if (auto time_element =
                    contents_element->FirstChildElement("LastModified"))
              if (auto text = time_element->GetText())
                timestamp = util::parse_time(text);
          }
          auto type = data.second.back() == '/' ? IItem::FileType::Directory
                                                : IItem::FileType::Unknown;
          auto item = std::make_shared<Item>(
              getFilename(data.second), id,
              type == IItem::FileType::Directory ? IItem::UnknownSize : size,
              timestamp, type);
          if (item->type() != IItem::FileType::Directory)
            item->set_url(getUrl(*item));
          callback(EitherError<IItem>(item));
          r->done(EitherError<IItem>(item));
        },
        output);
  });
  return r->run();
}

IHttpRequest::Pointer AmazonS3::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id())
    return http()->create(endpoint() + "/", "GET");
  else {
    auto data = split(item.id());
    auto request = http()->create(
        "https://" + data.first + ".s3." + region() + ".amazonaws.com/", "GET");
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
  return http()->create("https://" + data.first + ".s3." + region() +
                            ".amazonaws.com/" +
                            escapePath(data.second + filename),
                        "PUT");
}

IHttpRequest::Pointer AmazonS3::downloadFileRequest(const IItem& item,
                                                    std::ostream&) const {
  auto data = split(item.id());
  return http()->create("https://" + data.first + ".s3." + region() +
                            ".amazonaws.com/" + escapePath(data.second),
                        "GET");
}

std::vector<IItem::Pointer> AmazonS3::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str(), sstream.str().size()) !=
      tinyxml2::XML_SUCCESS)
    throw std::logic_error("invalid xml");
  std::vector<IItem::Pointer> result;
  if (auto buckets = document.RootElement()->FirstChildElement("Buckets")) {
    for (auto child = buckets->FirstChild(); child;
         child = child->NextSibling()) {
      auto name_element = child->FirstChildElement("Name");
      if (!name_element) throw std::logic_error("invalid xml");
      std::string name = name_element->GetText();
      auto item = util::make_unique<Item>(
          name, name + Auth::SEPARATOR, IItem::UnknownSize,
          IItem::UnknownTimeStamp, IItem::FileType::Directory);
      result.push_back(std::move(item));
    }
  } else if (auto name_element =
                 document.RootElement()->FirstChildElement("Name")) {
    std::string bucket = name_element->GetText();
    for (auto child = document.RootElement()->FirstChildElement("Contents");
         child; child = child->NextSiblingElement("Contents")) {
      auto size_element = child->FirstChildElement("Size");
      if (!size_element) throw std::logic_error("invalid xml");
      auto size = std::atoll(size_element->GetText());
      if (size == 0) continue;
      auto key_element = child->FirstChildElement("Key");
      if (!key_element) throw std::logic_error("invalid xml");
      std::string id = key_element->GetText();
      auto timestamp_element = child->FirstChildElement("LastModified");
      if (!timestamp_element) throw std::logic_error("invalid xml");
      std::string timestamp = timestamp_element->GetText();
      auto item = util::make_unique<Item>(
          getFilename(id), bucket + Auth::SEPARATOR + id, size,
          util::parse_time(timestamp), IItem::FileType::Unknown);
      item->set_url(getUrl(*item));
      result.push_back(std::move(item));
    }
    for (auto child =
             document.RootElement()->FirstChildElement("CommonPrefixes");
         child; child = child->NextSiblingElement("CommonPrefixes")) {
      auto prefix_element = child->FirstChildElement("Prefix");
      if (!prefix_element) throw std::logic_error("invalid xml");
      std::string id = prefix_element->GetText();
      auto item = util::make_unique<Item>(
          getFilename(id), bucket + Auth::SEPARATOR + id, IItem::UnknownSize,
          IItem::UnknownTimeStamp, IItem::FileType::Directory);
      result.push_back(std::move(item));
    }
    auto is_truncated_element =
        document.RootElement()->FirstChildElement("IsTruncated");
    if (!is_truncated_element) throw std::logic_error("invalid xml");
    if (is_truncated_element->GetText() == std::string("true")) {
      auto next_token_element =
          document.RootElement()->FirstChildElement("NextContinuationToken");
      if (!next_token_element) throw std::logic_error("invalid xml");
      next_page_token = next_token_element->GetText();
    }
  }
  return result;
}

void AmazonS3::authorizeRequest(IHttpRequest& request) const {
  if (!crypto()) throw std::runtime_error("no crypto functions provided");
  std::string current_date = currentDate();
  std::string time = currentDateAndTime();
  std::string scope = current_date + "/" + region() + "/s3/aws4_request";
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
      sign(sign(sign(sign("AWS4" + secret(), current_date), region()), "s3"),
           "aws4_request");
  std::string signature = hex(sign(key, string_to_sign));

  request.setParameter("X-Amz-Signature", signature);

  auto params = request.parameters();
  for (auto p : params)
    request.setParameter(p.first, util::Url::escape(p.second));
}

bool AmazonS3::reauthorize(int code) const {
  return CloudProvider::reauthorize(code) || code == IHttpRequest::Forbidden ||
         access_id().empty() || secret().empty();
}

std::string AmazonS3::access_id() const {
  auto lock = auth_lock();
  return access_id_;
}

std::string AmazonS3::secret() const {
  auto lock = auth_lock();
  return secret_;
}

std::string AmazonS3::region() const {
  auto lock = auth_lock();
  return region_;
}

std::pair<std::string, std::string> AmazonS3::split(const std::string& str) {
  return credentialsFromString(str);
}

bool AmazonS3::unpackCredentials(const std::string& code) {
  auto lock = auth_lock();
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
  auto request = http()->create("https://" + data.first + ".s3." + region() +
                                    ".amazonaws.com/" + escapePath(data.second),
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
