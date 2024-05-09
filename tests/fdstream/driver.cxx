// file      : tests/fdstream/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#ifdef _WIN32
#  include <libbutl/win32-utility.hxx>
#endif

#ifndef _WIN32
#  include <chrono>
#endif

#include <ios>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <utility>   // move()
#include <iostream>
#include <exception>

#ifndef LIBBUTL_MINGW_STDTHREAD
#  include <thread>
#else
#  include <libbutl/mingw-thread.hxx>
#endif

#include <libbutl/path.hxx>
#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

static const string text1 ("ABCDEF\nXYZ");
static const string text2 ("12");            // Keep shorter than text1.

// Windows text mode write-translated form of text1.
//
#ifdef _WIN32
static const string text3 ("ABCDEF\r\nXYZ");
#endif

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
  S os (p.string ());
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
  S is (p.string ());
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
#ifndef LIBBUTL_MINGW_STDTHREAD
  using std::thread;
#else
  using mingw_stdthread::thread;
#endif

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

    size_t n (1000);
    for (size_t i (0); i < s.size (); i += n)
    {
      // MINGW GCC 4.9 doesn't implement this_thread so use Win32 Sleep().
      //
#ifndef _WIN32
      this_thread::sleep_for (chrono::milliseconds (50));
#else
      Sleep (50);
#endif

      cout.write (s.c_str () + i, min (n, s.size () - i));
      cout.flush ();
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
  assert (from_file (f, fdopen_mode::in | fdopen_mode::create) == "");
  assert (try_rmfile (f) == rmfile_status::success);

  // Read from the newly created non-empty file.
  //
  to_file (f, text1, fdopen_mode::out | fdopen_mode::create);
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
  assert (from_file (f, fdopen_mode::in | fdopen_mode::out) == text1);

  // Read starting from the file's end.
  //
  assert (from_file (f, fdopen_mode::in | fdopen_mode::at_end) == "");

  try
  {
    // Fail to create if the file already exists.
    //
    fdopen (f, (fdopen_mode::out    |
                fdopen_mode::create |
                fdopen_mode::exclusive));

    assert (false);
  }
  catch (const ios::failure&)
  {
  }

  // Write text2 over text1.
  //
  to_file (f, text2, fdopen_mode::out);
  string s (text2);
  s += string (text1, text2.size ());
  assert (from_file (f) == s);

  // Truncate before reading.
  //
  assert (from_file (f, fdopen_mode::out | fdopen_mode::truncate) == "");

  // Append to the file.
  //
  to_file (f, text1, fdopen_mode::out | fdopen_mode::truncate);
  to_file (f, text2, fdopen_mode::out | fdopen_mode::append);
  assert (from_file (f) == text1 + text2);

  // Append to the file with the yet another way.
  //
  to_file (f, text1, fdopen_mode::out | fdopen_mode::truncate);
  to_file (f, text2, fdopen_mode::out | fdopen_mode::at_end);
  assert (from_file (f) == text1 + text2);

  // Check creating unopened ifdstream with a non-default exception mask.
  //
  to_file (f, "", fdopen_mode::out | fdopen_mode::truncate);

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
    ofdstream ofs (f, ofdstream::badbit);
    ofs.clear (ofdstream::failbit);
  }

  // Note that on Windows regular file symlinks may not be supported (see
  // mksymlink() for details), so the following tests are allowed to fail
  // with ENOSYS on Windows.
  //
  try
  {
    // Fail for an existing symlink to unexistent file.
    //
    path link (td / path ("link"));
    mksymlink (td / path ("unexistent"), link);

    try
    {
      fdopen (link, (fdopen_mode::out    |
                     fdopen_mode::create |
                     fdopen_mode::exclusive));

      assert (false);
    }
    catch (const ios::failure&)
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

#ifdef _WIN32

  // Check translation modes.
  //
  to_file (f, text1, fdopen_mode::out | fdopen_mode::truncate);
  assert (from_file (f, fdopen_mode::binary) == text3);

  to_file (f, text3, (fdopen_mode::out      |
                      fdopen_mode::truncate |
                      fdopen_mode::binary));
  assert (from_file (f) == text1);

#endif

  // Test non-blocking reading.
  //
  {
    string s;
    for (size_t i (0); i < 300; ++i)
      s += "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n";

    const char* args[] = {argv[0], "-c", nullptr};

    auto test_read = [&args, &s] (bool timeout)
    {
      try
      {
        process   pr (args, -1, -1);
        ofdstream os (move (pr.out_fd));
        ifdstream is (move (pr.in_ofd), fdstream_mode::non_blocking);

        os << s;
        os.close ();

        fdselect_set rds ({fdselect_state (is.fd ())});
        fdselect_set wds;

        string r;
        char buf[300];
        bool timedout (false);
        while (!is.eof ())
        {
          if (timeout)
          {
            pair<size_t, size_t> nd (
              fdselect (rds, wds, chrono::milliseconds (3)));

            assert (((nd.first == 0 && !rds[0].ready) ||
                     (nd.first == 1 && rds[0].ready)) &&
                    nd.second == 0);

            if (nd.first == 0)
            {
              timedout = true;
              continue;
            }
          }
          else
          {
            pair<size_t, size_t> nd (fdselect (rds, wds));
            assert (nd.first == 1 && nd.second == 0 && rds[0].ready);
          }

          for (streamsize n; (n = is.readsome (buf, sizeof (buf))) != 0; )
            r.append (buf, static_cast<size_t> (n));
        }

        is.close ();

        assert (r == s);

        // If timeout is used, then it most likely timedout, at least once.
        //
        assert (timedout == timeout);
      }
      catch (const ios::failure&)
      {
        assert (false);
      }
      catch (const process_error&)
      {
        assert (false);
      }
    };

    vector<thread> threads;
    for (size_t i (0); i < 10; ++i)
    {
      threads.emplace_back ([&test_read] {test_read (true  /* timeout */);});
      threads.emplace_back ([&test_read] {test_read (false /* timeout */);});
    }

    // While the threads are busy, let's test the skip/non_blocking modes
    // combination.
    //
    try
    {
      process   pr (args, -1, -1);
      ofdstream os (move (pr.out_fd));

      ifdstream is (move (pr.in_ofd),
                    fdstream_mode::non_blocking | fdstream_mode::skip);

      os << s;
      os.close ();

      is.close (); // Set the blocking mode, skip and close.
    }
    catch (const ios::failure&)
    {
      assert (false);
    }
    catch (const process_error&)
    {
      assert (false);
    }

    // Join the non-blocking reading test threads.
    //
    for (thread& t: threads)
      t.join ();
  }

  // Test (non-blocking) reading with getline_non_blocking().
  //
  {
    const string ln (
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

    string s;
    for (size_t i (0); i < 300; ++i)
    {
      s += ln;
      s += '\n';
    }

    const char* args[] = {argv[0], "-c", nullptr};

    auto test_read = [&args, &s, &ln] ()
    {
      try
      {
        process   pr (args, -1, -1);
        ofdstream os (move (pr.out_fd));

        ifdstream is (move (pr.in_ofd),
                      fdstream_mode::non_blocking,
                      ios_base::badbit);

        os << s;
        os.close ();

        fdselect_set fds {is.fd ()};
        fdselect_state& ist (fds[0]);

        string r;
        for (string l; ist.fd != nullfd; )
        {
          if (ist.fd != nullfd && getline_non_blocking (is, l))
          {
            if (eof (is))
              ist.fd = nullfd;
            else
            {
              assert (l == ln);

              r += l;
              r += '\n';

              l.clear ();
            }

            continue;
          }

          ifdselect (fds);
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
    };

    vector<thread> threads;
    for (size_t i (0); i < 20; ++i)
      threads.emplace_back (test_read);

    for (thread& t: threads)
      t.join ();
  }

  // Test setting and getting position via the non-standard fdstreambuf
  // interface.
  //
  // Seek for read.
  //
  {
    to_file (f, "012\n3\n4567", fdopen_mode::out | fdopen_mode::truncate);

    ifdstream is (f);

    fdstreambuf* buf (dynamic_cast<fdstreambuf*> (is.rdbuf ()));
    assert (buf != nullptr);

    char c;
    for (size_t i (0); i < 7; ++i)
      is.get (c);

    uint64_t p (buf->tellg ());
    assert (p == 7);

    is.get (c);
    assert (c == '5');

    buf->seekg (p);
    assert (buf->tellg () == p);

    is.get (c);
    assert (c == '5');

    // Can't seek beyond the end of the stream.
    //
    try
    {
      buf->seekg (20);
      assert (false);
    }
    catch (const ios::failure&) {}
  }

  // Seek for write.
  //
  {
    // Let's test replacing the '3' fragment with 'XYZ' in the following file.
    //
    to_file (f, "012\n3\n4567", fdopen_mode::out | fdopen_mode::truncate);

    auto_fd fd;
    string suffix;
    size_t p (4); // Logical position of the fragment being replaced.

    {
      ifdstream is (f, fdopen_mode::in | fdopen_mode::out);

      fdstreambuf* buf (dynamic_cast<fdstreambuf*> (is.rdbuf ()));
      assert (buf != nullptr);

      // Read till the end of the fragment.
      //
      char c;
      for (size_t i (0); i < p + 1; ++i)
        is.get (c);

      assert (c == '3');

      // Read the suffix.
      //
      suffix = is.read_text ();
      assert (suffix == "\n4567");

      // Seek to the beginning of the fragment and detach the file descriptor.
      //
      buf->seekg (p);
      fd = is.release ();
    }

    // Rewrite the fragment.
    //
    // Note that on Windows in the text mode the logical position differs from
    // the file descriptor position, so we need to query the later one to
    // truncate the file.
    //
    fdtruncate (fd.get (), fdseek (fd.get (), 0, fdseek_mode::cur));

    ofdstream os (move (fd), ofdstream::badbit | ofdstream::failbit, p);

    os << "XYZ" << suffix;
    os.close ();

    assert (from_file (f) == "012\nXYZ\n4567");
  }

  // Test setting and getting position via the standard [io]stream interface.
  //
  to_file (f, "0123456789", fdopen_mode::out | fdopen_mode::truncate);

  // Seek for read.
  //
  {
    ifdstream is (f);

    char c;
    is.get (c);

    is.seekg (5, ios::beg);
    is.get (c);
    assert (c == '5');

    is.seekg (2, ios::cur);

    assert (static_cast<streamoff> (is.tellg ()) == 8);

    const fdstreambuf* buf (dynamic_cast<const fdstreambuf*> (is.rdbuf ()));
    assert (buf != nullptr && buf->tellg () == 8);

    assert (from_stream (is) == "89");
  }

  // Seek for write.
  //
  {
    ofdstream os (f, fdopen_mode::out);
    os.seekp (4, ios::beg);
    os << "ABC";
    os.seekp (-4, ios::end);
    os << "XYZ";
    os.seekp (-8, ios::cur);
    os << 'C';

    assert (static_cast<streamoff> (os.tellp ()) == 2);

    const fdstreambuf* buf (dynamic_cast<const fdstreambuf*> (os.rdbuf ()));
    assert (buf != nullptr && buf->tellp () == 2);

    os.close ();
    assert (from_file (f) == "0C23ABXYZ9");
  }

#ifdef _WIN32

  // Test handling newline characters on Windows while setting and getting
  // position via the standard [io]stream interface.
  //
  // Save the string in the text mode, so the newline character is translated
  // into the 0xD, 0xA character sequence on Windows.
  //
  to_file (f, "01234\n56789", fdopen_mode::out | fdopen_mode::truncate);

  // Seek for read in the text mode.
  //
  {
    ifdstream is (f);

    char c;
    is.get (c);

    is.seekg (2, ios::cur);
    is.get (c);

    assert (c == '3');

    is.seekg (4, ios::cur);

    assert (static_cast<streamoff> (is.tellg ()) == 8);
    assert (from_stream (is) == "6789");
  }

  // Seek for read in the binary mode.
  //
  {
    ifdstream is (f, fdopen_mode::binary);

    char c;
    is.get (c);

    is.seekg (2, ios::cur);
    is.get (c);

    assert (c == '3');

    is.seekg (4, ios::cur);

    assert (static_cast<streamoff> (is.tellg ()) == 8);

    const fdstreambuf* buf (dynamic_cast<const fdstreambuf*> (is.rdbuf ()));
    assert (buf != nullptr && buf->tellp () == 8);

    assert (from_stream (is) == "6789");
  }

  // Research the positioning misbehavior of std::ifstream object opened
  // in the text mode on Windows.
  //
#if 0

  to_file (f, "012\r\n3\n4567", (fdopen_mode::out      |
                                 fdopen_mode::truncate |
                                 fdopen_mode::binary));

  {
    ifstream is (f.string ());
//    ifdstream is (f);

    char c1;
    for (size_t i (0); i < 2; ++i)
      is.get (c1);

    is.seekg (6, ios::cur);

    streamoff p1 (is.tellg ());

    is.get (c1);

    cout << "c1: '" << c1 << "' pos " << p1 << endl;

    char c2;
    is.seekg (8, ios::beg);

    streamoff p2 (is.tellg ());
    is.get (c2);

    cout << "c2: '" << c2 << "' pos " << p2 << endl;

    // One could expect the positions and characters to match, but:
    //
    // VC's ifstream and ifdstream end up with:
    //
    // c1: '4' pos 7
    // c2: '5' pos 8
    //
    // MinGW's ifstream ends up with:
    //
    // c1: '6' pos 9
    // c2: '5' pos 8
    //
    // These assertions fail for all implementations:
    //
    // assert (p1 == p2);
    // assert (c1 == c2);
  }

  {
    ifstream is (f.string ());
//    ifdstream is (f);

    char c1;
    for (size_t i (0); i < 2; ++i)
      is.get (c1);

    auto p1 (is.tellg ());
    is.get (c1);

    cout << "c1: '" << c1 << "' pos " << p1 << endl;

    is.seekg (p1, ios::beg);

    auto p2 (is.tellg ());

    char c2;
    is.get (c2);

    cout << "c2: '" << c2 << "' pos " << p2 << endl;

    // One could expect the positions and characters to match, but:
    //
    // VC's ifstream and ifdstream end up with:
    //
    // c1: '2' pos 1
    // c2: '1' pos 1
    //
    // MinGW's ifstream ends up with:
    //
    // c1: '2' pos 3
    // c2: '\n' pos 3
    //
    // This assertion fails for all implementations:
    //
    // assert (c1 == c2);
  }

#endif

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

  // Test fdterm().
  //
  assert (!fdterm (fdopen_null ().get ())); // /dev/null is not a terminal.

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
