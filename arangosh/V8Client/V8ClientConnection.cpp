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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "V8ClientConnection.h"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/v8-json.h"
#include "V8/v8-globals.h"

using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;
using namespace arangodb::v8client;
using namespace std;

V8ClientConnection::V8ClientConnection(
    Endpoint* endpoint, std::string databaseName, std::string const& username,
    std::string const& password, double requestTimeout, double connectTimeout,
    size_t numRetries, uint32_t sslProtocol, bool warn)
    : _connection(nullptr),
      _databaseName(databaseName),
      _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _client(nullptr),
      _httpResult(nullptr) {
  _connection = GeneralClientConnection::factory(
      endpoint, requestTimeout, connectTimeout, numRetries, sslProtocol);

  if (_connection == nullptr) {
    throw "out of memory";
  }

  _client = new SimpleHttpClient(_connection, requestTimeout, warn);

  if (_client == nullptr) {
    LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
  }

  _client->setLocationRewriter(this, &rewriteLocation);
  _client->setUserNamePassword("/", username, password);

  // connect to server and get version number
  std::map<std::string, std::string> headerFields;
  std::unique_ptr<SimpleHttpResult> result(
      _client->request(HttpRequest::HTTP_REQUEST_GET,
                       "/_api/version?details=true", nullptr, 0, headerFields));

  if (result == nullptr || !result->isComplete()) {
    // save error message
    _lastErrorMessage = _client->getErrorMessage();
    _lastHttpReturnCode = 500;
  } else {
    _lastHttpReturnCode = result->getHttpReturnCode();

    if (result->getHttpReturnCode() == HttpResponse::OK) {
      // default value
      _version = "arango";
      _mode = "unknown mode";

      try {
        std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
        VPackSlice const body = parsedBody->slice();
        std::string const server =
            arangodb::basics::VelocyPackHelper::getStringValue(body, "server",
                                                               "");

        // "server" value is a string and content is "arango"
        if (server == "arango") {
          // look up "version" value
          _version = arangodb::basics::VelocyPackHelper::getStringValue(
              body, "version", "");
          VPackSlice const details = body.get("details");
          if (details.isObject()) {
            VPackSlice const mode = details.get("mode");
            if (mode.isString()) {
              _mode = mode.copyString();
            }
          }
        }
      } catch (...) {
        // Ignore all parse errors
      }
    } else {
      // initial request for /_api/version returned some non-HTTP 200 response.
      // now set up an error message
      _lastErrorMessage = _client->getErrorMessage();

      if (result->getHttpReturnCode() > 0) {
        _lastErrorMessage = StringUtils::itoa(result->getHttpReturnCode()) +
                            ": " + result->getHttpReturnMessage();
      }
    }
  }
}

V8ClientConnection::~V8ClientConnection() {
  delete _httpResult;
  delete _client;
  delete _connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if it is connected
////////////////////////////////////////////////////////////////////////////////

bool V8ClientConnection::isConnected() { return _connection->isConnected(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current operation to interrupted
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setInterrupted(bool value) {
  _connection->setInterrupted(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current database name
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getDatabaseName() {
  return _databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current database name
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setDatabaseName(std::string const& databaseName) {
  _databaseName = databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getVersion() { return _version; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server mode
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getMode() { return _mode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
////////////////////////////////////////////////////////////////////////////////

int V8ClientConnection::getLastHttpReturnCode() { return _lastHttpReturnCode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getErrorMessage() {
  return _lastErrorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::SimpleHttpClient* V8ClientConnection::getHttpClient() {
  return _client;
}

  v8::Handle<v8::Value> postData(
      v8::Isolate* isolate, std::string const& location, char const* body,
      size_t bodySize, std::map<std::string, std::string> const& headerFields);

v8::Handle<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, std::string const& location, char const* body,
    size_t bodySize, std::map<std::string, std::string> const& headerFields) {
  return requestData(isolate, HttpRequest::HTTP_REQUEST_POST, location, body,
                     bodySize, headerFields);
}

  v8::Handle<v8::Value> requestData(
      v8::Isolate* isolate, rest::HttpRequest::HttpRequestType method,
      std::string const& location, char const* body, size_t bodySize,
      std::map<std::string, std::string> const& headerFields);

v8::Handle<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, HttpRequest::HttpRequestType method,
    std::string const& location, char const* body, size_t bodySize,
    std::map<std::string, std::string> const& headerFields) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  _httpResult =
      _client->request(method, location, body, bodySize, headerFields);

  return handleResult(isolate);
}
