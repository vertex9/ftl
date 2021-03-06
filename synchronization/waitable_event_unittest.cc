// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/ftl/synchronization/waitable_event.h"

#include <stdint.h>
#include <stdlib.h>

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "lib/ftl/arraysize.h"
#include "lib/ftl/macros.h"
#include "lib/ftl/synchronization/sleep.h"
#include "lib/ftl/test/timeout_tolerance.h"
#include "lib/ftl/time/stopwatch.h"

namespace ftl {
namespace {

constexpr TimeDelta kEpsilonTimeout = TimeDelta::FromMilliseconds(20);
constexpr TimeDelta kTinyTimeout = TimeDelta::FromMilliseconds(100);
constexpr TimeDelta kActionTimeout = TimeDelta::FromMilliseconds(10000);

// Sleeps for a "very small" amount of time.
void EpsilonRandomSleep() {
  TimeDelta duration =
      TimeDelta::FromMilliseconds(static_cast<unsigned>(rand()) % 20u);
  SleepFor(duration);
}

// AutoResetWaitableEvent ------------------------------------------------------

TEST(AutoResetWaitableEventTest, Basic) {
  AutoResetWaitableEvent ev;
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_TRUE(ev.IsSignaledForTest());
  ev.Wait();
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Reset();
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_TRUE(ev.IsSignaledForTest());
  ev.Reset();
  EXPECT_FALSE(ev.IsSignaledForTest());
  EXPECT_TRUE(ev.WaitWithTimeout(TimeDelta::Zero()));
  EXPECT_FALSE(ev.IsSignaledForTest());
  EXPECT_TRUE(ev.WaitWithTimeout(TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_TRUE(ev.IsSignaledForTest());
  EXPECT_FALSE(ev.WaitWithTimeout(TimeDelta::Zero()));
  EXPECT_FALSE(ev.IsSignaledForTest());
  EXPECT_TRUE(ev.WaitWithTimeout(TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_FALSE(ev.WaitWithTimeout(TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(ev.IsSignaledForTest());
}

TEST(AutoResetWaitableEventTest, MultipleWaiters) {
  AutoResetWaitableEvent ev;

  for (size_t i = 0u; i < 5u; i++) {
    std::atomic_uint wake_count(0u);
    std::vector<std::thread> threads;
    for (size_t j = 0u; j < 4u; j++) {
      threads.push_back(std::thread([&ev, &wake_count]() {
        if (rand() % 2 == 0)
          ev.Wait();
        else
          EXPECT_FALSE(ev.WaitWithTimeout(kActionTimeout));
        wake_count.fetch_add(1u);
        // Note: We can't say anything about the signaled state of |ev| here,
        // since the main thread may have already signaled it again.
      }));
    }

    // Unfortunately, we can't really wait for the threads to be waiting, so we
    // just sleep for a bit, and count on them having started and advanced to
    // waiting.
    SleepFor(kTinyTimeout + kTinyTimeout);

    for (size_t j = 0u; j < threads.size(); j++) {
      unsigned old_wake_count = wake_count.load();
      EXPECT_EQ(j, old_wake_count);

      // Each |Signal()| should wake exactly one thread.
      ev.Signal();

      // Poll for |wake_count| to change.
      while (wake_count.load() == old_wake_count)
        SleepFor(kEpsilonTimeout);

      EXPECT_FALSE(ev.IsSignaledForTest());

      // And once it's changed, wait a little longer, to see if any other
      // threads are awoken (they shouldn't be).
      SleepFor(kEpsilonTimeout);

      EXPECT_EQ(old_wake_count + 1u, wake_count.load());

      EXPECT_FALSE(ev.IsSignaledForTest());
    }

    // Having done that, if we signal |ev| now, it should stay signaled.
    ev.Signal();
    SleepFor(kEpsilonTimeout);
    EXPECT_TRUE(ev.IsSignaledForTest());

    for (auto& thread : threads)
      thread.join();

    ev.Reset();
  }
}

TEST(AutoResetWaitableEventTest, Timeouts) {
  static const unsigned kTestTimeoutsMs[] = {0, 10, 20, 40, 80};

  Stopwatch stopwatch;

  AutoResetWaitableEvent ev;

  for (size_t i = 0u; i < arraysize(kTestTimeoutsMs); i++) {
    TimeDelta timeout = TimeDelta::FromMilliseconds(kTestTimeoutsMs[i]);

    stopwatch.Start();
    EXPECT_TRUE(ev.WaitWithTimeout(timeout));
    TimeDelta elapsed = stopwatch.Elapsed();

    // It should time out after *at least* the specified amount of time.
    EXPECT_GE(elapsed, timeout - kTimeoutTolerance);
    // But we expect that it should time out soon after that amount of time.
    EXPECT_LT(elapsed, timeout + kEpsilonTimeout);
  }
}

// ManualResetWaitableEvent ----------------------------------------------------

TEST(ManualResetWaitableEventTest, Basic) {
  ManualResetWaitableEvent ev;
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_TRUE(ev.IsSignaledForTest());
  ev.Wait();
  EXPECT_TRUE(ev.IsSignaledForTest());
  ev.Reset();
  EXPECT_FALSE(ev.IsSignaledForTest());
  EXPECT_TRUE(ev.WaitWithTimeout(TimeDelta::Zero()));
  EXPECT_FALSE(ev.IsSignaledForTest());
  EXPECT_TRUE(ev.WaitWithTimeout(TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(ev.IsSignaledForTest());
  ev.Signal();
  EXPECT_TRUE(ev.IsSignaledForTest());
  EXPECT_FALSE(ev.WaitWithTimeout(TimeDelta::Zero()));
  EXPECT_TRUE(ev.IsSignaledForTest());
  EXPECT_FALSE(ev.WaitWithTimeout(TimeDelta::FromMilliseconds(1)));
  EXPECT_TRUE(ev.IsSignaledForTest());
}

TEST(ManualResetWaitableEventTest, SignalMultiple) {
  ManualResetWaitableEvent ev;

  for (size_t i = 0u; i < 10u; i++) {
    for (size_t num_waiters = 1u; num_waiters < 5u; num_waiters++) {
      std::vector<std::thread> threads;
      for (size_t j = 0u; j < num_waiters; j++) {
        threads.push_back(std::thread([&ev]() {
          EpsilonRandomSleep();

          if (rand() % 2 == 0)
            ev.Wait();
          else
            EXPECT_FALSE(ev.WaitWithTimeout(kActionTimeout));
        }));
      }

      EpsilonRandomSleep();

      ev.Signal();

      // The threads will only terminate once they've successfully waited (or
      // timed out).
      for (auto& thread : threads)
        thread.join();

      ev.Reset();
    }
  }
}

// Tries to test that threads that are awoken may immediately call |Reset()|
// without affecting other threads that are awoken.
TEST(ManualResetWaitableEventTest, SignalMultipleWaitReset) {
  ManualResetWaitableEvent ev;

  for (size_t i = 0u; i < 5u; i++) {
    std::vector<std::thread> threads;
    for (size_t j = 0u; j < 4u; j++) {
      threads.push_back(std::thread([&ev]() {
        if (rand() % 2 == 0)
          ev.Wait();
        else
          EXPECT_FALSE(ev.WaitWithTimeout(kActionTimeout));
        ev.Reset();
      }));
    }

    // Unfortunately, we can't really wait for the threads to be waiting, so we
    // just sleep for a bit, and count on them having started and advanced to
    // waiting.
    SleepFor(kTinyTimeout + kTinyTimeout);

    ev.Signal();

    // In fact, we may ourselves call |Reset()| immediately.
    ev.Reset();

    // The threads will only terminate once they've successfully waited (or
    // timed out).
    for (auto& thread : threads)
      thread.join();

    ASSERT_FALSE(ev.IsSignaledForTest());
  }
}

TEST(ManualResetWaitableEventTest, Timeouts) {
  static const unsigned kTestTimeoutsMs[] = {0, 10, 20, 40, 80};

  Stopwatch stopwatch;

  ManualResetWaitableEvent ev;

  for (size_t i = 0u; i < arraysize(kTestTimeoutsMs); i++) {
    TimeDelta timeout = TimeDelta::FromMilliseconds(kTestTimeoutsMs[i]);

    stopwatch.Start();
    EXPECT_TRUE(ev.WaitWithTimeout(timeout));
    TimeDelta elapsed = stopwatch.Elapsed();

    // It should time out after *at least* the specified amount of time.
    EXPECT_GE(elapsed, timeout - kTimeoutTolerance);
    // But we expect that it should time out soon after that amount of time.
    EXPECT_LT(elapsed, timeout + kEpsilonTimeout);
  }
}

}  // namespace
}  // namespace ftl
