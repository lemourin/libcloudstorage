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

#include <json/json.h>
#include <tinyxml2.h>
#include <algorithm>
#include <iomanip>

#include "Request/RecursiveRequest.h"
#include "Utility/Utility.h"

using namespace std::placeholders;

namespace cloudstorage {

using ItemId = std::pair<std::string, std::string>;

namespace {

std::string to_string(ItemId str) {
  Json::Value json;
  json["b"] = str.first;
  json["p"] = str.second;
  return util::to_base64(util::to_string(json));
}

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
  Json::Value json;
  json["username"] = access_id();
  json["password"] = secret();
  json["region"] = region();
  return util::to_base64(util::to_string(json));
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
  using Request = RecursiveRequest<EitherError<IItem>>;
  auto source_id = extract(source->id());
  auto destination_id = extract(destination->id());
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback callback) {
    auto id = extract(item->id());
    auto l = getPath("/" + source_id.second).length();
    std::string new_path = destination_id.second + id.second.substr(l);
    r->request(
        [=](util::Output) {
          auto request =
              http()->create("https://" + id.first + ".s3." + region() +
                                 ".amazonaws.com/" + new_path,
                             "PUT");
          if (item->type() != IItem::FileType::Directory)
            request->setHeaderParameter("x-amz-copy-source",
                                        id.first + "/" + escapePath(id.second));
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return callback(e.left());
          r->request(
              [=](util::Output) {
                return http()->create("https://" + id.first + ".s3." +
                                          region() + ".amazonaws.com/" +
                                          escapePath(id.second),
                                      "DELETE");
              },
              [=](EitherError<Response> e) {
                if (e.left()) return callback(e.left());
                IItem::Pointer renamed = std::make_shared<Item>(
                    getFilename(new_path), to_string({id.first, new_path}),
                    item->size(), item->timestamp(), item->type());
                callback(renamed);
              });
        });
  };
  return std::make_shared<Request>(shared_from_this(), source, callback,
                                   visitor)
      ->run();
}

ICloudProvider::RenameItemRequest::Pointer AmazonS3::renameItemAsync(
    IItem::Pointer root, const std::string& name, RenameItemCallback callback) {
  using Request = RecursiveRequest<EitherError<IItem>>;
  auto root_id = extract(root->id());
  auto new_prefix = (getPath("/" + root_id.second) + "/" + name).substr(1);
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback callback) {
    auto id = extract(item->id());
    auto new_path =
        new_prefix + "/" + id.second.substr(root_id.second.length());
    if (!new_path.empty() && new_path.back() == '/' &&
        item->type() != IItem::FileType::Directory)
      new_path.pop_back();
    r->request(
        [=](util::Output) {
          auto request =
              http()->create("https://" + id.first + ".s3." + region() +
                                 ".amazonaws.com/" + new_path,
                             "PUT");
          if (item->type() != IItem::FileType::Directory)
            request->setHeaderParameter("x-amz-copy-source",
                                        id.first + "/" + escapePath(id.second));
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return callback(e.left());
          r->request(
              [=](util::Output) {
                return http()->create("https://" + id.first + ".s3." +
                                          region() + ".amazonaws.com/" +
                                          escapePath(id.second),
                                      "DELETE");
              },
              [=](EitherError<Response> e) {
                if (e.left()) return callback(e.left());
                IItem::Pointer renamed = std::make_shared<Item>(
                    getFilename(new_path), to_string({id.first, new_path}),
                    item->size(), item->timestamp(), item->type());
                callback(renamed);
              });
        });
  };
  return std::make_shared<Request>(shared_from_this(), root, callback, visitor)
      ->run();
}

IHttpRequest::Pointer AmazonS3::createDirectoryRequest(const IItem& parent,
                                                       const std::string& name,
                                                       std::ostream&) const {
  auto data = extract(parent.id());
  return http()->create("https://" + data.first + ".s3." + region() +
                            ".amazonaws.com/" +
                            escapePath(data.second + name + "/"),
                        "PUT");
}

IItem::Pointer AmazonS3::createDirectoryResponse(const IItem& parent,
                                                 const std::string& name,
                                                 std::istream&) const {
  auto data = extract(parent.id());
  return std::make_shared<Item>(
      name, to_string({data.first, data.second + name + "/"}), 0,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);
}

ICloudProvider::DeleteItemRequest::Pointer AmazonS3::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  using Request = RecursiveRequest<EitherError<void>>;
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback complete) {
    auto id = extract(item->id());
    r->request(
        [=](util::Output) {
          return http()->create("https://" + id.first + ".s3." + region() +
                                    ".amazonaws.com/" + escapePath(id.second),
                                "DELETE");
        },
        [=](EitherError<Response> e) {
          if (e.left())
            complete(e.left());
          else
            complete(nullptr);
        });
  };
  return std::make_shared<Request>(shared_from_this(), item, callback, visitor)
      ->run();
}

ICloudProvider::GetItemDataRequest::Pointer AmazonS3::getItemDataAsync(
    const std::string& id, GetItemCallback callback) {
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<IItem>>::Pointer r) {
               if (id == rootDirectory()->id()) return r->done(rootDirectory());
               auto data = extract(id);
               if (data.second.empty()) {
                 auto item = std::make_shared<Item>(
                     data.first, id, IItem::UnknownSize,
                     IItem::UnknownTimeStamp, IItem::FileType::Directory);
                 return r->done(EitherError<IItem>(item));
               }
               auto factory = [=](util::Output) {
                 auto request =
                     http()->create("https://" + data.first + ".s3." +
                                        region() + ".amazonaws.com/",
                                    "GET");
                 request->setParameter("list-type", "2");
                 request->setParameter("prefix", data.second);
                 request->setParameter("delimiter", "/");
                 return request;
               };
               r->request(factory, [=](EitherError<Response> e) {
                 if (e.left()) return r->done(e.left());
                 std::stringstream sstream;
                 sstream << e.right()->output().rdbuf();
                 tinyxml2::XMLDocument document;
                 if (document.Parse(sstream.str().c_str(),
                                    sstream.str().size()) !=
                     tinyxml2::XML_SUCCESS)
                   return r->done(Error{IHttpRequest::Failure, "invalid xml"});
                 auto node = document.RootElement();
                 auto size = IItem::UnknownSize;
                 auto timestamp = IItem::UnknownTimeStamp;
                 if (auto contents_element =
                         node->FirstChildElement("Contents")) {
                   if (auto size_element =
                           contents_element->FirstChildElement("Size"))
                     if (auto text = size_element->GetText())
                       size = std::atoll(text);
                   if (auto time_element =
                           contents_element->FirstChildElement("LastModified"))
                     if (auto text = time_element->GetText())
                       timestamp = util::parse_time(text);
                 }
                 auto type = data.second.back() == '/'
                                 ? IItem::FileType::Directory
                                 : IItem::FileType::Unknown;
                 auto item = std::make_shared<Item>(
                     getFilename(data.second), id,
                     type == IItem::FileType::Directory ? IItem::UnknownSize
                                                        : size,
                     timestamp, type);
                 if (item->type() != IItem::FileType::Directory)
                   item->set_url(getUrl(*item));
                 r->done(EitherError<IItem>(item));
               });
             })
      ->run();
}

IHttpRequest::Pointer AmazonS3::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id())
    return http()->create(endpoint() + "/", "GET");
  else {
    auto data = extract(item.id());
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
  auto data = extract(directory.id());
  return http()->create("https://" + data.first + ".s3." + region() +
                            ".amazonaws.com/" +
                            escapePath(data.second + filename),
                        "PUT");
}

IItem::Pointer AmazonS3::uploadFileResponse(const IItem& item,
                                            const std::string& filename,
                                            uint64_t size,
                                            std::istream&) const {
  auto data = extract(item.id());
  return util::make_unique<Item>(
      filename, to_string({data.first, data.second + filename}), size,
      std::chrono::system_clock::now(), IItem::FileType::Unknown);
}

IHttpRequest::Pointer AmazonS3::downloadFileRequest(const IItem& item,
                                                    std::ostream&) const {
  auto data = extract(item.id());
  return http()->create("https://" + data.first + ".s3." + region() +
                            ".amazonaws.com/" + escapePath(data.second),
                        "GET");
}

std::vector<IItem::Pointer> AmazonS3::listDirectoryResponse(
    const IItem& parent, std::istream& stream,
    std::string& next_page_token) const {
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
          name, to_string({name, ""}), IItem::UnknownSize,
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
      auto key_element = child->FirstChildElement("Key");
      if (!key_element) throw std::logic_error("invalid xml");
      std::string id = key_element->GetText();
      if (size == 0 && parent.id() == to_string({bucket, id})) continue;
      auto timestamp_element = child->FirstChildElement("LastModified");
      if (!timestamp_element) throw std::logic_error("invalid xml");
      std::string timestamp = timestamp_element->GetText();
      auto item = util::make_unique<Item>(
          getFilename(id), to_string({bucket, id}), size,
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
          getFilename(id), to_string({bucket, id}), IItem::UnknownSize,
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
  util::Url url(request.url());
  request.setParameter("X-Amz-Algorithm", "AWS4-HMAC-SHA256");
  request.setParameter("X-Amz-Credential", access_id() + "/" + scope);
  request.setParameter("X-Amz-Date", time);
  request.setParameter("X-Amz-Expires", "86400");
  request.setHeaderParameter("host", url.host());

  std::vector<std::pair<std::string, std::string>> header_parameters;
  for (auto q : request.headerParameters())
    header_parameters.push_back({util::to_lower(q.first), q.second});
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

  std::string canonical_request = request.method() + "\n" + url.path() + "\n";

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

bool AmazonS3::reauthorize(int code,
                           const IHttpRequest::HeaderParameters& h) const {
  return CloudProvider::reauthorize(code, h) ||
         code == IHttpRequest::Forbidden || access_id().empty() ||
         secret().empty();
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

std::pair<std::string, std::string> AmazonS3::extract(const std::string& str) {
  try {
    Json::Value json;
    std::stringstream(util::from_base64(str)) >> json;
    return {json["b"].asString(), json["p"].asString()};
  } catch (std::exception) {
    return {};
  }
}

bool AmazonS3::unpackCredentials(const std::string& code) {
  auto lock = auth_lock();
  try {
    Json::Value json;
    std::stringstream(util::from_base64(code)) >> json;
    access_id_ = json["username"].asString();
    secret_ = json["password"].asString();
    region_ = json["region"].asString();
    return true;
  } catch (std::exception) {
    return false;
  }
}

std::string AmazonS3::getUrl(const Item& item) const {
  auto data = extract(item.id());
  auto request = http()->create("https://" + data.first + ".s3." + region() +
                                    ".amazonaws.com/" + escapePath(data.second),
                                "GET");
  authorizeRequest(*request);
  std::string parameters;
  for (const auto& p : request->parameters())
    parameters += p.first + "=" + p.second + "&";
  return request->url() + "?" + parameters;
}

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
