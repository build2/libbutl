// file      : tests/target-triplet/driver.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <string>
#include <iostream>
#include <stdexcept> // invalid_argument

#include <libbutl/target-triplet.hxx>

#undef NDEBUG
#include <cassert>

using namespace std;
using namespace butl;

static bool
fail (const char*);

static bool
test (const char*,
      const char* canon,
      const char* cpu,
      const char* vendor,
      const char* system,
      const char* version,
      const char* class_ = "other");

int
main ()
{
  assert (fail (""));
  assert (fail ("mingw32"));
  assert (fail ("-"));
  assert (fail ("arm-"));
  assert (fail ("-mingw32"));
  assert (fail ("a-b-c-d-e"));
  assert (fail ("arm-pc--"));
  assert (fail ("arm-pc-linux-"));
  assert (fail ("arm-pc--gnu"));

  assert (test ("i686-elf",
                "i686-elf",
                "i686", "", "elf", ""));

  assert (test ("arm-eabi",
                "arm-eabi",
                "arm", "", "eabi", ""));

  assert (test ("arm-none-eabi",
                "arm-eabi",
                "arm", "", "eabi", ""));

  assert (test ("arm-unknown-none-eabi",
                "arm-none-eabi",
                "arm", "", "none-eabi", ""));

  assert (test ("arm-none",
                "arm-none",
                "arm", "", "none", ""));

  assert (test ("arm-none-linux-gnueabi",
                "arm-linux-gnueabi",
                "arm", "", "linux-gnueabi", "", "linux"));

  assert (test ("arm-softfloat-linux-gnu",
                "arm-softfloat-linux-gnu",
                "arm", "softfloat", "linux-gnu", "", "linux"));

  assert (test ("i686-pc-mingw32",
                "i686-mingw32",
                "i686", "", "mingw32", "", "windows"));

  assert (test ("i686-w64-mingw32",
                "i686-w64-mingw32",
                "i686", "w64", "mingw32", "", "windows"));

  assert (test ("x86_64-w64-windows-gnu",
                "x86_64-w64-mingw32",
                "x86_64", "w64", "mingw32", "", "windows"));

  assert (test ("i686-lfs-linux-gnu",
                "i686-lfs-linux-gnu",
                "i686", "lfs", "linux-gnu", "", "linux"));

  assert (test ("x86_64-unknown-linux-gnu",
                "x86_64-linux-gnu",
                "x86_64", "", "linux-gnu", "", "linux"));

  assert (test ("x86_64-redhat-linux",
                "x86_64-redhat-linux-gnu",
                "x86_64", "redhat", "linux-gnu", "", "linux"));

  assert (test ("x86_64-linux-gnux32",
                "x86_64-linux-gnux32",
                "x86_64", "", "linux-gnux32", "", "linux"));

  assert (test ("x86_64--netbsd",
                "x86_64-netbsd",
                "x86_64", "", "netbsd", "", "bsd"));

  assert (test ("aarch64-unknown-nto-qnx7.0.0",
                "aarch64-nto-qnx7.0.0",
                "aarch64", "", "nto-qnx", "7.0.0", "other"));

  assert (test ("aarch64-nto-qnx7.0.0",
                "aarch64-nto-qnx7.0.0",
                "aarch64", "", "nto-qnx", "7.0.0", "other"));

  assert (test ("wasm32-emscripten",
                "wasm32-emscripten",
                "wasm32", "", "emscripten", "", "other"));

  assert (test ("arm64-apple-darwin20.1.0",
                "aarch64-apple-darwin20.1.0",
                "aarch64", "apple", "darwin", "20.1.0", "macos"));

  assert (test ("arm64-apple-ios14.4",
                "aarch64-apple-ios14.4",
                "aarch64", "apple", "ios", "14.4", "ios"));

  assert (test ("arm64-apple-ios",
                "aarch64-apple-ios",
                "aarch64", "apple", "ios", "", "ios"));

  assert (test ("arm64-apple-ios14.4-simulator",
                "aarch64-apple-ios14.4-simulator",
                "aarch64", "apple", "ios-simulator", "14.4", "ios"));

  assert (test ("arm64-apple-ios-simulator",
                "aarch64-apple-ios-simulator",
                "aarch64", "apple", "ios-simulator", "", "ios"));

  assert (test ("x86_64-apple-ios14.4-macabi",
                "x86_64-apple-ios14.4-macabi",
                "x86_64", "apple", "ios-macabi", "14.4", "ios"));

  // Version extraction.
  //
  assert (test ("x86_64-apple-darwin14.5.0",
                "x86_64-apple-darwin14.5.0",
                "x86_64", "apple", "darwin", "14.5.0", "macos"));

  assert (test ("x86_64-unknown-freebsd10.2",
                "x86_64-freebsd10.2",
                "x86_64", "", "freebsd", "10.2", "bsd"));

  assert (test ("x86_64-unknown-netbsd9.0",
                "x86_64-netbsd9.0",
                "x86_64", "", "netbsd", "9.0", "bsd"));

  assert (test ("x86_64-pc-openbsd5.6",
                "x86_64-openbsd5.6",
                "x86_64", "", "openbsd", "5.6", "bsd"));

  assert (test ("sparc-sun-solaris2.9",
                "sparc-sun-solaris2.9",
                "sparc", "sun", "solaris", "2.9"));

  assert (test ("x86_64-microsoft-win32-msvc14.0",
                "x86_64-microsoft-win32-msvc14.0",
                "x86_64", "microsoft", "win32-msvc", "14.0", "windows"));

  assert (test ("x86_64-windows-msvc",
                "x86_64-windows-msvc",
                "x86_64", "", "windows-msvc", "", "windows"));

  assert (test ("x86_64-pc-windows-msvc",
                "x86_64-windows-msvc",
                "x86_64", "", "windows-msvc", "", "windows"));

  assert (test ("x86_64-pc-windows-msvc19.11.25547",
                "x86_64-windows-msvc19.11.25547",
                "x86_64", "", "windows-msvc", "19.11.25547", "windows"));
}

static bool
test (const char* s,
      const char* canon,
      const char* cpu,
      const char* vendor,
      const char* system,
      const char* version,
      const char* class_)
{
  target_triplet t (s);
  string c (t.string ());

  auto cmp = [] (const string& a, const char* e, const char* n) -> bool
  {
    if (a != e)
    {
      cerr << n << " actual: " << a << endl
           << n << " expect: " << e << endl;

      return false;
    }

    return true;
  };

  return
    cmp (c, canon, "canonical") &&
    cmp (t.cpu, cpu, "cpu") &&
    cmp (t.vendor, vendor, "vendor") &&
    cmp (t.system, system, "system") &&
    cmp (t.version, version, "version") &&
    cmp (t.class_, class_, "class");
}

static bool
fail (const char* s)
{
  try
  {
    target_triplet t (s);
    cerr << "nofail: " << s << endl;
    return false;
  }
  catch (const invalid_argument&)
  {
    //cerr << e << endl;
  }

  return true;
}
