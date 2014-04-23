// Copyright (c) 2014, Leif Walsh <leif.walsh@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <chrono>
#include <random>

/**
 * dyn_lock is a proof of concept of dynamo-style hashing applied to
 * readers-writer locking (the SharedTimedMutex concept).  A single
 * dyn_lock provides a shared mutex facility for a single resource.
 *
 * If read locks don't scale well, this spreads them out over K read
 * locks, at the expense of write locks, which now must take K locks.  In
 * general, if we pick K locks, readers must take R locks and writers must
 * take W locks such that R+W > K (and the choice of such R or W locks can
 * be arbitrary), but this implementation only uses R=1, W=K (and the
 * choice of the R=1 lock to take for lock_shared is chosen randomly).
 */
template<
  class SharedTimedMutex,
  size_t bits=4,
  class Engine=std::default_random_engine,
  class RandomGenerator=std::independent_bits_engine<Engine, bits, std::uint_fast64_t>
  >
class dyn_lock {
  friend class shared_timed_mutex;

  static constexpr size_t K() {
    return 1 << bits;
  }

  SharedTimedMutex _locks[K()];

public:
  class shared_timed_mutex {
#ifdef _GLIBCXX_USE_CLOCK_MONOTONIC
    typedef std::chrono::steady_clock 	  	__clock_t;
#else
    typedef std::chrono::high_resolution_clock 	__clock_t;
#endif

    dyn_lock &_dlock;
    RandomGenerator _gen;
    std::uint_fast64_t _lockid;

  public:
    shared_timed_mutex(dyn_lock &dlock) : _dlock(dlock) {}

    void lock() {
      for (size_t i = 0; i < K(); ++i) {
        _dlock._locks[i].lock();
      }
    }

    void try_lock() {
      bool ret = true;
      size_t i;
      for (i = 0; i < K(); ++i) {
        if (!_dlock._locks[i].try_lock()) {
          ret = false;
          break;
        }
      }
      if (!ret) {
        while (--i >= 0) {
          _dlock._locks[i].unlock();
        }
      }
      return ret;
    }

    template<typename _Clock, typename _Duration>
    bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& __atime) {
      bool ret = true;
      size_t i;
      for (i = 0; i < K(); ++i) {
        if (!_dlock._locks[i].try_lock_until(__atime)) {
          ret = false;
          break;
        }
      }
      if (!ret) {
        while (--i >= 0) {
          _dlock._locks[i].unlock();
        }
      }
      return ret;
    }

    template<typename _Rep, typename _Period>
    bool try_lock_for(const std::chrono::duration<_Rep, _Period>& __rtime) {
      auto __rt = std::chrono::duration_cast<__clock_t::duration>(__rtime);
      if (std::ratio_greater<__clock_t::period, _Period>()) {
        ++__rt;
      }

      return try_lock_until(__clock_t::now() + __rt);
    }

    void unlock() {
      for (size_t i = 0; i < K(); ++i) {
        _dlock._locks[i].unlock();
      }
    }

    void lock_shared() {
      _lockid = _gen();
      _dlock._locks[_lockid].lock_shared();
    }

    bool try_lock_shared() {
      _lockid = _gen();
      return _dlock._locks[_lockid].try_lock_shared();
    }

    template<typename _Clock, typename _Duration>
    bool try_lock_shared_until(const std::chrono::time_point<_Clock, _Duration>& __atime) {
      _lockid = _gen();
      return _dlock._locks[_lockid].try_lock_shared_until(__atime);
    }

    template<typename _Rep, typename _Period>
    bool try_lock_shared_for(const std::chrono::duration<_Rep, _Period>& __rtime) {
      _lockid = _gen();
      return _dlock._locks[_lockid].try_lock_shared_for(__rtime);
    }

    void unlock_shared() {
      _dlock._locks[_lockid].unlock_shared();
    }
  };
};
