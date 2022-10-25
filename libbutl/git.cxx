// file      : libbutl/git.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/git.hxx>

#include <libbutl/optional.hxx>
#include <libbutl/filesystem.hxx>       // entry_exists()
#include <libbutl/semantic-version.hxx>

using namespace std;

namespace butl
{
  bool
  git_repository (const dir_path& d)
  {
    // .git can be either a directory or a file in case of a submodule or a
    // separate working tree.
    //
    // NOTE: remember to update load_default_options_files() if changing
    // anything here!
    //
    return entry_exists (d / ".git",
                         true /* follow_symlinks */,
                         true /* ignore_errors   */);
  }

  optional<semantic_version>
  git_version (const string& s)
  {
    // There is some variety across platforms in the version
    // representation.
    //
    // Linux:  git version 2.14.3
    // MacOS:  git version 2.10.1 (Apple Git-78)
    // MinGit: git version 2.16.1.windows.1
    //
    if (s.compare (0, 12, "git version ") == 0)
      return parse_semantic_version (s, 12,
                                     semantic_version::allow_build,
                                     "" /* build_separators */);

    return nullopt;
  }
}
