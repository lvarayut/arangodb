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

  // .............................................................................
  // set-up client connection
  // .............................................................................


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

