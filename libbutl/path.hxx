// file      : libbutl/path.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <ostream>
#include <cstddef>    // ptrdiff_t
#include <cstdint>    // uint16_t
#include <cstring>    // str*()
#include <utility>    // move(), swap()
#include <iterator>
#include <stdexcept>  // invalid_argument
#include <functional> // hash

#ifdef _WIN32
#include <algorithm> // replace()
#endif

#include <libbutl/optional.hxx>
#include <libbutl/small-vector.hxx>

#ifdef _WIN32
#include <libbutl/utility.hxx> // *case*()
#endif

#include <libbutl/export.hxx>

namespace butl
{
  // Wish list/ideas for improvements.
  //
  // - Ability to convert to directory/leaf/base in-place, without dynamic
  //   allocation. One idea is something like this:
  //
  //   p -= "/*"; // directory
  //   p -= "*/"; // leaf
  //   p -= ".*"; // base
  //
  // - Faster normalize() implementation. In many cases (e.g., in build2)
  //   the path is either already normal or the difference is just slashes
  //   (i.e., there are no '.' or '..' components). So a fast path case
  //   might be in order.
  //

  // @@ This should probably be called invalid_path_argument
  //
  struct LIBBUTL_SYMEXPORT invalid_path_base: public std::invalid_argument
  {
    invalid_path_base ();
  };

  template <typename C>
  struct invalid_basic_path: invalid_path_base
  {
    using string_type = std::basic_string<C>;
    using size_type = typename string_type::size_type;

    string_type path;

    explicit
    invalid_basic_path (string_type p): path (std::move (p)) {}
    explicit
    invalid_basic_path (const C* p): path (p) {}
    invalid_basic_path (const C* p, size_type n): path (p, n) {}
  };

  enum class path_abnormality: std::uint16_t
  {
    none      = 0x00, // Path is normal.
    separator = 0x01, // Wrong or multiple consequitive directory separators.
    current   = 0x02, // Contains current directory (`.`) component.
    parent    = 0x04  // Contains parent directory (`..`) component.
  };

  inline path_abnormality operator& (path_abnormality, path_abnormality);
  inline path_abnormality operator| (path_abnormality, path_abnormality);
  inline path_abnormality operator&= (path_abnormality&, path_abnormality);
  inline path_abnormality operator|= (path_abnormality&, path_abnormality);

  // The only currently available specialization is for the char type.
  //
  template <typename C>
  struct path_traits
  {
    using string_type = std::basic_string<C>;
    using char_traits_type = typename string_type::traits_type;
    using size_type = typename string_type::size_type;

    // Canonical directory and path seperators.
    //
#ifdef _WIN32
    static constexpr const C directory_separator = '\\';
    static constexpr const C path_separator = ';';
#else
    static constexpr const C directory_separator = '/';
    static constexpr const C path_separator = ':';
#endif

    // Canonical and alternative directory separators. Canonical should be
    // first.
    //
#ifdef _WIN32
    static constexpr const char* const directory_separators = "\\/";
#else
    static constexpr const char* const directory_separators = "/";
#endif

    // Directory separator tests. On some platforms there could be multiple
    // seperators. For example, on Windows we check for both '/' and '\'.
    //
    static bool
    is_separator (C c)
    {
#ifdef _WIN32
      return c == '\\' || c == '/';
#else
      return c == '/';
#endif
    }

    // Return 1-based index in directory_separators string or 0 if not a
    // separator.
    //
    static size_type
    separator_index (C c)
    {
#ifdef _WIN32
      return c == '\\' ? 1 : c == '/' ? 2 : 0;
#else
      return c == '/' ? 1 : 0;
#endif
    }

    static bool
    absolute (const string_type& s)
    {
      return absolute (s.c_str (), s.size ());
    }

    static bool
    absolute (const C* s)
    {
      return absolute (s, char_traits_type::length (s));
    }

    static bool
    absolute (const C* s, size_type n)
    {
#ifdef _WIN32
      return n > 1 && s[1] == ':';
#else
      return n != 0 && is_separator (s[0]);
#endif
    }

    static bool
    current (const string_type& s)
    {
      return current (s.c_str (), s.size ());
    }

    static bool
    current (const C* s)
    {
      return current (s, char_traits_type::length (s));
    }

    static bool
    current (const C* s, size_type n)
    {
      return n == 1 && s[0] == '.';
    }

    static bool
    parent (const string_type& s)
    {
      return parent (s.c_str (), s.size ());
    }

    static bool
    parent (const C* s)
    {
      return parent (s, char_traits_type::length (s));
    }

    static bool
    parent (const C* s, size_type n)
    {
      return n == 2 && s[0] == '.' && s[1] == '.';
    }

    static bool
    normalized (const string_type& s, bool sep)
    {
      return normalized (s.c_str (), s.size (), sep);
    }

    static bool
    normalized (const C* s, bool sep)
    {
      return normalized (s, char_traits_type::length (s), sep);
    }

    static bool
    normalized (const C*, size_type, bool);

    static path_abnormality
    abnormalities (const string_type& s)
    {
      return abnormalities (s.c_str (), s.size ());
    }

    static path_abnormality
    abnormalities (const C* s)
    {
      return abnormalities (s, char_traits_type::length (s));
    }

    static path_abnormality
    abnormalities (const C*, size_type);

    static bool
    root (const string_type& s)
    {
      return root (s.c_str (), s.size ());
    }

    static bool
    root (const C* s)
    {
      return root (s, char_traits_type::length (s));
    }

    static bool
    root (const C* s, size_type n)
    {
#ifdef _WIN32
    return n == 2 && s[1] == ':';
#else
    return n == 1 && is_separator (s[0]);
#endif
    }

    static size_type
    find_separator (string_type const& s,
                    size_type pos = 0,
                    size_type n = string_type::npos)
    {
      if (n == string_type::npos)
        n = s.size ();

      const C* r (find_separator (s.c_str () + pos, n - pos));
      return r != nullptr ? r - s.c_str () : string_type::npos;
    }

    static const C*
    find_separator (const C* s)
    {
      return find_separator (s, char_traits_type::length (s));
    }

    static const C*
    find_separator (const C* s, size_type n)
    {
      for (const C* e (s + n); s != e; ++s)
      {
        if (is_separator (*s))
          return s;
      }

      return nullptr;
    }

    static size_type
    rfind_separator (string_type const& s, size_type pos = string_type::npos)
    {
      if (pos == string_type::npos)
        pos = s.size ();
      else
        pos++;

      const C* r (rfind_separator (s.c_str (), pos));
      return r != nullptr ? r - s.c_str () : string_type::npos;
    }

    static const C*
    rfind_separator (const C* s)
    {
      return rfind_separator (s, char_traits_type::length (s));
    }

    static const C*
    rfind_separator (const C* s, size_type n)
    {
      for (; n != 0; --n)
      {
        if (is_separator (s[n - 1]))
          return s + n - 1;
      }

      return nullptr;
    }

    // Return the position of '.' or npos if there is no extension.
    //
    static size_type
    find_extension (string_type const& s, size_type n = string_type::npos)
    {
      if (n == string_type::npos)
        n = s.size ();

      const C* r (find_extension (s.c_str (), n));
      return r != nullptr ? r - s.c_str () : string_type::npos;
    }

    static const C*
    find_extension (const C* s)
    {
      return find_extension (s, char_traits_type::length (s));
    }

    static const C*
    find_extension (const C* s, size_type n)
    {
      size_type i (n);

      for (; i > 0; --i)
      {
        C c (s[i - 1]);

        if (c == '.')
          break;

        if (is_separator (c))
        {
          i = 0;
          break;
        }
      }

      // Weed out paths like ".txt" (and "/.txt") and "txt.".
      //
      if (i > 1 && !is_separator (s[i - 2]) && i != n)
        return s + i - 1;
      else
        return nullptr;
    }

    // Return the start of the leaf (last path component) in the path. Note
    // that the leaf will include the trailing separator, if any (i.e., the
    // leaf of /tmp/bar/ is bar/).
    //
    static size_type
    find_leaf (string_type const& s)
    {
      const C* r (find_leaf (s.c_str (), s.size ()));
      return r != nullptr ? r - s.c_str () : string_type::npos;
    }

    static const C*
    find_leaf (const C* s)
    {
      return find_leaf (s, char_traits_type::length (s));
    }

    static const C*
    find_leaf (const C* s, size_type n)
    {
      const C* p;
      return n == 0
        ? nullptr
        : (p = rfind_separator (s, n - 1)) == nullptr ? s : ++p;
    }

    // Return true if sb is a sub-path of sp (i.e., sp is a prefix). Expects
    // both paths to be normalized. Note that this function returns true if
    // the paths are equal. Empty path is considered a prefix of any path.
    //
    static bool
    sub (const C* sb, size_type nb,
         const C* sp, size_type np);

    // Return true if sp is a super-path of sb (i.e., sb is a suffix). Expects
    // both paths to be normalized. Note that this function returns true if
    // the paths are equal. Empty path is considered a prefix of any path.
    //
    static bool
    sup (const C* sp, size_type np,
         const C* sb, size_type nb);

    static int
    compare (string_type const& l,
             string_type const& r,
             size_type n = string_type::npos)
    {
      return compare (l.c_str (), n < l.size () ? n : l.size (),
                      r.c_str (), n < r.size () ? n : r.size ());
    }

    // @@ Currently for case-insensitive filesystems (Windows) compare()
    // works properly only for ASCII.
    //
    static int
    compare (const C* l, size_type ln, const C* r, size_type rn)
    {
      //@@ TODO: would be nice to ignore difference in trailing slashes
      //   (except for POSIX root).

      for (size_type i (0), n (ln < rn ? ln : rn); i != n; ++i)
      {
#ifdef _WIN32
        C lc (lcase (l[i])), rc (lcase (r[i]));
#else
        C lc (l[i]), rc (r[i]);
#endif
        if (is_separator (lc) && is_separator (rc))
          continue;

        if (lc < rc) return -1;
        if (lc > rc) return 1;
      }

      return ln < rn ? -1 : (ln > rn ? 1 : 0);
    }

    static void
    canonicalize (string_type& s, char ds = '\0')
    {
      //canonicalize (s.data (), s.size ()); // C++17

      if (ds == '\0')
        ds = directory_separator;

      for (size_t i (0), n (s.size ()); i != n; ++i)
        if (is_separator (s[i]) && s[i] != ds)
          s[i] = ds;
    }

    static void
    canonicalize (C* s, size_type n, char ds = '\0')
    {
      if (ds == '\0')
        ds = directory_separator;

      for (const C* e (s + n); s != e; ++s)
        if (is_separator (*s) && *s != ds)
          *s = ds;
    }

    // Get/set current working directory. Throw std::system_error to report
    // underlying OS errors.
    //
    // The curren_directory() accessor (as well as the relevant process
    // startup functions) have a notion of a "thread working directory" which
    // is implemented as a thread-specific override that can be added/removed
    // with thread_current_directory() below.
    //
    // Note that the current_directory() modifier always sets the process-wide
    // working directory.
    //
    // See also thread_env().
    //
    static string_type
    current_directory ();

    static void
    current_directory (const string_type&);

    // Get/set thread working directory override. Note that the passed
    // pointed-to string should be valid (and immutable) for as long as the
    // override is in effect.
    //
    static const string_type*
    thread_current_directory ();

    static void
    thread_current_directory (const string_type*);

    // Return the user home directory. Throw std::system_error to report
    // underlying OS errors.
    //
    static string_type
    home_directory ();

    // Return the temporary directory. Throw std::system_error to report
    // underlying OS errors.
    //
    static string_type
    temp_directory ();

    // Return a temporary name. The name is constructed by starting with the
    // prefix followed by the process id following by a unique counter value
    // inside the process (MT-safe). Throw std::system_error to report
    // underlying OS errors.
    //
    static string_type
    temp_name (string_type const& prefix);

    // Make the path real (by calling realpath(3)). Throw invalid_basic_path
    // if the path is invalid (e.g., some components do not exist) and
    // std::system_error to report other underlying OS errors.
    //
#ifndef _WIN32
    static void
    realize (string_type&);
#endif

    // Utilities.
    //
#ifdef _WIN32
    static C
    tolower (C);

    static C
    toupper (C);
#endif
  };

  // This implementation of a filesystem path has two types: path, which can
  // represent any path (file, directory, etc) and dir_path, which is derived
  // from path. The internal representation of directories maintains a
  // trailing directory separator (slash). However, it is ignored in path
  // comparison, size, and string spelling. For example:
  //
  // path p1 ("foo");       // File path.
  // path p2 ("bar/");      // Directory path.
  //
  // path p3 (p1 / p2);     // Throw: p1 is not a directory.
  // path p4 (p2 / p1);     // Ok, file "bar/foo".
  // path p5 (p2 / p2);     // Ok, directory "bar/bar/".
  //
  // dir_path d1 ("foo");   // Directory path "foo/".
  // dir_path d2 ("bar\\"); // Directory path "bar\".
  //
  // dir_path d3 (d2 / d1); // "bar\\foo/"
  //
  // (p4 == d3);            // true
  // d3.string ();          // "bar\\foo"
  // d3.representation ();  // "bar\\foo/"
  //
  template <typename C, typename K>
  class basic_path;

  template <typename C> struct any_path_kind;
  template <typename C> struct dir_path_kind;

  using path = basic_path<char, any_path_kind<char>>;
  using dir_path = basic_path<char, dir_path_kind<char>>;
  using invalid_path = invalid_basic_path<char>;

  // Cast from one path kind to another. Note that no checking is performed
  // (e.g., that there is a trailing slash if casting to dir_path) but the
  // representation is adjusted if necessary (e.g., the trailing slash is
  // added to dir_path if missing).
  //
  template <class P, class C, class K> P path_cast (const basic_path<C, K>&);
  template <class P, class C, class K> P path_cast (basic_path<C, K>&&);

  // In certain cases we may need to translate a special path (e.g., `-`) to a
  // name that may not be a valid path (e.g., `<stdin>` or `<stdout>`), for
  // example, for diagnostics. In this case we can use path_name which
  // contains the original path plus an optional translation as a string. Note
  // that this is a view-like type with the original path shallow-referenced
  // rather than copied.
  //
  template <typename P>
  struct basic_path_name;

  using path_name = basic_path_name<path>;
  using dir_path_name = basic_path_name<dir_path>;

  // The copying version of the above that derives from the view (and thus can
  // be passed down as a view).
  //
  template <typename P>
  struct basic_path_name_value;

  using path_name_value = basic_path_name_value<path>;
  using dir_name_value = basic_path_name_value<dir_path>;

  // A "full" view version of the above that also shallow-references the
  // optional name. The "partial" view derives from this "full" view.
  //
  template <typename P>
  struct basic_path_name_view;

  using path_name_view = basic_path_name_view<path>;
  using dir_name_view = basic_path_name_view<dir_path>;

  // Low-level path data storage. It is also used by the implementation to
  // pass around initialized/valid paths.
  //
  template <typename C>
  struct path_data
  {
    using string_type = std::basic_string<C>;
    using size_type = typename string_type::size_type;
    using difference_type = typename string_type::difference_type;

    // The idea is as follows: path_ is always the "traditional" form; that
    // is, "/" for the root directory and "/tmp" (no trailing slash) for the
    // rest. This means we can return/store references to path_.
    //
    // Then we have tsep_ ("trailing separator") which is the size difference
    // between path_ and its "pure" part, that is, without any trailing
    // slashes, even for "/". So:
    //
    // tsep_ == -1  -- trailing slash in path_ (the "/" case)
    // tsep_ ==  0  -- no trailing slash
    //
    // Finally, to represent non-root ("/") trailing slashes we use positive
    // tsep_ values. In this case tsep_ is interpreted as a 1-based index in
    // the path_traits::directory_separators string.
    //
    // Notes:
    //  - If path_ is empty, then tsep_ can only be 0.
    //  - We could have used a much narrower integer for tsep_.
    //  - We could give the rest of tsep_ to the user to use as flags, etc.
    //
    string_type path_;
    difference_type tsep_;

    size_type
    _size () const {return path_.size () + (tsep_ < 0 ? -1 : 0);}

    void
    _swap (path_data& d) {path_.swap (d.path_); std::swap (tsep_, d.tsep_);}

    void
    _clear () {path_.clear (); tsep_ = 0;}

    // Constructors.
    //
    path_data () noexcept
        : tsep_ (0) {}

    path_data (string_type&& p, difference_type ts) noexcept
        : path_ (std::move (p)), tsep_ (path_.empty () ? 0 : ts) {}

    explicit
    path_data (string_type&& p) noexcept
        : path_ (std::move (p)) { _init (); }

    void
    _init () noexcept
    {
      size_type n (path_.size ()), i;

      if (n != 0 && (i = path_traits<C>::separator_index (path_[n - 1])) != 0)
      {
        if (n == 1) // The "/" case.
          tsep_ = -1;
        else
        {
          tsep_ = i;
          path_.pop_back ();
        }
      }
      else
        tsep_ = 0;
    }
  };

  template <typename C>
  struct any_path_kind
  {
    class base_type: public path_data<C> // In essence protected path_data.
    {
    protected:
      using path_data<C>::path_data;

      base_type () = default;
      base_type (path_data<C>&& d) noexcept
        : path_data<C> (std::move (d)) {}
    };

    using dir_type = basic_path<C, dir_path_kind<C>>;

    // Init and cast.
    //
    // If exact is true, return the path if the initialization was successful,
    // that is, the passed string is a valid path and no modifications were
    // necessary. Otherwise, return the empty object and leave the passed
    // string untouched.
    //
    // If extact is false, throw invalid_path if the string is not a valid
    // path (e.g., uses an unsupported path notation on Windows).
    //
    using data_type = path_data<C>;
    using string_type = std::basic_string<C>;

    static data_type
    init (string_type&&, bool exact = false);

    static void
    cast (data_type&) {}
  };

  template <typename C>
  struct dir_path_kind
  {
    using base_type = basic_path<C, any_path_kind<C>>;
    using dir_type = basic_path<C, dir_path_kind<C>>;

    // Init and cast.
    //
    using data_type = path_data<C>;
    using string_type = std::basic_string<C>;

    static data_type
    init (string_type&&, bool exact = false);

    static void
    cast (data_type&);
  };

  template <typename C, typename K>
  class basic_path: public K::base_type
  {
  public:
    using string_type = std::basic_string<C>;
    using size_type = typename string_type::size_type;
    using difference_type = typename string_type::difference_type;
    using traits_type = path_traits<C>;

    struct iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;

    using base_type = typename K::base_type;
    using dir_type = typename K::dir_type;

    // Create a special empty path. Note that we have to provide our own
    // implementation rather than using '=default' to make Clang allow
    // default-initialized const instances of this type.
    //
    basic_path () {}

    // Constructors that initialize a path from a string argument throw the
    // invalid_path exception if the string is not a valid path (e.g., uses
    // unsupported path notations on Windows). Note that an empty string
    // initializes an empty path.
    //
    explicit
    basic_path (C const* s): base_type (K::init (s)) {}

    basic_path (C const* s, size_type n)
        : base_type (K::init (string_type (s, n))) {}

    explicit
    basic_path (string_type s): base_type (K::init (std::move (s))) {}

    basic_path (const string_type& s, size_type n)
        : base_type (K::init (string_type (s, 0, n))) {}

    basic_path (const string_type& s, size_type p, size_type n)
        : base_type (K::init (string_type (s, p, n))) {}

    // Create a path using the exact string representation. If the string is
    // not a valid path or if it would require a modification, then empty path
    // is created instead and the passed string rvalue-reference is left
    // untouched. Note that no exception is thrown if the path is invalid. See
    // also representation()&& below.
    //
    enum exact_type {exact};
    basic_path (string_type&& s, exact_type)
        : base_type (K::init (std::move (s), true)) {}

    // Create a path as a sub-path identified by the [begin, end) range of
    // components.
    //
    basic_path (const iterator& begin, const iterator& end);

    basic_path (const reverse_iterator& rbegin, const reverse_iterator& rend)
        : basic_path (rend.base (), rbegin.base ()) {}

    void
    swap (basic_path& p) {this->_swap (p);}

    void
    clear () {this->_clear ();}

    // Get/set current working directory. Throw std::system_error to report
    // underlying OS errors.
    //
    static dir_type
    current_directory () {
      return dir_type (traits_type::current_directory ());}

    static void
    current_directory (basic_path const&);

    // Return the user home directory. Throw std::system_error to report
    // underlying OS errors.
    //
    static dir_type
    home_directory () {return dir_type (traits_type::home_directory ());}

    // Return the temporary directory. Throw std::system_error to report
    // underlying OS errors.
    //
    static dir_type
    temp_directory () {return dir_type (traits_type::temp_directory ());}

    // Return a temporary path. The path is constructed by starting with the
    // temporary directory and then appending a path component consisting of
    // the specified prefix followed by the process id following by a unique
    // counter value inside the process (all separated with `-`). Throw
    // std::system_error to report underlying OS errors.
    //
    static basic_path
    temp_path (const string_type& prefix)
    {
      basic_path r (temp_directory ());
      r /= traits_type::temp_name (prefix);
      return r;
    }

  public:
    bool
    empty () const {return this->path_.empty ();}

    // Note that size does not include the trailing separator except for
    // the POSIX root case.
    //
    size_type
    size () const {return this->path_.size ();}

    // Return true if this path doesn't have any directories. Note that `/foo`
    // is not a simple path (it is `foo` in root directory) while `/` is (it
    // is the root directory).
    //
    bool
    simple () const;

    bool
    absolute () const;

    bool
    relative () const {return !absolute ();}

    bool
    root () const;

    // The following predicates return true for the `.` and `..` paths,
    // respectively. Note that the result doesn't depend on the presence or
    // spelling of the trailing directory separator.
    //
    // Also note that the path must literally match the specified values rather
    // than be semantically current or parent. For example for paths `foo/..`
    // or `bar/../..` these predicates return false.
    //
    bool
    current () const;

    bool
    parent () const;

    // Return true if the path is normalized, that is, does not contain any
    // current or parent directory components or multiple consecutive and,
    // unless sep is false, non-canonical directory separators. Empty path
    // is considered normalized.
    //
    // Note that for a relative path normalize() may produce a path for which
    // normalized() will still return false (for example, ../foo/../ which
    // will be normalized to ../).
    //
    bool
    normalized (bool sep = true) const;

    // Similar to normalized() but return details on what renders the path
    // abnormal.
    //
    path_abnormality
    abnormalities () const;

    // Test, based on the presence/absence of the trailing separator, if the
    // path is to a directory.
    //
    bool
    to_directory () const {return this->tsep_ != 0;}

    // Return true if *this is a sub-path of the specified path (i.e.,
    // the specified path is a prefix). Expects both paths to be
    // normalized. Note that this function returns true if the paths
    // are equal. Empty path is considered a prefix of any path.
    //
    bool
    sub (const basic_path&) const;

    // Return true if *this is a super-path of the specified path (i.e.,
    // the specified path is a suffix). Expects both paths to be
    // normalized. Note that this function returns true if the paths
    // are equal. Empty path is considered a suffix of any path.
    //
    bool
    sup (const basic_path&) const;

  public:
    // Return the path without the directory part. Leaf of a directory is
    // itself a directory (contains trailing slash). Leaf of a root is the
    // path itself.
    //
    basic_path
    leaf () const;

    // As above but make the instance itself the leaf. Return *this.
    //
    basic_path&
    make_leaf ();

    // Return the path without the specified directory part. Returns empty
    // path if the paths are the same. Throw invalid_path if the directory is
    // not a prefix of *this. Expects both paths to be normalized.
    //
    basic_path
    leaf (basic_path const&) const;

    // Return the directory part of the path or empty path if there is no
    // directory. Directory of a root is an empty path.
    //
    dir_type
    directory () const;

    // As above but make the instance itself the directory. Return *this.
    //
    basic_path&
    make_directory ();

    // Return the directory part of the path without the specified leaf part.
    // Throw invalid_path if the leaf is not a suffix of *this. Expects both
    // paths to be normalized.
    //
    dir_type
    directory (basic_path const&) const;

    // Return the root directory of the path or empty path if the directory is
    // not absolute.
    //
    dir_type
    root_directory () const;

    // Return the path without the extension, if any.
    //
    basic_path
    base () const;

    // As above but make the instance itself the base. Return *this.
    //
    basic_path&
    make_base ();

    // Return the extension or empty string if not present. If not empty, then
    // the result starts with the character past the dot.
    //
    string_type
    extension () const;

    // Return the in-place pointer to extension or NULL if not present. If not
    // NULL, then the result points to the character past the dot but it is
    // legal to decrement it once to obtain the value with the dot.
    //
    const C*
    extension_cstring () const;

    // Return a path relative to the specified path that is equivalent
    // to *this. Throw invalid_path if a relative path cannot be derived
    // (e.g., paths are on different drives on Windows).
    //
    basic_path
    relative (basic_path) const;

    // As above but return nullopt rather than throw if a relative path cannot
    // be derived.
    //
    optional<basic_path>
    try_relative (basic_path) const;

    // Iteration over path components.
    //
    // Note that for an absolute POSIX path the first component is empty,
    // not `/`. Which means recombining a path with operator/= is not going
    // to work. Instead, do something along these lines:
    //
    // dir_path r;
    // for (auto i (d.begin ()); i != d.end (); ++i)
    //   r.combine (*i, i.separator ());
    //
    // @@ TODO: would be nice to skip consecutive separators (foo//bar).
    //
  public:
    struct iterator
    {
      using value_type = string_type ;
      using pointer = string_type*;
      using reference = string_type ;
      using size_type = typename string_type::size_type;
      using difference_type = std::ptrdiff_t ;
      using iterator_category = std::bidirectional_iterator_tag ;

      using data_type = path_data<C>;

      iterator (): p_ (nullptr) {}
      iterator (const data_type* p, size_type b, size_type e)
          : p_ (p), b_ (b), e_ (e) {}

      // Create an iterator by "rebasing" an old iterator onto a new path
      // object. Can, for example, be used to "move" an iterator when moving
      // the path object. Note: potentially dangerous if the old iterator used
      // to point to a different path.
      //
      iterator (const basic_path& p, const iterator& i)
          : p_ (&p), b_ (i.b_), e_ (i.e_) {}

      iterator&
      operator++ ()
      {
        const string_type& s (p_->path_);

        // Position past trailing separator, if any.
        //
        b_ = e_ != string_type::npos && ++e_ != s.size ()
          ? e_
          : string_type::npos;

        // Find next trailing separator.
        //
        e_ = b_ != string_type::npos
             ? traits_type::find_separator (s, b_)
             : b_;

        return *this;
      }

      iterator&
      operator-- ()
      {
        const string_type& s (p_->path_);

        // Find the new end.
        //
        e_ = b_ == string_type::npos               // Past end?
          ? (traits_type::is_separator (s.back ()) // Have trailing slash?
             ? s.size () - 1
             : string_type::npos)
          : b_ - 1;

        // Find the new begin.
        //
        b_ = e_ == 0 // Empty component?
          ? string_type::npos
          : traits_type::rfind_separator (s,
                                          e_ != string_type::npos
                                          ? e_ - 1
                                          : e_);

        b_ = b_ == string_type::npos // First component?
          ? 0
          : b_ + 1;

        return *this;
      }

      iterator
      operator++ (int) {iterator r (*this); operator++ (); return r;}

      iterator
      operator-- (int) {iterator r (*this); operator-- (); return r;}

      // @@ TODO: this should return string_view.
      //
      string_type
      operator* () const
      {
        return string_type (p_->path_,
                            b_,
                            e_ != string_type::npos ? e_ - b_ : e_);
      }

      // Return the directory separator after this component or '\0' if there
      // is none. This, for example, can be used to determine if the last
      // component is a directory.
      //
      C
      separator () const
      {
        return e_ != string_type::npos
          ? p_->path_[e_]
          : (p_->tsep_ > 0
             ? path_traits<C>::directory_separators[p_->tsep_ - 1]
             : 0);
      }

      pointer operator-> () const = delete;

      friend bool
      operator== (const iterator& x, const iterator& y)
      {
        return x.p_ == y.p_ && x.b_ == y.b_ && x.e_ == y.e_;
      }

      friend bool
      operator!= (const iterator& x, const iterator& y) {return !(x == y);}

    private:
      friend class basic_path;

      // b - first character of component
      // e - separator after component (or npos if none)
      // b == npos && e == npos - one past last component (end)
      //
      const data_type* p_;
      size_type b_;
      size_type e_;
    };

    iterator begin () const;
    iterator end () const;

    reverse_iterator rbegin () const {return reverse_iterator (end ());}
    reverse_iterator rend () const {return reverse_iterator (begin ());}

  public:
    // Canonicalize the path and return *this. Canonicalization involves
    // converting all directory separators to the canonical form (or to the
    // alternative separator if specified). Note that multiple directory
    // separators are not collapsed.
    //
    // Note that the alternative separator must be listed in path_trait::
    // directory_separators.
    //
    basic_path&
    canonicalize (char dir_sep = '\0');

    // Normalize the path and return *this. Throw invalid_path if the
    // resulting path would be invalid (e.g., /tmp/../..).
    //
    // Normalization involves collapsing the '.' and '..'  directories if
    // possible, collapsing multiple directory separators, and converting all
    // directory separators to the canonical form. If cur_empty is true then
    // collapse relative paths representing the current directory (for
    // example, '.', './', 'foo/..')  to an empty path. Otherwise convert it
    // to the canonical form (./ on POSIX systems). Note that a non-empty path
    // cannot become an empty one in the latter case.
    //
    // If actual is true, then for case-insensitive filesystems obtain the
    // actual spelling of the path. Only an absolute path can be actualized.
    // If a path component does not exist, then its (and all subsequent)
    // spelling is unchanged. Throw system_error on all other underlying
    // filesystem errors. Note that this is a potentially expensive operation.
    // Normally one can assume that "well-known" directories (current, home,
    // etc.) are returned in their actual spelling.
    //
    // Note that for a relative path normalize() may produce a path for which
    // normalized() will still return false (for example, ../foo/../ which
    // will be normalized to ../).
    //
    // Note also that on POSIX the parent directory ('..') components are
    // resolved relative to a symlink target. As a result, it's possible to
    // construct a valid path that this function will either consider as
    // invalid or produce a path that points to an incorrect filesystem entry
    // (it's also possible that it returns the correct path by accident). For
    // example:
    //
    //           /tmp/sym/../../../  -> <invalid> (should be /tmp)
    //                 |
    // /tmp/sub1/sub2/tgt
    //
    //           /tmp/sym/../../     -> /         (should be /tmp/sub1)
    //                 |
    // /tmp/sub1/sub2/tgt
    //
    // The common property of such paths is '..' crossing symlink boundaries
    // and it's impossible to normalize them without touching the filesystem
    // *and* resolving their symlink components (see realize() below).
    //
    basic_path&
    normalize (bool actual = false, bool cur_empty = false);

    // Make the path absolute using the current directory unless it is already
    // absolute. Return *this.
    //
    basic_path&
    complete ();

    // Make the path real, that is, absolute, normalized, and with resolved
    // symlinks. On POSIX systems this is accomplished with the call to
    // realpath(3). On Windows -- complete() and normalize(). Return *this.
    //
    basic_path&
    realize ();

  public:
    // Combine two paths. Note: empty path on RHS has no effect.
    //
    basic_path&
    operator/= (basic_path const&);

    // Combine a single path component (must not contain directory separators)
    // as a string, without first constructing the path object. Note: empty
    // string has no effect.
    //
    basic_path&
    operator/= (string_type const&);

    basic_path&
    operator/= (const C*);

    // As above but with an optional separator after the component. Note that
    // if the LHS is empty and the string is empty but the separator is not
    // '\0', then on POSIX this is treated as a root component.
    //
    void
    combine (string_type const&, C separator);

    void
    combine (const C*, C separator);

    void
    combine (const C*, size_type, C separator);

    // Append to the end of the path (normally an extension, etc).
    //
    basic_path&
    operator+= (string_type const&);

    basic_path&
    operator+= (const C*);

    basic_path&
    operator+= (C);

    void
    append (const C*, size_type);

    // Note that comparison is case-insensitive if the filesystem is not
    // case-sensitive (e.g., Windows). And it ignored trailing slashes
    // except for the root case.
    //
    template <typename K1>
    int
    compare (const basic_path<C, K1>& x) const {
      return traits_type::compare (this->path_, x.path_);}

  public:
    // Path string and representation. The string does not contain the
    // trailing slash except for the root case. In other words, it is the
    // "traditional" spelling of the path that can be passed to system calls,
    // etc. Representation, on the other hand is the "precise" spelling that
    // includes the trailing slash, if any. One cannot always round-trip a
    // path using string() but can using representation(). Note also that
    // representation() returns a copy while string() returns a (tracking)
    // reference.
    //
    const string_type&
    string () const& {return this->path_;}

    string_type
    representation () const&;

    // Moves the underlying path string out of the path object. The path
    // object becomes empty. Usage: std::move (p).string ().
    //
    string_type
    string () && {string_type r; r.swap (this->path_); return r;}

    string_type
    representation () &&;

    // Trailing directory separator or '\0' if there is none.
    //
    C
    separator () const;

    // As above but return it as a (potentially empty) string.
    //
    string_type
    separator_string () const;

    // If possible, return a POSIX version of the path. For example, for a
    // Windows path in the form foo\bar this function will return foo/bar. If
    // it is not possible to create a POSIX version for this path (e.g.,
    // c:\foo), this function will throw the invalid_path exception.
    //
    string_type
    posix_string () const&;

    string_type
    posix_representation () const&;

    string_type
    posix_string () &&;

    string_type
    posix_representation () &&;

    // Implementation details.
    //
  protected:
    using data_type = path_data<C>;

    // Direct initialization without init()/cast().
    //
    explicit
    basic_path (data_type&& d) noexcept
      : base_type (std::move (d)) {}

    using base_type::_size;
    using base_type::_init;

    // Common implementation for operator/=.
    //
    void
    combine_impl (const C*, size_type, difference_type);

    void
    combine_impl (const C*, size_type);

    // Friends.
    //
    template <class C1, class K1>
    friend class basic_path;

    template <class C1, class K1, class K2>
    friend basic_path<C1, K1>
    path_cast_impl (const basic_path<C1, K2>&, basic_path<C1, K1>*);

    template <class C1, class K1, class K2>
    friend basic_path<C1, K1>
    path_cast_impl (basic_path<C1, K2>&&, basic_path<C1, K1>*);
  };

  template <typename C, typename K>
  inline basic_path<C, K>
  operator/ (const basic_path<C, K>& x, const basic_path<C, K>& y)
  {
    basic_path<C, K> r (x);
    r /= y;
    return r;
  }

  template <typename C, typename K>
  inline basic_path<C, K>
  operator+ (const basic_path<C, K>& x, const std::basic_string<C>& y)
  {
    basic_path<C, K> r (x);
    r += y;
    return r;
  }

  template <typename C, typename K>
  inline basic_path<C, K>
  operator+ (const basic_path<C, K>& x, const C* y)
  {
    basic_path<C, K> r (x);
    r += y;
    return r;
  }

  template <typename C, typename K>
  inline basic_path<C, K>
  operator+ (const basic_path<C, K>& x, C y)
  {
    basic_path<C, K> r (x);
    r += y;
    return r;
  }

  template <typename C, typename K1, typename K2>
  inline bool
  operator== (const basic_path<C, K1>& x, const basic_path<C, K2>& y)
  {
    return x.compare (y) == 0;
  }

  template <typename C, typename K1, typename K2>
  inline bool
  operator!= (const basic_path<C, K1>& x, const basic_path<C, K2>& y)
  {
    return !(x == y);
  }

  template <typename C, typename K1, typename K2>
  inline bool
  operator< (const basic_path<C, K1>& x, const basic_path<C, K2>& y)
  {
    return x.compare (y) < 0;
  }

  // Additional operators for certain path kind combinations.
  //
  template <typename C>
  inline basic_path<C, any_path_kind<C>>
  operator/ (const basic_path<C, dir_path_kind<C>>& x,
             const basic_path<C, any_path_kind<C>>& y)
  {
    basic_path<C, any_path_kind<C>> r (x);
    r /= y;
    return r;
  }

  // Note that the result of (foo / "bar") is always a path, even if foo
  // is dir_path. An idiom to force it to dir_path is:
  //
  // dir_path foo_bar (dir_path (foo) /= "bar");
  //
  template <typename C, typename K>
  inline basic_path<C, any_path_kind<C>>
  operator/ (const basic_path<C, K>& x, const std::basic_string<C>& y)
  {
    basic_path<C, any_path_kind<C>> r (x);
    r /= y;
    return r;
  }

  template <typename C, typename K>
  inline basic_path<C, any_path_kind<C>>
  operator/ (const basic_path<C, K>& x, const C* y)
  {
    basic_path<C, any_path_kind<C>> r (x);
    r /= y;
    return r;
  }

  template <typename C, typename K>
  std::basic_ostream<C>&
  to_stream (std::basic_ostream<C>&,
             const basic_path<C, K>&,
             bool representation);

  // For operator<< (ostream) see the path-io header.

  // path_name
  //

  template <typename P>
  struct basic_path_name_view
  {
    using path_type = P;
    using string_type = typename path_type::string_type;

    const path_type*             path;
    const optional<string_type>* name;

    explicit
    basic_path_name_view (const basic_path_name<P>& v)
        : path (v.path), name (&v.name) {}

    basic_path_name_view (const path_type* p, const optional<string_type>* n)
        : path (p), name (n) {}

    basic_path_name_view () // Create empty/NULL path name.
        : path (nullptr), name (nullptr) {}


    bool
    null () const
    {
      return path == nullptr && (name == nullptr || !*name);
    }

    bool
    empty () const
    {
      // assert (!null ());
      return name != nullptr && *name ? (*name)->empty () : path->empty ();
    }
  };

  template <typename P>
  struct basic_path_name: basic_path_name_view<P>
  {
    using base = basic_path_name_view<P>;

    using path_type = typename base::path_type;
    using string_type = typename base::string_type;

    optional<string_type> name;

    // Note that a NULL name is converted to absent.
    //
    explicit
    basic_path_name (const basic_path_name_view<P>& v)
        : base (v.path, &name),
          name (v.name != nullptr ? *v.name : nullopt) {}

    explicit
    basic_path_name (const path_type& p, optional<string_type> n = nullopt)
        : base (&p, &name), name (std::move (n)) {}

    explicit
    basic_path_name (path_type&&, optional<string_type> = nullopt) = delete;

    explicit
    basic_path_name (string_type n)
        : base (nullptr, &name), name (std::move (n)) {}

    explicit
    basic_path_name (const path_type* p, optional<string_type> n = nullopt)
        : base (p, &name), name (std::move (n)) {}

    basic_path_name (): // Create empty/NULL path name.
        base (nullptr, &name) {}

    basic_path_name (basic_path_name&&) noexcept;
    basic_path_name (const basic_path_name&);
    basic_path_name& operator= (basic_path_name&&) noexcept;
    basic_path_name& operator= (const basic_path_name&);
  };

  template <typename P>
  struct basic_path_name_value: basic_path_name<P>
  {
    using base = basic_path_name<P>;

    using path_type = typename base::path_type;
    using string_type = typename base::string_type;

    path_type path;

    // Note that a NULL path/name is converted to empty/absent.
    //
    explicit
    basic_path_name_value (const basic_path_name_view<P>& v)
        : base (&path, v.name != nullptr ? *v.name : nullopt),
          path (v.path != nullptr ? *v.path : path_type ()) {}

    explicit
    basic_path_name_value (path_type p, optional<string_type> n = nullopt)
        : base (&path, std::move (n)), path (std::move (p)) {}

    basic_path_name_value (): base (&path) {} // Create empty/NULL path name.

    basic_path_name_value (basic_path_name_value&&) noexcept;
    basic_path_name_value (const basic_path_name_value&);
    basic_path_name_value& operator= (basic_path_name_value&&) noexcept;
    basic_path_name_value& operator= (const basic_path_name_value&);
  };
}

namespace std
{
  template <typename C, typename K>
  struct hash<butl::basic_path<C, K>>: hash<basic_string<C>>
  {
    using argument_type = butl::basic_path<C, K>;

    size_t
    operator() (const butl::basic_path<C, K>& p) const noexcept
    {
#ifndef _WIN32
      return hash<basic_string<C>>::operator() (p.string ());
#else
      // Case-insensitive FNV hash.
      //
      const auto& s (p.string ());

      size_t hash (static_cast<size_t> (2166136261UL));
      for (size_t i (0), n (s.size ()); i != n; ++i)
      {
        hash ^= static_cast<size_t> (butl::lcase (s[i]));

        // We are using C-style cast to suppress VC warning for 32-bit target
        // (the value is compiled but not used).
        //
        hash *= sizeof (size_t) == 4
          ? static_cast<size_t> (16777619UL)
          : (size_t) 1099511628211ULL;
      }
      return hash;
#endif
    }
  };
}

#include <libbutl/path.ixx>
#include <libbutl/path.txx>
