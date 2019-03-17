#ifndef PROMISE_H
#define PROMISE_H

#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>

namespace util {
namespace detail {

template <class Value>
class Promise;

template <>
class Promise<void>;

template <class Value, class PromiseValue>
struct FulfillImpl {
  template <class Callable, class Promise>
  std::function<void(const Value&)> operator()(const Promise& p,
                                               const Callable& callable) const {
    return [=](const Value& d) { p.fulfill(callable(d)); };
  }
};

template <class Value>
struct FulfillImpl<Value, void> {
  template <class Callable, class Promise>
  std::function<void(const Value&)> operator()(const Promise& p,
                                               const Callable& callable) const {
    return [=](const Value& d) {
      callable(d);
      p.fulfill();
    };
  }
};

template <class PromiseValue>
struct FulfillImpl<void, PromiseValue> {
  template <class Callable, class Promise>
  std::function<void()> operator()(const Promise& p,
                                   const Callable& callable) const {
    return [=] { p.fulfill(callable()); };
  }
};

template <>
struct FulfillImpl<void, void> {
  template <class Callable, class Promise>
  std::function<void()> operator()(const Promise& p,
                                   const Callable& callable) const {
    return [=] {
      callable();
      p.fulfill();
    };
  }
};

template <class Value, class Callable>
struct ReturnType {
  using type = typename std::result_of<Callable(Value)>::type;
};

template <class Callable>
struct ReturnType<void, Callable> {
  using type = typename std::result_of<Callable()>::type;
};

template <class Value, class Callable>
using FulfillCallable =
    FulfillImpl<Value, typename ReturnType<Value, Callable>::type>;

template <class Value>
struct Fulfill {
  template <class Promise>
  std::function<void(const Value&)> operator()(const Promise& p) const {
    return [=](const Value& d) { p.fulfill(d); };
  }
};

template <>
struct Fulfill<void> {
  template <class Promise>
  std::function<void()> operator()(const Promise& p) const {
    return [=] { p.fulfill(); };
  }
};

template <class Value, typename IsPromise>
struct Then;

template <class Value>
struct PromiseType {};

template <class Value>
struct PromiseType<Promise<Value>> {
  using Type = Value;
};

template <class T>
struct IsPromise {
  using type = std::false_type;
};

template <class T>
struct IsPromise<Promise<T>> {
  using type = std::true_type;
};

template <class T>
struct Callback {
  using type = std::function<void(const T&)>;
};

template <>
struct Callback<void> {
  using type = std::function<void()>;
};

template <class Value>
class PromiseCommon {
 public:
  PromiseCommon() : data_(std::make_shared<Data>()) {}

  Promise<Value>& error(std::function<void(const std::exception&)> f) {
    std::unique_lock<std::mutex> lock(this->data_->mutex_);
    if (this->data_->ready_) {
      lock.unlock();
      try {
        this->get();
      } catch (const std::exception& e) {
        f(e);
      }
    } else {
      this->data_->error_ = f;
    }
    return static_cast<Promise<Value>&>(*this);
  }

  void fulfill_exception(std::exception_ptr e) const {
    data_->result_.set_exception(e);
    std::unique_lock<std::mutex> lock(data_->mutex_);
    data_->ready_ = true;
    auto exception = data_->exception_;
    auto error = data_->error_;
    lock.unlock();
    if (exception) exception(e);
    if (error) {
      try {
        std::rethrow_exception(e);
      } catch (const std::exception& e) {
        error(e);
      }
    }
  }

  struct Data {
    Data() : future_(result_.get_future()) {}

    std::mutex mutex_;
    bool ready_ = false;
    std::promise<Value> result_;
    std::shared_future<Value> future_;
    typename Callback<Value>::type callback_;
    std::function<void(std::exception_ptr)> exception_;
    std::function<void(const std::exception&)> error_;
  };

  Value get() const {
    auto future = this->data_->future_;
    return future.get();
  }

  std::shared_ptr<Data> data_;
};

template <>
class Promise<void> : public PromiseCommon<void> {
 public:
  template <class Callable>
  using ThenType =
      Then<void,
           typename IsPromise<typename std::result_of<Callable()>::type>::type>;

  template <class Callable>
  typename ThenType<Callable>::template ReturnType<Callable> then(
      const Callable& callable) {
    return ThenType<Callable>()(*this, callable);
  }

  void fulfill() const {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    if (data_->callback_) {
      data_->ready_ = true;
      lock.unlock();
      try {
        data_->callback_();
        data_->result_.set_value();
      } catch (const std::exception& e) {
        data_->exception_(std::current_exception());
      }
    }
  }

 private:
  void set_callback(std::function<void()> callback,
                    std::function<void(std::exception_ptr)> exception) const {
    std::unique_lock<std::mutex> lock(data_->mutex_);
    if (data_->ready_ && callback) {
      lock.unlock();
      try {
        get();
        callback();
      } catch (const std::exception&) {
        exception(std::current_exception());
      }
    } else {
      data_->callback_ = callback;
      data_->exception_ = exception;
    }
  }

  template <class, class>
  friend class Then;

  template <class>
  friend struct ForwardThen;
};

template <class Value>
class Promise : public PromiseCommon<Value> {
 public:
  template <class Callable>
  using ThenType =
      Then<Value, typename IsPromise<
                      typename std::result_of<Callable(Value)>::type>::type>;
  template <class Callable>
  typename ThenType<Callable>::template ReturnType<Callable> then(
      const Callable& callable) {
    return ThenType<Callable>()(*this, callable);
  }

  void fulfill(const Value& d) const {
    std::unique_lock<std::mutex> lock(this->data_->mutex_);
    if (this->data_->callback_) {
      this->data_->ready_ = true;
      lock.unlock();
      try {
        this->data_->callback_(d);
        this->data_->result_.set_value(d);
      } catch (const std::exception& e) {
        this->data_->exception_(std::current_exception());
      }
    }
  }

 private:
  template <class, class>
  friend class Then;

  template <class>
  friend struct ForwardThen;

  void set_callback(std::function<void(const Value&)> callback,
                    std::function<void(std::exception_ptr)> exception) const {
    std::unique_lock<std::mutex> lock(this->data_->mutex_);
    if (this->data_->ready_ && callback) {
      lock.unlock();
      try {
        callback(this->get());
      } catch (const std::exception& e) {
        exception(std::current_exception());
      }
    } else {
      this->data_->callback_ = callback;
      this->data_->exception_ = exception;
    }
  }
};

template <class Value>
struct ForwardThen {
  template <class Callable>
  using ReturnType = typename detail::ReturnType<Value, Callable>::type;

  template <class Promise, class Callable>
  std::function<void(const Value&)> operator()(Promise& d,
                                               const Callable& callback) {
    return [=](const Value& value) {
      using Type = typename PromiseType<ReturnType<Callable>>::Type;
      auto r = callback(value);
      r.set_callback(Fulfill<Type>()(d),
                     [=](std::exception_ptr e) { d.fulfill_exception(e); });
    };
  }
};

template <>
struct ForwardThen<void> {
  template <class Callable>
  using ReturnType = typename detail::ReturnType<void, Callable>::type;

  template <class Promise, class Callable>
  std::function<void()> operator()(Promise& d, const Callable& callback) {
    return [=] {
      using Type = typename PromiseType<ReturnType<Callable>>::Type;
      auto r = callback();
      r.set_callback(Fulfill<Type>()(d),
                     [=](std::exception_ptr e) { d.fulfill_exception(e); });
    };
  }
};

template <class Value>
struct Then<Value, std::true_type> {
  template <class Callable>
  using ReturnType = typename detail::ReturnType<Value, Callable>::type;

  template <class Callable>
  ReturnType<Callable> operator()(Promise<Value>& d, const Callable& callback) {
    ReturnType<Callable> result;
    d.set_callback(ForwardThen<Value>()(result, callback),
                   [=](std::exception_ptr e) { result.fulfill_exception(e); });
    return result;
  }
};

template <class Value>
struct Then<Value, std::false_type> {
  template <class Callable>
  using ReturnType =
      Promise<typename detail::ReturnType<Value, Callable>::type>;

  template <class Callable>
  ReturnType<Callable> operator()(Promise<Value>& p, const Callable& callable) {
    ReturnType<Callable> result;
    p.set_callback(FulfillCallable<Value, Callable>()(result, callable),
                   [=](std::exception_ptr e) { result.fulfill_exception(e); });
    return result;
  }
};

}  // namespace detail

using detail::Promise;

}  // namespace util

#endif
