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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_ARANGORESTORE_FEATURE_H
#define APPLICATION_FEATURES_ARANGORESTORE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "ApplicationFeatures/ClientFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "V8Client/ArangoClientHelper.h"

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}

class ArangorestoreFeature final
    : public application_features::ApplicationFeature,
      public ArangoClientHelper {
 public:
  ArangorestoreFeature(application_features::ApplicationServer* server,
                       int* result);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(
      std::shared_ptr<options::ProgramOptions> options) override;
  void prepare() override;
  void start() override;
  void stop() override;

 private:
  std::vector<std::string> _collections;
  uint64_t _chunkSize;
  bool _includeSystemCollections;
  bool _createDatabase;
  std::string _inputDirectory;
  bool _importData;
  bool _importStructure;
  bool _progress;
  bool _overwrite;
  bool _recycleIds;
  bool _force;
  bool _clusterMode;
  uint64_t _defaultNumberOfShards;

 private:
  int tryCreateDatabase(ClientFeature*, std::string const& name);
  int sendRestoreCollection(VPackSlice const& slice, std::string const& name,
                            std::string& errorMsg);
  int sendRestoreIndexes(VPackSlice const& slice, std::string& errorMsg);
  int sendRestoreData(std::string const& cname, char const* buffer,
                      size_t bufferSize, std::string& errorMsg);
  int processInputDirectory(std::string& errorMsg);

 private:
  int* _result;

  // statistics
  struct {
    uint64_t _totalBatches;
    uint64_t _totalCollections;
    uint64_t _totalRead;
  } _stats;
};
}

#endif
