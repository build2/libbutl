// file      : libbutl/manifest-serializer.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <vector>
#include <iosfwd>
#include <cstddef>    // size_t
#include <stdexcept>  // runtime_error
#include <functional>

#include <libbutl/manifest-types.hxx>

#include <libbutl/export.hxx>

namespace butl
{
  class LIBBUTL_SYMEXPORT manifest_serialization: public std::runtime_error
  {
  public:
    manifest_serialization (const std::string& name,
                            const std::string& description);

    std::string name;
    std::string description;
  };

  class LIBBUTL_SYMEXPORT manifest_serializer
  {
  public:
    // The filter, if specified, is called by next() prior to serializing the
    // pair into the stream. If the filter returns false, then the pair is
    // discarded.
    //
    // Note that currently there is no way for the filter to modify the name
    // or value. If we ever need this functionality, then we can add an
    // "extended" filter alternative with two "receiving" arguments:
    //
    // bool (..., optional<string>& n, optional<string>& v);
    //
    using filter_function = bool (const std::string& name,
                                  const std::string& value);

    // Unless long_lines is true, break lines in values (including multi-line)
    // so that their length does not exceed 78 codepoints (including '\n').
    //
    // Note that the multiline_v2 flag is temporary and should not be used
    // except by the implementation for testing.
    //
    manifest_serializer (std::ostream& os,
                         const std::string& name,
                         bool long_lines = false,
                         std::function<filter_function> filter = {},
                         bool multiline_v2 = false)
      : os_ (os),
        name_ (name),
        long_lines_ (long_lines),
        filter_ (std::move (filter)),
        multiline_v2_ (multiline_v2)
    {
    }

    const std::string&
    name () const {return name_;}

    // The first name-value pair should be the special "start-of-manifest"
    // with empty name and value being the format version. After that we
    // have a sequence of ordinary pairs which are the manifest. At the
    // end of the manifest we have the special "end-of-manifest" pair
    // with empty name and value. After that we can either have another
    // start-of-manifest pair (in which case the whole sequence repeats
    // from the beginning) or we get another end-of-manifest pair which
    // signals the end of stream. The end-of-manifest pair can be omitted
    // if it is followed by the start-of-manifest pair.
    //
    void
    next (const std::string& name, const std::string& value);

    // Write a comment. The supplied text is prefixed with "# " and
    // terminated with a newline.
    //
    void
    comment (const std::string&);

    // Merge the manifest value and a comment into the single string, having
    // the '<value>; <comment>' form. Escape ';' characters in the value with
    // the backslash.
    //
    static std::string
    merge_comment (const std::string& value, const std::string& comment);

  private:
    friend class manifest_rewriter;

    void
    write_next (const std::string& name, const std::string& value);

    // Validate and write a name and return its length in codepoints.
    //
    size_t
    write_name (const std::string&);

    // Write a non-empty value assuming the current line already has the
    // specified codepoint offset. If the resulting line length would be too
    // large then the multi-line representation will be used. For the
    // single-line representation the space character is written before the
    // value. It is assumed that the name, followed by the colon, is already
    // written.
    //
    void
    write_value (const std::string&, std::size_t offset);

    // Write the specified number of characters from the specified string
    // (assuming there are no newlines) split into multiple lines at or near
    // the 78 codepoints boundary. Assume the current line already has the
    // specified codepoint offset.
    //
    void
    write_value (const char* s, std::size_t n, std::size_t offset);

  private:
    enum {start, body, end} s_ = start;
    std::string version_; // Current format version.

  private:
    std::ostream& os_;
    const std::string name_;
    bool long_lines_;
    const std::function<filter_function> filter_;
    bool multiline_v2_;
  };

  // Serialize a manifest to a stream adding the leading format version pair
  // and the trailing end-of-manifest pair. Unless eos is false, then also
  // write the end-of-stream pair.
  //
  LIBBUTL_SYMEXPORT void
  serialize_manifest (manifest_serializer&,
                      const std::vector<manifest_name_value>&,
                      bool eos = true);
}

#include <libbutl/manifest-serializer.ixx>
