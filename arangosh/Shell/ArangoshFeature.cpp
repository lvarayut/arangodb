////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ArangoshFeature.h"

#include <iostream>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ClientFeature.h"
#include "ArangoShell/ArangoClient.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/SslInterface.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::options;
using namespace arangodb::rest;

ArangoshFeature::ArangoshFeature(
    application_features::ApplicationServer* server, int* result)
    : ApplicationFeature(server, "ArangoshFeature") {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("ConfigFeature");
  startsAfter("LoggerFeature");
}

void ArangoshFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection(
      Section("", "Global configuration", "global options", false, false));
}

void ArangoshFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();
}

void ArangoshFeature::prepare() {
#warning check features;
}

void ArangoshFeature::start() {
  ClientFeature* client =
      dynamic_cast<ClientFeature*>(server()->feature("ClientFeature"));
}

#if 0

  int ret = EXIT_SUCCESS;

  *_result = ret;

  client->createEndpointServer();

  if (client->endpointServer() == nullptr) {
    LOG(FATAL) << "invalid value for --server.endpoint ('" << client->endpoint()
               << "')";
    FATAL_ERROR_EXIT();
  }

  _connection = GeneralClientConnection::factory(
      client->endpointServer(), client->requestTimeout(),
      client->connectionTimeout(), ClientFeature::DEFAULT_RETRIES,
      client->sslProtocol());

  if (_connection == nullptr) {
    LOG(FATAL) << "out of memory";
    FATAL_ERROR_EXIT();
  }

  _httpClient =
      new SimpleHttpClient(_connection, client->requestTimeout(), false);

  std::string dbName = client->databaseName();

  _httpClient->setLocationRewriter((void*)&dbName, &rewriteLocation);
  _httpClient->setUserNamePassword("/", client->username(), client->password());

  std::string const versionString = getArangoVersion(nullptr);

  if (!_connection->isConnected()) {
    LOG(ERR) << "Could not connect to endpoint '" << client->endpoint()
             << "', database: '" << dbName << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << "Error message: '" << _httpClient->getErrorMessage() << "'";

    FATAL_ERROR_EXIT();
  }

  // successfully connected
  std::cout << "Server version: " << versionString << std::endl;

  // validate server version
  int major = 0;
  int minor = 0;

  if (sscanf(versionString.c_str(), "%d.%d", &major, &minor) != 2) {
    LOG(FATAL) << "invalid server version '" << versionString << "'";
    FATAL_ERROR_EXIT();
  }

  if (major != 3) {
    // we can connect to 3.x
    LOG(ERR) << "Error: got incompatible server version '" << versionString
             << "'";

    if (!_force) {
      FATAL_ERROR_EXIT();
    }
  }

  if (major >= 2) {
    // Version 1.4 did not yet have a cluster mode
    _clusterMode = getArangoIsCluster(nullptr);

    if (_clusterMode) {
      if (_tickStart != 0 || _tickEnd != 0) {
        LOG(ERR) << "Error: cannot use tick-start or tick-end on a cluster";
        FATAL_ERROR_EXIT();
      }
    }
  }

  if (!_connection->isConnected()) {
    LOG(ERR) << "Lost connection to endpoint '" << client->endpoint()
             << "', database: '" << dbName << "', username: '"
             << client->username() << "'";
    LOG(FATAL) << "Error message: '" << _httpClient->getErrorMessage() << "'";
    FATAL_ERROR_EXIT();
  }

  if (_progress) {
    std::cout << "Connected to ArangoDB '" << client->endpoint()
              << "', database: '" << dbName << "', username: '"
              << client->username() << "'" << std::endl;

    std::cout << "Writing sh to output directory '" << _outputDirectory << "'"
              << std::endl;
  }

  memset(&_stats, 0, sizeof(_stats));

  std::string errorMsg = "";

  int res;

  try {
    if (!_clusterMode) {
      res = startBatch("", errorMsg);

      if (res != TRI_ERROR_NO_ERROR && _force) {
        res = TRI_ERROR_NO_ERROR;
      }

      if (res == TRI_ERROR_NO_ERROR) {
        res = runSh(dbName, errorMsg);
      }

      if (_batchId > 0) {
        endBatch("");
      }
    } else {
      res = runClusterSh(errorMsg);
    }
  } catch (std::exception const& ex) {
    LOG(ERR) << "caught exception " << ex.what();
    res = TRI_ERROR_INTERNAL;
  } catch (...) {
    LOG(ERR) << "Error: caught unknown exception";
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (!errorMsg.empty()) {
      LOG(ERR) << errorMsg;
    } else {
      LOG(ERR) << "An error occurred";
    }
    ret = EXIT_FAILURE;
  }

  if (_progress) {
    if (_shData) {
      std::cout << "Processed " << _stats._totalCollections
                << " collection(s), "
                << "wrote " << _stats._totalWritten
                << " byte(s) into datafiles, "
                << "sent " << _stats._totalBatches << " batch(es)" << std::endl;
    } else {
      std::cout << "Processed " << _stats._totalCollections << " collection(s)"
                << std::endl;
    }
  }

  *_result = ret;
}

#endif

void ArangoshFeature::stop() {
#if 0
  if (_httpClient != nullptr) {
    delete _httpClient;
  }
#endif
}
