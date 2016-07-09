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
    std::string page_token;
    do {
      std::stringstream output_stream;
      int code = sendRequest(
          [this, &page_token](std::ostream& i) {
            return provider()->listDirectoryRequest(*directory_, page_token, i);
          },
          output_stream);
      if (HttpRequest::isSuccess(code)) {
        page_token = "";
        for (auto& t :
             provider()->listDirectoryResponse(output_stream, page_token)) {
          if (callback_) callback_->receivedItem(t);
          result.push_back(t);
        }
      }
    } while (!page_token.empty());

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

void ListDirectoryRequest::error(int code, const std::string& description) {
  if (callback_) callback_->error(std::to_string(code) + ": " + description);
}

}  // namespace cloudstorage
