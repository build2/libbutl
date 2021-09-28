// file      : libbutl/project-name.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <string>
#include <utility> // move()
#include <ostream>

#include <libbutl/utility.hxx> // icasecmp(), sanitize_identifier()

#include <libbutl/export.hxx>

namespace butl
{
  // Build system project name.
  //
  // Since a build system project is often a package, it is also used as a
  // package name by the package dependency manager. And since a package is
  // often a project (in the "collection of related packages" sense), it is
  // also used as a project name by the project dependency manager.
  //
  class LIBBUTL_SYMEXPORT project_name
  {
  public:
    // Create project name from string verifying that it complies with the
    // specification and throw std::invalid_argument if that's not the
    // case. Note that in this case the passed value is guaranteed to be
    // unchanged.
    //
    explicit
    project_name (const std::string& s): project_name (std::string (s)) {}

    explicit
    project_name (std::string&&);

    // Create a special empty project name.
    //
    project_name () {} // For Clang 3.8 (const initialization).

    // Create an arbitrary string that can be used in contexts that expect a
    // project name. For example, a project name pattern for use in ODB query
    // expressions.
    //
    enum raw_string_type {raw_string};
    project_name (std::string s, raw_string_type): value_ (std::move (s)) {}

    bool
    empty () const noexcept {return value_.empty ();}

    const std::string&
    string () const& noexcept {return value_;}

    // Moves the underlying project name string out of the project name object.
    // The object becomes empty. Usage: std::move (name).string ().
    //
    std::string
    string () && {std::string r; r.swap (this->value_); return r;}

    // Project name base and extension (without the dot). If there is no
    // extension, then the base name is the same as the full name and the
    // returned extension is empty.
    //
    // If the ext argument is not NULL, then only remove the specified
    // extension. Note that the extension should not include the dot and the
    // comparison is always case-insensitive.
    //
    std::string
    base (const char* ext = nullptr) const;

    std::string
    extension () const;

    // Project name sanitized to a canonical variable name. Specifically,
    // '.', '-', and '+' are replaced with '_'.
    //
    std::string
    variable () const {return sanitize_identifier (value_);}

    // Compare ignoring case. Note that a string is not checked to be a valid
    // project name.
    //
    int compare (const project_name& n) const {return compare (n.value_);}
    int compare (const std::string& n) const {return compare (n.c_str ());}
    int compare (const char* n) const {return icasecmp (value_, n);}

  private:
    std::string value_;
  };

  inline bool
  operator< (const project_name& x, const project_name& y)
  {
    return x.compare (y) < 0;
  }

  inline bool
  operator> (const project_name& x, const project_name& y)
  {
    return x.compare (y) > 0;
  }

  inline bool
  operator== (const project_name& x, const project_name& y)
  {
    return x.compare (y) == 0;
  }

  inline bool
  operator<= (const project_name& x, const project_name& y)
  {
    return x.compare (y) <= 0;
  }

  inline bool
  operator>= (const project_name& x, const project_name& y)
  {
    return x.compare (y) >= 0;
  }

  inline bool
  operator!= (const project_name& x, const project_name& y)
  {
    return x.compare (y) != 0;
  }

  template <typename T>
  inline auto
  operator< (const project_name& x, const T& y)
  {
    return x.compare (y) < 0;
  }

  template <typename T>
  inline auto
  operator> (const project_name& x, const T& y)
  {
    return x.compare (y) > 0;
  }

  template <typename T>
  inline auto
  operator== (const project_name& x, const T& y)
  {
    return x.compare (y) == 0;
  }

  template <typename T>
  inline auto
  operator<= (const project_name& x, const T& y)
  {
    return x.compare (y) <= 0;
  }

  template <typename T>
  inline auto
  operator>= (const project_name& x, const T& y)
  {
    return x.compare (y) >= 0;
  }

  template <typename T>
  inline auto
  operator!= (const project_name& x, const T& y)
  {
    return x.compare (y) != 0;
  }

  template <typename T>
  inline auto
  operator< (const T& x, const project_name& y)
  {
    return y > x;
  }

  template <typename T>
  inline auto
  operator> (const T& x, const project_name& y)
  {
    return y < x;
  }

  template <typename T>
  inline auto
  operator== (const T& x, const project_name& y)
  {
    return y == x;
  }

  template <typename T>
  inline auto
  operator<= (const T& x, const project_name& y)
  {
    return y >= x;
  }

  template <typename T>
  inline auto
  operator>= (const T& x, const project_name& y)
  {
    return y <= x;
  }

  template <typename T>
  inline auto
  operator!= (const T& x, const project_name& y)
  {
    return y != x;
  }

  inline std::ostream&
  operator<< (std::ostream& os, const project_name& v)
  {
    return os << v.string ();
  }
}
