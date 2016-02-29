////////////////////////////////////////////////////////////////////////////////
/// @brief runs the jslint tests
////////////////////////////////////////////////////////////////////////////////

static bool RunJsLint(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);
  bool ok;

  // set-up jslint files array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0; i < JsLint.size(); ++i) {
    sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(JsLint[i]));
  }

  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                         v8::True(isolate));

  // run tests
  auto input =
      TRI_V8_ASCII_STRING("require(\"jslint\").runCommandLineTests({ });");
  auto name = TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME);
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    BaseClient.printErrLine(TRI_StringifyV8Exception(isolate, &tryCatch));
    ok = false;
  } else {
    ok = TRI_ObjectToBoolean(
        context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief files to jslint
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> JsLint;


