#ifndef PROMISE_H
#define PROMISE_H

#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#ifdef __cpp_lib_coroutine
#include <coroutine>
#define HAVE_COROUTINES
#define HAVE_COROUTINE_SUPPORT
#endif

#ifdef __has_include
#if __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#define HAVE_EXPERIMENTAL_COROUTINES
#define HAVE_COROUTINE_SUPPORT
#endif
#endif

namespace util {

#ifdef HAVE_COROUTINES
template <typename T>
using coroutine_handle = std::coroutine_handle<T>;
using suspend_never = std::suspend_never;
#endif

#ifdef HAVE_EXPERIMENTAL_COROUTINES
template <typename T>
using coroutine_handle = std::experimental::coroutine_handle<T>;
using suspend_never = std::experimental::suspend_never;
#endif

namespace v2 {
namespace detail {

template <class... Ts>
class Promise;

template <typename T>
struct PromisedType {
  using type = void;
};

template <typename... Ts>
struct PromisedType<Promise<Ts...>> {
  using type = std::tuple<Ts...>;
};

template <typename T>
struct IsPromise {
  static constexpr bool value = false;
};

template <typename... Ts>
struct IsPromise<Promise<Ts...>> {
  static constexpr bool value = true;
};

template <typename T>
struct IsTuple {
  static constexpr bool value = false;
};

template <typename... Ts>
struct IsTuple<std::tuple<Ts...>> {
  static constexpr bool value = true;
};

template <int... T>
struct Sequence {
  template <class Callable, class Tuple>
  static void call(Callable&& callable, Tuple&& d) {
    callable(std::move(std::get<T>(d))...);
  }
};

template <int G>
struct SequenceGenerator {
  template <class Sequence>
  struct Extract;

  template <int... Ints>
  struct Extract<Sequence<Ints...>> {
    using type = Sequence<Ints..., G - 1>;
  };

  using type = typename Extract<typename SequenceGenerator<G - 1>::type>::type;
};

template <>
struct SequenceGenerator<0> {
  using type = Sequence<>;
};

template <class Element>
struct AppendElement;

template <class T1, class T2>
struct Concatenate;

template <class... Fst, class... Nd>
struct Concatenate<std::tuple<Fst...>, std::tuple<Nd...>> {
  using type = std::tuple<Fst..., Nd...>;
};

template <class T>
struct Flatten;

template <class First, class... Rest>
struct Flatten<std::tuple<First, Rest...>> {
  using rest = typename Flatten<std::tuple<Rest...>>::type;
  using type = typename Concatenate<First, rest>::type;
};

template <>
struct Flatten<std::tuple<>> {
  using type = std::tuple<>;
};

template <typename T>
struct SlotsReserved {
  static constexpr int value = 1;
};

template <typename... T>
struct SlotsReserved<Promise<T...>> {
  static constexpr int value = std::tuple_size<std::tuple<T...>>::value;
};

template <class... Ts>
class Promise {
 public:
  Promise() : data_(std::make_shared<CommonData>()) {}

#ifdef HAVE_COROUTINE_SUPPORT
  class promise_type_impl {
   public:
    Promise& get_return_object() { return promise_; }

    suspend_never initial_suspend() noexcept { return {}; }
    suspend_never final_suspend() noexcept { return {}; }

    void unhandled_exception() { std::terminate(); }

   protected:
    Promise promise_;
  };

  class promise_type_non_void : public promise_type_impl {
   public:
    template <typename... ValuesT>
    void return_value(ValuesT&&... values) {
      this->promise_.fulfill(std::forward<ValuesT>(values)...);
    }
  };

  class promise_type_void : public promise_type_impl {
   public:
    void return_void() { this->promise_.fulfill(); }
  };

  using promise_type =
      std::conditional_t<(sizeof...(Ts) > 0), promise_type_non_void,
                         promise_type_void>;

  bool await_ready() {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    return data_->ready_ || data_->error_ready_;
  }

  auto await_resume() {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    if (data_->exception_) {
      std::rethrow_exception(data_->exception_);
    } else {
      return std::get<Ts...>(data_->value_);
    }
  }

  void await_suspend(coroutine_handle<void> handle) {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    data_->handle_ = handle;
  }
#endif

#if (__cplusplus >= 201703L)
  template <typename Callable>
  using ReturnType = std::invoke_result_t<Callable, Ts...>;
#else
  template <typename Callable>
  using ReturnType = std::result_of_t<Callable(Ts...)>;
#endif

  template <class First, class Promise>
  struct Prepend;

  template <class First, class... Args>
  struct Prepend<First, Promise<Args...>> {
    using type = Promise<First, Args...>;
  };

  template <class T>
  using ResolvedType = typename std::conditional<
      IsPromise<T>::value, typename PromisedType<T>::type, std::tuple<T>>::type;

  template <class T>
  struct PromiseType;

  template <class... T>
  struct PromiseType<std::tuple<T...>> {
    template <class R>
    struct Replace;
    template <class... R>
    struct Replace<std::tuple<R...>> {
      using type = Promise<R...>;
    };
    using type = typename Replace<
        typename Flatten<std::tuple<ResolvedType<T>...>>::type>::type;
  };

  template <class T>
  struct ReturnedTuple;

  template <class... T>
  struct ReturnedTuple<Promise<T...>> {
    using type = std::tuple<T...>;
  };

  template <int Index, int ResultIndex, class T>
  struct EvaluateThen;

  template <int Index, int ResultIndex, class... T>
  struct EvaluateThen<Index, ResultIndex, std::tuple<T...>> {
    template <class PromiseType, class ResultTuple>
    static void call(std::tuple<T...>&& d, const PromiseType& p,
                     const std::shared_ptr<ResultTuple>& result) {
      using CurrentType =
          typename std::tuple_element<Index, std::tuple<T...>>::type;
      auto element = std::get<Index>(d);
      auto continuation = [d = std::move(d), p, result]() mutable {
        EvaluateThen<Index - 1, ResultIndex - SlotsReserved<CurrentType>::value,
                     std::tuple<T...>>::call(std::move(d), p, result);
      };
      AppendElement<CurrentType>::template call<ResultIndex>(
          std::forward<CurrentType>(element), p, result, continuation);
    }
  };

  template <int ResultIndex, class... T>
  struct EvaluateThen<-1, ResultIndex, std::tuple<T...>> {
    template <class Q>
    struct Call;

    template <class... Q>
    struct Call<std::tuple<Q...>> {
      template <class Promise>
      static void call(std::tuple<T...>&&, const Promise& p,
                       const std::shared_ptr<std::tuple<Q...>>& result) {
        SequenceGenerator<std::tuple_size<std::tuple<Q...>>::value>::type::call(
            [p](Q&&... args) { p.fulfill(std::forward<Q>(args)...); }, *result);
      }
    };

    template <class Promise, class ResultTuple>
    static void call(std::tuple<T...>&& d, const Promise& p,
                     const std::shared_ptr<ResultTuple>& result) {
      Call<ResultTuple>::call(std::move(d), p, result);
    }
  };

  template <typename Callable, typename Tuple = ReturnType<Callable>,
            typename ReturnedPromise =
                typename PromiseType<ReturnType<Callable>>::type,
            typename = typename std::enable_if<IsTuple<Tuple>::value>::type>
  ReturnedPromise then(Callable&& cb) {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    ReturnedPromise promise;
    promise.data_->on_cancel_ = propagateCancel();
    data_->on_fulfill_ = [promise, cb = std::forward<Callable>(cb)](
                             Ts&&... args) mutable {
      try {
        using StateTuple = typename ReturnedTuple<ReturnedPromise>::type;
        auto r = cb(std::forward<Ts>(args)...);
        auto common_state = std::make_shared<StateTuple>();
        EvaluateThen<static_cast<int>(std::tuple_size<Tuple>::value) - 1,
                     static_cast<int>(std::tuple_size<StateTuple>::value) - 1,
                     Tuple>::call(std::move(r), promise, common_state);
      } catch (...) {
        promise.reject(std::current_exception());
      }
    };
    data_->on_reject_ = [promise](std::exception_ptr&& e) {
      promise.reject(std::move(e));
    };
    if (data_->error_ready_) {
      auto callback = std::move(data_->on_reject_);
      data_->on_fulfill_ = nullptr;
      data_->on_reject_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      callback(std::move(data_->exception_));
    } else if (data_->ready_) {
      auto callback = std::move(data_->on_fulfill_);
      data_->on_fulfill_ = nullptr;
      data_->on_reject_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      SequenceGenerator<std::tuple_size<std::tuple<Ts...>>::value>::type::call(
          callback, data_->value_);
    }
    return promise;
  }

  template <typename Callable, typename = typename std::enable_if<std::is_void<
                                   ReturnType<Callable>>::value>::type>
  Promise<> then(Callable&& cb) {
    return then([cb = std::forward<Callable>(cb)](Ts&&... args) mutable {
      cb(std::forward<Ts>(args)...);
      return std::make_tuple();
    });
  }

  template <typename Callable,
            typename = typename std::enable_if<
                !IsTuple<ReturnType<Callable>>::value &&
                !std::is_void<ReturnType<Callable>>::value>::type,
            typename ReturnedPromise =
                typename PromiseType<std::tuple<ReturnType<Callable>>>::type>
  ReturnedPromise then(Callable&& cb) {
    return then([cb = std::forward<Callable>(cb)](Ts&&... args) mutable {
      return std::make_tuple(cb(std::forward<Ts>(args)...));
    });
  }

  template <typename Exception, typename Callable>
  Promise<Ts...> error(Callable&& e) {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    Promise<Ts...> promise;
    promise.data_->on_cancel_ = propagateCancel();
    data_->on_reject_ = [promise, cb = std::forward<Callable>(e)](
                            std::exception_ptr&& e) mutable {
      try {
        std::rethrow_exception(std::move(e));
      } catch (Exception& exception) {
        cb(std::move(exception));
      } catch (...) {
        promise.reject(std::current_exception());
      }
    };
    data_->on_fulfill_ = [promise](Ts&&... args) {
      promise.fulfill(std::forward<Ts>(args)...);
    };
    if (data_->error_ready_) {
      auto callback = std::move(data_->on_reject_);
      data_->on_reject_ = nullptr;
      data_->on_fulfill_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      callback(std::move(data_->exception_));
    }
    return promise;
  }

  template <class Func>
  void cancel(Func&& f) {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    if (data_->ready_ || data_->error_ready_) return;
    data_->on_cancel_ = std::forward<Func>(f);
    if (data_->cancelled_) {
      auto cb = std::move(data_->on_cancel_);
      data_->on_cancel_ = nullptr;
      lock.unlock();
      cb();
    }
  }

  template <typename Callable>
  Promise<Ts...> error_ptr(Callable&& e) {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    Promise<Ts...> promise;
    promise.data_->on_cancel_ = propagateCancel();
    data_->on_reject_ = e;
    data_->on_fulfill_ = [promise](Ts&&... args) {
      promise.fulfill(std::forward<Ts>(args)...);
    };
    if (data_->error_ready_) {
      auto callback = std::move(data_->on_reject_);
      data_->on_reject_ = nullptr;
      data_->on_fulfill_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      callback(std::move(data_->exception_));
    }
    return promise;
  }

  template <typename... Ds>
  void fulfill(Ds&&... value) const {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    data_->ready_ = true;
    if (data_->on_fulfill_) {
      auto callback = std::move(data_->on_fulfill_);
      data_->on_reject_ = nullptr;
      data_->on_fulfill_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      callback(std::forward<Ts>(value)...);
    } else {
      data_->value_ = std::make_tuple(std::move(value)...);
#ifdef HAVE_COROUTINE_SUPPORT
      if (data_->handle_) {
        lock.unlock();
        data_->handle_.resume();
      }
#endif
    }
  }

  template <class Exception,
            typename = typename std::enable_if<std::is_base_of<
                std::exception,
                typename std::remove_reference<Exception>::type>::value>::type>
  void reject(Exception&& e) const {
    reject(std::make_exception_ptr(std::forward<Exception>(e)));
  }

  void reject(std::exception_ptr&& e) const {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    data_->error_ready_ = true;
    if (data_->on_reject_) {
      auto callback = std::move(data_->on_reject_);
      data_->on_reject_ = nullptr;
      data_->on_fulfill_ = nullptr;
      data_->on_cancel_ = nullptr;
      lock.unlock();
      callback(std::move(e));
    } else {
      data_->exception_ = std::move(e);
#ifdef HAVE_COROUTINE_SUPPORT
      if (data_->handle_) {
        lock.unlock();
        data_->handle_.resume();
      }
#endif
    }
  }

  void cancel() const {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    data_->cancelled_ = true;
    if (data_->on_cancel_) {
      auto callback = std::move(data_->on_cancel_);
      lock.unlock();
      callback();
    }
  }

  uintptr_t id() const { return reinterpret_cast<uintptr_t>(data_.get()); }

 private:
  template <class... T>
  friend class Promise;

  template <class... T>
  auto propagateCancel() const {
    return [data_weak = std::weak_ptr<CommonData>(data_)] {
      auto data = data_weak.lock();
      if (data) {
        auto lock = std::unique_lock<std::mutex>(data->mutex_);
        if (data->on_cancel_) {
          auto callback = std::move(data->on_cancel_);
          data->on_cancel_ = nullptr;
          lock.unlock();
          callback();
        }
      }
    };
  }

  struct CommonData {
    std::mutex mutex_;
    bool ready_ = false;
    bool error_ready_ = false;
    bool cancelled_ = false;
    std::function<void(Ts&&...)> on_fulfill_;
    std::function<void(std::exception_ptr&&)> on_reject_;
    std::function<void()> on_cancel_;
    std::tuple<Ts...> value_;
    std::exception_ptr exception_;
#ifdef HAVE_COROUTINE_SUPPORT
    coroutine_handle<void> handle_;
#endif
  };
  std::shared_ptr<CommonData> data_;
};  // namespace detail

template <class Element>
struct AppendElement {
  template <int Index, class PromiseType, class ResultTuple, class Callable>
  static void call(Element&& result, const PromiseType&,
                   const std::shared_ptr<ResultTuple>& output,
                   Callable&& callable) {
    std::get<Index>(*output) = std::forward<Element>(result);
    callable();
  }
};

template <int Index, typename Tuple, class... Args>
struct SetRange;

template <int Index, typename Tuple>
struct SetRange<Index, Tuple> {
  static void call(Tuple&) {}
};

template <int Index, typename Tuple, class First, class... Rest>
struct SetRange<Index, Tuple, First, Rest...> {
  static void call(Tuple& d, First&& f, Rest&&... rest) {
    std::get<Index>(d) = std::forward<First>(f);
    SetRange<Index + 1, Tuple, Rest...>::call(d, std::forward<Rest>(rest)...);
  }
};

template <class... PromisedType>
struct AppendElement<Promise<PromisedType...>> {
  template <int Index, class PromiseType, class ResultTuple, class Callable>
  static void call(Promise<PromisedType...>&& d, const PromiseType& p,
                   const std::shared_ptr<ResultTuple>& output,
                   Callable&& callable) {
    d.then([output, callable = std::forward<Callable>(callable)](
               PromisedType&&... args) mutable {
       SetRange<Index -
                    static_cast<int>(
                        std::tuple_size<std::tuple<PromisedType...>>::value) +
                    1,
                ResultTuple, PromisedType...>::call(*output,
                                                    std::forward<PromisedType>(
                                                        args)...);
       callable();
       return std::make_tuple();
     })
        .error_ptr([p](std::exception_ptr&& e) { p.reject(std::move(e)); });
  }
};

}  // namespace detail
}  // namespace v2

using v2::detail::Promise;
}  // namespace util

#endif
