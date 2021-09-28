// file      : libbutl/default-options.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/small-vector.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Default options files helper implementation.
  //
  struct default_options_files
  {
    small_vector<path, 2> files;
    optional<dir_path>    start;
  };

  template <typename O>
  struct default_options_entry
  {
    path                         file;
    O                            options;
    small_vector<std::string, 1> arguments;
    bool                         remote;
  };

  template <typename O>
  using default_options = small_vector<default_options_entry<O>, 4>;

  // Search for and load the specified list of options files in the specified
  // directories returning a vector of option class instances (O). Read
  // additional options from files referenced by the specified option
  // (normally --options-file). If args is false, only options are allowed and
  // are parsed using scanner S in the U::fail mode. If args is true, then
  // both options and arguments are allowed in any order with options parsed
  // in the U::stop mode.
  //
  // Pass each default options file path to the specified function prior to
  // load (can be used for tracing, etc). The function signature is:
  //
  // void (const path&, bool remote, bool overwrite)
  //
  // Note that the function may be called for the same file twice if it was
  // later discovered that it is in fact remote. In the second call the
  // overwrite flag will be true.
  //
  // Throw `pair<path, system_error>` on the underlying OS error with the
  // first half referring the filesystem entry the error relates to and pass
  // through exceptions thrown by the options scanner/parser.
  //
  // Search order:
  //
  // - sys_dir
  // - home_dir
  // - extra_dir (can also be handled during the start/outer traversal)
  // - start_dir and outer until home_dir or root (both excluding)
  //
  // Except for sys_dir and extra_dir, the options files are looked for in the
  // .build2/ and .build2/local/ subdirectories of each directory. For
  // sys_dir and extra_dir they are looked for in the directory itself (e.g.,
  // /etc/build2/).
  //
  // Note that the search is stopped at the directory containing a file with
  // --no-default-options.
  //
  // Also note that all the directories should be absolute and normalized.
  //
  // The presence of the .git filesystem entry causes the options files in
  // this directory and any of its subdirectories to be considered remote
  // (note that in the current implementation this is the case even for files
  // from the .build2/local/ subdirectory since the mere location is not a
  // sufficient ground to definitively conclude that the file is not remote;
  // to be sure we would need to query the VCS or some such).
  //
  // Note that the extra directory options files are never considered remote.
  //
  // For the convenience of implementation, the function parses the option
  // files in the reverse order. Thus, to make sure that positions in the
  // options list monotonically increase, it needs the maximum number of
  // arguments, globally and per file, to be specified. This way the starting
  // options position for each file will be less than for the previously
  // parsed file by arg_max_file and equal to arg_max - arg_max_file for the
  // first file. If the actual number of arguments exceeds the specified, then
  // invalid_argument is thrown.
  //
  template <typename O, typename S, typename U, typename F>
  default_options<O>
  load_default_options (const optional<dir_path>& sys_dir,
                        const optional<dir_path>& home_dir,
                        const optional<dir_path>& extra_dir,
                        const default_options_files&,
                        F&&,
                        const std::string& option,
                        std::size_t arg_max,
                        std::size_t arg_max_file,
                        bool args = false);

  // Merge the default options/arguments and the command line
  // options/arguments.
  //
  // Note that these are the default implementations and in some cases you may
  // want to provide an options class-specific version that verifies/sanitizes
  // the default options/arguments (e.g., you may not want to allow certain
  // options to be specified in the default options files) or warns/prompts
  // about potentially dangerous options if they came from the remote options
  // files.
  //
  template <typename O>
  O
  merge_default_options (const default_options<O>&, const O& cmd_ops);

  template <typename O, typename AS>
  AS
  merge_default_arguments (const default_options<O>&, const AS& cmd_args);

  // As above but pass each default option/argument entry to the specified
  // function prior to merging. The function signature is:
  //
  // void (const default_options_entry<O>&, const O& cmd_ops)
  //
  // This version can be used to verify the default options/arguments. For
  // example, you may want to disallow certain options/arguments from being
  // specified in the default options files.
  //
  template <typename O, typename F>
  O
  merge_default_options (const default_options<O>&, const O&, F&&);

  template <typename O, typename AS, typename F>
  AS
  merge_default_arguments (const default_options<O>&, const AS&, F&&);

  // Find a common start (parent) directory for directories specified as an
  // iterator range, stopping at home or root (excluding). Optionally pass a
  // function resolving an iterator into a directory in a way other than just
  // dereferencing it. The function signature is:
  //
  // const dir_path& (I)
  //
  template <typename I, typename F>
  optional<dir_path>
  default_options_start (const optional<dir_path>& home, I, I, F&&);

  template <typename I>
  inline optional<dir_path>
  default_options_start (const optional<dir_path>& home, I b, I e)
  {
    return default_options_start (home,
                                  b, e,
                                  [] (I i) -> const dir_path& {return *i;});
  }
}

#include <libbutl/default-options.ixx>
#include <libbutl/default-options.txx>
