////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "InitializeRest.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

#define OPENSSL_THREAD_DEFINES

#include <openssl/opensslconf.h>

#ifndef OPENSSL_THREADS
#error missing thread support for openssl, please recomple OpenSSL with threads
#endif

#include "Basics/Logger.h"
#include "Basics/RandomGenerator.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/mimetypes.h"
#include "Basics/process-utils.h"
#include "Basics/random.h"
#include "Basics/Thread.h"
#include "Rest/Version.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// OPEN SSL support
// -----------------------------------------------------------------------------

namespace {
long* opensslLockCount;
TRI_mutex_t* opensslLocks;

#if OPENSSL_VERSION_NUMBER < 0x01000000L

unsigned long opensslThreadId() { return (unsigned long)TRI_CurrentThreadId(); }

#else

// The compiler chooses the right one from the following two,
// according to the type of the return value of pthread_self():

#ifndef __sun
template <typename T>
void setter(CRYPTO_THREADID* id, T p) {
  CRYPTO_THREADID_set_pointer(id, p);
}
#else
template <typename T>
void setter(CRYPTO_THREADID* id, T p) {
  CRYPTO_THREADID_set_pointer(id, (void *) (intptr_t) p);
}
#endif

#ifndef __APPLE__
template <>
void setter(CRYPTO_THREADID* id, unsigned long val) {
  CRYPTO_THREADID_set_numeric(id, val);
}
#endif

static void arango_threadid_func(CRYPTO_THREADID* id) {
  auto self = Thread::currentThreadId();

  setter<decltype(self)>(id, self);
}

#endif

void opensslLockingCallback(int mode, int type, char const* /* file */,
                            int /* line */) {
  if (mode & CRYPTO_LOCK) {
    TRI_LockMutex(&(opensslLocks[type]));
    opensslLockCount[type]++;
  } else {
    TRI_UnlockMutex(&(opensslLocks[type]));
  }
}

void opensslSetup() {
  opensslLockCount = (long*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
  opensslLocks =
      (TRI_mutex_t*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(TRI_mutex_t));

  for (long i = 0; i < CRYPTO_num_locks(); ++i) {
    opensslLockCount[i] = 0;
    TRI_InitMutex(&(opensslLocks[i]));
  }

#if OPENSSL_VERSION_NUMBER < 0x01000000L
  CRYPTO_set_id_callback(opensslThreadId);
  CRYPTO_set_locking_callback(opensslLockingCallback);
#else
  CRYPTO_THREADID_set_callback(arango_threadid_func);
  CRYPTO_set_locking_callback(opensslLockingCallback);
#endif
}

void opensslCleanup() {
  CRYPTO_set_locking_callback(nullptr);

#if OPENSSL_VERSION_NUMBER < 0x01000000L
  CRYPTO_set_id_callback(nullptr);
#else
  CRYPTO_THREADID_set_callback(nullptr);
#endif

  for (long i = 0; i < CRYPTO_num_locks(); ++i) {
    TRI_DestroyMutex(&(opensslLocks[i]));
  }

  OPENSSL_free(opensslLocks);
  OPENSSL_free(opensslLockCount);
}
}

using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------

namespace arangodb {
namespace rest {
void InitializeRest(int argc, char* argv[]) {
  TRI_InitializeMemory();
  TRI_InitializeDebugging();
  TRI_InitializeError();
  TRI_InitializeFiles();
  TRI_InitializeMimetypes();
  Logger::initialize(false);
  TRI_InitializeHashes();
  TRI_InitializeRandom();
  TRI_InitializeProcess(argc, argv);

  // use the rng so the linker does not remove it from the executable
  // we might need it later because .so files might refer to the symbols
  Random::random_e v = Random::selectVersion(Random::RAND_MERSENNE);
  Random::UniformInteger random(0, INT32_MAX);
  random.random();
  Random::selectVersion(v);

#ifdef TRI_BROKEN_CXA_GUARD
  pthread_cond_t cond;
  pthread_cond_init(&cond, 0);
  pthread_cond_broadcast(&cond);
#endif

  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();

  opensslSetup();

  Version::initialize();
}

void ShutdownRest() {
  opensslCleanup();

  ERR_free_strings();
  EVP_cleanup();
  CRYPTO_cleanup_all_ex_data();

  TRI_ShutdownProcess();
  TRI_ShutdownRandom();
  TRI_ShutdownHashes();
  Logger::shutdown(true);
  TRI_ShutdownMimetypes();
  TRI_ShutdownFiles();
  TRI_ShutdownError();
  TRI_ShutdownDebugging();
  TRI_ShutdownMemory();
}
}
}
