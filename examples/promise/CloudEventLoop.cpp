/*****************************************************************************
 * CloudEventLoop.cpp
 *
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN
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
#include "CloudEventLoop.h"

#include "Utility/Utility.h"

namespace cloudstorage {

CloudEventLoop::CloudEventLoop()
    : impl_(std::make_shared<priv::LoopImpl>(this)) {}

CloudEventLoop::~CloudEventLoop() {
  impl_->clear();
  impl_->process_events();
}

void CloudEventLoop::onEventAdded() {}

void CloudEventLoop::processEvents() { impl_->process_events(); }

namespace priv {

LoopImpl::LoopImpl(CloudEventLoop *loop) : last_tag_(), event_loop_(loop) {}

void LoopImpl::add(uint64_t tag,
                   const std::shared_ptr<IGenericRequest> &request) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = pending_.find(tag);
  if (it == pending_.end()) {
    pending_.insert({tag, request});
  } else {
    pending_.erase(it);
  }
}

void LoopImpl::fulfill(uint64_t tag, const std::function<void()> &f) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = pending_.find(tag);
    if (it != pending_.end()) {
      auto request = std::move(it->second);
      pending_.erase(it);
      lock.unlock();
      invoke([request = std::move(request)] { request->finish(); });
    } else {
      pending_.insert({tag, nullptr});
    }
  }
  invoke(f);
}

void LoopImpl::invoke(const std::function<void()> &f) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    events_.emplace_back(f);
  }
  event_loop_->onEventAdded();
}

void LoopImpl::process_events() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (size_t i = 0; i < events_.size(); i++) {
    lock.unlock();
    events_[i]();
    lock.lock();
  }
  events_.clear();
}

void LoopImpl::clear() {
  std::unique_lock<std::mutex> lock(mutex_);
  while (!pending_.empty()) {
    auto it = pending_.begin();
    auto request = std::move(it->second);
    pending_.erase(it);
    if (request) {
      lock.unlock();
      request->cancel();
      lock.lock();
    }
  }
}

uint64_t LoopImpl::next_tag() { return last_tag_++; }

}  // namespace priv

Exception::Exception(int code, const std::string &description)
    : code_(code), description_(description) {}

Exception::Exception(const std::shared_ptr<Error> &e)
    : Exception(e->code_, e->description_) {}

// namespace priv

}  // namespace cloudstorage
