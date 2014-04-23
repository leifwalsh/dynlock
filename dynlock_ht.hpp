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

#include <algorithm>
#include <functional>
#include <random>

/**
 * dyn_lock is a proof of concept of dynamo-style hashing applied to
 * readers-writer locking (the SharedTimedMutex concept).  A single
 * dyn_lock provides a shared mutex facility on keys of type T.
 *
 * If read locks don't scale well, this spreads them out over K read
 * locks, at the expense of write locks, which now must take K locks.  In
 * general, if we choose K hash functions, readers must take R locks and
 * writers must take W locks such that R+W > K.
 */
template<class SharedMutex, size_t N=1<<10, size_t K=8, size_t R=1, size_t W=K>
class dyn_lock {
  SharedMutex _locks[N];

  template<class T>
  static size_t h(size_t i, const T &key) {
    static std::hash<size_t> ih;
    static std::hash<T> th;
    return ih(i) ^ th(key);
  }

  template<class T>
  SharedMutex &get_lock(size_t i, const T &key) {
    return _locks[h(i, key) % N];
  }

  static std::vector<size_t> array_of_K() {
    std::vector<size_t> vec(K);
    size_t i = 0;
    std::generate(vec.begin(), vec.end(), [&i]() { return i++; });
    return vec;
  }

  static std::vector<size_t> hash_choices(size_t k) {
    static const std::vector<size_t> all = array_of_K();
    std::vector<size_t> choices = all;
    std::random_shuffle(choices.begin(), choices.end());
    choices.resize(k);
    std::sort(choices.begin(), choices.end());
    return choices;
  }

public:
  typedef std::vector<size_t> token;

  template<class T>
  token lock(const T &key) {
    token tok = hash_choices(W);
    std::for_each(tok.begin(), tok.end(), [this, key](size_t i) {
        get_lock(i, key).lock();
      });
    return tok;
  }
  template<class T>
  void unlock(const T &key, const token &tok) {
    std::for_each(tok.begin(), tok.end(), [this, key](size_t i) {
        get_lock(i, key).unlock();
      });
  }

  template<class T>
  token lock_shared(const T &key) {
    token tok = hash_choices(W);
    std::for_each(tok.begin(), tok.end(), [this, key](size_t i) {
        get_lock(i, key).lock_shared();
      });
    return tok;
  }
  template<class T>
  void unlock_shared(const T &key, const token &tok) {
    std::for_each(tok.begin(), tok.end(), [this, key](size_t i) {
        get_lock(i, key).unlock_shared();
      });
  }

  template<class T>
  class shared_mutex {
    dyn_lock &_dlock;
    const T &_key;
    token _tok;

  public:
    shared_mutex(dyn_lock &dlock, const T &key)
      : _dlock(dlock), _key(key)
    {}

    void lock() {
      _tok = _dlock.lock(_key);
    }

    void unlock() {
      _dlock.unlock(_key, _tok);
    }

    void lock_shared() {
      _tok = _dlock.lock_shared(_key);
    }

    void unlock_shared() {
      _dlock.unlock_shared(_key, _tok);
    }
  };
};

// Optimize for the special case of R=1, W=K, we don't need to mess around
// with vectors in this case.
template<class SharedMutex, size_t N, size_t K>
class dyn_lock<SharedMutex, N, K, 1, K> {
  SharedMutex _locks[N];

  template<class T>
  static size_t h(size_t i, const T &key) {
    static std::hash<size_t> ih;
    static std::hash<T> th;
    return ih(i) ^ th(key);
  }

  template<class T>
  SharedMutex &get_lock(size_t i, const T &key) {
    return _locks[h(i, key) % N];
  }

public:
  typedef size_t token;

  template<class T>
  token lock(const T &key) {
    for (size_t i = 0; i < K; ++i) {
      get_lock(i, key).lock();
    }
    return 0;
  }
  template<class T>
  void unlock(const T &key, const token &) {
    for (size_t i = 0; i < K; ++i) {
      get_lock(i, key).unlock();
    }
  }

  template<class T>
  token lock_shared(const T &key) {
    static std::default_random_engine generator;
    static std::uniform_int_distribution<size_t> dist(0, K-1);
    size_t i = dist(generator);
    get_lock(i, key).lock_shared();
    return i;
  }
  template<class T>
  void unlock_shared(const T &key, const token &tok) {
    get_lock(tok, key).unlock_shared();
  }

  template<class T>
  class shared_mutex {
    dyn_lock &_dlock;
    const T &_key;
    token _tok;

  public:
    shared_mutex(dyn_lock &dlock, const T &key)
      : _dlock(dlock), _key(key)
    {}

    void lock() {
      _tok = _dlock.lock(_key);
    }

    void unlock() {
      _dlock.unlock(_key, _tok);
    }

    void lock_shared() {
      _tok = _dlock.lock_shared(_key);
    }

    void unlock_shared() {
      _dlock.unlock_shared(_key, _tok);
    }
  };
};
