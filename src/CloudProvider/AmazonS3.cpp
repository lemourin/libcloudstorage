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

HttpRequest::Pointer AmazonS3::getItemDataRequest(
    const std::string&, std::ostream& input_stream) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id())
    return make_unique<HttpRequest>("https://s3-eu-central-1.amazonaws.com/",
                                    HttpRequest::Type::GET);
  else {
    const S3Item& i = static_cast<const S3Item&>(item);
    auto request = make_unique<HttpRequest>(
        "https://" + i.bucket_ + ".s3.amazonaws.com/", HttpRequest::Type::GET);
    request->setParameter("list-type", "2");
    request->setParameter("prefix", i.id());
    request->setParameter("delimiter", "/");
    if (!page_token.empty())
      request->setParameter("continuation-token", page_token);
    return request;
  }
}

HttpRequest::Pointer AmazonS3::uploadFileRequest(
    const IItem& directory, const std::__cxx11::string& filename,
    std::ostream& prefix_stream, std::ostream& suffix_stream) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::downloadFileRequest(
    const IItem&, std::ostream& input_stream) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::deleteItemRequest(
    const IItem&, std::ostream& input_stream) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::createDirectoryRequest(
    const IItem&, const std::__cxx11::string& name, std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::moveItemRequest(const IItem&, const IItem&,
                                               std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::renameItemRequest(
    const IItem&, const std::__cxx11::string& name, std::ostream&) const {
  return nullptr;
}

IItem::Pointer AmazonS3::getItemDataResponse(std::istream& response) const {
  return nullptr;
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
      auto item = make_unique<S3Item>(name, "", IItem::FileType::Directory);
      item->bucket_ = name;
      result.push_back(std::move(item));
    }
  } else {
    std::string bucket =
        document.RootElement()->FirstChildElement("Name")->GetText();
    auto first = document.RootElement()->FirstChildElement("Contents");
    if (document.RootElement()->FirstChildElement("Prefix")->GetText() &&
        !document.RootElement()->FirstChildElement("ContinuationToken") &&
        first)
      first = first->NextSiblingElement("Contents");
    for (auto child = first; child;
         child = child->NextSiblingElement("Contents")) {
      std::string id = child->FirstChildElement("Key")->GetText();
      auto item =
          make_unique<S3Item>(getFilename(id), id, IItem::FileType::Unknown);
      item->bucket_ = bucket;
      result.push_back(std::move(item));
    }
    auto common_prefixes =
        document.RootElement()->FirstChildElement("CommonPrefixes");
    if (common_prefixes) {
      for (auto child = common_prefixes->FirstChildElement("Prefix"); child;
           child = child->NextSiblingElement("Prefix")) {
        std::string id = child->GetText();
        auto item = make_unique<S3Item>(getFilename(id), id,
                                        IItem::FileType::Directory);
        item->bucket_ = bucket;
        result.push_back(std::move(item));
      }
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
  request.setParameter("X-Amz-SignedHeaders", "host");
  request.setHeaderParameter("host", host(request.url()));

  std::string canonical_request =
      std::string("GET\n") + path(request.url()) + "\n";

  std::vector<std::pair<std::string, std::string>> query_parameters;
  for (auto q : request.parameters())
    query_parameters.push_back({q.first, q.second});
  std::sort(query_parameters.begin(), query_parameters.end());
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

  std::vector<std::pair<std::string, std::string>> header_parameters;
  for (auto q : request.headerParameters())
    header_parameters.push_back({q.first, q.second});
  std::sort(header_parameters.begin(), header_parameters.end());
  for (auto q : header_parameters)
    canonical_request += HttpRequest::escape(q.first) + ":" +
                         HttpRequest::escape(q.second) + "\n";
  canonical_request += "\n";
  canonical_request += "host\n";
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
  return false;
  return CloudProvider::reauthorize(code) || code == 400 || code == 403;
}

std::string AmazonS3::access_id() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return access_id_;
}

std::string AmazonS3::secret() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return secret_;
}

AmazonS3::Auth::Auth() {}

std::string AmazonS3::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login";
}

HttpRequest::Pointer AmazonS3::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  return nullptr;
}

HttpRequest::Pointer AmazonS3::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
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
