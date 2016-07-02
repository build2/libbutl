// file      : tests/fdstream/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <cassert>
#include <system_error>

#include <butl/path>
#include <butl/fdstream>
#include <butl/filesystem>

using namespace std;
using namespace butl;

static const string text1 ("ABCDEF\nXYZ");
static const string text2 ("12");            // Keep shorter than text1.
static const string text3 ("ABCDEF\r\nXYZ");

static string
from_file (const path& f, fdopen_mode m = fdopen_mode::none)
{
  ifdstream ifs (fdopen (f, m | fdopen_mode::in));
  ifs.exceptions (ifdstream::badbit);

  string s;
  getline (ifs, s, '\0');
  return s;
}

static void
to_file (const path& f, const string& s, fdopen_mode m = fdopen_mode::none)
{
  ofdstream ofs (fdopen (f, m | fdopen_mode::out));
  ofs.exceptions (ofdstream::badbit | ofdstream::failbit);
  ofs << s;
}

int
main ()
{
  dir_path td (dir_path::temp_directory () / dir_path ("butl-fdstream"));

  // Recreate the temporary directory (that possibly exists from the previous
  // faulty run) for the test files. Delete the directory only if the test
  // succeeds to simplify the failure research.
  //
  try_rmdir_r (td);
  assert (try_mkdir (td) == mkdir_status::success);

  path f (td / path ("file"));

  try
  {
    fdopen (f, fdopen_mode::out); // fdopen_mode::create is missed.
    assert (false);
  }
  catch (const system_error&)
  {
  }

  // Read from the newly created empty file.
  //
  assert (from_file (f, fdopen_mode::create) == "");
  assert (try_rmfile (f) == rmfile_status::success);

  to_file (f, text1, fdopen_mode::create);
  assert (from_file (f) == text1);

  // Read from the file opened in R/W mode.
  //
  assert (from_file (f, fdopen_mode::out) == text1);

  try
  {
    // Fail to create if the file already exists.
    //
    fdopen (
      f, fdopen_mode::out | fdopen_mode::create | fdopen_mode::exclusive);

    assert (false);
  }
  catch (const system_error&)
  {
  }

  // Write text2 over text1.
  //
  to_file (f, text2);
  string s (text2);
  s += string (text1, text2.size ());
  assert (from_file (f) == s);

  // Truncate before reading.
  //
  assert (from_file (f, fdopen_mode::out | fdopen_mode::truncate) == "");

  // Append to the file.
  //
  to_file (f, text1, fdopen_mode::truncate);
  to_file (f, text2, fdopen_mode::append);
  assert (from_file (f) == text1 + text2);

#ifndef _WIN32

  // Fail for an existing symlink to unexistent file.
  //
  path link (td / path ("link"));
  mksymlink (td / path ("unexistent"), link);

  try
  {
    fdopen (
      link, fdopen_mode::out | fdopen_mode::create | fdopen_mode::exclusive);

    assert (false);
  }
  catch (const system_error&)
  {
  }

#else

  // Check translation modes.
  //
  to_file (f, text1, fdopen_mode::truncate);
  assert (from_file (f, fdopen_mode::binary) == text3);

  to_file (f, text3, fdopen_mode::truncate | fdopen_mode::binary);
  assert (from_file (f) == text1);

#endif

  rmdir_r (td);
}
