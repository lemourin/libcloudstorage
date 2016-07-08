/*****************************************************************************
 * ListDirectoryRequest.cpp : ListDirectoryRequest implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "ListDirectoryRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "HttpCallback.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

ListDirectoryRequest::ListDirectoryRequest(std::shared_ptr<CloudProvider> p,
                                           IItem::Pointer directory,
                                           ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      callback_(std::move(callback)) {
  result_ = std::async(std::launch::async, [this]() {
    std::vector<IItem::Pointer> result;
    std::stringstream input_stream;
    HttpRequest::Pointer r =
        provider()->listDirectoryRequest(*directory_, input_stream);
    try {
      do {
        std::stringstream output_stream;
        std::string backup_data = input_stream.str();
        provider()->authorizeRequest(*r);
        int code =
            r->send(input_stream, output_stream, nullptr, httpCallback());
        if (HttpRequest::isSuccess(code)) {
          for (auto& t : provider()->listDirectoryResponse(output_stream, r,
                                                           input_stream)) {
            if (callback_) callback_->receivedItem(t);
            result.push_back(t);
          }
        } else if (HttpRequest::isAuthorizationError(code)) {
          if (!reauthorize()) {
            if (callback_) callback_->error("Failed to authorize.");
            return result;
          }
          input_stream = std::stringstream();
          input_stream << backup_data;
        } else if (HttpRequest::isClientError(code)) {
          if (callback_) callback_->error(output_stream.str());
          return result;
        } else
          break;
      } while (r);
    } catch (const HttpException& e) {
      if (callback_) callback_->error(e.what());
      return result;
    }

    if (callback_) callback_->done(result);
    return result;
  });
}

ListDirectoryRequest::~ListDirectoryRequest() { cancel(); }

void ListDirectoryRequest::finish() {
  if (result_.valid()) result_.get();
}

std::vector<IItem::Pointer> ListDirectoryRequest::result() {
  std::shared_future<std::vector<IItem::Pointer>> future = result_;
  if (!future.valid()) throw std::logic_error("Future invalid.");
  return future.get();
}

}  // namespace cloudstorage
