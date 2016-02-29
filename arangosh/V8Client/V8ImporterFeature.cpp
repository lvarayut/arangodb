////////////////////////////////////////////////////////////////////////////////
/// @brief imports a CSV file
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The separator is @CODE{\,} and the quote is @CODE{"}.
///
/// @FUN{importCsvFile(@FA{filename}, @FA{collection}, @FA{options})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
////The separator is @CODE{\,} and the quote is @CODE{"}.
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportCsvFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "importCsvFile(<filename>, <collection>[, <options>])");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  // extract the options
  v8::Handle<v8::String> separatorKey = TRI_V8_ASCII_STRING("separator");
  v8::Handle<v8::String> quoteKey = TRI_V8_ASCII_STRING("quote");

  std::string separator = ",";
  std::string quote = "\"";

  if (3 <= args.Length()) {
    v8::Handle<v8::Object> options = args[2]->ToObject();
    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(options->Get(separatorKey));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.separator must be at least one character");
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(options->Get(quoteKey));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.quote must be at most one character");
      }
    }
  }

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  std::string fileName = TRI_ObjectToString(args[0]);
  std::string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));
    result->Set(TRI_V8_ASCII_STRING("errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));
    result->Set(TRI_V8_ASCII_STRING("updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));
    result->Set(TRI_V8_ASCII_STRING("ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                 ih.getErrorMessage().c_str());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a JSON file
///
/// @FUN{importJsonFile(@FA{filename}, @FA{collection})}
///
/// Imports data of a CSV file. The data is imported to @FA{collection}.
///
////////////////////////////////////////////////////////////////////////////////

static void JS_ImportJsonFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importJsonFile(<filename>, <collection>)");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  ImportHelper ih(ClientConnection->getHttpClient(), ChunkSize);

  std::string fileName = TRI_ObjectToString(args[0]);
  std::string collectionName = TRI_ObjectToString(args[1]);

  if (ih.importJson(collectionName, fileName)) {
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));
    result->Set(TRI_V8_ASCII_STRING("created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));
    result->Set(TRI_V8_ASCII_STRING("errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));
    result->Set(TRI_V8_ASCII_STRING("updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));
    result->Set(TRI_V8_ASCII_STRING("ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));
    TRI_V8_RETURN(result);
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                 ih.getErrorMessage().c_str());
  TRI_V8_TRY_CATCH_END
}

  description("chunk-size", &ChunkSize, "maximum size for individual data batches (in bytes)"
