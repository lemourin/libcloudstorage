/*****************************************************************************
 * Request.cpp : Request implementation
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

#include "Request.h"

#include "CloudProvider.h"
#include "Utility.h"

namespace cloudstorage {

Request::Request(CloudProvider* provider) : provider_(provider) {}

void Request::cancel() {
  // TODO
}

ListDirectoryRequest::ListDirectoryRequest(CloudProvider* p,
                                           IItem::Pointer directory,
                                           ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      callback_(std::move(callback)) {
  result_ = std::async(std::launch::async, [this]() {
    std::vector<IItem::Pointer> result;
    std::string page_token;
    bool retry;
    do {
      retry = false;
      provider()->waitForAuthorized();
      HttpRequest::Pointer r = provider()->listDirectoryRequest(
          *directory_, page_token, input_stream(), provider()->access_token());
      std::stringstream output_stream;
      if (HttpRequest::isSuccess(r->send(input_stream(), output_stream))) {
        page_token = "";
        for (auto& t :
             provider()->listDirectoryResponse(output_stream, page_token)) {
          if (callback_)
            callback_->receivedItem(std::move(t));
          else
            result.push_back(std::move(t));
        }
      } else {
        if (!provider()->authorize()) throw AuthorizationException();
        retry = true;
      }
    } while (!page_token.empty() || retry);
    return result;
  });
}

std::vector<IItem::Pointer> ListDirectoryRequest::result() {
  if (!result_.valid()) throw std::logic_error("Future invalid.");
  return result_.get();
}

}  // namespace cloudstorage
