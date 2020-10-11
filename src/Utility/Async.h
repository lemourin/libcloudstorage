#ifndef ASYNC_H
#define ASYNC_H

#ifdef __cpp_lib_coroutine

#include <coroutine>

#include "Utility/Promise.h"

namespace cloudstorage {

template <typename T>
class Async {
 public:
  struct promise_type {
    Async get_return_object() {
      return Async{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    void return_value(T t) {}

    static std::suspend_always initial_suspend() noexcept { return {}; }
    static std::suspend_always final_suspend() noexcept { return {}; }
  };

  Async(::util::Promise<T>&& promise) : promise_(std::move(promise)) {}
  Async(std::coroutine_handle<promise_type> handle) {}

 private:
  ::util::Promise<T> promise_;
};

}  // namespace cloudstorage

#endif

#endif
