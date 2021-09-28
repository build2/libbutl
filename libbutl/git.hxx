// file      : libbutl/git.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/semantic-version.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Return true if the specified directory is a git repository root (contains
  // the .git filesystem entry).
  //
  LIBBUTL_SYMEXPORT bool
  git_repository (const dir_path&);

  // Try to parse the line printed by the 'git --version' command. Return git
  // version if succeed, nullopt otherwise.
  //
  LIBBUTL_SYMEXPORT optional<semantic_version>
  git_version (const std::string&);
}
