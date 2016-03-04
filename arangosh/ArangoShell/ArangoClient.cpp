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

#include "ArangoClient.h"

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/messages.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

double const ArangoClient::DEFAULT_CONNECTION_TIMEOUT = 5.0;
double const ArangoClient::DEFAULT_REQUEST_TIMEOUT = 1200.0;
size_t const ArangoClient::DEFAULT_RETRIES = 2;

double const ArangoClient::LONG_TIMEOUT = 86400.0;
#ifdef _WIN32
bool cygwinShell = false;
#endif
namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (starting point)
///
/// This sequence must be used before any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const* ArangoClient::PROMPT_IGNORE_START = "\001";

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore sequence used for prompt length calculation (end point)
///
/// This sequence must be used behind any non-visible characters in the prompt.
////////////////////////////////////////////////////////////////////////////////

char const* ArangoClient::PROMPT_IGNORE_END = "\002";

ArangoClient::ArangoClient(char const* appName) : _endpointServer(nullptr) {
  TRI_SetApplicationName(appName);
}

ArangoClient::~ArangoClient() { delete _endpointServer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a program-specific help message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setupSpecificHelp(std::string const& progname,
                                     std::string const& message) {
  _specificHelp.progname = progname;
  _specificHelp.message = message;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses command line and config file and prepares logging
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::parse(ProgramOptions& options,
                         ProgramOptionsDescription& description,
                         std::string const& example, int argc, char* argv[],
                         std::string const& initFilename) {
#if 0
  // if options are invalid, exit directly
  if (!options.parse(description, argc, argv)) {
    LOG(FATAL) << options.lastError();
    FATAL_ERROR_EXIT();
  }

  // setup the logging

  std::unordered_set<std::string> existingAppenders;
  if (_loggerFeature._output.empty()) {
    Logger::addAppender("-", true, "", existingAppenders);
  } else {
    for (auto& definition : _loggerFeature._output) {
      Logger::addAppender(definition, true, "", existingAppenders);
    }
  }

  // parse config file
  std::string configFile = "";
  bool allowLocal = false;

  if (!_configFile.empty()) {
    if (StringUtils::tolower(_configFile) == std::string("none")) {
      LOG(DEBUG) << "using no init file at all";
    } else {
      configFile = _configFile;
    }
  }

  else {
    char* d = TRI_LocateConfigDirectory();

    if (d != nullptr) {
      std::string sysDir = std::string(d) + initFilename;
      TRI_FreeString(TRI_CORE_MEM_ZONE, d);

      if (FileUtils::exists(sysDir)) {
        configFile = sysDir;
        allowLocal = true;
      } else {
        LOG(DEBUG) << "no system init file '" << sysDir << "'";
      }
    }
  }

  if (!configFile.empty()) {
    if (allowLocal) {
      std::string localConfigFile = configFile + ".local";

      if (FileUtils::exists(localConfigFile)) {
        LOG(DEBUG) << "using init override file '" << localConfigFile << "'";

        if (!options.parse(description, localConfigFile)) {
          LOG(FATAL) << "cannot parse config file '" << localConfigFile
                     << "': " << options.lastError();
          FATAL_ERROR_EXIT();
        }
      }
    }

    LOG(DEBUG) << "using init file '" << configFile << "'";

    if (!options.parse(description, configFile)) {
      LOG(FATAL) << "cannot parse config file '" << configFile
                 << "': " << options.lastError();
      FATAL_ERROR_EXIT();
    }
  }

  // configuration is parsed and valid if we got to this point

  // check for --help
  std::set<std::string> help = options.needHelp("help");

  if (!help.empty()) {
    if (!example.empty()) {
      std::cout << "USAGE:  " << argv[0] << " " << example << std::endl
                << std::endl;
    }
    std::cout << description.usage(help) << std::endl;

    // check for program-specific help
    std::string const progname(argv[0]);
    if (!_specificHelp.progname.empty() &&
        progname.size() >= _specificHelp.progname.size() &&
        progname.substr(progname.size() - _specificHelp.progname.size(),
                        _specificHelp.progname.size()) ==
            _specificHelp.progname) {
      // found a program-specific help
      std::cout << _specificHelp.message << std::endl;
    }

    // --help always returns success
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // check if have a password
  _hasPassword = options.has("server.password") || _disableAuthentication ||
                 options.has("jslint") || options.has("javascript.unit-tests");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string and a newline to stdout
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printLine(std::string const& s, bool forceNewLine) {
#warning moved to ConsoleFeature
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a string to stdout, without a newline (Non-Windows only)
/// on Windows, we'll print the line and a newline
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printContinuous(std::string const& s) {
#warning moved to ConsoleFeature
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints to pager
////////////////////////////////////////////////////////////////////////////////

-> ConsoleFeature

void ArangoClient::internalPrint(std::string const& str) {
#warning TODO
#if 0
  if (_pager == stdout) {
#ifdef _WIN32
    // at moment the formating is ignored in windows
    printLine(str);
#else
    fprintf(_pager, "%s", str.c_str());
#endif
    if (_auditFile) {
      std::string sanitized = StripBinary(str.c_str());
      log("%s", sanitized.c_str());
    }
  } else {
    std::string sanitized = StripBinary(str.c_str());

    if (!sanitized.empty()) {
      fprintf(_pager, "%s", sanitized.c_str());
      log("%s", sanitized.c_str());
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::openLog() {
#warning TODO
#if 0
  if (!_consoleFeature._auditFile.empty()) {
    _auditFile = fopen(_consoleFeature._auditFile.c_str(), "w");

    std::ostringstream s;
    if (_auditFile == nullptr) {
      s << "Cannot open file '" << _consoleFeature._auditFile
        << "' for logging.";
      printErrLine(s.str());
    } else {
      s << "Logging input and output to '" << _consoleFeature._auditFile
        << "'.";
      printLine(s.str());
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the log file
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::closeLog() {
#warning TODO
#if 0
  if (!_consoleFeature._auditFile.empty() && _auditFile != nullptr) {
    fclose(_auditFile);
    _auditFile = nullptr;
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints info message
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::printWelcomeInfo() {
  if (_consoleFeature._pager) {
    std::ostringstream s;
    s << "Using pager '" << _consoleFeature._pagerCommand
      << "' for output buffering.";

    printLine(s.str());
  }

  if (_consoleFeature._prettyPrint) {
    printLine("Pretty printing values.");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints bye-bye
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint() {
#warning TODO
#if 0
  createEndpoint(_endpointString);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new endpoint
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::createEndpoint(std::string const& definition) {
#warning TODO
#if 0
  // close previous endpoint
  if (_endpointServer != nullptr) {
    delete _endpointServer;
    _endpointServer = nullptr;
  }

  _endpointServer = Endpoint::clientFactory(definition);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet start
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::quiet() const { return _consoleFeature._quiet; }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts up arangosh
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::shutup() { _consoleFeature._quiet = true; }

////////////////////////////////////////////////////////////////////////////////
/// @brief deactivates colors
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::colors() const {
#warning TODO
#if 0
  return (!_noColors && isatty(STDIN_FILENO));
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the auto completion flag
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::autoComplete() const {
#warning TODO
#if 0
  return !_noAutoComplete;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief use pretty print
////////////////////////////////////////////////////////////////////////////////

bool ArangoClient::prettyPrint() const { return _consoleFeature._prettyPrint; }

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the output pager
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::outputPager() const {
  return _consoleFeature._pagerCommand;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets use pager
////////////////////////////////////////////////////////////////////////////////

-> ConsoleFeature
bool ArangoClient::usePager() const { return _consoleFeature._pager; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets use pager
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsePager(bool value) { _consoleFeature._pager = value; }

////////////////////////////////////////////////////////////////////////////////
/// @brief gets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::endpointString() const {
#warning TODO
#if 0
  return _endpointString;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets endpoint to connect to as string
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setEndpointString(std::string const& value) {
#warning TODO
#if 0
  _endpointString = value;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint
////////////////////////////////////////////////////////////////////////////////

Endpoint* ArangoClient::endpointServer() const { return _endpointServer; }

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::databaseName() const {
#warning TODO
#if 0
  return _databaseName;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief user to send to endpoint
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::username() const {
#warning TODO
#if 0
return _username;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief password to send to endpoint
////////////////////////////////////////////////////////////////////////////////

std::string const& ArangoClient::password() const {
#warning TODO
#if 0
return _password;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets database name
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setDatabaseName(std::string const& databaseName) {
#warning TODO
#if 0
  _databaseName = databaseName;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setUsername(std::string const& username) {
#warning TODO
#if 0
  _username = username;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets password
////////////////////////////////////////////////////////////////////////////////

void ArangoClient::setPassword(std::string const& password) {
#warning TODO
#if 0
  _password = password;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::connectTimeout() const {
#warning TODO
#if 0
return _connectTimeout;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request timeout (in seconds)
////////////////////////////////////////////////////////////////////////////////

double ArangoClient::requestTimeout() const {
#warning TODO
#if 0
 return _requestTimeout;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl protocol
////////////////////////////////////////////////////////////////////////////////

uint32_t ArangoClient::sslProtocol() const {
#warning TODO
#if 0
 return _sslProtocol;
#endif
}
