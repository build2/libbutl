// file      : tests/link/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>

#ifndef __cpp_lib_modules_ts
#include <set>
#include <utility>      // pair
#include <iostream>     // cerr
#include <system_error>
#endif

// Other includes.

#ifdef __cpp_modules_ts
#ifdef __cpp_lib_modules_ts
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
  for (const dir_entry& de: dir_iterator (tp, false /* ignore_dangling */))
    te.emplace (de.ltype (), de.path ());

  set<pair<entry_type, path>> le;
  for (const dir_entry& de: dir_iterator (link, false /* ignore_dangling */))
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
  assert (link_dir (dp, ld, false /* hard */, true /* check_content */));

  // Create the symlink to a directory symlink using an absolute path.
  //
  dir_path lld (td / dir_path ("dslinkslink"));
  assert (link_dir (ld, lld, false /* hard */, true /* check_content */));

  {
    pair<bool, entry_stat> pe (path_entry (ld / "f"));
    assert (pe.first && pe.second.type == entry_type::regular);
  }

  {
    pair<bool, entry_stat> pe (path_entry (lld / "f"));
    assert (pe.first && pe.second.type == entry_type::regular);
  }

  {
    pair<bool, entry_stat> pe (path_entry (ld));
    assert (pe.first && pe.second.type == entry_type::symlink);
  }

  {
    pair<bool, entry_stat> pe (path_entry (ld, true /* follow_symlinks */));
    assert (pe.first && pe.second.type == entry_type::directory);
  }

  {
    pair<bool, entry_stat> pe (path_entry (lld));
    assert (pe.first && pe.second.type == entry_type::symlink);
  }

  {
    pair<bool, entry_stat> pe (path_entry (lld, true /* follow_symlinks */));
    assert (pe.first && pe.second.type == entry_type::directory);
  }

  for (const dir_entry& de: dir_iterator (td, false /* ignore_dangling */))
  {
    assert (de.path () != path ("dslink") ||
            (de.type () == entry_type::directory &&
             de.ltype () == entry_type::symlink));

    assert (de.path () != path ("dslinkslink") ||
            (de.type () == entry_type::directory &&
             de.ltype () == entry_type::symlink));
  }

  // Remove the directory symlink and make sure the target's content still
  // exists.
  //
  assert (try_rmsymlink (lld) == rmfile_status::success);
  assert (try_rmsymlink (ld) == rmfile_status::success);

  {
    pair<bool, entry_stat> pe (path_entry (dp / "f"));
    assert (pe.first && pe.second.type == entry_type::regular);
  }

#ifndef _WIN32
  // Create the directory symlink using an unexistent directory path.
  //
  assert (link_dir (dp / dir_path ("a"), td / dir_path ("dsa"), false, false));

  // Create the directory symlink using a relative path.
  //
  assert (link_dir (dn, td / dir_path ("rdslink"), false, true));
#endif

  // Delete the junction target and verify the junction entry status.
  //
  assert (link_dir (dp, ld, false /* hard */, true /* check_content */));
  rmdir_r (dp);

  // On Wine dangling junctions are not visible. That's why we also re-create
  // the target before the junction removal.
  //
#if 0
  {
    pair<bool, entry_stat> pe (path_entry (ld));
    assert (pe.first && pe.second.type == entry_type::symlink);
  }
#endif

  {
    pair<bool, entry_stat> pe (path_entry (ld, true /* follow_symlinks */));
    assert (!pe.first);
  }

  assert (try_mkdir (dp) == mkdir_status::success);
  assert (try_rmsymlink (ld) == rmfile_status::success);

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
