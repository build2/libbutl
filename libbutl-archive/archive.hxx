// file      : libbutl-archive/archive.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <libbutl/timestamp.hxx>

#include <libbutl-archive/export.hxx>

namespace butl
{
  LIBBUTL_ARCHIVE_SYMEXPORT void
  archive (const char* archive, const char* file, timestamp mtime);
}
