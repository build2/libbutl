// file      : libbutl/prompt.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/prompt.hxx>

#include <iostream>

#include <libbutl/diagnostics.hxx> // diag_stream

using namespace std;

namespace butl
{
  bool
  yn_prompt (const string& prompt, char def)
  {
    // Writing a robust Y/N prompt is more difficult than one would expect.
    //
    string a;
    do
    {
      *diag_stream << prompt << ' ';

      // getline() will set the failbit if it failed to extract anything,
      // not even the delimiter and eofbit if it reached eof before seeing
      // the delimiter.
      //
      getline (cin, a);

      bool f (cin.fail ());
      bool e (cin.eof ());

      if (f || e)
        *diag_stream << endl; // Assume no delimiter (newline).

      if (f)
        throw ios_base::failure ("unable to read y/n answer from stdout");

      if (a.empty () && def != '\0')
      {
        // Don't treat eof as the default answer. We need to see the actual
        // newline.
        //
        if (!e)
          a = def;
      }
    } while (a != "y" && a != "Y" && a != "n" && a != "N");

    return a == "y" || a == "Y";
  }
}
