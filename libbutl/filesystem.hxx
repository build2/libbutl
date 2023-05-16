// file      : libbutl/filesystem.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <errno.h> // E*

#ifndef _WIN32
#  include <dirent.h> // DIR
#else
#  include <stddef.h> // intptr_t
#endif

// VC's sys/types.h header file doesn't define mode_t type. So we define it
// ourselves according to the POSIX specification.
//
#ifndef _MSC_VER
#  include <sys/types.h> // mode_t
#else
   using mode_t = int;
#endif

#include <string>
#include <cstddef>    // ptrdiff_t
#include <cstdint>    // uint16_t, etc
#include <utility>    // move(), pair
#include <iterator>   // input_iterator_tag
#include <functional>

#include <libbutl/path.hxx>
#include <libbutl/optional.hxx>
#include <libbutl/timestamp.hxx>
#include <libbutl/path-pattern.hxx> // path_match_flags

#include <libbutl/export.hxx>

namespace butl
{
  // Path permissions.
  //
  enum class permissions: std::uint16_t
  {
    // Note: matching POSIX values.
    //
    xo = 0001,
    wo = 0002,
    ro = 0004,

    xg = 0010,
    wg = 0020,
    rg = 0040,

    xu = 0100,
    wu = 0200,
    ru = 0400,

    none = 0
  };

  inline permissions operator& (permissions, permissions);
  inline permissions operator| (permissions, permissions);
  inline permissions operator&= (permissions&, permissions);
  inline permissions operator|= (permissions&, permissions);

  // Return true if the path is to an existing regular file. Note that by
  // default this function follows symlinks. Underlying OS errors are reported
  // by throwing std::system_error, unless ignore_error is true (in which case
  // erroneous entries are treated as non-existent).
  //
  LIBBUTL_SYMEXPORT bool
  file_exists (const char*,
               bool follow_symlinks = true,
               bool ignore_error = false);

  inline bool
  file_exists (const path& p, bool fs = true, bool ie = false) {
    return file_exists (p.string ().c_str (), fs, ie);}

  // Return true if the path is to an existing directory. Note that this
  // function follows symlinks. Underlying OS errors are reported by throwing
  // std::system_error, unless ignore_error is true (in which case erroneous
  // entries are treated as non-existent).
  //
  LIBBUTL_SYMEXPORT bool
  dir_exists (const char*, bool ignore_error = false);

  inline bool
  dir_exists (const path& p, bool ie = false) {
    return dir_exists (p.string ().c_str (), ie);}

  // Return true if the path is to an existing file system entry. Note that by
  // default this function doesn't follow symlinks. Underlying OS errors are
  // reported by throwing std::system_error, unless ignore_error is true (in
  // which case erroneous entries are treated as non-existent).
  //
  LIBBUTL_SYMEXPORT bool
  entry_exists (const char*,
                bool follow_symlinks = false,
                bool ignore_error = false);

  inline bool
  entry_exists (const path& p, bool fs = false, bool ie = false) {
    return entry_exists (p.string ().c_str (), fs, ie);}

  // Filesystem entry type.
  //
  enum class entry_type
  {
    unknown,
    regular,
    directory,
    symlink,
    other
  };

  // Filesystem entry info. The size is only meaningful for regular files.
  //
  struct entry_stat
  {
    entry_type    type;
    std::uint64_t size;
  };

  // Return a flag indicating if the path is to an existing filesystem entry
  // and its info if so. Note that by default this function doesn't follow
  // symlinks. Underlying OS errors are reported by throwing
  // std::system_error, unless ignore_error is true (in which case erroneous
  // entries are treated as non-existent).
  //
  // See also fdstat() in fdstream.
  //
  LIBBUTL_SYMEXPORT std::pair<bool, entry_stat>
  path_entry (const char*,
              bool follow_symlinks = false,
              bool ignore_error = false);

  inline std::pair<bool, entry_stat>
  path_entry (const path& p, bool fs = false, bool ie = false) {
    return path_entry (p.string ().c_str (), fs, ie);}

  // Return true if the directory is empty. Note that the path must exist
  // and be a directory. This function follows symlinks.
  //
  bool
  dir_empty (const dir_path&);

  // Return true if the file is empty. Note that the path must exist and be a
  // regular file. This function follows symlinks.
  //
  bool
  file_empty (const path&);

  // Set the file access and modification times to the current time. If the
  // file does not exist and create is true, create it and fail otherwise.
  // Return true if the file was created and false otherwise. Errors are
  // reported by throwing std::system_error.
  //
  LIBBUTL_SYMEXPORT bool
  touch_file (const path&, bool create = true);

  // Try to create a directory unless it already exists. If you expect
  // the directory to exist and performance is important, then you
  // should first call dir_exists() above since that's what this
  // implementation will do to make sure the path is actually a
  // directory.
  //
  // You should also probably use the default mode 0777 and let the
  // umask mechanism adjust it to the user's preferences.
  //
  // Errors are reported by throwing std::system_error.
  //
  enum class mkdir_status {success, already_exists};

  LIBBUTL_SYMEXPORT mkdir_status
  try_mkdir (const dir_path&, mode_t = 0777);

  // The '-p' version of the above (i.e., it creates the parent
  // directories if necessary).
  //
  LIBBUTL_SYMEXPORT mkdir_status
  try_mkdir_p (const dir_path&, mode_t = 0777);

  // Try to remove the directory returning not_exist if it does not exist
  // and not_empty if it is not empty. Unless ignore_error is true, all
  // other errors are reported by throwing std::system_error.
  //
  enum class rmdir_status {success, not_exist, not_empty};

  LIBBUTL_SYMEXPORT rmdir_status
  try_rmdir (const dir_path&, bool ignore_error = false);

  // The '-r' (recursive) version of the above. Note that it will
  // never return not_empty.
  //
  LIBBUTL_SYMEXPORT rmdir_status
  try_rmdir_r (const dir_path&, bool ignore_error = false);

  // As above but throws rather than returns not_exist if the directory
  // does not exist (unless ignore_error is true), so check before calling.
  // If the second argument is false, then the directory itself is not removed.
  //
  LIBBUTL_SYMEXPORT void
  rmdir_r (const dir_path&, bool dir = true, bool ignore_error = false);

  // Try to remove the file (or symlink) returning not_exist if it does not
  // exist. Unless ignore_error is true, all other errors are reported by
  // throwing std::system_error.
  //
  // Note that if it is known that the path refers to a symlink, then usage of
  // try_rmsymlink() function must be preferred, as a more efficient one.
  //
  enum class rmfile_status {success, not_exist};

  // Note that on Windows the read-only attribute is reset prior to the file
  // removal (as it can't otherwise be deleted). In such a case the operation
  // is not atomic. It is also not atomic for the directory-type reparse point
  // removal.
  //
  rmfile_status
  try_rmfile (const path&, bool ignore_error = false);

  optional<rmfile_status>
  try_rmfile_ignore_error (const path&);

  // Automatically try to remove a non-empty path on destruction unless
  // cancelled. Since the non-cancelled destruction will normally happen as a
  // result of an exception, the failure to remove the path is silently
  // ignored.
  //
  template <typename P>
  struct auto_rm
  {
    P path;
    bool active;

    explicit
    auto_rm (P p = P (), bool a = true): path (std::move (p)), active (a) {}

    void
    cancel () {active = false;}

    // Movable-only type. Move-assignment cancels the lhs object.
    //
    auto_rm (auto_rm&&) noexcept;
    auto_rm& operator= (auto_rm&&) noexcept;
    auto_rm (const auto_rm&) = delete;
    auto_rm& operator= (const auto_rm&) = delete;

    ~auto_rm ();
  };

  using auto_rmfile = auto_rm<path>;
  using auto_rmdir = auto_rm<dir_path>; // Note: recursive (rm_r).

  // Create a symbolic link to a file (default) or directory (third argument
  // is true). Assume a relative target to be relative to the link's
  // directory. Throw std::system_error on failures.
  //
  // Note that on Windows symlinks are supported partially:
  //
  // - File symlinks are implemented via the Windows symlink mechanism and may
  //   only be created on Windows 10 Build 14972 and above with either the
  //   Developer Mode enabled or if the process runs in the elevated command
  //   prompt.
  //
  // - Directory symlinks are implemented via the Windows symlink mechanism if
  //   possible (see above) and via the Windows junction mechanism otherwise.
  //   Note that creating a junction doesn't require a process to have
  //   administrative privileges and so succeeds regardless of the Windows
  //   version and mode. Also note that junctions, in contrast to symlinks,
  //   may only store target absolute paths. Thus, when create a junction with
  //   a relative target we complete it using the link directory and, if the
  //   latter is also relative, using the process' current working directory.
  //   This makes it impossible for a mksymlink() caller to rely on the target
  //   path staying relative. Note that we also normalize the junction target
  //   path regardless if we complete it or not.
  //
  // - Functions other than mksymlink() fully support Windows reparse points
  //   and treat them as follows:
  //
  //   - consider the file symlink entries (file-type reparse points tagged
  //     as IO_REPARSE_TAG_SYMLINK and referring to files) as regular file
  //     symlinks (having the entry_type::symlink type).
  //
  //   - consider the directory symlink entries (same as above but refer to
  //     directories) and junctions (directory-type reparse points tagged as
  //     IO_REPARSE_TAG_MOUNT_POINT and referring to directories) as directory
  //     symlinks (having the entry_type::symlink type).
  //
  //   - consider all other reparse point types (volume mount points, Unix
  //     domain sockets, etc) as other entries (having the entry_type::other
  //     type).
  //
  // Also note that symlinks are currently not supported properly on Wine due
  // to some differences in the underlying API behavior.
  //
  LIBBUTL_SYMEXPORT void
  mksymlink (const path& target, const path& link, bool dir = false);

  // Create a symbolic link to a directory. Throw std::system_error on
  // failures.
  //
  inline void
  mksymlink (const dir_path& target, const dir_path& link)
  {
    mksymlink (target, link, true);
  }

  // Return the symbolic link target. Throw std::system_error on failures.
  //
  // Note that this function doesn't follow symlinks, so if a symlink refers
  // to another symlink then the second link's path is returned.
  //
  // Also note that the function returns the exact target path as it is stored
  // in the symlink filesystem entry, without completing or normalizing it.
  //
  LIBBUTL_SYMEXPORT path
  readsymlink (const path&);

  // Follow a symbolic link chain until non-symlink filesystem entry is
  // encountered and return its path. Throw std::system_error on failures,
  // including on encountering a non-existent filesystem entry anywhere in the
  // chain (but see try_followsymlink() below).
  //
  // The resulting path is constructed by starting with the specified path and
  // then by sequentially resolving the symlink chain rebasing a relative
  // target path over the current resulting path and resetting it to the path
  // itself on encountering an absolute target path. For example:
  //
  // for  a/b/c -> ../d/e            the result is a/d/e
  // for  a/b/c -> /x/y/z -> ../d/e  the result is /x/d/e
  //
  path
  followsymlink (const path&);

  // As above but instead of failing on the dangling symlink return its path
  // (first) as well as as an indication of this condition (false as second).
  //
  LIBBUTL_SYMEXPORT std::pair<path, bool>
  try_followsymlink (const path&);

  // Remove a symbolic link to a file (default) or directory (third argument
  // is true). Throw std::system_error on failures.
  //
  LIBBUTL_SYMEXPORT rmfile_status
  try_rmsymlink (const path&, bool dir = false, bool ignore_error = false);

  // Remove a symbolic link to a directory. Throw std::system_error on
  // failures.
  //
  inline rmfile_status
  try_rmsymlink (const dir_path& link, bool ignore_error = false)
  {
    return try_rmsymlink (link, true /* dir */, ignore_error);
  }

  // Create a hard link to a file (default) or directory (third argument is
  // true). Throw std::system_error on failures.
  //
  // Note that on Linux, FreeBSD, Windows and some other platforms the target
  // cannot be a directory.
  //
  LIBBUTL_SYMEXPORT void
  mkhardlink (const path& target, const path& link, bool dir = false);

  // Create a hard link to a directory. Throw std::system_error on failures.
  //
  inline void
  mkhardlink (const dir_path& target, const dir_path& link)
  {
    mkhardlink (target, link, true /* dir */);
  }

  // Make a symlink, hardlink, or, if `copy` is true, a copy of a file (note:
  // no directories, only files), whichever is possible in that order. If
  // `relative` is true, then make the symlink target relative to the link
  // directory (note: it is the caller's responsibility to make sure this is
  // possible). Otherwise, assume a relative target to be relative to the
  // link directory and complete it accordingly when create a hardlink or a
  // copy.
  //
  // On success, return the type of entry created: `regular` for copy,
  // `symlink` for symlink, and `other` for hardlink. On failure, throw a
  // `pair<entry_type, system_error>` with the first half indicating the part
  // of the logic that caused the error.
  //
  LIBBUTL_SYMEXPORT entry_type
  mkanylink (const path& target,
             const path& link,
             bool copy,
             bool relative = false);

  // File copy flags.
  //
  enum class cpflags: std::uint16_t
  {
    overwrite_content     = 0x1, // Overwrite content of destination.
    overwrite_permissions = 0x2, // Overwrite permissions of destination.

    copy_timestamps       = 0x4, // Copy timestamps from source.

    none = 0
  };

  inline cpflags operator& (cpflags, cpflags);
  inline cpflags operator| (cpflags, cpflags);
  inline cpflags operator&= (cpflags&, cpflags);
  inline cpflags operator|= (cpflags&, cpflags);

  // Copy a regular file, including its permissions (unless custom permissions
  // are specified), and optionally timestamps. Throw std::system_error on
  // failure. Fail if the destination file exists and the overwrite_content
  // flag is not set. Leave permissions of an existing destination file intact
  // (including if custom permissions are specified) unless the
  // overwrite_permissions flag is set. Delete incomplete copies before
  // throwing.
  //
  // Note that in case of overwriting, the existing destination file gets
  // truncated (not deleted) prior to being overwritten. As a side-effect,
  // hard link to the destination file will still reference the same file
  // system node after the copy.
  //
  // Also note that if the overwrite_content flag is not set and the
  // destination is a dangling symbolic link, then this function will still
  // fail.
  //
  LIBBUTL_SYMEXPORT void
  cpfile (const path& from,
          const path& to,
          cpflags = cpflags::none,
          optional<permissions> perm = nullopt);

  // Copy a regular file into (inside) an existing directory.
  //
  inline void
  cpfile_into (const path& from,
               const dir_path& into,
               cpflags fl = cpflags::none)
  {
    cpfile (from, into / from.leaf (), fl);
  }

  // Rename a filesystem entry (file, symlink, or directory). Throw
  // std::system_error on failure.
  //
  // If the source path refers to a directory, then the destination path must
  // either not exist, or refer to an empty directory. If the source path
  // refers to an entry that is not a directory, then the destination path must
  // not exist or not refer to a directory.
  //
  // If the source path refers to a symlink, then the link is renamed. If the
  // destination path refers to a symlink, then the link will be overwritten.
  //
  // If the source and destination paths are on different file systems (or
  // different drives on Windows) and the underlying OS does not support move
  // for the source entry, then fail unless the source paths refers to a file
  // or a file symlink. In this case fall back to copying the source file
  // (content, permissions, access and modification times) and removing the
  // source entry afterwards.
  //
  // Note that the operation is atomic only on POSIX, only if source and
  // destination paths are on the same file system, and only if the
  // overwrite_content flag is specified.
  //
  LIBBUTL_SYMEXPORT void
  mventry (const path& from,
           const path& to,
           cpflags = cpflags::overwrite_permissions);

  // Move a filesystem entry into (inside) an existing directory.
  //
  inline void
  mventry_into (const path& from,
                const dir_path& into,
                cpflags f = cpflags::overwrite_permissions)
  {
    mventry (from, into / from.leaf (), f);
  }

  // Raname file or file symlink.
  //
  inline void
  mvfile (const path& from,
          const path& to,
          cpflags f = cpflags::overwrite_permissions)
  {
    mventry (from, to, f);
  }

  inline void
  mvfile_into (const path& from,
               const dir_path& into,
               cpflags f = cpflags::overwrite_permissions)
  {
    mventry_into (from, into, f);
  }

  // Raname directory or directory symlink.
  //
  inline void
  mvdir (const dir_path& from,
         const dir_path& to,
         cpflags f = cpflags::overwrite_permissions)
  {
    mventry (from, to, f);
  }

  inline void
  mvdir_into (const path& from,
              const dir_path& into,
              cpflags f = cpflags::overwrite_permissions)
  {
    mventry_into (from, into, f);
  }

  struct entry_time
  {
    timestamp modification;
    timestamp access;
  };

  // Return timestamp_nonexistent for the modification and access times if the
  // entry at the specified path does not exist or is not a regular file. All
  // other errors are reported by throwing std::system_error. Note that these
  // functions resolves symlinks.
  //
  LIBBUTL_SYMEXPORT entry_time
  file_time (const char*);

  inline entry_time
  file_time (const path& p) {return file_time (p.string ().c_str ());}

  inline timestamp
  file_mtime (const char* p) {return file_time (p).modification;}

  inline timestamp
  file_mtime (const path& p) {return file_mtime (p.string ().c_str ());}

  inline timestamp
  file_atime (const char* p) {return file_time (p).access;}

  inline timestamp
  file_atime (const path& p) {return file_atime (p.string ().c_str ());}

  // As above but return the directory times.
  //
  LIBBUTL_SYMEXPORT entry_time
  dir_time (const char*);

  inline entry_time
  dir_time (const dir_path& p) {return dir_time (p.string ().c_str ());}

  inline timestamp
  dir_mtime (const char* p) {return dir_time (p).modification;}

  inline timestamp
  dir_mtime (const dir_path& p) {return dir_mtime (p.string ().c_str ());}

  inline timestamp
  dir_atime (const char* p) {return dir_time (p).access;}

  inline timestamp
  dir_atime (const dir_path& p) {return dir_atime (p.string ().c_str ());}

  // Set a regular file modification and access times. If a time value is
  // timestamp_nonexistent then it is left unchanged. All errors are reported
  // by throwing std::system_error.
  //
  // Note: use touch_file() instead of file_mtime(system_clock::now()).
  //
  LIBBUTL_SYMEXPORT void
  file_time (const char*, const entry_time&);

  inline void
  file_time (const path& p, const entry_time& t)
  {
    return file_time (p.string ().c_str (), t);
  }

  inline void
  file_mtime (const char* p, timestamp t)
  {
    return file_time (p, {t, timestamp_nonexistent});
  }

  inline void
  file_mtime (const path& p, timestamp t)
  {
    return file_mtime (p.string ().c_str (), t);
  }

  inline void
  file_atime (const char* p, timestamp t)
  {
    return file_time (p, {timestamp_nonexistent, t});
  }

  inline void
  file_atime (const path& p, timestamp t)
  {
    return file_atime (p.string ().c_str (), t);
  }

  // As above but set the directory times.
  //
  LIBBUTL_SYMEXPORT void
  dir_time (const char*, const entry_time&);

  inline void
  dir_time (const dir_path& p, const entry_time& t)
  {
    return dir_time (p.string ().c_str (), t);
  }

  inline void
  dir_mtime (const char* p, timestamp t)
  {
    return dir_time (p, {t, timestamp_nonexistent});
  }

  inline void
  dir_mtime (const dir_path& p, timestamp t)
  {
    return dir_mtime (p.string ().c_str (), t);
  }

  inline void
  dir_atime (const char* p, timestamp t)
  {
    return dir_time (p, {timestamp_nonexistent, t});
  }

  inline void
  dir_atime (const dir_path& p, timestamp t)
  {
    return dir_atime (p.string ().c_str (), t);
  }

  // Get path permissions. Throw std::system_error on failure. Note that this
  // function resolves symlinks.
  //
  LIBBUTL_SYMEXPORT permissions
  path_permissions (const path&);

  // Set path permissions. Throw std::system_error on failure. Note that this
  // function resolves symlinks.
  //
  LIBBUTL_SYMEXPORT void
  path_permissions (const path&, permissions);

  // Directory entry iteration.
  //
  class LIBBUTL_SYMEXPORT dir_entry
  {
  public:
    using path_type = butl::path;

    // Symlink target type in case of the symlink, ltype() otherwise.
    //
    // If type() returns entry_type::unknown then this entry is inaccessible
    // (ltype() also returns entry_type::unknown) or is a dangling symlink
    // (ltype() returns entry_type::symlink). Used with the detect_dangling
    // dir_iterator mode. Note that on POSIX ltype() can never return unknown
    // (because it is part of the directory iteration result).
    //
    entry_type
    type () const;

    entry_type
    ltype () const;

    // Modification and access times of the filesystem entry if it is not a
    // symlink and of the symlink target otherwise.
    //
    // These are provided as an optimization if they can be obtained as a
    // byproduct of work that is already being done anyway (iteration itself,
    // calls to [l]type(), etc). If (not yet) available, timestamp_unknown is
    // returned.
    //
    // Specifically:
    //
    // - On Windows mtime is always set by dir_iterator for entries other than
    //   reparse points.
    //
    // - On all platforms mtime and atime are always set for symlink targets
    //   by dir_iterator in the {detect,ignore}_dangling modes.
    //
    // - On all platforms mtime and atime can potentially be set by [l]type()
    //   if the stat() call is required to retrieve the type information (the
    //   native directory entry iterating API doesn't provide it, the type of
    //   the symlink target is queried, etc).
    //
    timestamp
    mtime () const {return mtime_;}

    timestamp
    atime () const {return atime_;}

    // Entry path (excluding the base). To get the full path, do
    // base () / path ().
    //
    const path_type&
    path () const {return p_;}

    const dir_path&
    base () const {return b_;}

    dir_entry () = default;

    dir_entry (entry_type t,
               path_type p,
               dir_path b,
               timestamp mt = timestamp_unknown,
               timestamp at = timestamp_unknown)
      : t_ (t),
        mtime_ (mt),
        atime_ (at),
        p_ (std::move (p)),
        b_ (std::move (b)) {}

  private:
    entry_type
    type (bool follow_symlinks) const;

  private:
    friend class dir_iterator;

    // Note: lazy evaluation.
    //
    mutable optional<entry_type> t_;  // Entry type.
    mutable optional<entry_type> lt_; // Symlink target type.

    mutable timestamp mtime_ = timestamp_unknown;
    mutable timestamp atime_ = timestamp_unknown;

    path_type p_;
    dir_path b_;
  };

  class LIBBUTL_SYMEXPORT dir_iterator
  {
  public:
    using value_type = dir_entry;
    using pointer = const dir_entry*;
    using reference = const dir_entry&;
    using difference_type =  std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    ~dir_iterator ();
    dir_iterator () = default;

    // If the mode is either ignore_dangling or detect_dangling, then stat()
    // the entry and either ignore inaccessible/dangling entry or return it
    // with the corresponding dir_entry type set to unknown (see dir_entry
    // type()/ltype() for details).
    //
    enum mode {no_follow, detect_dangling, ignore_dangling};

    explicit
    dir_iterator (const dir_path&, mode);

    dir_iterator (const dir_iterator&) = delete;
    dir_iterator& operator= (const dir_iterator&) = delete;

    dir_iterator (dir_iterator&&) noexcept;
    dir_iterator& operator= (dir_iterator&&);

    dir_iterator& operator++ () {next (); return *this;}

    reference operator* () const {return e_;}
    pointer operator-> () const {return &e_;}

    friend bool operator== (const dir_iterator&, const dir_iterator&);
    friend bool operator!= (const dir_iterator&, const dir_iterator&);

  private:
    void
    next ();

  private:
    dir_entry e_;

#ifndef _WIN32
    DIR* h_ = nullptr;
#else
    intptr_t h_ = -1; // INVALID_HANDLE_VALUE
#endif

    mode mode_ = no_follow;
  };

  // Range-based for loop support.
  //
  // for (const auto& de: dir_iterator (dir_path ("/tmp"))) ...
  //
  // Note that the "range" (which is the "begin" iterator), is no
  // longer usable. In other words, don't do this:
  //
  // dir_iterator i (...);
  // for (...: i) ...
  // ++i; // Invalid.
  //
  inline dir_iterator begin (dir_iterator&);
  inline dir_iterator end (const dir_iterator&);

  // MSVC in the strict mode (/permissive-), which we enable by default from
  // 15.5, needs this declaration to straighten its brains out.
  //
#if defined(_MSC_VER) && _MSC_VER >= 1912
  inline dir_iterator begin (dir_iterator&&);
#endif

  // Wildcard pattern search (aka glob).
  //
  // For details on the wildcard patterns see <libbutl/path-pattern.hxx>

  // Search for paths matching the pattern calling the specified function for
  // each matching path (see below for details).
  //
  // If the pattern is relative, then search in the start directory. If the
  // start directory is empty, then search in the current working directory.
  // Searching in non-existent directories is not an error. Throw
  // std::system_error in case of a failure (insufficient permissions, etc).
  //
  // The pattern may contain multiple components that include wildcards. On
  // Windows the drive letter may not be a wildcard.
  //
  // In addition to the wildcard characters, path_search() also recognizes the
  // ** and *** wildcard sequences. If a path component contains **, then it
  // is matched just like * but in all the subdirectories, recursively. The
  // *** wildcard behaves like ** but also matches the start directory itself.
  // Note that if the first pattern component contains ***, then the start
  // directory must be empty or be terminated with a "meaningful" component
  // (e.g., probably not '.' or '..').
  //
  // So, for example, foo/bar-**.txt will return all the files matching the
  // bar-*.txt pattern in all the subdirectoris of foo/. And foo/f***/ will
  // return all the subdirectories matching the f*/ pattern plus foo/ itself.
  //
  // Note that having multiple recursive components in the pattern we can end
  // up with calling func() multiple times (once per such a component) for the
  // same path. For example the search with pattern f***/b**/ starting in
  // directory foo, that has the foo/fox/box/ structure, will result in
  // calling func(foo/fox/box/) twice: first time for being a child of fox/,
  // second time for being a child of foo/.
  //
  // The callback function is called for both intermediate matches (interm is
  // true) and final matches (interm is false). Pattern is what matched the
  // last component in the path and is empty if the last component is not a
  // pattern (final match only; say as in */foo.txt).
  //
  // If the callback function returns false for an intermediate path, then no
  // further search is performed at or below this path. If false is returned
  // for a final match, then the entire search is stopped.
  //
  // The path can be moved for the final match or for an intermediate match
  // but only if false is returned.
  //
  // As an example, consider pattern f*/bar/b*/*.txt and path
  // foo/bar/baz/x.txt. The sequence of calls in this case will be:
  //
  // (foo/,              f*/,   true)
  // (foo/bar/baz/,      b*/,   true)
  // (foo/bar/baz/x.txt, *.txt, false)
  //
  // If the pattern contains a recursive wildcard, then the callback function
  // can be called for the same directory twice: first time as an intermediate
  // match with */ pattern to decide if to recursively traverse the directory,
  // and the second time if the directory matches the pattern component (either
  // as an intermediate or a final match). As an example, consider pattern
  // b**/c* and directory tree a/b/c/. The sequence of calls in this case will
  // be:
  //
  // (a/,     */,  true)
  // (a/b/,   */   true)
  // (a/b/c/, */,  true)
  // (a/b/,   b*/, true)
  // (a/b/c/, c*/, false)
  //
  // Note that recursive iterating through directories currently goes depth-
  // first which make sense for the cleanup use cases. In the future we may
  // want to make this controllable.
  //
  // If the match flags contain follow_symlinks, then call the dangling
  // callback function for inaccessible/dangling entries if specified, and
  // throw appropriate std::system_error otherwise. If the callback function
  // returns true, then inaccessible/dangling entry is ignored. Otherwise,
  // the entire search is stopped.
  //
  // Note also that if pattern is not simple (that is, contains directory
  // components), then some symlinks (those that are matched against the
  // directory components) may still be followed and thus the dangling
  // function called.
  //
  LIBBUTL_SYMEXPORT void
  path_search (const path& pattern,
               const std::function<bool (path&&,
                                         const std::string& pattern,
                                         bool interm)>&,
               const dir_path& start = dir_path (),
               path_match_flags = path_match_flags::follow_symlinks,
               const std::function<bool (const dir_entry&)>& dangling = nullptr);

  // Same as above, but behaves as if the directory tree being searched
  // through contains only the specified entry. The start directory is used if
  // the first pattern component is a self-matching wildcard (see above).
  //
  // If pattern or entry is relative, then it is assumed to be relative to the
  // start directory (which, if relative itself, is assumed to be relative to
  // the current directory). Note that the implementation can optimize the
  // case when pattern and entry are both non-empty and relative.
  //
  LIBBUTL_SYMEXPORT void
  path_search (const path& pattern,
               const path& entry,
               const std::function<bool (path&&,
                                         const std::string& pattern,
                                         bool interm)>&,
               const dir_path& start = dir_path (),
               path_match_flags = path_match_flags::none);
}

#include <libbutl/filesystem.ixx>
