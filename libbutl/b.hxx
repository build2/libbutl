// file      : libbutl/b.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>
#include <utility>    // move()
#include <cstddef>    // size_tu
#include <cstdint>    // uint16_t
#include <stdexcept>  // runtime_error
#include <functional>

#include <libbutl/url.hxx>
#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/project-name.hxx>
#include <libbutl/standard-version.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  class LIBBUTL_SYMEXPORT b_error: public std::runtime_error
  {
  public:
    // Build system program exit information. May be absent if the error
    // occured before the process has been started.
    //
    // Can be used by the caller to decide if to print the error message to
    // stderr. Normally, it is not required if the process exited normally
    // with non-zero code, since presumably it has issued diagnostics. Note
    // that the normal() function can be used to check for this.
    //
    optional<process_exit> exit;

    // Return true if the build2 process exited normally with non-zero code.
    //
    bool
    normal () const {return exit && exit->normal () && !*exit;}

    explicit
    b_error (const std::string& description, optional<process_exit> = nullopt);
  };

  // Run `b info: <project-dir>...` command and parse and return (via argument
  // to allow appending and for error position; see below) the build2 projects
  // information it prints to stdout. Return the empty list if the specified
  // project list is empty. Throw b_error on error. Note that the size of the
  // result vector can be used to determine which project information caused
  // the error.
  //
  // You can also specify the build2 verbosity level, command line callback
  // (see process_run_callback() for details), build program search details,
  // and additional options.
  //
  // Note that version_string is only parsed to standard_version if a project
  // uses the version module. Otherwise, standard_version is empty.
  //
  struct b_project_info
  {
    using url_type = butl::url;

    struct subproject
    {
      project_name name;  // Empty if anonymous.
      dir_path     path;  // Relative to the project root.
    };

    project_name     project;
    std::string      version_string;
    standard_version version;
    std::string      summary;
    url_type         url;

    dir_path src_root;
    dir_path out_root;

    dir_path                amalgamation; // Relative to project root and
                                          // empty if not amalgmated.
    std::vector<subproject> subprojects;

    std::vector<std::string> operations;
    std::vector<std::string> meta_operations;

    std::vector<std::string> modules;
  };

  enum class b_info_flags: std::uint16_t
  {
    // Retrieve information that may come from external modules (operations,
    // meta-operations, etc). Omitting this flag results in passing
    // --no-external-modules to the build2 program and speeds up its
    // execution.
    //
    ext_mods = 0x1,

    // Discover subprojects. Omitting this flag results in passing
    // no_subprojects info meta-operation parameter to the build2 program and
    // speeds up its execution.
    //
    subprojects = 0x2,

    none = 0
  };

  inline b_info_flags operator& (b_info_flags, b_info_flags);
  inline b_info_flags operator| (b_info_flags, b_info_flags);
  inline b_info_flags operator&= (b_info_flags&, b_info_flags);
  inline b_info_flags operator|= (b_info_flags&, b_info_flags);

  using b_callback = void (const char* const args[], std::size_t n);

  LIBBUTL_SYMEXPORT void
  b_info (std::vector<b_project_info>& result,
          const std::vector<dir_path>& projects,
          b_info_flags,
          std::uint16_t verb = 1,
          const std::function<b_callback>& cmd_callback = {},
          const path& program = path ("b"),
          const dir_path& search_fallback = {},
          const std::vector<std::string>& options = {});

  // As above but retrieve information for a single project.
  //
  inline b_project_info
  b_info (const dir_path& project,
          b_info_flags fl,
          std::uint16_t verb = 1,
          const std::function<b_callback>& cmd_callback = {},
          const path& program = path ("b"),
          const dir_path& search_fallback = {},
          const std::vector<std::string>& options = {})
  {
    std::vector<b_project_info> r;
    b_info (r,
            std::vector<dir_path> ({project}),
            fl,
            verb,
            cmd_callback,
            program,
            search_fallback,
            options);

    return std::move (r[0]);
  }
}

#include <libbutl/b.ixx>
