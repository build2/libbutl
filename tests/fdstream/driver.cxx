// file      : tests/fdstream/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef _WIN32
#  include <chrono>
#  include <thread> // this_thread::sleep_for()
#endif

#include <ios>
#include <string>
#include <vector>
#include <iomanip>
#include <cassert>
#include <sstream>
#include <fstream>
#include <utility>   // move()
#include <iostream>
#include <exception>

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

using namespace std;
using namespace butl;

static const string text1 ("ABCDEF\nXYZ");
static const string text2 ("12");            // Keep shorter than text1.

// Windows text mode write-translated form of text1.
//
static const string text3 ("ABCDEF\r\nXYZ");

static string
from_stream (ifdstream& is)
{
  string s (is.read_text ());
  is.close (); // Not to miss failed close of the underlying file descriptor.
  return s;
}

static string
from_file (const path& f, fdopen_mode m = fdopen_mode::none)
{
  ifdstream ifs (f, m, ifdstream::badbit);
  return from_stream (ifs);
}

static void
to_stream (ofdstream& os, const string& s)
{
  os << s;
  os.close ();
}

static void
to_file (const path& f, const string& s, fdopen_mode m = fdopen_mode::none)
{
  ofdstream ofs (f, m);
  to_stream (ofs, s);
}

template <typename S, typename T>
static duration
write_time (const path& p, const T& s, size_t n)
{
  timestamp t (system_clock::now ());
  S os (p.string (), ofstream::out);
  os.exceptions (S::failbit | S::badbit);

  for (size_t i (0); i < n; ++i)
  {
    if (i > 0)
      os << '\n'; // Important not to use endl as it syncs a stream.

    os << s;
  }

  os.close ();
  return system_clock::now () - t;
}

template <typename S, typename T>
static duration
read_time (const path& p, const T& s, size_t n)
{
  vector<T> v (n);

  timestamp t (system_clock::now ());
  S is (p.string (), ofstream::in);
  is.exceptions (S::failbit | S::badbit);

  for (auto& ve: v)
    is >> ve;

  assert (is.eof ());

  is.close ();
  duration d (system_clock::now () - t);

  for (const auto& ve: v)
    assert (ve == s);

  return d;
}

int
main (int argc, const char* argv[])
{
  bool v (false);
  bool child (false);

  int i (1);
  for (; i != argc; ++i)
  {
    string a (argv[i]);
    if (a == "-c")
      child = true;
    else if (a == "-v")
      v = true;
    else
    {
      cerr << "usage: " << argv[0] << " [-v] [-c]" << endl;
      return 1;
    }
  }

  // To test non-blocking reading from ifdstream the test program launches
  // itself as a child process with -c option and roundtrips a string through
  // it. The child must write the string in chunks with some delays to make
  // sure the parent reads in chunks as well.
  //
  if (child)
  {
    cin.exceptions (ios_base::badbit);
    cout.exceptions (ios_base::failbit | ios_base::badbit | ios_base::eofbit);

    string s;
    getline (cin, s, '\0');

    size_t n (10);
    for (size_t i (0); i < s.size (); i += n)
    {
      cout.write (s.c_str () + i, min (n, s.size () - i));
      cout.flush ();

      // @@ MINGW GCC 4.9 doesn't implement this_thread. If ifdstream
      //    non-blocking read will ever be implemented on Windows use Win32
      //    Sleep() instead.
      //
#ifndef _WIN32
      this_thread::sleep_for (chrono::milliseconds (50));
#endif
    }

    return 0;
  }

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
    ifdstream ifs (ifdstream::badbit);
    ifs.open (f);

    string s;
    assert (!getline (ifs, s));
  }

  {
    ifdstream ifs (nullfd, fdstream_mode::text, ifdstream::badbit);
    ifs.open (f);

    string s;
    assert (!getline (ifs, s));
  }

  // Check creating unopened ofdstream with a non-default exception mask.
  //
  {
    ofdstream ofs (ifdstream::badbit);
    ofs.open (f);

    istringstream is;
    ofs << is.rdbuf (); // Sets failbit if no characters is inserted.
    ofs.close ();
  }

  {
    ofdstream ofs (nullfd, fdstream_mode::binary, ifdstream::badbit);
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

  // Test non-blocking reading.
  //
  try
  {
    const char* args[] = {argv[0], "-c", nullptr};
    process pr (args, -1, -1);

    ofdstream os (move (pr.out_fd));
    ifdstream is (move (pr.in_ofd), fdstream_mode::non_blocking);

    const string s (
      "0123456789\nABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz");

    os << s;
    os.close ();

    string r;
    char buf[3];
    while (!is.eof ())
    {
      streamsize n (is.readsome (buf, sizeof (buf)));
      r.append (buf, n);
    }

    is.close ();

    assert (r == s);
  }
  catch (const ios::failure&)
  {
    assert (false);
  }
  catch (const process_error&)
  {
    assert (false);
  }

#else

  // Check translation modes.
  //
  to_file (f, text1, fdopen_mode::truncate);
  assert (from_file (f, fdopen_mode::binary) == text3);

  to_file (f, text3, fdopen_mode::truncate | fdopen_mode::binary);
  assert (from_file (f) == text1);

#endif

  // Test pipes.
  //
  // Here we rely on buffering being always enabled for pipes.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    ofdstream os (move (pipe.out));
    ifdstream is (move (pipe.in));
    to_stream (os, text1);
    assert (from_stream (is) == text1);
  }

#ifdef _WIN32

  // Test opening a pipe in the text mode.
  //
  {
    fdpipe pipe (fdopen_pipe ());
    ofdstream os (move (pipe.out));
    ifdstream is (move (pipe.in), fdstream_mode::binary);
    to_stream (os, text1);
    assert (from_stream (is) == text3);
  }

  // Test opening a pipe in the binary mode.
  //
  {
    fdpipe pipe (fdopen_pipe (fdopen_mode::binary));
    ofdstream os (move (pipe.out), fdstream_mode::text);
    ifdstream is (move (pipe.in));
    to_stream (os, text1);
    assert (from_stream (is) == text3);
  }

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
