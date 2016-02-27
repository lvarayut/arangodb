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

#include "Basics/Common.h"

#include <iostream>

#include "ArangoShell/ArangoClient.h"
#include "Basics/Logger.h"
#include "Basics/Mutex.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/levenshtein.h"
#include "Basics/random.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Benchmark/ArangobFeature.h"
#include "ProgramOptions2/ArgumentParser.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "Rest/Endpoint.h"
#include "Rest/InitializeRest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup and exit functions
////////////////////////////////////////////////////////////////////////////////

static void arangobEntryFunction();
static void arangobExitFunction(int, void*);

#ifdef _WIN32

// .............................................................................
// Call this function to do various initializations for windows only
// .............................................................................

void arangobEntryFunction() {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  // res = initializeWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initializeWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,
                          (char const*)(&maxOpenFiles));
  if (res != 0) {
    _exit(1);
  }

  res = initializeWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);
  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(arangobExitFunction);
}

static void arangobExitFunction(int exitCode, void* data) {
  int res = finalizeWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}
#else

static void arangobEntryFunction() {}

static void arangobExitFunction(int exitCode, void* data) {}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  arangobEntryFunction();
  TRIAGENS_REST_INITIALIZE();

  std::shared_ptr<options::ProgramOptions> options(new options::ProgramOptions(
      argv[0], "Usage: arangob [<options>]", "For more information use:",
      TRI_ColumnsWidth, TRI_Levenshtein));

  ApplicationServer server(options);

  int ret;

  server.addFeature(new LoggerFeature(&server));
  server.addFeature(new ConfigFeature(&server, "arangob"));
  server.addFeature(new ClientFeature(&server));
  server.addFeature(new ArangobFeature(&server, &ret));

  server.run(argc, argv);

  arangobExitFunction(ret, nullptr);
  TRIAGENS_REST_SHUTDOWN;

  return ret;
}
