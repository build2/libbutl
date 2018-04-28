// file      : tests/link/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules
#include <set>
#include <utility>      // pair
#include <iostream>     // cerr
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules
#ifdef __cpp_lib_modules
import std.core;
import std.io;
#endif
import butl.path;
import butl.path_io;
import butl.utility;
import butl.fdstream;
import butl.filesystem;
#else
#include <libbutl/path.mxx>
#include <libbutl/path-io.mxx>
#include <libbutl/utility.mxx>
#include <libbutl/fdstream.mxx>
#include <libbutl/filesystem.mxx>
#endif

using namespace std;
using namespace butl;

static const char text[] = "ABCDEF";

static bool
link_file (const path& target, const path& link, bool hard, bool check_content)
{
  try
  {
    if (hard)
      mkhardlink (target, link);
    else
      mksymlink (target, link);
  }
  catch (const system_error&)
  {
    return false;
  }

  if (!check_content)
    return true;

  string s;
  ifdstream ifs (link);
  ifs >> s;
  ifs.close (); // Not to miss failed close of the underlying file descriptor.
  return s == text;
}

static bool
link_dir (const dir_path& target,
          const dir_path& link,
          bool hard,
          bool check_content)
{
  try
  {
    if (hard)
      mkhardlink (target, link);
    else
      mksymlink (target, link);
  }
  catch (const system_error&)
  {
    //cerr << e << endl;
    return false;
  }

  {
    auto pe (path_entry (link, false /* follow_symlinks */));
    assert (pe.first && pe.second.type == entry_type::symlink);
  }

  {
    auto pe (path_entry (link, true /* follow_symlinks */));
    assert (!pe.first || pe.second.type == entry_type::directory);
  }

  if (!check_content)
    return true;

  dir_path tp (target.absolute () ? target : link.directory () / target);

  set<pair<entry_type, path>> te;
  for (const dir_entry& de: dir_iterator (tp))
    te.emplace (de.ltype (), de.path ());

  set<pair<entry_type, path>> le;
  for (const dir_entry& de: dir_iterator (link))
    le.emplace (de.ltype (), de.path ());

  return te == le;
}

int
main ()
{
  dir_path td (dir_path::temp_directory () / dir_path ("butl-link"));

  // Recreate the temporary directory (that possibly exists from the previous
  // faulty run) for the test files. Delete the directory only if the test
  // succeeds to simplify the failure research.
  //
  try
  {
    try_rmdir_r (td);
  }
  catch (const system_error& e)
  {
    cerr << "unable to remove " << td << ": " << e << endl;
    return 1;
  }

  assert (try_mkdir (td) == mkdir_status::success);

  // Prepare the target file.
  //
  path fn ("target");
  path fp (td / fn);

  {
    ofdstream ofs (fp);
    ofs << text;
    ofs.close ();
  }

  // Create the file hard link.
  //
  assert (link_file (fp, td / path ("hlink"), true, true));

#ifndef _WIN32
  // Create the file symlink using an absolute path.
  //
  assert (link_file (fp, td / path ("slink"), false, true));

  // Create the file symlink using a relative path.
  //
  assert (link_file (fn, td / path ("rslink"), false, true));

  // Create the file symlink using an unexistent file path.
  //
  assert (link_file (fp + "-a", td / path ("sa"), false, false));
#endif

  // Prepare the target directory.
  //
  dir_path dn ("dir");
  dir_path dp (td / dn);

  assert (try_mkdir (dp) == mkdir_status::success);

  {
    ofdstream ofs (dp / path ("f"));
    ofs << text;
    ofs.close ();
  }

#ifndef _WIN32
  assert (link_file (fp, dp / path ("hlink"), true, true));
  assert (link_file (fp, dp / path ("slink"), false, true));
#endif

  // Create the directory symlink using an absolute path.
  //
  dir_path ld (td / dir_path ("dslink"));
  assert (link_dir (dp, ld, false, true));

  rmsymlink (ld);

#ifndef _WIN32
  // Create the directory symlink using an unexistent directory path.
  //
  assert (link_dir (dp / dir_path ("a"), td / dir_path ("dsa"), false, false));

  // Create the directory symlink using a relative path.
  //
  assert (link_dir (dn, td / dir_path ("rdslink"), false, true));
#endif

  try
  {
    rmdir_r (td);
  }
  catch (const system_error& e)
  {
    cerr << "unable to remove " << td << ": " << e << endl;
    return 1;
  }
}
