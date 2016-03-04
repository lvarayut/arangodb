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

#include "V8ShellFeature.h"

#include "ApplicationFeatures/ClientFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/shell-colors.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/V8ClientConnection.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

V8ShellFeature::V8ShellFeature(application_features::ApplicationServer* server,
                               std::string const& name)
    : ApplicationFeature(server, "V8ShellFeature"),
      _startupDirectory("js"),
      _currentModuleDirectory(true),
      _gcInterval(10),
      _name(name),
      _isolate(nullptr),
      _console(nullptr) {
  requiresElevatedPrivileges(false);
  setOptional(false);
  startsAfter("LoggerFeature");
  startsAfter("ConsoleFeature");
  startsAfter("V8PlatformFeature");
}

void V8ShellFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("javascript", "Configure the Javascript engine");

  options->addHiddenOption("--javascript.startup-directory",
                           "startup paths containing the Javascript files",
                           new StringParameter(&_startupDirectory));

  options->addOption("--javascript.current-module-directory",
                     "add current directory to module path",
                     new BooleanParameter(&_currentModuleDirectory));

  options->addOption(
      "--javascript.gc-interval",
      "request-based garbage collection interval (each n.th commands)",
      new UInt64Parameter(&_gcInterval));
}

void V8ShellFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  if (_startupDirectory.empty()) {
    LOG(FATAL) << "'--javascript.startup-directory' is empty, giving up";
    abortInvalidParameters();
  }

  LOG_TOPIC(DEBUG, Logger::V8) << "using Javascript startup files at '"
                               << _startupDirectory << "'";
}

void V8ShellFeature::start() {
  _console = dynamic_cast<ConsoleFeature*>(server()->feature("ConsoleFeature"));

  _isolate = v8::Isolate::New();

  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  // create the global template
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(_isolate);

  // create the context
  _context.Reset(_isolate, v8::Context::New(_isolate, nullptr, global));

  auto context = v8::Local<v8::Context>::New(_isolate, _context);

  if (context.IsEmpty()) {
    LOG(FATAL) << "cannot initialize V8 engine";
    FATAL_ERROR_EXIT();
  }

  // fill the global object
  v8::Context::Scope context_scope{context};

  v8::Handle<v8::Object> globalObj = context->Global();
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "GLOBAL"), globalObj);
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "global"), globalObj);
  globalObj->Set(TRI_V8_ASCII_STRING2(_isolate, "root"), globalObj);

  initGlobals();
}

void V8ShellFeature::stop() {
  {
    v8::Locker locker{_isolate};
    v8::Isolate::Scope isolate_scope{_isolate};
    _context.Reset();
  }

  _isolate->Dispose();
}

bool V8ShellFeature::printHello(V8ClientConnection* v8connection) {
  bool promptError = false;

  // for the banner see
  // http://www.network-science.de/ascii/   Font: ogre

  if (!_console->quiet()) {
    std::string g = TRI_SHELL_COLOR_GREEN;
    std::string r = TRI_SHELL_COLOR_RED;
    std::string z = TRI_SHELL_COLOR_RESET;

    if (!_console->colors()) {
      g = "";
      r = "";
      z = "";
    }

    // clang-format off

    _console->printLine("");
    _console->printLine(g + "                                  "     + r + "     _     " + z);
    _console->printLine(g + "  __ _ _ __ __ _ _ __   __ _  ___ "     + r + " ___| |__  " + z);
    _console->printLine(g + " / _` | '__/ _` | '_ \\ / _` |/ _ \\"   + r + "/ __| '_ \\ " + z);
    _console->printLine(g + "| (_| | | | (_| | | | | (_| | (_) "     + r + "\\__ \\ | | |" + z);
    _console->printLine(g + " \\__,_|_|  \\__,_|_| |_|\\__, |\\___/" + r + "|___/_| |_|" + z);
    _console->printLine(g + "                       |___/      "     + r + "           " + z);
    _console->printLine("");

    // clang-format on

    std::ostringstream s;

    s << "arangosh (" << rest::Version::getVerboseVersionString() << ")\n"
      << "Copyright (c) ArangoDB GmbH";

    _console->printLine(s.str(), true);
    _console->printLine("", true);

    _console->printWelcomeInfo();

    if (v8connection != nullptr) {
      if (v8connection->isConnected() &&
          v8connection->lastHttpReturnCode() == HttpResponse::OK) {
        std::ostringstream is;

        is << "Connected to ArangoDB '" << v8connection->endpointSpecification()
           << "' version: " << v8connection->version() << " ["
           << v8connection->mode() << "], database: '"
           << v8connection->databaseName() << "', username: '"
           << v8connection->username() << "'";

        _console->printLine(is.str(), true);
      } else {
        std::ostringstream is;

        is << "Could not connect to endpoint '"
           << v8connection->endpointSpecification() << "', database: '"
           << v8connection->databaseName() << "', username: '"
           << v8connection->username() << "'";

        _console->printErrorLine(is.str());

        if (!v8connection->lastErrorMessage().empty()) {
          std::ostringstream is2;

          is2 << "Error message '" << v8connection->lastErrorMessage() << "'";

          _console->printErrorLine(is2.str());
        }

        promptError = true;
      }

      _console->printLine("", true);
    }
  }

  return promptError;
}

int V8ShellFeature::runShell(std::vector<std::string> const& positionals) {
  ClientFeature* client =
      dynamic_cast<ClientFeature*>(server()->feature("ClientFeature"));

  auto connection = client->createConnection();
  auto v8connection = std::make_unique<V8ClientConnection>(
      connection, client->databaseName(), client->username(),
      client->password(), client->requestTimeout());

  v8::Locker locker{_isolate};

  v8::Isolate::Scope isolate_scope(_isolate);
  v8::HandleScope handle_scope(_isolate);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(_isolate, _context);

  v8::Context::Scope context_scope{context};

  initMode(ArangoshFeature::RunMode::INTERACTIVE, positionals);
  v8connection->initServer(_isolate, context, client);
  bool promptError = printHello(v8connection.get());
  loadModules(ArangoshFeature::RunMode::INTERACTIVE);

  V8LineEditor v8LineEditor(_isolate, context, "." + _name + ".history");

  v8LineEditor.setSignalFunction(
      [&v8connection]() { v8connection->setInterrupted(true); });

  v8LineEditor.open(_console->autoComplete());

  v8::Local<v8::String> name(
      TRI_V8_ASCII_STRING2(_isolate, TRI_V8_SHELL_COMMAND_NAME));

  uint64_t nrCommands = 0;

  while (true) {
    _console->setPromptError(promptError);
    auto prompt = _console->buildPrompt(client);

    bool eof;
    std::string input =
        v8LineEditor.prompt(prompt._colored, prompt._plain, eof);

    if (eof) {
      break;
    }

    if (input.empty()) {
      promptError = false;
      continue;
    }

    _console->log(prompt._plain + input + "\n");

    std::string i = StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      break;
    }

    if (i == "help" || i == "help;") {
      input = "help()";
    }

    v8LineEditor.addHistory(input);

    v8::TryCatch tryCatch;

    _console->startPager();

    // assume the command succeeds
    promptError = false;

    // execute command and register its result in __LAST__
    v8LineEditor.setExecutingCommand(true);

    v8::Handle<v8::Value> v = TRI_ExecuteJavaScriptString(
        _isolate, context, TRI_V8_STRING2(_isolate, input.c_str()), name, true);

    v8LineEditor.setExecutingCommand(false);

    if (v.IsEmpty()) {
      context->Global()->Set(TRI_V8_ASCII_STRING2(_isolate, "_last"),
                             v8::Undefined(_isolate));
    } else {
      context->Global()->Set(TRI_V8_ASCII_STRING2(_isolate, "_last"), v);
    }

    // command failed
    if (tryCatch.HasCaught()) {
      std::string exception;

      if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
        exception = "command locally aborted\n";
      } else {
        exception = TRI_StringifyV8Exception(_isolate, &tryCatch);
      }

      _console->printErrorLine(exception);
      _console->log(exception);

      // this will change the prompt for the next round
      promptError = true;
    }

    v8connection->setInterrupted(false);

    _console->stopPager();
    _console->printLine("");

    _console->log("\n");

    // make sure the last command result makes it into the log file
    _console->flushLog();

    // gc
    if (++nrCommands >= _gcInterval) {
      nrCommands = 0;
      TRI_RunGarbageCollectionV8(_isolate, 500.0);
    }
  }

  _console->printLine("");
  _console->printByeBye();

  return promptError ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
////////////////////////////////////////////////////////////////////////////////

static void JS_PagerOutput(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = (ConsoleFeature*)(wrap->Value());

  for (int i = 0; i < args.Length(); i++) {
    // extract the next argument
    v8::Handle<v8::Value> val = args[i];

    std::string str = TRI_ObjectToString(val);

    console->print(str);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StartOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = (ConsoleFeature*)(wrap->Value());

  if (console->pager()) {
    console->print("Using pager already.\n");
  } else {
    console->setPager(true);
    console->print(std::string(std::string("Using pager ") +
                               console->pagerCommand() +
                               " for output buffering.\n"));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the output pager
////////////////////////////////////////////////////////////////////////////////

static void JS_StopOutputPager(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ConsoleFeature* console = (ConsoleFeature*)(wrap->Value());

  if (console->pager()) {
    console->print("Stopping pager.\n");
  } else {
    console->print("Pager not running.\n");
  }

  console->setPager(false);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_NormalizeString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("NORMALIZE_STRING(<string>)");
  }

  TRI_normalize_V8_Obj(args, args[0]);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_CompareString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(args[0]);
  v8::String::Value right(args[1]);

  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(
      *left, left.length(), *right, right.length());

  TRI_V8_RETURN(v8::Integer::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes global Javascript variables
////////////////////////////////////////////////////////////////////////////////

void V8ShellFeature::initGlobals() {
  auto context = _isolate->GetCurrentContext();

  // set pretty print default
  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "PRETTY_PRINT"),
      v8::Boolean::New(_isolate, _console->prettyPrint()));

  // add colors for print.js
  TRI_AddGlobalVariableVocbase(_isolate, context,
                               TRI_V8_ASCII_STRING2(_isolate, "COLOR_OUTPUT"),
                               v8::Boolean::New(_isolate, _console->colors()));

  // string functions
  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "NORMALIZE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_NormalizeString)->GetFunction());

  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "COMPARE_STRING"),
      v8::FunctionTemplate::New(_isolate, JS_CompareString)->GetFunction());

  // is quite
  TRI_AddGlobalVariableVocbase(_isolate, context,
                               TRI_V8_ASCII_STRING2(_isolate, "ARANGO_QUIET"),
                               v8::Boolean::New(_isolate, _console->quiet()));

  // initialize standard modules
  std::string modules =
      FileUtils::buildFilename(_startupDirectory, "client/modules") + ";" +
      FileUtils::buildFilename(_startupDirectory, "common/modules") + ";" +
      FileUtils::buildFilename(_startupDirectory, "node");

  if (_currentModuleDirectory) {
    modules += ";" + FileUtils::currentDirectory();
  }

  TRI_InitV8Buffer(_isolate, context);
  TRI_InitV8Utils(_isolate, context, _startupDirectory, modules);
  TRI_InitV8Shell(_isolate, context);

  // pager functions (overwrite existing SYS_OUTPUT from InitV8Utils)
  if (_console != nullptr) {
    v8::Local<v8::Value> console = v8::External::New(_isolate, _console);

    TRI_AddGlobalVariableVocbase(
        _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "SYS_OUTPUT"),
        v8::FunctionTemplate::New(_isolate, JS_PagerOutput, console)
            ->GetFunction());

    TRI_AddGlobalVariableVocbase(
        _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "SYS_START_PAGER"),
        v8::FunctionTemplate::New(_isolate, JS_StartOutputPager, console)
            ->GetFunction());

    TRI_AddGlobalVariableVocbase(
        _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "SYS_STOP_PAGER"),
        v8::FunctionTemplate::New(_isolate, JS_StopOutputPager, console)
            ->GetFunction());
  }
}

void V8ShellFeature::initMode(ArangoshFeature::RunMode runMode,
                              std::vector<std::string> const& positionals) {
  auto context = _isolate->GetCurrentContext();

  // add positional arguments
  v8::Handle<v8::Array> p = v8::Array::New(_isolate, (int)positionals.size());

  for (uint32_t i = 0; i < positionals.size(); ++i) {
    p->Set(i, TRI_V8_STD_STRING2(_isolate, positionals[i]));
  }

  TRI_AddGlobalVariableVocbase(_isolate, context,
                               TRI_V8_ASCII_STRING2(_isolate, "ARGUMENTS"), p);

  // set mode flags
  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "IS_EXECUTE_SCRIPT"),
      v8::Boolean::New(_isolate,
                       runMode == ArangoshFeature::RunMode::EXECUTE_SCRIPT));

  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "IS_EXECUTE_STRING"),
      v8::Boolean::New(_isolate,
                       runMode == ArangoshFeature::RunMode::EXECUTE_STRING));

  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "IS_CHECK_SCRIPT"),
      v8::Boolean::New(_isolate,
                       runMode == ArangoshFeature::RunMode::CHECK_SYNTAX));

  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "IS_UNIT_TESTS"),
      v8::Boolean::New(_isolate,
                       runMode == ArangoshFeature::RunMode::UNIT_TESTS));

  TRI_AddGlobalVariableVocbase(
      _isolate, context, TRI_V8_ASCII_STRING2(_isolate, "IS_JS_LINT"),
      v8::Boolean::New(_isolate, runMode == ArangoshFeature::RunMode::JSLINT));
}

void V8ShellFeature::loadModules(ArangoshFeature::RunMode runMode) {
  auto context = _isolate->GetCurrentContext();

  JSLoader loader;
  loader.setDirectory(_startupDirectory);

  // load all init files
  std::vector<std::string> files;

  files.push_back("common/bootstrap/scaffolding.js");
  files.push_back("common/bootstrap/modules/internal.js");  // deps: -
  files.push_back("common/bootstrap/errors.js");            // deps: internal
  files.push_back("client/bootstrap/modules/internal.js");  // deps: internal
  files.push_back("common/bootstrap/modules/vm.js");        // deps: internal
  files.push_back("common/bootstrap/modules/console.js");   // deps: internal
  files.push_back("common/bootstrap/modules/assert.js");    // deps: -
  files.push_back("common/bootstrap/modules/buffer.js");    // deps: internal
  files.push_back(
      "common/bootstrap/modules/fs.js");  // deps: internal, buffer (hidden)
  files.push_back("common/bootstrap/modules/path.js");     // deps: internal, fs
  files.push_back("common/bootstrap/modules/events.js");   // deps: -
  files.push_back("common/bootstrap/modules/process.js");  // deps: internal,
                                                           // fs, events,
                                                           // console
  files.push_back(
      "common/bootstrap/modules.js");  // must come last before patches

  if (runMode != ArangoshFeature::RunMode::JSLINT) {
    files.push_back("common/bootstrap/monkeypatches.js");
  }

  files.push_back("client/client.js");  // needs internal

  for (size_t i = 0; i < files.size(); ++i) {
    switch (loader.loadScript(_isolate, context, files[i])) {
      case JSLoader::eSuccess:
        LOG(TRACE) << "loaded JavaScript file '" << files[i] << "'";
        break;
      case JSLoader::eFailLoad:
        LOG(FATAL) << "cannot load JavaScript file '" << files[i] << "'";
        FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG(FATAL) << "error during execution of JavaScript file '" << files[i]
                   << "'";
        FATAL_ERROR_EXIT();
        break;
    }
  }
}

#if 0

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 4;


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static int Run(v8::Isolate* isolate, eRunMode runMode, bool promptError) {
  auto context = isolate->GetCurrentContext();
  bool ok = false;
  try {
    switch (runMode) {
      case eInteractive:
        ok = RunShell(isolate, context, promptError) == TRI_ERROR_NO_ERROR;
        if (isatty(STDIN_FILENO)) {
          ok = true;
        }
        break;
      case eExecuteScript:
        // we have scripts to execute
        ok = RunScripts(isolate, context, ExecuteScripts, true);
        break;
      case eExecuteString:
        // we have string to execute
        ok = RunString(isolate, context, ExecuteString);
        break;
      case eCheckScripts:
        // we have scripts to syntax check
        ok = RunScripts(isolate, context, CheckScripts, false);
        break;
      case eUnitTests:
        // we have unit tests
        ok = RunUnitTests(isolate, context);
        break;
      case eJsLint:
        // we don't have unittests, but we have files to jslint
        ok = RunJsLint(isolate, context);
        break;
    }
  } catch (std::exception const& ex) {
    cerr << "caught exception " << ex.what() << endl;
    ok = false;
  } catch (...) {
    cerr << "caught unknown exception" << endl;
    ok = false;
  }
  return (ok) ? EXIT_SUCCESS : EXIT_FAILURE;
}











start :

    v8::Isolate* isolate = v8::Isolate::New();
isolate->Enter();
{
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  {
    // reset the prompt error flag (will determine prompt colors)
    bool promptError = PrintHelo(useServer);

    ret = WarmupEnvironment(isolate, positionals, runMode);

    if (ret == EXIT_SUCCESS) {
      BaseClient.openLog();

      try {
        ret = Run(isolate, runMode, promptError);
      } catch (std::bad_alloc const&) {
        LOG(ERR) << "caught exception "
                 << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
        ret = EXIT_FAILURE;
      } catch (...) {
        LOG(ERR) << "caught unknown exception";
        ret = EXIT_FAILURE;
      }
    }

    isolate->LowMemoryNotification();

    // spend at least 3 seconds in GC
    LOG(DEBUG) << "entering final garbage collection";
    TRI_RunGarbageCollectionV8(isolate, 3000);
    LOG(DEBUG) << "final garbage collection completed";

    context->Exit();
    context.Reset();
  }
}

#endif
