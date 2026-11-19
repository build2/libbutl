// file      : libbutl-archive/archive.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <memory>    // std::unique_ptr
#include <stdexcept> // std::runtime_error

#include <libbutl/path.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/filesystem.hxx>

#include <libbutl-archive/export.hxx>

extern "C"
{
  struct archive;
}

namespace butl
{
  // Note that the file path may refer to the archive or to entry, and in the
  // latter case it can be the filesystem path or archive path.
  //
  class LIBBUTL_ARCHIVE_SYMEXPORT archive_error: public std::runtime_error
  {
  public:
    archive_error (path file, std::string description);

    // Derive the description from (opional) 'what' (e.g., "unable to set gzip
    // filter") and 'why' extracted from the archive object by calling
    // archive_error_string().
    //
    archive_error (path file, archive*, const char* what);

    path file;
    std::string description;
  };

  class LIBBUTL_ARCHIVE_SYMEXPORT archive_writer
  {
  public:
    // Throw std::bad_alloc if unable to allocate memory and archive_error for
    // all other errors (including IO).
    //
    explicit
    archive_writer (path file);

    // Add a file to the archive. It must exist and be a regular file. The
    // archive path must be relative.
    //
    // Throw std::bad_alloc if unable to allocate memory and archive_error for
    // all other errors (including IO).
    //
    void
    add_file_entry (const path& filesystem_path,
                    const path& archive_path,
                    unsigned short mode,
                    timestamp mtime);

    // Throw archive_error if unable to close the archive stream.
    //
    auto_rmfile
    close ();

    // Not copyable/movable.
    //
    archive_writer (archive_writer&&) = delete;
    archive_writer (const archive_writer&) = delete;
    archive_writer& operator= (archive_writer&&) = delete;
    archive_writer& operator= (const archive_writer&) = delete;

  private:
    struct archive_deleter
    {
      void operator() (archive*);
    };

    static std::streamsize
    write_callback (archive*, void* data, const void* buf, size_t size);

    // Note that we want this specific destruction order.
    //
    auto_rmfile file_;
    auto_fd fd_;
    std::unique_ptr<archive, archive_deleter> archive_;
  };
}
