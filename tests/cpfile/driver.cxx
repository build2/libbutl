// file      : tests/cpfile/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <ios>
#include <string>
#include <system_error>

#include <libbutl/path.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

static const char text1[] = "ABCDEF\nXYZ";
static const char text2[] = "12345\nDEF";
static const char text3[] = "XAB\r\n9";

static string
from_file (const path& f)
{
  ifdstream ifs (f, fdopen_mode::binary);
  string s (ifs.read_text ());
  ifs.close (); // Not to miss failed close of the underlying file descriptor.
  return s;
}

static void
to_file (const path& f, const char* s)
{
  ofdstream ofs (f, fdopen_mode::binary);
  ofs << s;
  ofs.close ();
}

int
main ()
{
  dir_path td (dir_path::temp_directory () / dir_path ("butl-cpfile"));

  // Recreate the temporary directory (that possibly exists from the previous
  // faulty run) for the test files. Delete the directory only if the test
  // succeeds to simplify the failure research.
  //
  try_rmdir_r (td);
  assert (try_mkdir (td) == mkdir_status::success);

  path from (td / path ("from"));
  path to (td / path ("to"));

  // Copy empty file.
  //
  to_file (from, "");
  cpfile (from, to);
  assert (from_file (to) == "");
  assert (try_rmfile (to) == rmfile_status::success);

  // Check that content and permissions of newly created destination file are
  // the same as that ones of the source file.
  //
  to_file (from, text1);

  permissions p (path_permissions (from));
  path_permissions (from, permissions::ru | permissions::xu);

  cpfile (from, to, cpflags::none);
  assert (from_file (to) == text1);
  assert (path_permissions (to) == path_permissions (from));

  // Check that permissions of an existent destination file stays intact if
  // their overwrite is not requested.
  //
  path_permissions (to, p);
  cpfile (from, to, cpflags::overwrite_content);
  assert (from_file (to) == text1);
  assert (path_permissions (to) == p);

  // Check that permissions of an existent destination file get overwritten if
  // requested.
  //
  cpfile (
    from, to, cpflags::overwrite_content | cpflags::overwrite_permissions);

  assert (from_file (to) == text1);
  assert (path_permissions (to) == path_permissions (from));

  path_permissions (to, p);
  path_permissions (from, p);

  try
  {
    cpfile (from, to, cpflags::none);
    assert (false);
  }
  catch (const system_error&)
  {
  }

  // Copy to the directory.
  //
  dir_path sd (td / dir_path ("sub"));
  assert (try_mkdir (sd) == mkdir_status::success);

  cpfile_into (from, sd, cpflags::none);

  assert (from_file (sd / path ("from")) == text1);

  // Check that a hard link to the destination file is preserved.
  //
  path hlink (td / path ("hlink"));
  mkhardlink (to, hlink);
  to_file (hlink, text1);

  to_file (from, text2);
  cpfile (from, to, cpflags::overwrite_content);

  assert (from_file (hlink) == text2);

  // Note that on Windows regular file symlinks may not be supported (see
  // mksymlink() for details), so the following tests are allowed to fail
  // with ENOSYS on Windows.
  //
  try
  {
    // Check that 'from' being a symbolic link is properly resolved.
    //
    path fslink (td / path ("fslink"));
    mksymlink (from, fslink);

    cpfile (fslink, to, cpflags::overwrite_content);

    // Make sure 'to' is not a symbolic link to 'from' and from_file() just
    // follows it.
    //
    assert (try_rmfile (from) == rmfile_status::success);
    assert (from_file (to) == text2);

    // Check that 'to' being a symbolic link is properly resolved.
    //
    path tslink (td / path ("tslink"));
    mksymlink (to, tslink);

    to_file (from, text3);
    cpfile (from, tslink, cpflags::overwrite_content);
    assert (from_file (to) == text3);

    // Check that permissions are properly overwritten when 'to' is a symbolic
    // link.
    //
    to_file (from, text1);
    path_permissions (from, permissions::ru | permissions::xu);

    cpfile (
      from, tslink, cpflags::overwrite_content | cpflags::overwrite_permissions);

    assert (from_file (to) == text1);
    assert (path_permissions (to) == path_permissions (from));

    path_permissions (to, p);
    path_permissions (from, p);

    // Check that no-overwrite file copy fails even if 'to' symlink points to
    // non-existent file.
    //
    assert (try_rmfile (to) == rmfile_status::success);

    try
    {
      cpfile (from, tslink, cpflags::none);
      assert (false);
    }
    catch (const system_error&)
    {
    }

    // Check that copy fail if 'from' symlink points to non-existent file. The
    // std::system_error is thrown as cpfile() fails to obtain permissions for
    // the 'from' symlink target.
    //
    try
    {
      cpfile (tslink, from, cpflags::none);
      assert (false);
    }
    catch (const system_error&)
    {
    }
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

  rmdir_r (td);
}
