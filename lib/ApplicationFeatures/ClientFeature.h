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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_CLIENT_FEATURE_H
#define APPLICATION_FEATURES_CLIENT_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace rest {
class Endpoint;
}

class ClientFeature final : public application_features::ApplicationFeature {
 public:
  constexpr static double const DEFAULT_REQUEST_TIMEOUT = 5.0;
  constexpr static double const DEFAULT_CONNECTION_TIMEOUT = 1200.0;
  constexpr static size_t const DEFAULT_RETRIES = 2;
  constexpr static double const LONG_TIMEOUT = 86400.0;

 public:
  explicit ClientFeature(application_features::ApplicationServer* server,
                         double connectionTimeout = DEFAULT_CONNECTION_TIMEOUT,
                         double requestTimeout = DEFAULT_REQUEST_TIMEOUT);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;

 public:
  std::string const& databaseName() const { return _databaseName; }
  bool authentication() const { return _authentication; }
  std::string const& endpoint() const { return _endpoint; }
  std::string const& username() const { return _username; }
  std::string const& password() const { return _password; }
  double connectionTimeout() const { return _connectionTimeout; }
  double requestTimeout() const { return _requestTimeout; }
  uint64_t sslProtocol() const { return _sslProtocol; }

 public:
  void createEndpointServer();
  void createEndpointServer(std::string const& endpoint);
  rest::Endpoint* endpointServer() { return _endpointServer; }
  void setDatabaseName(std::string const& databaseName) {
    _databaseName = databaseName;
  }

 private:
  std::string _databaseName;
  bool _authentication;
  std::string _endpoint;
  std::string _username;
  std::string _password;
  double _connectionTimeout;
  double _requestTimeout;
  uint64_t _sslProtocol;

 private:
  std::string _section;
  rest::Endpoint* _endpointServer;
};
}

#endif
