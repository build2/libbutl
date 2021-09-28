// file      : libbutl/manifest-rewriter.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <libbutl/path.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/manifest-types.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  // Rewrite a hand-written manifest file preserving formatting, comments,
  // etc., of the unaffected parts. The general workflow is as follows:
  //
  // 1. Parse the manifest file using manifest_parser into a sequence of
  //    name/value pairs and their positions.
  //
  // 2. Create an instance of manifest_rewriter for the manifest file. This
  //    opens the file in read/write mode with exclusive access.
  //
  // 3. Iterate over this sequence in reverse and apply changes to the desired
  //    name/value pairs using the below API. Doing this in reverse makes sure
  //    the positions obtained on step 1 remain valid.
  //
  // Note that if an exception is thrown by replace() or insert(), then the
  // writer is no longer usable and there is no guarantees that the file is
  // left in a consistent state.
  //
  class LIBBUTL_SYMEXPORT manifest_rewriter
  {
  public:
    // Unless long_lines is true, break lines in values (see
    // manifest_serializer for details).
    //
    manifest_rewriter (path, bool long_lines = false);

    // Replace the existing value at the specified position (specifically,
    // between colon_pos and end_pos) with the specified new value. The new
    // value is serialized as if by manifest_serializer.
    //
    void
    replace (const manifest_name_value&);

    // Insert a new name/value after the specified position (specifically,
    // after end_pos). To insert before the first value, use the special
    // start-of-manifest value as position. The new name/value is serialized
    // as if by manifest_serializer. Throw manifest_serialization exception
    // on error.
    //
    void
    insert (const manifest_name_value& pos, const manifest_name_value&);

  private:
    path path_;
    bool long_lines_;
    auto_fd fd_;
  };
}
