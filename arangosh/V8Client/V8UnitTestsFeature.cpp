////////////////////////////////////////////////////////////////////////////////
/// @brief runs the unit tests
////////////////////////////////////////////////////////////////////////////////

static bool RunUnitTests(v8::Isolate* isolate,
                         v8::Handle<v8::Context> context) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok;

  // set-up unit tests array
  v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

  for (size_t i = 0; i < UnitTests.size(); ++i) {
    sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(UnitTests[i]));
  }

  TRI_AddGlobalVariableVocbase(
      isolate, context, TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
  // do not use TRI_AddGlobalVariableVocBase because it creates read-only
  // variables!!
  context->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                         v8::True(isolate));

  // run tests
  auto input = TRI_V8_ASCII_STRING(
      "require(\"@arangodb/testrunner\").runCommandLineTests();");
  auto name = TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME);
  TRI_ExecuteJavaScriptString(isolate, context, input, name, true);

  if (tryCatch.HasCaught()) {
    std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));
    BaseClient.printErrLine(exception);
    ok = false;
  } else {
    ok = TRI_ObjectToBoolean(
        context->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unit file test cases
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> UnitTests;


