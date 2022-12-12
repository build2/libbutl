// file      : tests/link/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <set>
#include <utility>      // pair
#include <iostream>     // cerr
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/utility.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

static const char text[] = "ABCDEF";

enum class mklink
{
  sym,
  hard,
  any
};

static bool
link_file (const path& target, const path& link, mklink ml, bool check_content)
{
  try
  {
    switch (ml)
    {
    case mklink::sym:  mksymlink  (target, link);                  break;
    case mklink::hard: mkhardlink (target, link);                  break;
    case mklink::any:  mkanylink  (target, link, true /* copy */); break;
    }

    pair<bool, entry_stat> es (path_entry (link));
    assert (es.first);

    if (es.second.type == entry_type::symlink && readsymlink (link) != target)
      return false;
  }
  catch (const system_error&)
  {
    //cerr << e << endl;
    return false;
  }
  catch (const pair<entry_type, system_error>&)
  {
    //cerr << e.second << endl;
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

    pair<bool, entry_stat> es (path_entry (link));
    assert (es.first);

    if (es.second.type == entry_type::symlink)
      readsymlink (link); // // Check for errors.
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
  for (const dir_entry& de: dir_iterator (tp, dir_iterator::no_follow))
    te.emplace (de.ltype (), de.path ());

  set<pair<entry_type, path>> le;
  for (const dir_entry& de: dir_iterator (link, dir_iterator::no_follow))
    le.emplace (de.ltype (), de.path ());

  return te == le;
}

// Usages:
//
// argv[0]
// argv[0] -s <target> <link>
// argv[0] -f <path>
//
// In the first form run the basic symbolic and hard link tests.
//
// In the second form create a symlink. On error print the diagnostics to
// stderr and exit with the one code.
//
// In the third form follow symlinks and print the resulting target path to
// stdout. On error print the diagnostics to stderr and exit with the one
// code.
//
int
main (int argc, const char* argv[])
{
  bool create_symlink  (false);
  bool follow_symlinks (false);

  int i (1);
  for (; i != argc; ++i)
  {
    string v (argv[i]);

    if (v == "-s")
      create_symlink = true;
    else if (v == "-f")
      follow_symlinks = true;
    else
      break;
  }

  if (create_symlink)
  {
    assert (!follow_symlinks);
    assert (i + 2 == argc);

    try
    {
      path t (argv[i]);
      path l (argv[i + 1]);

      bool dir (dir_exists (t.relative () ? l.directory () / t : t));
      mksymlink (t, l, dir);

      path lt (readsymlink (l));

      // The targets may only differ for Windows directory junctions.
      //
      assert (lt == t || dir);

      return 0;
    }
    catch (const system_error& e)
    {
      cerr << "error: " << e << endl;
      return 1;
    }
  }
  else if (follow_symlinks)
  {
    assert (!create_symlink);
    assert (i + 1 == argc);

    try
    {
      cout << try_followsymlink (path (argv[i])).first << endl;
      return 0;
    }
    catch (const system_error& e)
    {
      cerr << "error: " << e << endl;
      return 1;
    }
  }
  else
    assert (i == argc);

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
  assert (link_file (fp, td / path ("hlink"), mklink::hard, true));

#ifndef _WIN32
  // Create the file symlink using an absolute path.
  //
  assert (link_file (fp, td / path ("slink"), mklink::sym, true));

  // Create the file symlink using a relative path.
  //
  assert (link_file (fn, td / path ("rslink"), mklink::sym, true));

  // Create the file symlink using an unexistent file path.
  //
  assert (link_file (fp + "-a", td / path ("sa"), mklink::sym, false));
#endif

  // Create the file any link.
  //
  assert (link_file (fp, td / path ("alink"), mklink::any, true));

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
  assert (link_file (fp, dp / path ("hlink"), mklink::hard, true));
  assert (link_file (fp, dp / path ("slink"), mklink::sym, true));
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

  for (const dir_entry& de: dir_iterator (td, dir_iterator::no_follow))
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

  {
    pair<bool, entry_stat> pe (path_entry (ld));
    assert (pe.first && pe.second.type == entry_type::symlink);
  }

  {
    pair<bool, entry_stat> pe (path_entry (ld, true /* follow_symlinks */));
    assert (!pe.first);
  }

  assert (try_rmsymlink (ld) == rmfile_status::success);

  // Try to create a dangling regular file symlink and make sure it is
  // properly removed via its parent recursive removal.
  //
  assert (try_mkdir (dp) == mkdir_status::success);

  // Note that on Windows regular file symlinks may not be supported (see
  // mksymlink() for details), so the following tests are allowed to fail
  // with ENOSYS on Windows.
  //
  try
  {
    mksymlink (dp / "non-existing", dp / "lnk");
    assert (!dir_empty (dp));

    assert (dir_iterator (dp, dir_iterator::ignore_dangling) ==
            dir_iterator ());
  }
  catch (const system_error& e)
  {
#ifndef _WIN32
    assert (false);
#else
    assert (e.code ().category () == generic_category () &&
            e.code ().value () == ENOSYS);
#endif
  }

  rmdir_r (dp);

  // Create a dangling directory symlink and make sure it is properly removed
  // via its parent recursive removal. Also make sure that removing directory
  // symlink keeps its target intact.
  //
  assert (try_mkdir (dp) == mkdir_status::success);

  dir_path tgd (td / dir_path ("tdir"));
  assert (try_mkdir (tgd) == mkdir_status::success);

  mksymlink  (dp / "non-existing", dp / "lnk1", true /* dir */);
  assert (!dir_empty (dp));
  assert (dir_iterator (dp, dir_iterator::ignore_dangling) == dir_iterator ());

  mksymlink  (tgd, dp / "lnk2", true /* dir */);
  assert (dir_iterator (dp, dir_iterator::ignore_dangling) != dir_iterator ());

  rmdir_r (dp);
  assert (dir_exists (tgd));

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
