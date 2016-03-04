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

#include "ArangoshFeature.h"

#include "ApplicationFeatures/ClientFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "Shell/V8ShellFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ArangoshFeature::ArangoshFeature(
    application_features::ApplicationServer* server, int* result)
    : ApplicationFeature(server, "ArangoshFeature"),
      _jslint(),
      _result(result),
      _runMode(RunMode::INTERACTIVE) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("ConfigFeature");
  startsAfter("LoggerFeature");
  startsAfter("LanguageFeature");
  startsAfter("V8ShellFeature");
}

void ArangoshFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection(
      Section("", "Global configuration", "global options", false, false));

  options->addOption("--jslint", "do not start as shell, run jslint instead",
                     new VectorParameter<StringParameter>(&_jslint));

  options->addOption("--javascript.execute",
                     "execute Javascript code from file",
                     new VectorParameter<StringParameter>(&_executeScripts));

  options->addOption("--javascript.execute-string",
                     "execute Javascript code from string",
                     new VectorParameter<StringParameter>(&_executeStrings));

  options->addOption("--javascript.check-syntax",
                     "syntax check code Javascript code from file",
                     new VectorParameter<StringParameter>(&_checkSyntaxFiles));

  options->addOption("--javascript.unit-tests",
                     "do not start as shell, run unit tests instead",
                     new VectorParameter<StringParameter>(&_unitTests));
}

void ArangoshFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  _positionals = options->processingResult()._positionals;

  ClientFeature* client =
      dynamic_cast<ClientFeature*>(server()->feature("ClientFeature"));

  if (client->endpoint() == "none") {
    client->disable();
  }

  if (!_jslint.empty()) {
    client->disable();
  }

  size_t n = 0;

  _runMode = RunMode::INTERACTIVE;

  if (!_executeScripts.empty()) {
    _runMode = RunMode::EXECUTE_SCRIPT;
    ++n;
  }

  if (!_executeStrings.empty()) {
    _runMode = RunMode::EXECUTE_STRING;
    ++n;
  }

  if (!_checkSyntaxFiles.empty()) {
    _runMode = RunMode::CHECK_SYNTAX;
    ++n;
  }

  if (!_unitTests.empty()) {
    _runMode = RunMode::UNIT_TESTS;
    ++n;
  }

  if (!_jslint.empty()) {
    _runMode = RunMode::JSLINT;
    ++n;
  }

  if (1 < n) {
    LOG(ERR) << "you cannot specify more than one type ("
             << "jslint, execute, execute-string, check-syntax, unit-tests)";
  }
}

void ArangoshFeature::prepare() {
#warning check features;
}

void ArangoshFeature::start() {
  ConsoleFeature* client =
      dynamic_cast<ConsoleFeature*>(server()->feature("ConsoleFeature"));

  if (_runMode != RunMode::INTERACTIVE) {
    client->setQuiet(true);
  }

  V8ShellFeature* shell =
      dynamic_cast<V8ShellFeature*>(server()->feature("V8ShellFeature"));

  switch (_runMode) {
    case RunMode::INTERACTIVE:
      shell->runShell(_positionals);
  }
}

void ArangoshFeature::stop() {}
