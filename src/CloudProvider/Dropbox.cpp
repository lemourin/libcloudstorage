/*****************************************************************************
 * Dropbox.cpp : implementation of Dropbox
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

#include "Dropbox.h"

#include <jsoncpp/json/json.h>
#include <iostream>
#include <sstream>

#include "Request/HttpCallback.h"
#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

Dropbox::Dropbox() : CloudProvider(make_unique<Auth>()) {}

std::string Dropbox::name() const { return "dropbox"; }

IItem::Pointer Dropbox::rootDirectory() const {
  return make_unique<Item>("/", "", IItem::FileType::Directory);
}

GetItemDataRequest::Pointer Dropbox::getItemDataAsync(
    IItem::Pointer item, std::function<void(IItem::Pointer)> callback) {
  return make_unique<DataRequest>(shared_from_this(), item, callback);
}

HttpRequest::Pointer Dropbox::listDirectoryRequest(
    const IItem& f, std::ostream& input_stream) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request =
      make_unique<HttpRequest>("https://api.dropboxapi.com/2/files/list_folder",
                               HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "application/json");

  Json::Value parameter;
  parameter["path"] = item.id();
  input_stream << Json::FastWriter().write(parameter);
  return request;
}

HttpRequest::Pointer Dropbox::uploadFileRequest(
    const IItem& directory, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  const Item& item = static_cast<const Item&>(directory);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://content.dropboxapi.com/1/files_put/auto/" + item.id() + filename,
      HttpRequest::Type::PUT);
  input_stream.rdbuf(stream.rdbuf());
  return request;
}

HttpRequest::Pointer Dropbox::downloadFileRequest(const IItem& f,
                                                  std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://content.dropboxapi.com/2/files/download",
      HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "");
  Json::Value parameter;
  parameter["path"] = item.id();
  std::string str = Json::FastWriter().write(parameter);
  str.pop_back();
  request->setHeaderParameter("Dropbox-API-arg", str);
  return request;
}

HttpRequest::Pointer Dropbox::getThumbnailRequest(
    const IItem&, std::ostream& input_stream) const {
  // TODO
  return 0;
}

std::vector<IItem::Pointer> Dropbox::listDirectoryResponse(
    std::istream& stream, HttpRequest::Pointer& next_page_request,
    std::ostream& next_page_request_input) const {
  Json::Value response;
  stream >> response;

  std::vector<IItem::Pointer> result;
  for (Json::Value v : response["entries"]) {
    IItem::FileType type;  // = v[".tag"].asString() == "folder";
    result.push_back(make_unique<Item>(v["name"].asString(),
                                       v["path_display"].asString(), type));
  }
  if (!response["has_more"].asBool())
    next_page_request = nullptr;
  else {
    next_page_request->set_url(
        "https://api.dropboxapi.com/2/files/list_folder/continue");
    Json::Value input;
    input["cursor"] = response["cursor"];
    next_page_request_input << input;
  }
  return result;
}

Dropbox::Auth::Auth() {
  set_client_id("ktryxp68ae5cicj");
  set_client_secret("6evu94gcxnmyr59");
}

std::string Dropbox::Auth::authorizeLibraryUrl() const {
  std::string url = "https://www.dropbox.com/oauth2/authorize?";
  url += "response_type=code&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  return url;
}

IAuth::Token::Pointer Dropbox::Auth::fromTokenString(
    const std::string& str) const {
  Token::Pointer token = make_unique<Token>();
  token->token_ = str;
  token->refresh_token_ = str;
  token->expires_in_ = -1;
  return token;
}

HttpRequest::Pointer Dropbox::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.dropboxapi.com/oauth2/token", HttpRequest::Type::POST);
  request->setParameter("grant_type", "authorization_code");
  request->setParameter("client_id", client_id());
  request->setParameter("client_secret", client_secret());
  request->setParameter("redirect_uri", redirect_uri());
  request->setParameter("code", authorization_code());
  return request;
}

HttpRequest::Pointer Dropbox::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
  return validateTokenRequest(input_data);
}

HttpRequest::Pointer Dropbox::Auth::validateTokenRequest(
    std::ostream& stream) const {
  Dropbox dropbox;
  dropbox.auth()->set_access_token(make_unique<Token>(*access_token()));

  HttpRequest::Pointer r = dropbox.listDirectoryRequest(
      *dropbox.rootDirectory(), static_cast<std::ostream&>(stream));
  dropbox.authorizeRequest(*r);
  return r;
}

IAuth::Token::Pointer Dropbox::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = token->token_;
  token->expires_in_ = -1;
  return token;
}

IAuth::Token::Pointer Dropbox::Auth::refreshTokenResponse(std::istream&) const {
  return make_unique<Token>(*access_token());
}

bool Dropbox::Auth::validateTokenResponse(std::istream&) const { return true; }

Dropbox::DataRequest::DataRequest(CloudProvider::Pointer p, IItem::Pointer item,
                                  std::function<void(IItem::Pointer)> callback)
    : GetItemDataRequest(p, item, callback, false) {
  result_ = std::async(std::launch::async, [this]() -> IItem::Pointer {
    int code = makeTemporaryLinkRequest();
    if (HttpRequest::isAuthorizationError(code)) {
      reauthorize();
      code = makeTemporaryLinkRequest();
    }
    if (HttpRequest::isSuccess(code)) {
      code = makeThumbnailRequest();
    }
    this->callback()(item_);
    return item_;
  });
}

void Dropbox::DataRequest::finish() { result_.wait(); }

IItem::Pointer Dropbox::DataRequest::result() {
  std::shared_future<IItem::Pointer> future = result_;
  return future.get();
}

int Dropbox::DataRequest::makeTemporaryLinkRequest() {
  HttpRequest request("https://api.dropboxapi.com/2/files/get_temporary_link",
                      HttpRequest::Type::POST);
  request.setHeaderParameter("Content-Type", "application/json");
  provider()->authorizeRequest(request);
  std::stringstream input, output, error;

  Json::Value parameter;
  parameter["path"] = item()->id();
  input << Json::FastWriter().write(parameter);

  int code = request.send(input, output, &error, httpCallback());
  if (HttpRequest::isSuccess(code)) {
    auto i = item()->copy();
    Json::Value response;
    output >> response;
    static_cast<Item*>(i.get())->set_url(response["link"].asString());
    item_ = std::move(i);
  } else {
    item_ = item();
  }
  return code;
}

int Dropbox::DataRequest::makeThumbnailRequest() {
  return 0;
  HttpRequest request("https://content.dropboxapi.com/2/files/get_thumbnail",
                      HttpRequest::Type::POST);
  request.setHeaderParameter("Content-Type", "");
  provider()->authorizeRequest(request);

  std::stringstream input, output, error;
  Json::Value parameter;
  parameter["path"] = item()->id();
  std::string str = Json::FastWriter().write(parameter);
  str.pop_back();
  std::cerr << str << "\n";
  request.setHeaderParameter("Dropbox-API-arg", str);
  int code = request.send(input, output, &error, httpCallback());
  if (HttpRequest::isSuccess(code)) {
    std::cerr << output.rdbuf();
    // auto i = item()->copy();

    // static_cast<Item*>(i.get())->set_thumbnail_url();
  } else {
    std::cerr << code << "\n";
    std::cerr << error.rdbuf() << "\n";
    // std::cerr << error.rdbuf() << "\n";
  }
}

}  // namespace cloudstorage
