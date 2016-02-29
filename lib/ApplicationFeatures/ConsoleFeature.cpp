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

#include "ApplicationFeatures/ConsoleFeature.h"

#include "Basics/terminal-utils.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

ConsoleFeature::ConsoleFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "ConsoleFeature"),
#if WIN32
      _codePage(-1),
#endif
      _quiet(false),
      _colors(true),
      _autoComplete(true),
      _prettyPrint(true),
      _auditFile(),
      _pager(false),
      _pagerCommand("less -X -R -F -L"),
      _prompt("%E@%d> "),
      _cygwinShell(false) {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void ConsoleFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection(Section("console", "Configure the console",
                              "console options", false, false));

  options->addOption("--console.quiet", "silent startup",
                     new BooleanParameter(&_quiet));

  options->addOption("--console.colors", "enable color support",
                     new BooleanParameter(&_colors));

  options->addOption("--console.auto-complete", "enable auto completion",
                     new BooleanParameter(&_autoComplete));

  options->addOption("--console.pretty-print", "enable pretty printing",
                     new BooleanParameter(&_prettyPrint));

  options->addOption("--console.audit-file",
                     "audit log file to save commands and results",
                     new StringParameter(&_auditFile));

  options->addOption("--console.pager", "enable paging",
                     new BooleanParameter(&_pager));

  options->addOption("--console.pager-command", "pager command",
                     new StringParameter(&_pagerCommand));

  options->addOption("--console.prompt", "prompt used in REPL",
                     new StringParameter(&_prompt));

#if WIN32
  options->addOption("--console.code-page", "Windows code page to use",
                     new Int16Parameter(&_codePage));
#endif
}

void ConsoleFeature::prepare() {
#if _WIN32
  if (getenv("SHELL") != nullptr) {
    _cygwinShell = true;
  }
#endif  
}

// prints a string to stdout, without a newline (Non-Windows only) on
// Windows, we'll print the line and a newline.  No, we cannot use
// std::cout as this doesn't support UTF-8 on Windows.

void ConsoleFeature::printContinuous(std::string const& s) {
#ifdef _WIN32
  // On Windows, we just print the line followed by a newline
  printLine(s, true);
#else
  fprintf(stdout, "%s", s.c_str());
  fflush(stdout);
#endif
}

#ifdef _WIN32
static bool _newLine() {
  COORD pos;
  CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
  if (bufferInfo.dwCursorPosition.Y + 1 >= bufferInfo.dwSize.Y) {
    // when we are at the last visible line of the console
    // the first line of console is deleted (the content of the console
    // is scrolled one line above
    SMALL_RECT srctScrollRect;
    srctScrollRect.Top = 0;
    srctScrollRect.Bottom = bufferInfo.dwCursorPosition.Y + 1;
    srctScrollRect.Left = 0;
    srctScrollRect.Right = bufferInfo.dwSize.X;
    COORD coordDest;
    coordDest.X = 0;
    coordDest.Y = -1;
    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
    CHAR_INFO chiFill;
    chiFill.Char.AsciiChar = (char)' ';
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                   &consoleScreenBufferInfo)) {
      chiFill.Attributes = consoleScreenBufferInfo.wAttributes;
    } else {
      // Fill the bottom row with green blanks.
      chiFill.Attributes = BACKGROUND_GREEN | FOREGROUND_RED;
    }
    ScrollConsoleScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE), &srctScrollRect,
                              nullptr, coordDest, &chiFill);
    pos.Y = bufferInfo.dwCursorPosition.Y;
    pos.X = 0;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    return true;
  } else {
    pos.Y = bufferInfo.dwCursorPosition.Y + 1;
    pos.X = 0;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    return false;
  }
}
#endif

#ifdef _WIN32
static void _printLine(std::string const& s) {
  LPWSTR wBuf = (LPWSTR)TRI_Allocate(TRI_CORE_MEM_ZONE,
                                     (sizeof WCHAR) * (s.size() + 1), true);
  int wLen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wBuf,
                                 (int)((sizeof WCHAR) * (s.size() + 1)));

  if (wLen) {
    DWORD n;
    COORD pos;
    CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    // save old cursor position
    pos = bufferInfo.dwCursorPosition;

    size_t newX = static_cast<size_t>(pos.X) + s.size();
    // size_t oldY = static_cast<size_t>(pos.Y);
    if (newX >= static_cast<size_t>(bufferInfo.dwSize.X)) {
      for (size_t i = 0; i <= newX / bufferInfo.dwSize.X; ++i) {
        // insert as many newlines as we need. this prevents running out of
        // buffer space when printing lines
        // at the end of the console output buffer
        if (_newLine()) {
          pos.Y = pos.Y - 1;
        }
      }
    }

    // save new cursor position
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &bufferInfo);
    auto newPos = bufferInfo.dwCursorPosition;

    // print the actual string. note: printing does not advance the cursor
    // position
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
    WriteConsoleOutputCharacterW(GetStdHandle(STD_OUTPUT_HANDLE), wBuf,
                                 (DWORD)s.size(), pos, &n);

    // finally set the cursor position to where the printing should have
    // stopped
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), newPos);
  } else {
    fprintf(stdout, "window error: '%d' \r\n", GetLastError());
    fprintf(stdout, "%s\r\n", s.c_str());
  }

  if (wBuf) {
    TRI_Free(TRI_CORE_MEM_ZONE, wBuf);
  }
}
#endif

void ConsoleFeature::printLine(std::string const& s, bool forceNewLine) {
#if _WIN32
  if (!cygwinShell) {
    // no, we cannot use std::cout as this doesn't support UTF-8 on Windows
    // fprintf(stdout, "%s\r\n", s.c_str());
    TRI_vector_string_t subStrings = TRI_SplitString(s.c_str(), '\n');
    bool hasNewLines = (s.find("\n") != std::string::npos) | forceNewLine;
    if (hasNewLines) {
      for (size_t i = 0; i < subStrings._length; i++) {
        _printLine(subStrings._buffer[i]);
        _newLine();
      }
    } else {
      _printLine(s);
    }
    TRI_DestroyVectorString(&subStrings);
  } else
#endif
    fprintf(stdout, "%s\n", s.c_str());
}

std::string ConsoleFeature::readPassword(std::string const& message) {
  std::string password;

  printContinuous(message);

#ifdef TRI_HAVE_TERMIOS_H
  TRI_SetStdinVisibility(false);
  std::getline(std::cin, password);

  TRI_SetStdinVisibility(true);
#else
  std::getline(std::cin, password);
#endif
  ConsoleFeature::printLine("");

  return password;
}

#warning TODO
#if 0

////////////////////////////////////////////////////////////////////////////////
/// @brief dynamically replace %d, %e, %u in the prompt
////////////////////////////////////////////////////////////////////////////////

static std::string BuildPrompt() {
  std::string result;

  char const* p = Prompt.c_str();
  bool esc = false;

  while (true) {
    char const c = *p;

    if (c == '\0') {
      break;
    }

    if (esc) {
      if (c == '%') {
        result.push_back(c);
      } else if (c == 'd') {
        result.append(BaseClient.databaseName());
      } else if (c == 'e' || c == 'E') {
        std::string ep;

        if (ClientConnection == nullptr) {
          ep = "none";
        } else {
          ep = BaseClient.endpointString();
        }

        if (c == 'E') {
          // replace protocol
          if (ep.find("tcp://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("ssl://") == 0) {
            ep = ep.substr(6);
          } else if (ep.find("unix://") == 0) {
            ep = ep.substr(7);
          }
        }

        result.append(ep);
      } else if (c == 'u') {
        result.append(BaseClient.username());
      }

      esc = false;
    } else {
      if (c == '%') {
        esc = true;
      } else {
        result.push_back(c);
      }
    }

    ++p;
  }

  return result;
}



static bool PrintHelo(bool useServer) {
  bool promptError = false;

  // .............................................................................
  // banner
  // .............................................................................

  // http://www.network-science.de/ascii/   Font: ogre

  if (!BaseClient.quiet()) {
#ifdef _WIN32

    // .............................................................................
    // Quick hack for windows
    // .............................................................................

    if (BaseClient.colors()) {
      int greenColour = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
      int redColour = FOREGROUND_RED | FOREGROUND_INTENSITY;
      int defaultColour = 0;
      CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

      if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                     &csbiInfo) != 0) {
        defaultColour = csbiInfo.wAttributes;
      }

      // not sure about the code page. let user set code page by command-line
      // argument if required
      if (CodePage > 0) {
        SetConsoleOutputCP((UINT)CodePage);
      } else {
        UINT cp = GetConsoleOutputCP();
        SetConsoleOutputCP(cp);
      }

      // TODO we should have a special "printf" which can handle the color
      // escape sequences!
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("                                  ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("     _     ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("  __ _ _ __ __ _ _ __   __ _  ___ ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf(" ___| |__  ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf(" / _` | '__/ _` | '_ \\ / _` |/ _ \\");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("/ __| '_ \\ ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("| (_| | | | (_| | | | | (_| | (_) ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("\\__ \\ | | |");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf(" \\__,_|_|  \\__,_|_| |_|\\__, |\\___/");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("|___/_| |_|");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), greenColour);
      printf("                       |___/      ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), redColour);
      printf("           ");
      SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), defaultColour);
      printf("\n");
    }

#else

    char const* g = TRI_SHELL_COLOR_GREEN;
    char const* r = TRI_SHELL_COLOR_RED;
    char const* z = TRI_SHELL_COLOR_RESET;

    if (!BaseClient.colors()) {
      g = "";
      r = "";
      z = "";
    }

    BaseClient.printLine("");

    printf("%s                                  %s     _     %s\n", g, r, z);
    printf("%s  __ _ _ __ __ _ _ __   __ _  ___ %s ___| |__  %s\n", g, r, z);
    printf("%s / _` | '__/ _` | '_ \\ / _` |/ _ \\%s/ __| '_ \\ %s\n", g, r, z);
    printf("%s| (_| | | | (_| | | | | (_| | (_) %s\\__ \\ | | |%s\n", g, r, z);
    printf("%s \\__,_|_|  \\__,_|_| |_|\\__, |\\___/%s|___/_| |_|%s\n", g, r,
           z);
    printf("%s                       |___/      %s           %s\n", g, r, z);

#endif
    BaseClient.printLine("");

    ostringstream s;
    s << "arangosh (" << arangodb::rest::Version::getVerboseVersionString()
      << ")" << std::endl;
    s << "Copyright (c) ArangoDB GmbH";

    BaseClient.printLine(s.str(), true);
    BaseClient.printLine("", true);

    BaseClient.printWelcomeInfo();

    if (useServer) {
      if (ClientConnection && ClientConnection->isConnected() &&
          ClientConnection->getLastHttpReturnCode() == HttpResponse::OK) {
        ostringstream is;
        is << "Connected to ArangoDB '" << BaseClient.endpointString()
           << "' version: " << ClientConnection->getVersion() << " ["
           << ClientConnection->getMode() << "], database: '"
           << BaseClient.databaseName() << "', username: '"
           << BaseClient.username() << "'";

        BaseClient.printLine(is.str(), true);
      } else {
        ostringstream is;
        is << "Could not connect to endpoint '" << BaseClient.endpointString()
           << "', database: '" << BaseClient.databaseName() << "', username: '"
           << BaseClient.username() << "'";
        BaseClient.printErrLine(is.str());

        if (ClientConnection && ClientConnection->getErrorMessage() != "") {
          ostringstream is2;
          is2 << "Error message '" << ClientConnection->getErrorMessage()
              << "'";
          BaseClient.printErrLine(is2.str());
        }
        promptError = true;
      }

      BaseClient.printLine("", true);
    }
  }

  return promptError;
}

#endif
