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

#include "ArangoClientHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

ArangoClientHelper::ArangoClientHelper() : _httpClient(nullptr) {}

// helper to rewrite HTTP location
std::string ArangoClientHelper::rewriteLocation(void* data,
                                                std::string const& location) {
  std::string* dbName = (std::string*)data;

  if (location.substr(0, 5) == "/_db/") {
    return location;
  }

  if (location[0] == '/') {
    return "/_db/" + (*dbName) + location;
  } else {
    return "/_db/" + (*dbName) + "/" + location;
  }
}

// extract an error message from a response
std::string ArangoClientHelper::getHttpErrorMessage(SimpleHttpResult* result,
                                                    int* err) {
  if (err != nullptr) {
    *err = TRI_ERROR_NO_ERROR;
  }
  std::string details;

  try {
    std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
    VPackSlice const body = parsedBody->slice();

    std::string const& errorMessage =
        arangodb::basics::VelocyPackHelper::getStringValue(body, "errorMessage",
                                                           "");
    int errorNum = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
        body, "errorNum", 0);

    if (errorMessage != "" && errorNum > 0) {
      if (err != nullptr) {
        *err = errorNum;
      }
      details =
          ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
    }
  } catch (...) {
    // No action, fallthrough for error
  }
  return "got error from server: HTTP " +
         StringUtils::itoa(result->getHttpReturnCode()) + " (" +
         result->getHttpReturnMessage() + ")" + details;
}

// fetch the version from the server
std::string ArangoClientHelper::getArangoVersion(int* err) {
  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      HttpRequest::HTTP_REQUEST_GET, "/_api/version", nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return "";
  }

  std::string version;

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    // default value
    version = "arango";

    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();

      // look up "server" value
      std::string const server =
          arangodb::basics::VelocyPackHelper::getStringValue(body, "server",
                                                             "");

      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        version = arangodb::basics::VelocyPackHelper::getStringValue(
            body, "version", "");
      }

    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      _httpClient->setErrorMessage(getHttpErrorMessage(response.get(), err),
                                   false);
    }

    _httpClient->disconnect();
  }

  return version;
}

// check if server is a coordinator of a cluster
bool ArangoClientHelper::getArangoIsCluster(int* err) {
  std::unique_ptr<SimpleHttpResult> response(_httpClient->request(
      HttpRequest::HTTP_REQUEST_GET, "/_admin/server/role", "", 0));

  if (response == nullptr || !response->isComplete()) {
    return false;
  }

  std::string role = "UNDEFINED";

  if (response->getHttpReturnCode() == HttpResponse::OK) {
    try {
      std::shared_ptr<VPackBuilder> parsedBody = response->getBodyVelocyPack();
      VPackSlice const body = parsedBody->slice();
      role = arangodb::basics::VelocyPackHelper::getStringValue(body, "role",
                                                                "UNDEFINED");
    } catch (...) {
      // No Action
    }
  } else {
    if (response->wasHttpError()) {
      _httpClient->setErrorMessage(getHttpErrorMessage(response.get(), err),
                                   false);
    }

    _httpClient->disconnect();
  }

  return role == "COORDINATOR";
}
