// file      : libbutl/filesystem.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_FILESYSTEM_HXX
#define LIBBUTL_FILESYSTEM_HXX

#ifndef _WIN32
#  include <dirent.h> // DIR
#else
#  include <stddef.h> // intptr_t
#endif

// VC's sys/types.h header file doesn't define mode_t type. So let's define it
// ourselves according to the POSIX specification.
//
#ifndef _MSC_VER
#  include <sys/types.h> // mode_t
#else
   typedef int mode_t;
#endif

#include <cstddef>    // ptrdiff_t
#include <cstdint>    // uint16_t
#include <utility>    // move(), pair
#include <iterator>
#include <functional>

#include <libbutl/export.hxx>

#include <libbutl/path.hxx>
#include <libbutl/timestamp.hxx>

namespace butl
{
  // Return true if the path is to an existing regular file. Note that by
  // default this function follows symlinks.
  //
  LIBBUTL_EXPORT bool
  file_exists (const char*, bool follow_symlinks = true);

  inline bool
  file_exists (const path& p, bool fs = true) {
    return file_exists (p.string ().c_str (), fs);}

  // Return true if the path is to an existing directory. Note that this
  // function follows symlinks.
  //
  LIBBUTL_EXPORT bool
  dir_exists (const char*);

  inline bool
  dir_exists (const path& p) {return dir_exists (p.string ().c_str ());}

  // Return true if the path is to an existing file system entry. Note that by
  // default this function doesn't follow symlinks.
  //
  LIBBUTL_EXPORT bool
  entry_exists (const char*, bool follow_symlinks = false);

  inline bool
  entry_exists (const path& p, bool fs = false) {
    return entry_exists (p.string ().c_str (), fs);}

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

  // Return a flag indicating if the path is to an existing file system entry
  // and its type if so. Note that by default this function doesn't follow
  // symlinks.
  //
  LIBBUTL_EXPORT std::pair<bool, entry_type>
  path_entry (const char*, bool follow_symlinks = false);

  inline std::pair<bool, entry_type>
  path_entry (const path& p, bool fs = false) {
    return path_entry (p.string ().c_str (), fs);}

  // Return true if the directory is empty. Note that the path must exist
  // and be a directory.
  //
  LIBBUTL_EXPORT bool
  dir_empty (const dir_path&);

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

  LIBBUTL_EXPORT mkdir_status
  try_mkdir (const dir_path&, mode_t = 0777);

  // The '-p' version of the above (i.e., it creates the parent
  // directories if necessary).
  //
  LIBBUTL_EXPORT mkdir_status
  try_mkdir_p (const dir_path&, mode_t = 0777);

  // Try to remove the directory returning not_exist if it does not exist
  // and not_empty if it is not empty. Unless ignore_error is true, all
  // other errors are reported by throwing std::system_error.
  //
  enum class rmdir_status {success, not_exist, not_empty};

  LIBBUTL_EXPORT rmdir_status
  try_rmdir (const dir_path&, bool ignore_error = false);

  // The '-r' (recursive) version of the above. Note that it will
  // never return not_empty.
  //
  LIBBUTL_EXPORT rmdir_status
  try_rmdir_r (const dir_path&, bool ignore_error = false);

  // As above but throws rather than returns not_exist if the directory
  // does not exist (unless ignore_error is true), so check before calling.
  // If the second argument is false, then the directory itself is not removed.
  //
  LIBBUTL_EXPORT void
  rmdir_r (const dir_path&, bool dir = true, bool ignore_error = false);

  // Try to remove the file (or symlinks) returning not_exist if
  // it does not exist. Unless ignore_error is true, all other
  // errors are reported by throwing std::system_error.
  //
  enum class rmfile_status {success, not_exist};

  LIBBUTL_EXPORT rmfile_status
  try_rmfile (const path&, bool ignore_error = false);

  // Automatically try to remove the path on destruction unless cancelled.
  // Since the non-cancelled destruction will normally happen as a result
  // of an exception, the failure to remove the path is silently ignored.
  //
  template <typename P>
  struct auto_rm
  {
    explicit
    auto_rm (P p = P ()): path_ (std::move (p)) {}

    void
    cancel () {path_ = P ();}

    const P&
    path () const {return path_;}

    // Movable-only type. Move-assignment cancels the lhs object.
    //
    auto_rm (auto_rm&&);
    auto_rm& operator= (auto_rm&&);
    auto_rm (const auto_rm&) = delete;
    auto_rm& operator= (const auto_rm&) = delete;

    ~auto_rm ();

  private:
    P path_;
  };

  using auto_rmfile = auto_rm<path>;
  using auto_rmdir = auto_rm<dir_path>; // Note: recursive (rm_r).

  // Create a symbolic link to a file (default) or directory (third argument
  // is true). Throw std::system_error on failures.
  //
  // Note that Windows symlinks are currently not supported.
  //
  LIBBUTL_EXPORT void
  mksymlink (const path& target, const path& link, bool dir = false);

  // Create a symbolic link to a directory. Throw std::system_error on
  // failures.
  //
  inline void
  mksymlink (const dir_path& target, const dir_path& link)
  {
    mksymlink (target, link, true);
  }

  // Create a hard link to a file (default) or directory (third argument is
  // true). Throw std::system_error on failures.
  //
  // Note that on Linix, FreeBSD and some other platforms the target can not
  // be a directory. While Windows support directories (via junktions), this
  // is currently not implemented.
  //
  LIBBUTL_EXPORT void
  mkhardlink (const path& target, const path& link, bool dir = false);

  // Create a hard link to a directory. Throw std::system_error on failures.
  //
  inline void
  mkhardlink (const dir_path& target, const dir_path& link)
  {
    mkhardlink (target, link, true);
  }

  // File copy flags.
  //
  enum class cpflags: std::uint16_t
  {
    overwrite_content     = 0x1,
    overwrite_permissions = 0x2,

    none = 0
  };

  inline cpflags operator& (cpflags, cpflags);
  inline cpflags operator| (cpflags, cpflags);
  inline cpflags operator&= (cpflags&, cpflags);
  inline cpflags operator|= (cpflags&, cpflags);

  // Copy a regular file, including its permissions. Throw std::system_error
  // on failure. Fail if the destination file exists and the overwrite_content
  // flag is not set. Leave permissions of an existing destination file intact
  // unless the overwrite_permissions flag is set. Delete incomplete copies
  // before throwing.
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
  LIBBUTL_EXPORT void
  cpfile (const path& from, const path& to, cpflags = cpflags::none);

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
  LIBBUTL_EXPORT void
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

  // Return timestamp_nonexistent if the entry at the specified path
  // does not exist or is not a path. All other errors are reported
  // by throwing std::system_error. Note that this function resolves
  // symlinks.
  //
  LIBBUTL_EXPORT timestamp
  file_mtime (const char*);

  inline timestamp
  file_mtime (const path& p) {return file_mtime (p.string ().c_str ());}

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

  // Get path permissions. Throw std::system_error on failure. Note that this
  // function resolves symlinks.
  //
  LIBBUTL_EXPORT permissions
  path_permissions (const path&);

  // Set path permissions. Throw std::system_error on failure. Note that this
  // function resolves symlinks.
  //
  LIBBUTL_EXPORT void
  path_permissions (const path&, permissions);

  // Directory entry iteration.
  //
  class LIBBUTL_EXPORT dir_entry
  {
  public:
    typedef butl::path path_type;

    // Symlink target type in case of the symlink, ltype() otherwise.
    //
    entry_type
    type () const;

    entry_type
    ltype () const;

    // Entry path (excluding the base). To get the full path, do
    // base () / path ().
    //
    const path_type&
    path () const {return p_;}

    const dir_path&
    base () const {return b_;}

    dir_entry () = default;
    dir_entry (entry_type t, path_type p, dir_path b)
        : t_ (t), p_ (std::move (p)), b_ (std::move (b)) {}

  private:
    entry_type
    type (bool link) const;

  private:
    friend class dir_iterator;

    mutable entry_type t_ = entry_type::unknown;  // Lazy evaluation.
    mutable entry_type lt_ = entry_type::unknown; // Lazy evaluation.
    path_type p_;
    dir_path b_;
  };

  class LIBBUTL_EXPORT dir_iterator
  {
  public:
    typedef dir_entry value_type;
    typedef const dir_entry* pointer;
    typedef const dir_entry& reference;
    typedef std::ptrdiff_t difference_type;
    typedef std::input_iterator_tag iterator_category;

    ~dir_iterator ();
    dir_iterator () = default;

    explicit
    dir_iterator (const dir_path&);

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
    intptr_t h_ = -1;
#endif
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

  // Wildcard pattern match and search (aka glob).
  //

  // Return true if name matches pattern. Both must be single path components,
  // possibly with a trailing directory separator to indicate a directory.
  //
  // If the pattern ends with a directory separator, then it only matches a
  // directory name (i.e., ends with a directory separator, but potentially
  // different). Otherwise, it only matches a non-directory name (no trailing
  // directory separator).
  //
  // Currently the following wildcard characters are supported:
  //
  // * - match any number of characters (including zero)
  // ? - match any single character
  //
  LIBBUTL_EXPORT bool
  path_match (const std::string& pattern, const std::string& name);

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
  // In addition to the wildcard characters listed in path_match(),
  // path_search() also recognizes the ** and *** wildcard sequences. If a
  // path component contains **, then it is matched just like * but in all the
  // subdirectories, recursively. The *** wildcard behaves like ** but also
  // matches the start directory itself.
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
  LIBBUTL_EXPORT void
  path_search (const path& pattern,
               const std::function<bool (path&&,
                                         const std::string& pattern,
                                         bool interm)>&,
               const dir_path& start = dir_path (),
               bool follow_symlinks = true);
}

#include <libbutl/filesystem.ixx>

#endif // LIBBUTL_FILESYSTEM_HXX
