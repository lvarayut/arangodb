////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript string
////////////////////////////////////////////////////////////////////////////////

static bool RunString(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                      std::string const& script) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok = true;

  v8::Handle<v8::Value> result =
      TRI_ExecuteJavaScriptString(isolate, context, TRI_V8_STD_STRING(script),
                                  TRI_V8_ASCII_STRING("(command-line)"), false);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

    BaseClient.printErrLine(exception);
    BaseClient.log("%s\n", exception.c_str());
    ok = false;
  } else {
    // check return value of script
    if (result->IsNumber()) {
      int64_t intResult = TRI_ObjectToInt64(result);

      if (intResult != 0) {
        ok = false;
      }
    }
  }

  BaseClient.flushLog();

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript string to execute
////////////////////////////////////////////////////////////////////////////////

static std::string ExecuteString;

////////////////////////////////////////////////////////////////////////////////
/// @brief javascript files to execute
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> ExecuteScripts;

