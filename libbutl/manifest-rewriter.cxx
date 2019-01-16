// file      : libbutl/manifest-rewriter.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2019 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef __cpp_modules
#include <libbutl/manifest-rewriter.mxx>
#endif

#include <cassert>

// C includes.

#ifndef __cpp_lib_modules
#include <string>
#include <cstdint> // uint64_t
#endif

// Other includes.

#ifdef __cpp_modules
module butl.manifest_rewriter;

// Only imports additional to interface.
#ifdef __clang__
#ifdef __cpp_lib_modules
import std.core;
#endif
import butl.path;
import butl.fdstream;
import butl.manifest_types;
#endif

import butl.manifest_serializer;
#else
#include <libbutl/manifest-serializer.mxx>
#endif

using namespace std;

namespace butl
{
  manifest_rewriter::
  manifest_rewriter (path p)
      : path_ (move (p)),
        fd_ (fdopen (path_,
                     fdopen_mode::in  |
                     fdopen_mode::out |
                     fdopen_mode::exclusive))
  {
  }

  // Seek the file descriptor to the specified logical position and truncate
  // the file. Return the file suffix (cached prior to truncating) starting
  // from the specified position.
  //
  static string
  truncate (auto_fd& fd, uint64_t pos, uint64_t suffix_pos)
  {
    string r;
    {
      // Temporary move the descriptor into the stream.
      //
      ifdstream is (move (fd));
      fdbuf& buf (static_cast<fdbuf&> (*is.rdbuf ()));

      // Read suffix.
      //
      buf.seekg (suffix_pos);
      r = is.read_text ();

      // Seek to the specified position and move the file descriptor back.
      //
      buf.seekg (pos);
      fd = is.release ();
    }

    // Truncate the file starting from the current position. Note that we need
    // to use the physical position rather than logical.
    //
    fdtruncate (fd.get (), fdseek (fd.get (), 0, fdseek_mode::cur));
    return r;
  }

  void manifest_rewriter::
  replace (const manifest_name_value& nv)
  {
    assert (nv.colon_pos != 0); // Sanity check.

    // Truncate right after the value colon.
    //
    string suffix (truncate (fd_, nv.colon_pos + 1, nv.end_pos));

    // Temporary move the descriptor into the stream.
    //
    ofdstream os (move (fd_));

    if (!nv.value.empty ())
    {
      os << ' ';

      manifest_serializer s (os, path_.string ());
      s.write_value (nv.value, nv.colon_pos - nv.start_pos + 2);
    }

    os << suffix;

    // Move the file descriptor back.
    //
    fd_ = os.release (); // Note: flushes the buffer.
  }

  void manifest_rewriter::
  insert (const manifest_name_value& pos, const manifest_name_value& nv)
  {
    assert (pos.end_pos != 0); // Sanity check.

    // We could have just started writing over the suffix but the truncation
    // doesn't hurt.
    //
    string suffix (truncate (fd_, pos.end_pos, pos.end_pos));

    // Temporary move the descriptor into the stream.
    //
    ofdstream os (move (fd_));
    os << '\n';

    manifest_serializer s (os, path_.string ());
    s.write_name (nv.name);

    os << ':';

    if (!nv.value.empty ())
    {
      os << ' ';
      s.write_value (nv.value, nv.colon_pos - nv.start_pos + 2);
    }

    os << suffix;

    // Move the file descriptor back.
    //
    fd_ = os.release (); // Note: flushes the buffer.
  }
}
