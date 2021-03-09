// file      : libbutl/lz4.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <cstdint>

#include <libbutl/optional.mxx>
#include <libbutl/fdstream.mxx>

#include <libbutl/export.hxx>

namespace butl
{
  namespace lz4
  {
    //@@ TODO: allow (re-)using external buffers, contexts?

    // Read the content from the input stream, compress it using the specified
    // compression level and block size, and write the compressed content to
    // the output stream. If content size is specified, then include it into
    // the compressed content header. Return the compressed content size.
    //
    // This function may throw std::bad_alloc as well as exceptions thrown by
    // fdstream read/write functions. It may also throw invalid_argument in
    // case of argument inconsistencies (e.g., content size does not match
    // actual) with what() returning the error description. The input stream
    // is expected to throw on badbit (but not failbit). The output stream is
    // expected to throw on badbit or failbit.
    //
    // The output and most likely the input streams must be in the binary
    // mode.
    //
    // Valid values for the compressions level are between 1 (fastest) and
    // 12 (best compression level).
    //
    // Valid block sizes and their IDs:
    //
    // 4:   64KB
    // 5:  256KB
    // 6:    1MB
    // 7:    4MB
    //
    // This function produces compressed content identical to:
    //
    // lz4 -z -<compression_level> -B<block_size_id> -BD [--content-size]
    //
    LIBBUTL_SYMEXPORT std::uint64_t
    compress (ofdstream&,
              ifdstream&,
              int compression_level,
              int block_size_id,
              optional<std::uint64_t> content_size);


    // Read the compressed content from the input stream, decompress it, and
    // write the decompressed content to the output stream. Return the
    // decompressed content size.
    //
    // This function may throw std::bad_alloc as well as exceptions thrown by
    // fdstream read/write functions. It may also throw invalid_argument if
    // the compressed content is invalid with what() returning the error
    // description. The input stream is expected to throw on badbit (but not
    // failbit). The output stream is expected to throw on badbit or failbit.
    //
    // The input and most likely the output streams must be in the binary
    // mode.
    //
    // Note that this function does not require the input stream to reach EOF
    // at the end of compressed content. So if you have this a requirement,
    // you will need to enforce it yourself.
    //
    LIBBUTL_SYMEXPORT std::uint64_t
    decompress (ofdstream&, ifdstream&);
  }
}
