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

#include "ApplicationFeatures/ConsoleFeature.h"

#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "ConsoleFeature"),
#if WIN32
      _codePage(-1),
#endif
      _quiet(false),
      _colors(true),
      _autoComplete(true),
      _prettyPrint(true),
      _auditFile(),
      _pager(false),
      _pagerCommand("less -X -R -F -L"),
      _prompt("%E@%d> ") {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void ConsoleFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(Section("console", "Configure the console",
                              "console options", false, false));

  options->addOption("--console.quiet", "silent startup",
                     new BooleanParameter(&_quiet));

  options->addOption("--console.colors", "enable color support",
                     new BooleanParameter(&_colors));

  options->addOption("--console.auto-complete", "enable auto completion",
                     new BooleanParameter(&_autoComplete));

  options->addOption("--console.pretty-print", "enable pretty printing",
                     new BooleanParameter(&_prettyPrint));

  options->addOption("--console.audit-file",
                     "audit log file to save commands and results",
                     new StringParameter(&_auditFile));

  options->addOption("--console.pager", "enable paging",
                     new BooleanParameter(&_pager));

  options->addOption("--console.pager-command", "pager command",
                     new StringParameter(&_pagerCommand));

  options->addOption("--console.prompt", "prompt used in REPL",
                     new StringParameter(&_prompt));

#if WIN32
  options->addOption("--console.code-page", "Windows code page to use",
                     new Int16Parameter(&_codePage));
#endif
}
