#ifndef CLOUDSTORAGE_THREADPOOLMOCK_H
#define CLOUDSTORAGE_THREADPOOLMOCK_H

#include "ICloudFactory.h"
#include "IThreadPool.h"
#include "gtest/gtest.h"

#include <cstdint>

class ThreadPoolFactoryMock : public cloudstorage::IThreadPoolFactory {
 public:
  MOCK_METHOD(cloudstorage::IThreadPool::Pointer, create, (uint32_t),
              (override));
};

class ThreadPoolMock : public cloudstorage::IThreadPool {
 public:
  MOCK_METHOD(void, schedule,
              (const Task& f, const std::chrono::system_clock::time_point&),
              (override));
};

#endif  // CLOUDSTORAGE_THREADPOOLMOCK_H
