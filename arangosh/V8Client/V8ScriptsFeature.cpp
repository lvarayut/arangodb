////////////////////////////////////////////////////////////////////////////////
/// @brief executes the Javascript files
////////////////////////////////////////////////////////////////////////////////

static bool RunScripts(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                       std::vector<std::string> const& scripts, bool execute) {
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  bool ok = true;

  for (size_t i = 0; i < scripts.size(); ++i) {
    if (!FileUtils::exists(scripts[i])) {
      std::string msg =
          "error: Javascript file not found: '" + scripts[i] + "'";

      BaseClient.printErrLine(msg.c_str());
      BaseClient.log("%s", msg.c_str());

      ok = false;
      break;
    }

    if (execute) {
      v8::Handle<v8::String> name = TRI_V8_STD_STRING(scripts[i]);
      v8::Handle<v8::Value> args[] = {name};
      v8::Handle<v8::Value> filename = args[0];

      v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
      auto oldFilename = current->Get(TRI_V8_ASCII_STRING("__filename"));
      current->ForceSet(TRI_V8_ASCII_STRING("__filename"), filename);

      auto oldDirname = current->Get(TRI_V8_ASCII_STRING("__dirname"));
      auto dirname = TRI_Dirname(TRI_ObjectToString(filename).c_str());
      current->ForceSet(TRI_V8_ASCII_STRING("__dirname"),
                        TRI_V8_STRING(dirname));
      TRI_FreeString(TRI_CORE_MEM_ZONE, dirname);

      ok = TRI_ExecuteGlobalJavaScriptFile(isolate, scripts[i].c_str());

      // restore old values for __dirname and __filename
      if (oldFilename.IsEmpty() || oldFilename->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING("__filename"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING("__filename"), oldFilename);
      }
      if (oldDirname.IsEmpty() || oldDirname->IsUndefined()) {
        current->Delete(TRI_V8_ASCII_STRING("__dirname"));
      } else {
        current->ForceSet(TRI_V8_ASCII_STRING("__dirname"), oldDirname);
      }
    } else {
      TRI_ParseJavaScriptFile(isolate, scripts[i].c_str());
    }

    if (tryCatch.HasCaught()) {
      std::string exception(TRI_StringifyV8Exception(isolate, &tryCatch));

      BaseClient.printErrLine(exception);
      BaseClient.log("%s\n", exception.c_str());

      ok = false;
      break;
    }
    if (!ok) {
      break;
    }
  }

  BaseClient.flushLog();

  return ok;
}

