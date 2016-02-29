int main(int argc, char* args[]) {
  {
    std::ostringstream foxxManagerHelp;
    foxxManagerHelp
        << "Use  " << args[0]
        << " help  to get an overview of the actions specific to foxx-manager."
        << endl
        << endl;
    foxxManagerHelp << "There is also an online manual available at:" << endl
                    << "https://docs.arangodb.com/Foxx/Install/" << endl
                    << endl;

    BaseClient.setupSpecificHelp("foxx-manager", foxxManagerHelp.str());
  }

  BaseClient.setEndpointString(Endpoint::getDefaultEndpoint());

  // .............................................................................
  // parse the program options
  // .............................................................................

  std::vector<std::string> positionals =
      ParseProgramOptions(argc, args, &runMode);

  // .............................................................................
  // set-up client connection
  // .............................................................................

  // check if we want to connect to a server
  bool useServer = (BaseClient.endpointString() != "none");

  // if we are in jslint mode, we will not need the server at all
  if (!JsLint.empty()) {
    useServer = false;
  }

  if (useServer) {
    BaseClient.createEndpoint();

    if (BaseClient.endpointServer() == nullptr) {
      ostringstream s;
      s << "invalid value for --server.endpoint ('"
        << BaseClient.endpointString() << "')";

      BaseClient.printErrLine(s.str());

      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    ClientConnection = CreateConnection();
  }

  // .............................................................................
  // set-up V8 objects
  // .............................................................................


  v8::V8::InitializeICU();

  // set V8 options
  if (!V8Options.empty()) {
    // explicit option --javascript.v8-options used
    v8::V8::SetFlagsFromString(V8Options.c_str(), (int)V8Options.size());
  } else {
    // no explicit option used, now pass all command-line arguments to v8
    v8::V8::SetFlagsFromCommandLine(&argc, args, true);
  }

#ifdef TRI_FORCE_ARMV6
  std::string const forceARMv6 = "--noenable-armv7";
  v8::V8::SetFlagsFromString(forceARMv6.c_str(), (int)forceARMv6.size());
#endif

  v8::Platform* platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();

  BufferAllocator bufferAllocator;
  v8::V8::SetArrayBufferAllocator(&bufferAllocator);

  v8::Isolate* isolate = v8::Isolate::New();
  isolate->Enter();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    {
      // create the global template
      v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

      // create the context
      v8::Persistent<v8::Context> context;
      context.Reset(isolate, v8::Context::New(isolate, 0, global));
      auto localContext = v8::Local<v8::Context>::New(isolate, context);

      if (localContext.IsEmpty()) {
        BaseClient.printErrLine("cannot initialize V8 engine");
        TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
      }

      localContext->Enter();

      v8::Handle<v8::Object> globalObj = localContext->Global();
      globalObj->Set(TRI_V8_ASCII_STRING("GLOBAL"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("global"), globalObj);
      globalObj->Set(TRI_V8_ASCII_STRING("root"), globalObj);

      InitCallbacks(isolate, useServer, runMode);

      // reset the prompt error flag (will determine prompt colors)
      bool promptError = PrintHelo(useServer);

      ret = WarmupEnvironment(isolate, positionals, runMode);

      if (ret == EXIT_SUCCESS) {
        BaseClient.openLog();

        try {
          ret = Run(isolate, runMode, promptError);
        } catch (std::bad_alloc const&) {
          LOG(ERR) << "caught exception " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
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

      localContext->Exit();
      context.Reset();
    }
  }

  if (ClientConnection != nullptr) {
    DestroyConnection(ClientConnection);
    ClientConnection = nullptr;
  }

  TRI_v8_global_t* v8g = TRI_GetV8Globals(isolate);
  delete v8g;

  isolate->Exit();
  isolate->Dispose();

  BaseClient.closeLog();

  TRIAGENS_REST_SHUTDOWN;

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete platform;
