#include <iostream>

#include <v8.h>
#include <libplatform/libplatform.h>

#include "ArangoShell/ArangoClient.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/messages.h"
#include "Basics/shell-colors.h"
#include "Basics/terminal-utils.h"
#include "Basics/tri-strings.h"
#include "Rest/Endpoint.h"
#include "Rest/HttpResponse.h"
#include "Rest/InitializeRest.h"
#include "Rest/Version.h"
#include "V8/JSLoader.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-shell.h"
#include "V8/v8-utils.h"
#include "Import/ImportHelper.h"
#include "V8Client/V8ClientConnection.h"

#include "3rdParty/valgrind/valgrind.h"

using namespace std;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::httpclient;
using namespace arangodb::v8client;

using namespace arangodb;
////////////////////////////////////////////////////////////////////////////////
/// @brief map of connection objects
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<void*, v8::Persistent<v8::External>> Connections;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief options to pass to V8
////////////////////////////////////////////////////////////////////////////////

static std::string V8Options;

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the arguments
///
/// @FUN{internal.output(@FA{string1}, @FA{string2}, @FA{string3}, ...)}
///
/// Outputs the arguments to standard output.
///
/// @verbinclude fluent39
////////////////////////////////////////////////////////////////////////////////

static void JS_PagerOutput(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  for (int i = 0; i < args.Length(); i++) {
    // extract the next argument
    v8::Handle<v8::Value> val = args[i];

    std::string str = TRI_ObjectToString(val);

    BaseClient.internalPrint(str);
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

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Using pager already.\n");
  } else {
    BaseClient.setUsePager(true);
    BaseClient.internalPrint(std::string(std::string("Using pager ") +
                                         BaseClient.outputPager() +
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

  if (BaseClient.usePager()) {
    BaseClient.internalPrint("Stopping pager.\n");
  } else {
    BaseClient.internalPrint("Pager not running.\n");
  }

  BaseClient.setUsePager(false);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static void JS_NormalizeString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
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
/// @brief enum for wrapped V8 objects
////////////////////////////////////////////////////////////////////////////////

enum WRAP_CLASS_TYPES { WRAP_TYPE_CONNECTION = 1 };


////////////////////////////////////////////////////////////////////////////////
/// @brief parses the program options
////////////////////////////////////////////////////////////////////////////////

typedef enum __eRunMode {
  eInteractive,
  eExecuteScript,
  eExecuteString,
  eCheckScripts,
  eUnitTests,
  eJsLint
} eRunMode;



  javascript(
             "javascript.execute", &ExecuteScripts, "execute Javascript code from file")
    ( "javascript.execute-string", &ExecuteString, "execute Javascript code from string")
    ( "javascript.check", &CheckScripts, "syntax check code Javascript code from file")
    ( "javascript.gc-interval", &GcInterval, "JavaScript request-based garbage collection interval (each x commands)")
    ( "javascript.startup-directory", &StartupPath, "startup paths containing the JavaScript files")
    ( "javascript.unit-tests", &UnitTests, "do not start as shell, run unit tests instead")
    ( "javascript.current-module-directory", &UseCurrentModulePath, "add current directory to module path")
    ( "javascript.v8-options", &V8Options, "options to pass to v8")
    ( "jslint", &JsLint, "do not start as shell, run jslint instead");

  // fill in used options
#warning TODO
#if 0
  BaseClient.setupGeneral(description);
  BaseClient.setupColors(description);
  BaseClient.setupAutoComplete(description);
  BaseClient.setupPrettyPrint(description);
  BaseClient.setupPager(description);
  BaseClient.setupLog(description);
  BaseClient.setupServer(description);
#endif

  // and parse the command line and config file
  ProgramOptions options;

  std::string conf = TRI_BinaryName(args[0]) + ".conf";

  BaseClient.parse(options, description, "<options>", argc, args, conf);

  // derive other paths from `--javascript.directory`
  StartupModules = StartupPath + TRI_DIR_SEPARATOR_STR + "client" +
                   TRI_DIR_SEPARATOR_STR + "modules;" + StartupPath +
                   TRI_DIR_SEPARATOR_STR + "common" + TRI_DIR_SEPARATOR_STR +
                   "modules;" + StartupPath + TRI_DIR_SEPARATOR_STR + "node";

  if (UseCurrentModulePath) {
    StartupModules += ";" + FileUtils::currentDirectory();
  }

  // disable excessive output in non-interactive mode
  if (!ExecuteScripts.empty() || !ExecuteString.empty() ||
      !CheckScripts.empty() || !UnitTests.empty() || !JsLint.empty()) {
    BaseClient.shutup();
  }

  if (!ExecuteScripts.empty()) {
    *runMode = eExecuteScript;
  } else if (!ExecuteString.empty()) {
    *runMode = eExecuteString;
  } else if (!CheckScripts.empty()) {
    *runMode = eCheckScripts;
  } else if (!UnitTests.empty()) {
    *runMode = eUnitTests;
  } else if (!JsLint.empty()) {
    *runMode = eJsLint;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief copies v8::Object to std::map<std::string, std::string>
////////////////////////////////////////////////////////////////////////////////

static void ObjectToMap(v8::Isolate* isolate,
                        std::map<std::string, std::string>& myMap,
                        v8::Handle<v8::Value> val) {
  v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

  if (v8Headers->IsObject()) {
    v8::Handle<v8::Array> const props = v8Headers->GetPropertyNames();

    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Handle<v8::Value> key = props->Get(v8::Integer::New(isolate, i));
      myMap.emplace(TRI_ObjectToString(key),
                    TRI_ObjectToString(v8Headers->Get(key)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void DestroyConnection(V8ClientConnection* connection) {
  TRI_ASSERT(connection != nullptr);

  auto it = Connections.find(connection);

  if (it != Connections.end()) {
    (*it).second.Reset();
    Connections.erase(it);
  }

  delete connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a new client connection instance
////////////////////////////////////////////////////////////////////////////////

static V8ClientConnection* CreateConnection() {
  return new V8ClientConnection(
      BaseClient.endpointServer(), BaseClient.databaseName(),
      BaseClient.username(), BaseClient.password(), BaseClient.requestTimeout(),
      BaseClient.connectTimeout(), ArangoClient::DEFAULT_RETRIES,
      BaseClient.sslProtocol(), false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback(
    const v8::WeakCallbackData<v8::External, v8::Persistent<v8::External>>&
        data) {
  auto persistent = data.GetParameter();
  auto myConnection =
      v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto connection = static_cast<V8ClientConnection*>(myConnection->Value());

  DestroyConnection(connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WrapV8ClientConnection(
    v8::Isolate* isolate, V8ClientConnection* connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl =
      v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Handle<v8::Object> result = localConnectionTempl->NewInstance();

  auto myConnection = v8::External::New(isolate, connection);
  result->SetInternalField(SLOT_CLASS_TYPE,
                           v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[connection].Reset(isolate, myConnection);
  Connections[connection].SetWeak(&Connections[connection],
                                  ClientConnection_DestructorCallback);
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_ConstructorCallback(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 0 && args[0]->IsString()) {
    std::string definition = TRI_ObjectToString(args[0]);

    BaseClient.createEndpoint(definition);

    if (BaseClient.endpointServer() == nullptr) {
      std::string errorMessage = "error in '" + definition + "'";
      TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
    }
  }

  if (BaseClient.endpointServer() == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }

  V8ClientConnection* connection = CreateConnection();

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (connection->isConnected() &&
      connection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '"
      << BaseClient.endpointServer()->getSpecification() << "', version "
      << connection->getVersion() << " [" << connection->getMode()
      << "], database '" << BaseClient.databaseName() << "', username: '"
      << BaseClient.username() << "'";
    BaseClient.printLine(s.str());
  } else {
    std::string errorMessage =
        "Could not connect. Error message: " + connection->getErrorMessage();
    delete connection;
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMessage.c_str());
  }

  TRI_V8_RETURN(WrapV8ClientConnection(isolate, connection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "reconnect(<endpoint>, <database>, [, <username>, <password>])");
  }

  std::string const definition = TRI_ObjectToString(args[0]);
  std::string databaseName = TRI_ObjectToString(args[1]);

  std::string username;
  if (args.Length() < 3) {
    username = BaseClient.username();
  } else {
    username = TRI_ObjectToString(args[2]);
  }

  std::string password;
  if (args.Length() < 4) {
    BaseClient.printContinuous("Please specify a password: ");

// now prompt for it
#ifdef TRI_HAVE_TERMIOS_H
    TRI_SetStdinVisibility(false);
    getline(cin, password);

    TRI_SetStdinVisibility(true);
#else
    getline(cin, password);
#endif
    BaseClient.printLine("");
  } else {
    password = TRI_ObjectToString(args[3]);
  }

  std::string const oldDefinition = BaseClient.endpointString();
  std::string const oldDatabaseName = BaseClient.databaseName();
  std::string const oldUsername = BaseClient.username();
  std::string const oldPassword = BaseClient.password();

  DestroyConnection(connection);
  ClientConnection = nullptr;

  BaseClient.setEndpointString(definition);
  BaseClient.setDatabaseName(databaseName);
  BaseClient.setUsername(username);
  BaseClient.setPassword(password);

  // re-connect using new options
  BaseClient.createEndpoint();

  if (BaseClient.endpointServer() == nullptr) {
    BaseClient.setEndpointString(oldDefinition);
    BaseClient.setDatabaseName(oldDatabaseName);
    BaseClient.setUsername(oldUsername);
    BaseClient.setPassword(oldPassword);
    BaseClient.createEndpoint();

    std::string errorMessage = "error in '" + definition + "'";
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
  }

  V8ClientConnection* newConnection = CreateConnection();

  if (newConnection->isConnected() &&
      newConnection->getLastHttpReturnCode() == HttpResponse::OK) {
    ostringstream s;
    s << "Connected to ArangoDB '"
      << BaseClient.endpointServer()->getSpecification()
      << "' version: " << newConnection->getVersion() << " ["
      << newConnection->getMode() << "], database: '"
      << BaseClient.databaseName() << "', username: '" << BaseClient.username()
      << "'";

    BaseClient.printLine(s.str());

    args.Holder()->SetInternalField(SLOT_CLASS,
                                    v8::External::New(isolate, newConnection));

    v8::Handle<v8::Value> db =
        isolate->GetCurrentContext()->Global()->Get(TRI_V8_ASCII_STRING("db"));
    if (db->IsObject()) {
      v8::Handle<v8::Object> dbObj = v8::Handle<v8::Object>::Cast(db);

      if (dbObj->Has(TRI_V8_ASCII_STRING("_flushCache")) &&
          dbObj->Get(TRI_V8_ASCII_STRING("_flushCache"))->IsFunction()) {
        v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(
            dbObj->Get(TRI_V8_ASCII_STRING("_flushCache")));

        v8::Handle<v8::Value>* args = nullptr;
        func->Call(dbObj, 0, args);
      }
    }

    ClientConnection = newConnection;

    // ok
    TRI_V8_RETURN_TRUE();
  } else {
    ostringstream s;
    s << "Could not connect to endpoint '" << BaseClient.endpointString()
      << "', username: '" << BaseClient.username() << "'";
    BaseClient.printErrLine(s.str());

    std::string errorMsg = "could not connect";
    if (newConnection->getErrorMessage() != "") {
      errorMsg = newConnection->getErrorMessage();
    }

    delete newConnection;

    // rollback
    BaseClient.setEndpointString(oldDefinition);
    BaseClient.setDatabaseName(oldDatabaseName);
    BaseClient.setUsername(oldUsername);
    BaseClient.setPassword(oldPassword);
    BaseClient.createEndpoint();

    ClientConnection = CreateConnection();
    args.Holder()->SetInternalField(
        SLOT_CLASS, v8::External::New(isolate, ClientConnection));

    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMsg.c_str());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->getData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGet(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->headData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHead(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(connection->deleteData(isolate, *url, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDelete(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("options(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(
      connection->optionsData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptions(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("post(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->postData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPost(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("put(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->putData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPut(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("patch(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);
  v8::String::Utf8Value body(args[1]);

  // check header fields
  std::map<std::string, std::string> headerFields;

  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(connection->patchData(isolate, *url, *body, headerFields, raw));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatch(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection send file helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpSendFile(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>)");
  }

  TRI_Utf8ValueNFC url(TRI_UNKNOWN_MEM_ZONE, args[0]);

  std::string const infile = TRI_ObjectToString(args[1]);

  if (!TRI_ExistsFile(infile.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  size_t bodySize;
  char* body = TRI_SlurpFile(TRI_UNKNOWN_MEM_ZONE, infile.c_str(), &bodySize);

  if (body == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "could not read file");
  }

  v8::TryCatch tryCatch;

  // check header fields
  std::map<std::string, std::string> headerFields;

  v8::Handle<v8::Value> result =
      connection->postData(isolate, *url, body, bodySize, headerFields);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, body);

  if (tryCatch.HasCaught()) {
    // string exception = TRI_StringifyV8Exception(isolate, &tryCatch);
    isolate->ThrowException(tryCatch.Exception());
    return;
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getEndpoint"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getEndpoint(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getEndpoint()");
  }

  std::string const endpoint = BaseClient.endpointString();
  TRI_V8_RETURN_STD_STRING(endpoint);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastHttpReturnCode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastHttpReturnCode()");
  }

  TRI_V8_RETURN(v8::Integer::New(isolate, connection->getLastHttpReturnCode()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastErrorMessage"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastErrorMessage(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastErrorMessage()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getErrorMessage());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_isConnected(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isConnected()");
  }

  if (connection->isConnected()) {
    TRI_V8_RETURN_TRUE();
  } else {
    TRI_V8_RETURN_FALSE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("toString()");
  }

  std::string result = "[object ArangoConnection:" +
                       BaseClient.endpointServer()->getSpecification();

  if (connection->isConnected()) {
    result += "," + connection->getVersion() + ",connected]";
  } else {
    result += ",unconnected]";
  }

  TRI_V8_RETURN_STD_STRING(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getVersion"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getVersion(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getVersion()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getVersion());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getMode"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getMode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getMode()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getMode());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDatabaseName()");
  }

  TRI_V8_RETURN_STD_STRING(connection->getDatabaseName());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "setDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_setDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("setDatabaseName(<name>)");
  }

  std::string const dbName = TRI_ObjectToString(args[0]);
  connection->setDatabaseName(dbName);
  BaseClient.setDatabaseName(dbName);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the shell
////////////////////////////////////////////////////////////////////////////////

static int RunShell(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                    bool promptError) {
  v8::Context::Scope contextScope(context);
  v8::Local<v8::String> name(TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

  auto cc = ClientConnection;

  V8LineEditor console(isolate, context, ".arangosh.history");
  console.setSignalFunction([&cc]() {
    if (cc != nullptr) {
      cc->setInterrupted(true);
    }
  });

  console.open(BaseClient.autoComplete());

  uint64_t nrCommands = 0;

  while (true) {
    // set up prompts
    std::string dynamicPrompt = BuildPrompt(_client);

    std::string goodPrompt;
    std::string badPrompt;

#if _WIN32

    // ........................................................................................
    // Windows console is not coloured by escape sequences. So the method given
    // below will not
    // work. For now we simply ignore the colours until we move the windows
    // version into
    // a GUI Window.
    // ........................................................................................

    goodPrompt = badPrompt = dynamicPrompt;

#else

    if (BaseClient.colors() && console.supportsColors()) {
      // TODO: this should be a function defined in "console"
      goodPrompt =
          TRI_SHELL_COLOR_BOLD_GREEN + dynamicPrompt + TRI_SHELL_COLOR_RESET;
      badPrompt =
          TRI_SHELL_COLOR_BOLD_RED + dynamicPrompt + TRI_SHELL_COLOR_RESET;
    } else {
      goodPrompt = badPrompt = dynamicPrompt;
    }

#endif

    // gc
    if (++nrCommands >= GcInterval) {
      nrCommands = 0;

      TRI_RunGarbageCollectionV8(isolate, 500.0);
    }

    bool eof;
    std::string input = console.prompt(
        promptError ? badPrompt.c_str() : goodPrompt.c_str(), "arangosh", eof);

    if (eof) {
      break;
    }

    if (input.empty()) {
      // input string is empty, but we must still free it
      continue;
    }

    BaseClient.log("%s%s\n", dynamicPrompt, input);

    std::string i = arangodb::basics::StringUtils::trim(input);

    if (i == "exit" || i == "quit" || i == "exit;" || i == "quit;") {
      break;
    }

    if (i == "help" || i == "help;") {
      input = "help()";
    }

    console.addHistory(input);

    v8::TryCatch tryCatch;

    BaseClient.startPager();

    // assume the command succeeds
    promptError = false;

    console.setExecutingCommand(true);

    // execute command and register its result in __LAST__
    v8::Handle<v8::Value> v = TRI_ExecuteJavaScriptString(
        isolate, context, TRI_V8_STRING(input.c_str()), name, true);

    console.setExecutingCommand(false);

    if (v.IsEmpty()) {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"),
                             v8::Undefined(isolate));
    } else {
      context->Global()->Set(TRI_V8_ASCII_STRING("_last"), v);
    }

    if (tryCatch.HasCaught()) {
      // command failed
      std::string exception;

      if (!tryCatch.CanContinue() || tryCatch.HasTerminated()) {
        exception = "command locally aborted\n";
      } else {
        exception = TRI_StringifyV8Exception(isolate, &tryCatch);
      }

      BaseClient.printErrLine(exception);
      BaseClient.log("%s", exception.c_str());

      // this will change the prompt for the next round
      promptError = true;
    }

    if (ClientConnection) {
      ClientConnection->setInterrupted(false);
    }

    BaseClient.stopPager();
    BaseClient.printLine("");

    BaseClient.log("%s\n", "");
    // make sure the last command result makes it into the log file
    BaseClient.flushLog();
  }

  BaseClient.printLine("");

  BaseClient.printByeBye();

  return promptError ? TRI_ERROR_INTERNAL : TRI_ERROR_NO_ERROR;
}




static void InitCallbacks(v8::Isolate* isolate, bool useServer,
                          eRunMode runMode) {
  auto context = isolate->GetCurrentContext();
  // set pretty print default: (used in print.js)
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("PRETTY_PRINT"),
      v8::Boolean::New(isolate, BaseClient.prettyPrint()));

  // add colors for print.js
  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("COLOR_OUTPUT"),
                               v8::Boolean::New(isolate, BaseClient.colors()));

  // add function SYS_OUTPUT to use pager
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_OUTPUT"),
      v8::FunctionTemplate::New(isolate, JS_PagerOutput)->GetFunction());

  TRI_InitV8Buffer(isolate, context);

  TRI_InitV8Utils(isolate, context, StartupPath, StartupModules);
  TRI_InitV8Shell(isolate, context);

  // .............................................................................
  // define ArangoConnection class
  // .............................................................................

  if (useServer) {
    v8::Handle<v8::FunctionTemplate> connection_templ =
        v8::FunctionTemplate::New(isolate);
    connection_templ->SetClassName(TRI_V8_ASCII_STRING("ArangoConnection"));

    v8::Handle<v8::ObjectTemplate> connection_proto =
        connection_templ->PrototypeTemplate();

    connection_proto->Set(
        isolate, "DELETE",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpDelete));
    connection_proto->Set(
        isolate, "DELETE_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpDeleteRaw));
    connection_proto->Set(
        isolate, "GET",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpGet));
    connection_proto->Set(
        isolate, "GET_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpGetRaw));
    connection_proto->Set(
        isolate, "HEAD",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpHead));
    connection_proto->Set(
        isolate, "HEAD_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpHeadRaw));
    connection_proto->Set(
        isolate, "OPTIONS",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptions));
    connection_proto->Set(
        isolate, "OPTIONS_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptionsRaw));
    connection_proto->Set(
        isolate, "PATCH",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatch));
    connection_proto->Set(
        isolate, "PATCH_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatchRaw));
    connection_proto->Set(
        isolate, "POST",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPost));
    connection_proto->Set(
        isolate, "POST_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPostRaw));
    connection_proto->Set(
        isolate, "PUT",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPut));
    connection_proto->Set(
        isolate, "PUT_RAW",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpPutRaw));
    connection_proto->Set(
        isolate, "SEND_FILE",
        v8::FunctionTemplate::New(isolate, ClientConnection_httpSendFile));
    connection_proto->Set(
        isolate, "getEndpoint",
        v8::FunctionTemplate::New(isolate, ClientConnection_getEndpoint));
    connection_proto->Set(isolate, "lastHttpReturnCode",
                          v8::FunctionTemplate::New(
                              isolate, ClientConnection_lastHttpReturnCode));
    connection_proto->Set(
        isolate, "lastErrorMessage",
        v8::FunctionTemplate::New(isolate, ClientConnection_lastErrorMessage));
    connection_proto->Set(
        isolate, "isConnected",
        v8::FunctionTemplate::New(isolate, ClientConnection_isConnected));
    connection_proto->Set(
        isolate, "reconnect",
        v8::FunctionTemplate::New(isolate, ClientConnection_reconnect));
    connection_proto->Set(
        isolate, "toString",
        v8::FunctionTemplate::New(isolate, ClientConnection_toString));
    connection_proto->Set(
        isolate, "getVersion",
        v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));
    connection_proto->Set(
        isolate, "getMode",
        v8::FunctionTemplate::New(isolate, ClientConnection_getMode));
    connection_proto->Set(
        isolate, "getDatabaseName",
        v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));
    connection_proto->Set(
        isolate, "setDatabaseName",
        v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName));
    connection_proto->SetCallAsFunctionHandler(
        ClientConnection_ConstructorCallback);

    v8::Handle<v8::ObjectTemplate> connection_inst =
        connection_templ->InstanceTemplate();
    connection_inst->SetInternalFieldCount(2);

    TRI_AddGlobalVariableVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("ArangoConnection"),
                                 connection_proto->NewInstance());
    ConnectionTempl.Reset(isolate, connection_inst);

    // add the client connection to the context:
    TRI_AddGlobalVariableVocbase(
        isolate, context, TRI_V8_ASCII_STRING("SYS_ARANGO"),
        WrapV8ClientConnection(isolate, ClientConnection));
  }

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_START_PAGER"),
      v8::FunctionTemplate::New(isolate, JS_StartOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_STOP_PAGER"),
      v8::FunctionTemplate::New(isolate, JS_StopOutputPager)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_CSV_FILE"),
      v8::FunctionTemplate::New(isolate, JS_ImportCsvFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_IMPORT_JSON_FILE"),
      v8::FunctionTemplate::New(isolate, JS_ImportJsonFile)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("NORMALIZE_STRING"),
      v8::FunctionTemplate::New(isolate, JS_NormalizeString)->GetFunction());
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("COMPARE_STRING"),
      v8::FunctionTemplate::New(isolate, JS_CompareString)->GetFunction());

  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ARANGO_QUIET"),
                               v8::Boolean::New(isolate, BaseClient.quiet()));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("VALGRIND"),
      v8::Boolean::New(isolate, (RUNNING_ON_VALGRIND > 0)));

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_SCRIPT"),
      v8::Boolean::New(isolate, runMode == eExecuteScript));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_EXECUTE_STRING"),
      v8::Boolean::New(isolate, runMode == eExecuteString));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_CHECK_SCRIPT"),
      v8::Boolean::New(isolate, runMode == eCheckScripts));
  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("IS_UNIT_TESTS"),
      v8::Boolean::New(isolate, runMode == eUnitTests));
  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("IS_JS_LINT"),
                               v8::Boolean::New(isolate, runMode == eJsLint));
}

static int WarmupEnvironment(v8::Isolate* isolate,
                             std::vector<std::string>& positionals,
                             eRunMode runMode) {
  auto context = isolate->GetCurrentContext();
  // .............................................................................
  // read files
  // .............................................................................

  // load java script from js/bootstrap/*.h files
  if (StartupPath.empty()) {
    LOG(FATAL) << "no 'javascript.startup-directory' has been supplied, giving up"; FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using JavaScript startup files at '" << StartupPath << "'";

  StartupLoader.setDirectory(StartupPath);

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

  if (runMode != eJsLint) {
    files.push_back("common/bootstrap/monkeypatches.js");
  }

  files.push_back("client/client.js");  // needs internal

  for (size_t i = 0; i < files.size(); ++i) {
    switch (StartupLoader.loadScript(isolate, context, files[i])) {
      case JSLoader::eSuccess:
        LOG(TRACE) << "loaded JavaScript file '" << files[i] << "'";
        break;
      case JSLoader::eFailLoad:
        LOG(FATAL) << "cannot load JavaScript file '" << files[i] << "'"; FATAL_ERROR_EXIT();
        break;
      case JSLoader::eFailExecute:
        LOG(FATAL) << "error during execution of JavaScript file '" << files[i] << "'"; FATAL_ERROR_EXIT();
        break;
    }
  }

  // .............................................................................
  // create arguments
  // .............................................................................

  v8::Handle<v8::Array> p = v8::Array::New(isolate, (int)positionals.size());

  for (uint32_t i = 0; i < positionals.size(); ++i) {
    p->Set(i, TRI_V8_STD_STRING(positionals[i]));
  }

  TRI_AddGlobalVariableVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ARGUMENTS"), p);
  return EXIT_SUCCESS;
}

class BufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    if (data != nullptr) {
      memset(data, 0, length);
    }
    return data;
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) {
    if (data != nullptr) {
      free(data);
    }
  }
};



////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collection interval
////////////////////////////////////////////////////////////////////////////////

static uint64_t GcInterval = 10;



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



////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to syntax check
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> CheckScripts;

////////////////////////////////////////////////////////////////////////////////
/// @brief the initial default connection
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection* ClientConnection = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief max size body size (used for imports)
////////////////////////////////////////////////////////////////////////////////

static uint64_t ChunkSize = 1024 * 1024 * 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup JavaScript files
////////////////////////////////////////////////////////////////////////////////

static JSLoader StartupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript modules files
////////////////////////////////////////////////////////////////////////////////

static std::string StartupModules = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief path for JavaScript files
////////////////////////////////////////////////////////////////////////////////

static std::string StartupPath = "";

////////////////////////////////////////////////////////////////////////////////
/// @brief put current directory into module path
////////////////////////////////////////////////////////////////////////////////

static bool UseCurrentModulePath = true;

