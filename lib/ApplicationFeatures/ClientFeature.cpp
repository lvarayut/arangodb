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

#include "ApplicationFeatures/ClientFeature.h"

#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"
#include "Rest/Endpoint.h"

using namespace arangodb;
using namespace arangodb::options;
using namespace arangodb::rest;

ClientFeature::ClientFeature(application_features::ApplicationServer* server,
                             double connectionTimeout, double requestTimeout)
    : ApplicationFeature(server, "ClientFeature"),
      _databaseName("_system"),
      _authentication(true),
      _endpoint(Endpoint::getDefaultEndpoint()),
      _username("root"),
      _password(""),
      _connectionTimeout(connectionTimeout),
      _requestTimeout(requestTimeout),
      _sslProtocol(4) {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void ClientFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  static std::string section = "server";

  options->addSection(Section(section, "Configure a connection to the server",
                              section + " options", false, false));

  options->addOption("--" + section + ".database",
                     "database name to use when connecting",
                     new StringParameter(&_databaseName));

  options->addOption("--" + section + ".authentication",
                     "require authentication when connecting",
                     new BooleanParameter(&_authentication));

  options->addOption("--" + section + ".username",
                     "username to use when connecting",
                     new StringParameter(&_username));

  options->addOption(
      "--" + section + ".endpoint",
      "endpoint to connect to, use 'none' to start without a server",
      new StringParameter(&_endpoint));

  options->addOption("--" + section + ".password",
                     "password to use when connection. If not specified and "
                     "authentication is required, the user will be prompted "
                     "for a password.",
                     new StringParameter(&_password));

  options->addOption("--" + section + ".connection-timeout",
                     "connection timeout in seconds",
                     new DoubleParameter(&_connectionTimeout));

  options->addOption("--" + section + ".request-timeout",
                     "request timeout in seconds",
                     new DoubleParameter(&_requestTimeout));

  options->addOption("--" + section + ".ssl-protocol",
                     "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1",
                     new UInt64Parameter(&_sslProtocol));
}

void ClientFeature::createEndpointServer() {
  createEndpointServer(_endpoint);
}

void ClientFeature::createEndpointServer(std::string const& definition) {

  // close previous endpoint
  if (_endpointServer != nullptr) {
    delete _endpointServer;
    _endpointServer = nullptr;
  }

  // create a new endpoint
  _endpointServer = Endpoint::clientFactory(definition);
}

#warning TODO
#if 0

  if (options.has("server.username")) {
    // if a username is specified explicitly, assume authentication is desired
    _disableAuthentication = false;
  }

  if (_serverOptions) {
    // check connection args
    if (_connectTimeout < 0.0) {
      LOG(FATAL) << "invalid value for --server.connect-timeout, must be >= 0";
      FATAL_ERROR_EXIT();
    } else if (_connectTimeout == 0.0) {
      _connectTimeout = LONG_TIMEOUT;
    }

    if (_requestTimeout < 0.0) {
      LOG(FATAL)
          << "invalid value for --server.request-timeout, must be positive";
      FATAL_ERROR_EXIT();
    } else if (_requestTimeout == 0.0) {
      _requestTimeout = LONG_TIMEOUT;
    }

    // must specify a user name
    if (_username.size() == 0) {
      LOG(FATAL) << "no value specified for --server.username";
      FATAL_ERROR_EXIT();
    }

    // no password given on command-line
    if (!_hasPassword) {
      usleep(10 * 1000);
      printContinuous("Please specify a password: ");

// now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
      TRI_SetStdinVisibility(false);
      getline(std::cin, _password);

      TRI_SetStdinVisibility(true);
#else
      getline(std::cin, _password);
#endif
      printLine("");
    }
  }

#endif
