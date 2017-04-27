// file      : butl/standard-version.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/standard-version>

#include <cassert>
#include <cstdlib>   // strtoull()
#include <cstddef>   // size_t
#include <utility>   // move()
#include <stdexcept> // invalid_argument

#include <butl/utility> // alnum()

using namespace std;

namespace butl
{
  // Utility functions
  //
  static uint64_t
  parse_num (const string& s, size_t& p,
             const char* m,
             uint64_t min = 0, uint64_t max = 999)
  {
    if (s[p] == '-' || s[p] == '+') // strtoull() allows these.
      throw invalid_argument (m);

    const char* b (s.c_str () + p);
    char* e (nullptr);
    uint64_t r (strtoull (b, &e, 10));

    if (b == e || r < min || r > max)
      throw invalid_argument (m);

    p = e - s.c_str ();
    return static_cast<uint64_t> (r);
  }

  static void
  check_version (uint64_t version, bool snapshot)
  {
    // Check that the version isn't too large.
    //
    //                 AAABBBCCCDDDE
    bool r (version < 10000000000000ULL);

    // Check that E version component is consistent with the snapshot flag.
    //
    if (r)
      r = (version % 10) == (snapshot ? 1 : 0);

    // Check that pre-release number is consistent with the snapshot flag.
    //
    if (r)
    {
      uint64_t ab (version / 10 % 1000);

      // Note that if ab is 0, it can either mean non-pre-release version in
      // the absence of snapshot number, or 'a.0' pre-release otherwise. If ab
      // is 500, it can only mean 'b.0', that must be followed by a snapshot
      // number.
      //
      if (ab != 0)
        r = ab != 500 || snapshot;
    }

    // Check that the major, the minor and the bugfix versions are not
    // simultaneously zeros.
    //
    if (r)
      r = (version / 10000) != 0;

    if (!r)
      throw invalid_argument ("invalid project version");
  }

  // standard_version
  //
  standard_version::
  standard_version (const std::string& s)
  {
    auto bail = [] (const char* m) {throw invalid_argument (m);};

    // Pre-parse the first component to see if the version starts with epoch,
    // to keep the subsequent parsing straightforward.
    //
    bool ep (false);
    {
      char* e (nullptr);
      strtoull (s.c_str (), &e, 10);
      ep = *e == '~';
    }

    size_t p (0);

    if (ep)
    {
      epoch = parse_num (s, p, "invalid epoch", 1, uint16_t (~0));
      ++p; // Skip '~'.
    }

    uint16_t ma, mi, bf, ab (0);

    ma = parse_num (s, p, "invalid major version");

    // Note that here and below p is less or equal n, and so s[p] is always
    // valid.
    //
    if (s[p] != '.')
      bail ("'.' expected after major version");

    mi = parse_num (s, ++p, "invalid minor version");

    if (s[p] != '.')
      bail ("'.' expected after minor version");

    bf = parse_num (s, ++p, "invalid bugfix version");

    //           AAABBBCCCDDDE
    version = ma * 10000000000ULL +
              mi *    10000000ULL +
              bf *       10000ULL;

    if (version == 0)
      bail ("0.0.0 version");

    // Parse the pre-release component if present.
    //
    if (s[p] == '-')
    {
      char k (s[++p]);

      if (k != 'a' && k != 'b')
        bail ("'a' or 'b' expected in pre-release");

      if (s[++p] != '.')
        bail ("'.' expected after pre-release letter");

      ab = parse_num (s, ++p, "invalid pre-release", 0, 499);

      if (k == 'b')
        ab += 500;

      // Parse the snapshot components if present. Note that pre-release number
      // can't be zero for the final pre-release.
      //
      if (s[p] == '.')
        parse_snapshot (s, ++p);
      else if (ab == 0 || ab == 500)
        bail ("invalid final pre-release");
    }

    if (s[p] == '+')
      revision = parse_num (s, ++p, "invalid revision", 1, uint16_t (~0));

    if (p != s.size ())
      bail ("junk after version");

    if (ab != 0)
      version -= 10000 - ab * 10;

    if (snapshot_sn != 0)
      version += 1;
  }

  standard_version::
  standard_version (uint64_t v)
      : version (v)
  {
    check_version (v, false);
  }

  standard_version::
  standard_version (uint64_t v, const std::string& s)
      : version (v)
  {
    bool snapshot (!s.empty ());
    check_version (version, snapshot);

    if (snapshot)
    {
      size_t p (0);
      parse_snapshot (s, p);

      if (p != s.size ())
        throw invalid_argument ("junk after snapshot");
    }
  }

  standard_version::
  standard_version (uint16_t ep,
                    uint64_t vr,
                    uint64_t sn,
                    std::string si,
                    uint16_t rv)
      : epoch (ep),
        version (vr),
        snapshot_sn (sn),
        snapshot_id (move (si)),
        revision (rv)
  {
    check_version (vr, true);

    if (!snapshot_id.empty () && (snapshot_id.size () > 16 ||
                                  snapshot_sn == 0 ||
                                  snapshot_sn == latest_sn))
      throw invalid_argument ("invalid snapshot");
  }

  void standard_version::
  parse_snapshot (const std::string& s, size_t& p)
  {
    // Note that snapshot id must be empty for 'z' snapshot number.
    //
    if (s[p] == 'z')
    {
      snapshot_sn = latest_sn;
      ++p;
      return;
    }

    uint64_t sn (parse_num (s,
                            p,
                            "invalid snapshot number",
                            1, latest_sn - 1));
    std::string id;
    if (s[p] == '.')
    {
      char c;
      for (++p; alnum (c = s[p]); ++p)
        id += c;

      if (id.empty () || id.size () > 16)
        throw invalid_argument ("invalid snapshot id");
    }

    snapshot_sn = sn;
    snapshot_id = move (id);
  }

  string standard_version::
  string_pre_release () const
  {
    std::string r;

    if (alpha () || beta ())
    {
      uint64_t ab (version / 10 % 1000);

      if (ab < 500)
      {
        r += "a.";
        r += to_string (ab);
      }
      else
      {
        r += "b.";
        r += to_string (ab - 500);
      }
    }

    return r;
  }

  string standard_version::
  string_version () const
  {
    std::string r (to_string (major ()) + '.' + to_string (minor ()) + '.' +
                   to_string (patch ()));

    if (alpha () || beta ())
    {
      r += '-';
      r += string_pre_release ();

      if (snapshot ())
        r += '.';
    }

    return r;
  }

  string standard_version::
  string_snapshot () const
  {
    std::string r;

    if (snapshot ())
    {
      r = snapshot_sn == latest_sn ? "z" : to_string (snapshot_sn);

      if (!snapshot_id.empty ())
      {
        r += '.';
        r += snapshot_id;
      }
    }

    return r;
  }

  string standard_version::
  string_project () const
  {
    std::string r (string_version ());

    if (snapshot ())
      r += string_snapshot (); // string_version() includes trailing dot.

    return r;
  }

  string standard_version::
  string () const
  {
    std::string r;

    if (epoch != 0)
    {
      r  = to_string (epoch);
      r += '~';
    }

    r += string_project ();

    if (revision != 0)
    {
      r += '+';
      r += to_string (revision);
    }

    return r;
  }

  // standard_version_constraint
  //
  standard_version_constraint::
  standard_version_constraint (const std::string& s)
  {
    using std::string; // Not to confuse with string().

    auto bail = [] (const string& m) {throw invalid_argument (m);};
    const char* spaces (" \t");

    size_t p (0);
    char c (s[p]);

    if (c == '(' || c == '[') // Can be '\0'.
    {
      bool min_open = c == '(';

      p = s.find_first_not_of (spaces, ++p);
      if (p == string::npos)
        bail ("no min version");

      size_t e (s.find_first_of (spaces, p));

      standard_version min_version;

      try
      {
        min_version = standard_version (s.substr (p, e - p));
      }
      catch (const invalid_argument& e)
      {
        bail (string ("invalid min version: ") + e.what ());
      }

      p = s.find_first_not_of (spaces, e);
      if (p == string::npos)
        bail ("no max version");

      e = s.find_first_of (" \t])", p);

      standard_version max_version;

      try
      {
        max_version = standard_version (s.substr (p, e - p));
      }
      catch (const invalid_argument& e)
      {
        bail (string ("invalid max version: ") + e.what ());
      }

      p = s.find_first_of ("])", e);
      if (p == string::npos)
        bail ("no closing bracket");

      bool max_open = s[p] == ')';

      if (++p != s.size ())
        bail ("junk after constraint");

      // Verify and copy the constraint.
      //
      *this = standard_version_constraint (min_version, min_open,
                                           max_version, max_open);
    }
    else
    {
      enum comparison {eq, lt, gt, le, ge};
      comparison operation (eq); // Uninitialized warning.

      if (s.compare (0, p = 2, "==") == 0)
        operation = eq;
      else if (s.compare (0, p = 2, ">=") == 0)
        operation = ge;
      else if (s.compare (0, p = 2, "<=") == 0)
        operation = le;
      else if (s.compare (0, p = 1, ">") == 0)
        operation = gt;
      else if (s.compare (0, p = 1, "<") == 0)
        operation = lt;
      else
        bail ("invalid constraint");

      p = s.find_first_not_of (spaces, p);

      if (p == string::npos)
        bail ("no version");

      standard_version v;

      try
      {
        v = standard_version (s.substr (p));
      }
      catch (const invalid_argument& e)
      {
        bail (string ("invalid version: ") + e.what ());
      }

      // Verify and copy the constraint.
      //
      switch (operation)
      {
      case comparison::eq:
        *this = standard_version_constraint (v);
        break;
      case comparison::lt:
        *this = standard_version_constraint (nullopt, true, move (v), true);
        break;
      case comparison::le:
        *this = standard_version_constraint (nullopt, true, move (v), false);
        break;
      case comparison::gt:
        *this = standard_version_constraint (move (v), true, nullopt, true);
        break;
      case comparison::ge:
        *this = standard_version_constraint (move (v), false, nullopt, true);
        break;
      }
    }
  }

  standard_version_constraint::
  standard_version_constraint (optional<standard_version> mnv, bool mno,
                               optional<standard_version> mxv, bool mxo)
      : min_version (move (mnv)),
        max_version (move (mxv)),
        min_open (mno),
        max_open (mxo)
  {
    assert (
      // Min and max versions can't both be absent.
      //
      (min_version || max_version) &&

      // Version should be non-empty.
      //
      (!min_version || !min_version->empty ()) &&
      (!max_version || !max_version->empty ()) &&

      // Absent version endpoint (infinity) should be open.
      //
      (min_version || min_open) && (max_version || max_open));

    if (min_version && max_version)
    {
      if (*min_version > *max_version)
        throw invalid_argument ("min version is greater than max version");

      if (*min_version == *max_version && (min_open || max_open))
        throw invalid_argument ("equal version endpoints not closed");
    }
  }

  string standard_version_constraint::
  string () const
  {
    assert (!empty ());

    if (!min_version)
      return (max_open ? "< " : "<= ") + max_version->string ();

    if (!max_version)
      return (min_open ? "> " : ">= ") + min_version->string ();

    if (*min_version == *max_version)
      return "== " + min_version->string ();

    return (min_open ? '(' : '[') + min_version->string () + ' ' +
      max_version->string () + (max_open ? ')' : ']');
  }
}
