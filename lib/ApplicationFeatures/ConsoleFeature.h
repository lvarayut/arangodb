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

#ifndef APPLICATION_FEATURES_CONSOLE_FEATURE_H
#define APPLICATION_FEATURES_CONSOLE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
class ConsoleFeature final : public application_features::ApplicationFeature {
 public:
  explicit ConsoleFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;

 public:
#ifdef WIN32
  int16_t _codePage;
#endif
  bool _quiet;
  bool _colors;
  bool _autoComplete;
  bool _prettyPrint;
  std::string _auditFile;
  bool _pager;
  std::string _pagerCommand;
  std::string _prompt;

 private:
  FILE* _toPager;
  FILE* _toAuditFile;
};
}

#endif
