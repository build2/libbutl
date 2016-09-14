// file      : tests/fdstream/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <ios>
#include <string>
#include <vector>
#include <iomanip>
#include <cassert>
#include <sstream>
#include <fstream>
#include <iostream>
#include <exception>

#include <butl/path>
#include <butl/fdstream>
#include <butl/timestamp>
#include <butl/filesystem>

using namespace std;
using namespace butl;

static const string text1 ("ABCDEF\nXYZ");
static const string text2 ("12");            // Keep shorter than text1.
static const string text3 ("ABCDEF\r\nXYZ");

static string
from_file (const path& f, fdopen_mode m = fdopen_mode::none)
{
  ifdstream ifs (f, m, ifdstream::badbit);

  string s;
  getline (ifs, s, '\0');
  ifs.close (); // Not to miss failed close of the underlying file descriptor.
  return s;
}

static void
to_file (const path& f, const string& s, fdopen_mode m = fdopen_mode::none)
{
  ofdstream ofs (f, m);
  ofs << s;
  ofs.close ();
}

template <typename S, typename T>
static duration
write_time (const path& p, const T& s, size_t n)
{
  timestamp t (timestamp::clock::now ());
  S os (p.string (), ofstream::out);
  os.exceptions (S::failbit | S::badbit);

  for (size_t i (0); i < n; ++i)
  {
    if (i > 0)
      os << '\n'; // Important not to use endl as it syncs a stream.

    os << s;
  }

  os.close ();
  return timestamp::clock::now () - t;
}

template <typename S, typename T>
static duration
read_time (const path& p, const T& s, size_t n)
{
  vector<T> v (n);

  timestamp t (timestamp::clock::now ());
  S is (p.string (), ofstream::in);
  is.exceptions (S::failbit | S::badbit);

  for (auto& ve: v)
    is >> ve;

  assert (is.eof ());

  is.close ();
  duration d (timestamp::clock::now () - t);

  for (const auto& ve: v)
    assert (ve == s);

  return d;
}

int
main (int argc, const char* argv[])
{
  if (!(argc == 1 || (argc == 2 && argv[1] == string ("-v"))))
  {
    cerr << "usage: " << argv[0] << " [-v]" << endl;
    return 1;
  }

  bool v (argc == 2);
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
  catch (const ios::failure&)
  {
  }

  // Read from the newly created empty file.
  //
  assert (from_file (f, fdopen_mode::create) == "");
  assert (try_rmfile (f) == rmfile_status::success);

  // Read from the newly created non-empty file.
  //
  to_file (f, text1, fdopen_mode::create);
  assert (from_file (f) == text1);

  // Check that skip on close as requested.
  //
  {
    ifdstream ifs (fdopen (f, fdopen_mode::in), fdstream_mode::skip);

    string s;
    getline (ifs, s);
    assert (!ifs.eof ());

    ifs.close ();
    assert (ifs.eof ());
  }

  // Check that don't skip on close by default.
  //
  {
    ifdstream ifs (fdopen (f, fdopen_mode::in));

    string s;
    getline (ifs, s);
    assert (!ifs.eof ());

    ifs.close ();
    assert (!ifs.eof ());
  }

  // Read from the file opened in R/W mode.
  //
  assert (from_file (f, fdopen_mode::out) == text1);

  // Read starting from the file's end.
  //
  assert (from_file (f, fdopen_mode::at_end) == "");

  try
  {
    // Fail to create if the file already exists.
    //
    fdopen (
      f, fdopen_mode::out | fdopen_mode::create | fdopen_mode::exclusive);

    assert (false);
  }
  catch (const ios::failure&)
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

  // Append to the file with the yet another way.
  //
  to_file (f, text1, fdopen_mode::truncate);
  to_file (f, text2, fdopen_mode::at_end);
  assert (from_file (f) == text1 + text2);

  // Check creating unopened ifdstream with a non-default exception mask.
  //
  to_file (f, "", fdopen_mode::truncate);

  {
    ifdstream ifs (-1, ifdstream::badbit);
    ifs.open (f);

    string s;
    assert (!getline (ifs, s));
  }

  {
    ifdstream ifs (-1, fdstream_mode::text, ifdstream::badbit);
    ifs.open (f);

    string s;
    assert (!getline (ifs, s));
  }

  // Check creating unopened ofdstream with a non-default exception mask.
  //
  {
    ofdstream ofs (-1, ifdstream::badbit);
    ofs.open (f);

    istringstream is;
    ofs << is.rdbuf (); // Sets failbit if no characters is inserted.
    ofs.close ();
  }

  {
    ofdstream ofs (-1, fdstream_mode::binary, ifdstream::badbit);
    ofs.open (f);

    istringstream is;
    ofs << is.rdbuf (); // Sets failbit if no characters is inserted.
    ofs.close ();
  }

  // Fail to write to a read-only file.
  //
  // Don't work well for MinGW GCC (5.2.0) that throws ios::failure, which in
  // combination with libstdc++'s ios::failure ABI fiasco (#66145) make it
  // impossible to properly catch in this situation.
  //
  try
  {
    {
      ofdstream ofs (fdopen (f, fdopen_mode::in));
      ofs << text1;
      ofs.flush ();
    }

    assert (false);
  }
#if !defined(_WIN32) || !defined(__GLIBCXX__)
  catch (const ios::failure&)
  {
  }
#else
  catch (const std::exception&)
  {
  }
#endif

  try
  {
    ofdstream ofs;
    ofs.open (fdopen (f, fdopen_mode::in));
    ofs << text1;
    ofs.close ();

    assert (false);
  }
#if !defined(_WIN32) || !defined(__GLIBCXX__)
  catch (const ios::failure&)
  {
  }
#else
  catch (const std::exception&)
  {
  }
#endif

  // Fail to read from a write-only file.
  //
  try
  {
    ifdstream ifs (fdopen (f, fdopen_mode::out));
    ifs.peek ();

    assert (false);
  }
  catch (const ios::failure&)
  {
  }

  try
  {
    ifdstream ifs;
    ifs.open (fdopen (f, fdopen_mode::out));
    ifs.peek ();

    assert (false);
  }
  catch (const ios::failure&)
  {
  }

  // Dtor of a not opened ofdstream doesn't terminate a program.
  //
  {
    ofdstream ofs;
  }

  // Dtor of an opened ofdstream doesn't terminate a program during the stack
  // unwinding.
  //
  try
  {
    ofdstream ofs (f);
    throw ios::failure ("test");
  }
  catch (const ios::failure&)
  {
  }

  // Dtor of an opened but being in a bad state ofdstream doesn't terminate a
  // program.
  //
  {
    ofdstream ofs (f, fdopen_mode::out, ofdstream::badbit);
    ofs.clear (ofdstream::failbit);
  }

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
  catch (const ios::failure&)
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

  // Compare fdstream and fstream operations performance.
  //
  duration fwd (0);
  duration dwd (0);
  duration frd (0);
  duration drd (0);

  path ff (td / path ("fstream"));
  path fd (td / path ("fdstream"));

  // Make several measurements with different ordering for each benchmark to
  // level fluctuations.
  //
  // Write/read ~10M-size files by 100, 1000, 10 000, 100 000 byte-length
  // strings.
  //
  size_t sz (100);
  for (size_t i (0); i < 4; ++i)
  {
    string s;
    s.reserve (sz);

    // Fill string with characters from '0' to 'z'.
    //
    for (size_t i (0); i < sz; ++i)
      s.push_back ('0' + i % (123 - 48));

    size_t n (10 * 1024 * 1024 / sz);

    for (size_t i (0); i < 4; ++i)
    {
      if (i % 2 == 0)
      {
        fwd += write_time<ofstream>  (ff, s, n);
        dwd += write_time<ofdstream> (fd, s, n);
        frd += read_time<ifstream>   (ff, s, n);
        drd += read_time<ifdstream>  (fd, s, n);
      }
      else
      {
        dwd += write_time<ofdstream> (fd, s, n);
        fwd += write_time<ofstream>  (ff, s, n);
        drd += read_time<ifdstream>  (fd, s, n);
        frd += read_time<ifstream>   (ff, s, n);
      }
    }

    sz *= 10;
  }

  // Write/read ~10M-size files by 64-bit integers.
  //
  uint64_t u (0x1234567890123456);
  size_t n (10 * 1024 * 1024 / sizeof (u));

  for (size_t i (0); i < 4; ++i)
  {
    if (i % 2 == 0)
    {
      fwd += write_time<ofstream>  (ff, u, n);
      dwd += write_time<ofdstream> (fd, u, n);
      frd += read_time<ifstream>   (ff, u, n);
      drd += read_time<ifdstream>  (fd, u, n);
    }
    else
    {
      dwd += write_time<ofdstream> (fd, u, n);
      fwd += write_time<ofstream>  (ff, u, n);
      drd += read_time<ifdstream>  (fd, u, n);
      frd += read_time<ifstream>   (ff, u, n);
    }
  }

  if (v)
    cerr << "fdstream/fstream write and read duration ratios are "
         << fixed << setprecision (2)
         << static_cast<double> (dwd.count ()) / fwd.count () << " and "
         << static_cast<double> (drd.count ()) / frd.count () << endl;

  rmdir_r (td);
}
