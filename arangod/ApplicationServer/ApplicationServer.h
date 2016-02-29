////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGOD_APPLICATION_SERVER_APPLICATION_SERVER_H
#define ARANGOD_APPLICATION_SERVER_APPLICATION_SERVER_H 1

#include "Basics/Common.h"

#include "Basics/ProgramOptions.h"
#include "Basics/ConditionVariable.h"

namespace arangodb {
namespace basics {
class ProgramOptionsDescription;
}

namespace rest {
class ApplicationFeature;

////////////////////////////////////////////////////////////////////////////////
/// @brief application server
////////////////////////////////////////////////////////////////////////////////

class ApplicationServer {
  ApplicationServer(const ApplicationServer&);
  ApplicationServer& operator=(const ApplicationServer&);

 public:
  ApplicationServer(std::string const& name, std::string const& title,
                    std::string const& version);

  virtual ~ApplicationServer();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a new feature
  //////////////////////////////////////////////////////////////////////////////

  void addFeature(ApplicationFeature*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the name of the system config file
  //////////////////////////////////////////////////////////////////////////////

  void setSystemConfigFile(std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the name of the user config file
  //////////////////////////////////////////////////////////////////////////////

  void setUserConfigFile(std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the name of the application
  //////////////////////////////////////////////////////////////////////////////

  std::string const& getName() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets up the logging
  //////////////////////////////////////////////////////////////////////////////

  void setupLogging(bool thread, bool daemon, bool backgrounded);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the command line options
  //////////////////////////////////////////////////////////////////////////////

  basics::ProgramOptions& programOptions();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the command line arguments
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> programArguments();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses the arguments with empty options description
  //////////////////////////////////////////////////////////////////////////////

  bool parse(int argc, char* argv[]);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses the arguments
  //////////////////////////////////////////////////////////////////////////////

  bool parse(
      int argc, char* argv[],
      std::map<std::string, arangodb::basics::ProgramOptionsDescription>);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepares the server
  //////////////////////////////////////////////////////////////////////////////

  void prepare();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepares the server
  //////////////////////////////////////////////////////////////////////////////

  void prepare2();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief starts the scheduler
  //////////////////////////////////////////////////////////////////////////////

  void start();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief waits for shutdown
  //////////////////////////////////////////////////////////////////////////////

  void wait();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief begins shutdown sequence
  //////////////////////////////////////////////////////////////////////////////

  void beginShutdown();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief stops everything
  //////////////////////////////////////////////////////////////////////////////

  void stop();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extracts the privileges from the command-line
  //////////////////////////////////////////////////////////////////////////////

  void extractPrivileges();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drops the privileges permanently
  //////////////////////////////////////////////////////////////////////////////

  void dropPrivilegesPermanently();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds options to description
  //////////////////////////////////////////////////////////////////////////////

  void setupOptions(std::map<std::string, basics::ProgramOptionsDescription>&);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if the parent is still alive
  //////////////////////////////////////////////////////////////////////////////

  bool checkParent();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reads the configuration files
  //////////////////////////////////////////////////////////////////////////////

  bool readConfigurationFile();

 private:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock generalHelp
  ////////////////////////////////////////////////////////////////////////////////

  basics::ProgramOptions _options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief command line option description
  //////////////////////////////////////////////////////////////////////////////

  basics::ProgramOptionsDescription _description;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief config file option description
  //////////////////////////////////////////////////////////////////////////////

  basics::ProgramOptionsDescription _descriptionFile;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief command line arguments
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> _arguments;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief defined features
  //////////////////////////////////////////////////////////////////////////////

  std::vector<ApplicationFeature*> _features;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief exit on parent death
  //////////////////////////////////////////////////////////////////////////////

  bool _exitOnParentDeath;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief watch parent
  //////////////////////////////////////////////////////////////////////////////

  int _watchParent;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shutdown requested
  //////////////////////////////////////////////////////////////////////////////

  volatile sig_atomic_t _stopping;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief name of the application
  //////////////////////////////////////////////////////////////////////////////

  std::string _name;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief title of the application
  //////////////////////////////////////////////////////////////////////////////

  std::string _title;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock generalVersion
  ////////////////////////////////////////////////////////////////////////////////

  std::string _version;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock configurationFilename
  ////////////////////////////////////////////////////////////////////////////////

  std::string _configFile;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief user config file
  //////////////////////////////////////////////////////////////////////////////

  std::string _userConfigFile;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief system config file
  //////////////////////////////////////////////////////////////////////////////

  std::string _systemConfigFile;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock configurationUid
  ////////////////////////////////////////////////////////////////////////////////

  std::string _uid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief numeric uid
  //////////////////////////////////////////////////////////////////////////////

  TRI_uid_t _numericUid;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock configurationGid
  ////////////////////////////////////////////////////////////////////////////////

  std::string _gid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief numeric gid
  //////////////////////////////////////////////////////////////////////////////

  TRI_gid_t _numericGid;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock logApplication
  ////////////////////////////////////////////////////////////////////////////////

  std::string _logApplicationName;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock logFacility
  ////////////////////////////////////////////////////////////////////////////////

  std::string _logFacility;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief log.level
  ////////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> _logLevel;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief log.output
  ////////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> _logOutput;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock logContentFilter
  ////////////////////////////////////////////////////////////////////////////////

  std::string _logContentFilter;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief was docuBlock randomGenerator
  ////////////////////////////////////////////////////////////////////////////////

  uint32_t _randomGenerator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief condition variable for done
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::ConditionVariable _finishedCondition;
};
}
}

#endif
