// file      : tests/link/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <set>
#include <cassert>
#include <utility>      // pair
#include <system_error>

#include <butl/path>
#include <butl/fdstream>
#include <butl/filesystem>

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

#ifndef _WIN32
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
  catch (const system_error& e)
  {
    //cerr << e.what () << endl;
    return false;
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
#endif

int
main ()
{
  dir_path td (dir_path::temp_directory () / dir_path ("butl-link"));

  // Recreate the temporary directory (that possibly exists from the previous
  // faulty run) for the test files. Delete the directory only if the test
  // succeeds to simplify the failure research.
  //
  try_rmdir_r (td);
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

  // Prepare the target directory.
  //
  dir_path dn ("dir");
  dir_path dp (td / dn);
  assert (try_mkdir (dp) == mkdir_status::success);
  assert (link_file (fp, dp / path ("hlink"), true, true));
  assert (link_file (fp, dp / path ("slink"), false, true));

  // Create the directory symlink using an absolute path.
  //
  assert (link_dir (dp, td / dir_path ("dslink"), false, true));

  // Create the directory symlink using a relative path.
  //
  assert (link_dir (dn, td / dir_path ("rdslink"), false, true));

  // Create the directory symlink using an unexistent directory path.
  //
  assert (link_dir (dp / dir_path ("a"), td / dir_path ("dsa"), false, false));
#endif

  rmdir_r (td);
}
