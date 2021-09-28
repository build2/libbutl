// file      : libbutl/command.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <map>
#include <string>
#include <cstddef>    // size_t
#include <functional>

#include <libbutl/process.hxx>
#include <libbutl/optional.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Run a process or a builtin, interpreting the command line as
  // whitespace-separated, potentially quoted program path/builtin name,
  // arguments, and redirects. Throw std::invalid_argument on the parsing
  // error, ios::failure on the underlying OS error, process_error on the
  // process running error and std::system_error on the builtin running error.
  //
  // To run a system utility rather than a builtin prefix its name with `^`,
  // for example:
  //
  // ^cat --squeeze-blank file
  //
  // The process environment path is unused and must point to the empty
  // process path.
  //
  // Currently only the following stdout redirects are supported:
  //
  // >file   # Overwrite file.
  // >>file  # Append to file.
  //
  // In particular, the file descriptor cannot be specified. The file path can
  // optionally be separated from '>' by whitespaces. Note that redirects are
  // distinguished from arguments by the presence of leading '>' and prior to
  // possible substitutions (so the redirect character cannot be the result of
  // a substitution; see below).
  //
  // The relative redirect file paths are completed using the command
  // current working directory. Note that if it is altered via the process
  // environment, then the new value is used.
  //
  // The command line elements (program, arguments, etc) may optionally
  // contain substitutions - variable names enclosed with the substitution
  // symbol ('@' by default) - which are replaced with the corresponding
  // variable values to produce the actual command. Variable names must not
  // contain whitespaces and an attempt to substitute an unknown or a
  // malformed variable is an error. Double substitution character ('@@' by
  // default) is an escape sequence.
  //
  // If the variable map is absent, then '@' has no special meaning and is
  // treated as a regular character.
  //
  // The callback function, if specified, is called prior to running the
  // command process with the substituted command elements and including
  // redirects which will be in the "canonical" form (single argument without
  // space after '>'). The callback can be used, for example, for tracing the
  // resulting command line, etc.
  //
  using command_substitution_map = std::map<std::string, std::string>;
  using command_callback = void (const char* const args[], std::size_t n);

  LIBBUTL_SYMEXPORT process_exit
  command_run (const std::string& command,
               const optional<process_env>& = nullopt,
               const optional<command_substitution_map>& = nullopt,
               char subst = '@',
               const std::function<command_callback>& = {});

  // Reusable substitution utility functions.
  //
  // Unlike command_run(), these support different opening and closing
  // substitution characters (e.g., <name>). Note that unmatched closing
  // characters are treated literally and there is no support for their
  // escaping (which would only be necessary if we needed to support variable
  // names containing the closing character).

  // Perform substitutions in a string. The second argument should be the
  // position of the openning substitution character in the passed string.
  // Throw invalid_argument for a malformed substitution or an unknown
  // variable name.
  //
  LIBBUTL_SYMEXPORT std::string
  command_substitute (const std::string&, std::size_t,
                      const command_substitution_map&,
                      char open, char close);

  // As above but using a callback instead of a map.
  //
  // Specifically, on success, the callback should substitute the specified
  // variable in out by appending its value and returning true. On failure,
  // the callback can either throw invalid_argument or return false, in which
  // case the standard "unknown substitution variable ..." exception will be
  // thrown.
  //
  using command_substitution_callback =
    bool (const std::string& var, std::string& out);

  LIBBUTL_SYMEXPORT std::string
  command_substitute (const std::string&, std::size_t,
                      const std::function<command_substitution_callback>&,
                      char open, char close);
}
