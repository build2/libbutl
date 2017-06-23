// file      : libbutl/manifest-serializer.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_MANIFEST_SERIALIZER_HXX
#define LIBBUTL_MANIFEST_SERIALIZER_HXX

#include <string>
#include <iosfwd>
#include <cstddef>   // size_t
#include <stdexcept> // runtime_error

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
    manifest_serializer (std::ostream& os, const std::string& name)
        : os_ (os), name_ (name) {}

    const std::string&
    name () const {return name_;}

    // The first name-value pair should be the special "start-of-manifest"
    // with empty name and value being the format version. After that we
    // have a sequence of ordinary pairs which are the manifest. At the
    // end of the manifest we have the special "end-of-manifest" pair
    // with empty name and value. After that we can either have another
    // start-of-manifest pair (in which case the whole sequence repeats
    // from the beginning) or we get another end-of-manifest pair which
    // signals the end of stream.
    //
    void
    next (const std::string& name, const std::string& value);

    // Write a comment. The supplied text is prefixed with "# " and
    // terminated with a newline.
    //
    void
    comment (const std::string&);

  private:
    void
    check_name (const std::string&);

    // Write 'n' characters from 's' (assuming there are no newlines)
    // split into multiple lines at or near the 78 characters
    // boundary. The first line starts at the 'column' offset.
    //
    void
    write_value (std::size_t column, const char* s, std::size_t n);

  private:
    enum {start, body, end} s_ = start;
    std::string version_; // Current format version.

  private:
    std::ostream& os_;
    const std::string name_;
  };
}

#endif // LIBBUTL_MANIFEST_SERIALIZER_HXX
