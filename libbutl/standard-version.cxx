// file      : libbutl/standard-version.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/standard-version.hxx>

#include <cassert>
#include <cstdlib>   // strtoull()
#include <utility>   // move()
#include <stdexcept> // invalid_argument

#include <libbutl/utility.hxx> // alnum()

using namespace std;

namespace butl
{
  using std::to_string;

  // Parse uint64_t from the specified string starting at the specified
  // position and check the min/max constraints. If successful, save the
  // result, update the position to point to the next character, and return
  // true. Otherwise return false (result and position are unchanged).
  //
  // Note: also used for semantic_version parsing.
  //
  bool
  parse_uint64 (const string& s, size_t& p,
                uint64_t& r,
                uint64_t min = 0, uint64_t max = uint64_t (~0))
  {
    if (s[p] == '-' || s[p] == '+') // strtoull() allows these.
      return false;

    const char* b (s.c_str () + p);
    char* e (nullptr);
    errno = 0; // We must clear it according to POSIX.
    uint64_t v (strtoull (b, &e, 10)); // Can't throw.

    if (errno == ERANGE || b == e || v < min || v > max)
      return false;

    r = v;
    p = e - s.c_str ();
    return true;
  }

  template <typename T>
  static bool
  parse_uint (const string& s, size_t& p, T& r, T min, T max)
  {
    uint64_t v;
    if (!parse_uint64 (s, p, v, min, max))
      return false;

    r = static_cast<T> (v);
    return true;
  }

  static inline bool
  parse_uint16 (const string& s, size_t& p,
                uint16_t& r,
                uint16_t min = 0, uint16_t max = 999)
  {
    return parse_uint (s, p, r, min, max);
  }

  static inline bool
  parse_uint32 (const string& s, size_t& p,
                uint32_t& r,
                uint32_t min = 0, uint32_t max = 99999)
  {
    return parse_uint (s, p, r, min, max);
  }

  static void
  check_version (uint64_t vr, bool sn, standard_version::flags fl)
  {
    bool r;
    if (vr == uint64_t (~0) && (fl & standard_version::allow_stub) != 0)
    {
      // Stub.
      //
      // Check that the snapshot flag is false.
      //
      r = !sn;
    }
    else
    {
      // Check that the version isn't too large, unless represents stub.
      //
      //        AAAAABBBBBCCCCCDDDE
      r = vr < 10000000000000000000ULL;

      // Check that E version component is consistent with the snapshot flag.
      // Note that if the allow_earliest flag is set, then E can be 1 for the
      // snapshot flag being false, denoting the earliest pre-release of the
      // version.
      //
      if (r)
      {
        uint64_t e (vr % 10);
        if ((fl & standard_version::allow_earliest) == 0)
          r = e == (sn ? 1 : 0);
        else
          r = e == 1 || (e == 0 && !sn);
      }

      // Check that pre-release number is consistent with the snapshot flag.
      //
      if (r)
      {
        uint64_t ab (vr / 10 % 1000);

        // Note that if ab is 0, it can either mean non-pre-release version in
        // the absence of snapshot number, or 'a.0' pre-release otherwise. If
        // ab is 500, it can only mean 'b.0', that must be followed by a
        // snapshot number.
        //
        if (ab != 0)
          r = ab != 500 || sn;
      }

      // Check that the major, the minor and the patch versions are not
      // simultaneously zeros.
      //
      if (r)
        r = (vr / 10000) != 0;
    }

    if (!r)
      throw invalid_argument ("invalid standard version");
  }

  static bool
  parse_snapshot (const std::string& s,
                  size_t& p,
                  standard_version& r,
                  std::string& failure_reason)
  {
    // Note that snapshot id must be empty for 'z' snapshot number.
    //
    if (s[p] == 'z')
    {
      r.snapshot_sn = standard_version::latest_sn;
      r.snapshot_id = "";
      ++p;
      return true;
    }

    uint64_t sn;
    if (!parse_uint64 (s, p, sn, 1, standard_version::latest_sn - 1))
    {
      failure_reason = "invalid snapshot number";
      return false;
    }

    std::string id;
    if (s[p] == '.')
    {
      char c;
      for (++p; alnum (c = s[p]); ++p)
        id += c;

      if (id.empty () || id.size () > 16)
      {
        failure_reason = "invalid snapshot id";
        return false;
      }
    }

    r.snapshot_sn = sn;
    r.snapshot_id = move (id);
    return true;
  }

  struct parse_result
  {
    optional<standard_version> version;
    string failure_reason;
  };

  static parse_result
  parse_version (const std::string& s, standard_version::flags f)
  {
    auto bail = [] (string m) -> parse_result
    {
      return parse_result {nullopt, move (m)};
    };

    standard_version r;

    // Note that here and below p is less or equal n, and so s[p] is always
    // valid.
    //
    size_t p (0), n (s.size ());

    bool ep (s[p] == '+'); // Has epoch.

    if (ep)
    {
      if (!parse_uint16 (s, ++p, r.epoch, 1, uint16_t (~0)))
        return bail ("invalid epoch");

      // Skip the terminating character if it is '-', otherwise fail.
      //
      if (s[p++] != '-')
        return bail ("'-' expected after epoch");
    }

    uint32_t ma, mi, bf;
    uint16_t ab (0);
    bool earliest (false);

    if (!parse_uint32 (s, p, ma))
      return bail ("invalid major version");

    // The only valid version that has no epoch, contains only the major
    // version being equal to zero, that is optionally followed by the plus
    // character, is the stub version, unless forbidden.
    //
    bool stub ((f & standard_version::allow_stub) != 0 &&
               !ep                                     &&
               ma == 0                                 &&
               (p == n || s[p] == '+'));

    if (stub)
    {
      r.epoch = 0;
      r.version = uint64_t (~0);
    }
    else
    {
      if (s[p] != '.')
        return bail ("'.' expected after major version");

      if (!parse_uint32 (s, ++p, mi))
        return bail ("invalid minor version");

      if (s[p] != '.')
        return bail ("'.' expected after minor version");

      if (!parse_uint32 (s, ++p, bf))
        return bail ("invalid patch version");

      //           AAAAABBBBBCCCCCDDDE
      r.version = ma * 100000000000000ULL +
                  mi *      1000000000ULL +
                  bf *           10000ULL;

      if (r.version == 0)
        return bail ("0.0.0 version");

      // Parse the pre-release component if present.
      //
      if (s[p] == '-')
      {
        char k (s[++p]);

        // If the last character in the string is dash, then this is the
        // earliest version pre-release, unless forbidden.
        //
        if (k == '\0' && (f & standard_version::allow_earliest) != 0)
          earliest = true;
        else
        {
          if (k != 'a' && k != 'b')
            return bail ("'a' or 'b' expected in pre-release");

          if (s[++p] != '.')
            return bail ("'.' expected after pre-release letter");

          if (!parse_uint16 (s, ++p, ab, 0, 499))
            return bail ("invalid pre-release");

          if (k == 'b')
            ab += 500;

          // Parse the snapshot components if present. Note that pre-release
          // number can't be zero for the final pre-release.
          //
          if (s[p] == '.')
          {
            string e;
            if (!parse_snapshot (s, ++p, r, e))
              return bail (move (e));
          }
          else if (ab == 0 || ab == 500)
            return bail ("invalid final pre-release");
        }
      }
    }

    if (s[p] == '+')
    {
      assert (!earliest); // Would bail out earlier (a or b expected after -).

      if (!parse_uint16 (s, ++p, r.revision, 1, uint16_t (~0)))
        return bail ("invalid revision");
    }

    if (p != n)
      return bail ("junk after version");

    if (ab != 0 || r.snapshot_sn != 0 || earliest)
      r.version -= 10000 - ab * 10;

    if (r.snapshot_sn != 0 || earliest)
      r.version += 1;

    return parse_result {move (r), string () /* failure_reason */};
  }

  optional<standard_version>
  parse_standard_version (const std::string& s, standard_version::flags f)
  {
    return parse_version (s, f).version;
  }

  // standard_version
  //
  standard_version::
  standard_version (const std::string& s, flags f)
  {
    parse_result r (parse_version (s, f));

    if (r.version)
      *this = move (*r.version);
    else
      throw invalid_argument (r.failure_reason);
  }

  standard_version::
  standard_version (uint64_t v, flags f)
      : version (v)
  {
    check_version (v, false, f);
  }

  standard_version::
  standard_version (uint64_t v, const std::string& s, flags f)
      : version (v)
  {
    bool snapshot (!s.empty ());
    check_version (version, snapshot, f);

    if (snapshot)
    {
      size_t p (0);
      std::string e;
      if (!parse_snapshot (s, p, *this, e))
        throw invalid_argument (e);

      if (p != s.size ())
        throw invalid_argument ("junk after snapshot");
    }
  }

  standard_version::
  standard_version (uint16_t e,
                    uint64_t v,
                    const std::string& s,
                    uint16_t r,
                    flags f)
      : standard_version (v, s, f)
  {
    if (stub () && e != 0)
      throw invalid_argument ("epoch for stub");

    // Can't initialize above due to ctor delegating.
    //
    epoch = e;
    revision = r;
  }

  standard_version::
  standard_version (uint16_t ep,
                    uint64_t vr,
                    uint64_t sn,
                    std::string si,
                    uint16_t rv,
                    flags fl)
      : epoch (ep),
        version (vr),
        snapshot_sn (sn),
        snapshot_id (move (si)),
        revision (rv)
  {
    check_version (vr, true, fl);

    if (stub ())
    {
      if (ep != 0)
        throw invalid_argument ("epoch for stub");

      if (sn != 0)
        throw invalid_argument ("snapshot for stub");
    }

    if (!snapshot_id.empty () &&
        (snapshot_id.size () > 16 ||
         snapshot_sn == 0         ||
         snapshot_sn == latest_sn))
      throw invalid_argument ("invalid snapshot");
  }

  string standard_version::
  string_pre_release () const
  {
    std::string r;

    if ((alpha () && !earliest ()) || beta ())
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
    if (empty ())
      return "";

    if (stub ())
      return "0";

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
  string_project (bool rev) const
  {
    std::string r (string_version ());

    if (snapshot ())
      r += string_snapshot (); // string_version() includes trailing dot.

    if (rev && revision != 0)
    {
      r += '+';
      r += to_string (revision);
    }

    return r;
  }

  string standard_version::
  string_project_id () const
  {
    std::string r (string_version ());

    if (snapshot ()) // Trailing dot already in id.
    {
      r += (snapshot_sn == latest_sn ? "z"                     :
            snapshot_id.empty ()     ? to_string (snapshot_sn) :
                                       snapshot_id);
    }

    return r;
  }

  string standard_version::
  string () const
  {
    std::string r;

    if (epoch != 1 && !stub ())
    {
      r += '+';
      r += to_string (epoch);
      r += '-';
    }

    r += string_project (true /* revision */);

    return r;
  }

  // standard_version_constraint
  //
  // Return the maximum version (right hand side) of the range the shortcut
  // operator translates to:
  //
  // ~X.Y.Z  ->  [X.Y.Z  X.Y+1.0-)
  // ^X.Y.Z  ->  [X.Y.Z  X+1.0.0-)
  // ^0.Y.Z  ->  [0.Y.Z  0.Y+1.0-)
  //
  // If it is impossible to construct such a version due to overflow (see
  // below) then throw std::invalid_argument, unless requested to ignore in
  // which case return an empty version.
  //
  static standard_version
  shortcut_max_version (char c,
                        const standard_version& version,
                        bool ignore_overflow)
  {
    assert (c == '~' || c == '^');

    // Advance major/minor version number by one and make the version earliest
    // (see standard_version() ctor for details).
    //
    uint64_t v;

    if (c == '~' || (c == '^' && version.major () == 0))
    {
      // If for ~X.Y.Z Y is 99999, then we cannot produce a valid X.Y+1.0-
      // version (due to an overflow).
      //
      if (version.minor () == 99999)
      {
        if (ignore_overflow)
          return standard_version ();

        throw invalid_argument ("invalid minor version");
      }

      //                       AAAAABBBBBCCCCCDDDE
      v = version.major ()       * 100000000000000ULL +
          (version.minor () + 1) *      1000000000ULL;
    }
    else
    {
      // If for ^X.Y.Z X is 99999, then we cannot produce a valid X+1.0.0-
      // version (due to an overflow).
      //
      if (version.major () == 99999)
      {
        if (ignore_overflow)
          return standard_version ();

        throw invalid_argument ("invalid major version");
      }

      //                       AAAAABBBBBCCCCCDDDE
      v = (version.major () + 1) * 100000000000000ULL;
    }

    return standard_version (version.epoch,
                             v - 10000 /* no alpha/beta */ + 1 /* earliest */,
                             string () /* snapshot */,
                             0 /* revision */,
                             standard_version::allow_earliest);
  }

  // Parse the version constraint, optionally completing it using the
  // specified dependent package version.
  //
  static standard_version_constraint
  parse_constraint (const string& s, const standard_version* v)
  {
    using std::string; // Not to confuse with string().

    auto bail = [] (const string& m) {throw invalid_argument (m);};

    // The dependent package version can't be empty or earliest. It, however,
    // can be a stub (think of build-time dependencies).
    //
    if (v != nullptr)
    {
      if (v->empty ())
        bail ("dependent version is empty");

      if (v->earliest ())
        bail ("dependent version is earliest");
    }

    // Strip the dependent version revision. Fail for stubs and latest
    // snapshots, which are meaningless to refer to from the constraint.
    // Cache the result on the first call.
    //
    standard_version dv;
    auto dependent_version = [v, &dv, &bail] () -> const standard_version&
    {
      if (dv.empty ())
      {
        assert (v != nullptr);

        if (v->latest_snapshot ())
          bail ("dependent version is latest snapshot");

        if (v->stub ())
          bail ("dependent version is stub");

        dv = *v;
        dv.revision = 0;
      }

      return dv;
    };

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
        string mnv (s, p, e - p);

        min_version = v != nullptr && mnv == "$"
          ? dependent_version ()
          : standard_version (mnv, standard_version::allow_earliest);
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
        string mxv (s, p, e - p);

        max_version = v != nullptr && mxv == "$"
          ? dependent_version ()
          : standard_version (mxv, standard_version::allow_earliest);
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

      // Can throw.
      //
      return standard_version_constraint (move (min_version), min_open,
                                          move (max_version), max_open);
    }
    else if (c == '~' || c == '^')
    {
      p = s.find_first_not_of (spaces, ++p);
      if (p == string::npos)
        bail ("no version");

      standard_version min_version;
      standard_version max_version;

      try
      {
        string cv (s, p);
        if (v != nullptr && cv == "$") // Dependent version reference.
        {
          const standard_version& dv (dependent_version ());

          // For a release set the min version endpoint patch to zero. For ^
          // also set the minor version to zero, unless the major version is
          // zero (reduced to ~).
          //
          if (dv.release ())
          {
            min_version = standard_version (
              dv.epoch,
              dv.major (),
              c == '^' && dv.major () != 0 ? 0 : dv.minor (),
              0 /* patch */);
          }
          //
          // For a final pre-release or a patch snapshot we check if there has
          // been a compatible final release (patch is not zero for ~ and
          // minor/patch are not zero for ^). If that's the case, then
          // fallback to the release case and start the range from the first
          // alpha otherwise.
          //
          else if (dv.final () || (dv.snapshot () && dv.patch () != 0))
          {
            min_version = standard_version (
              dv.epoch,
              dv.major (),
              c == '^' && dv.major () != 0 ? 0 : dv.minor (),
              0 /* patch */,
              dv.patch () != 0 || (c == '^' && dv.minor () != 0)
              ? 0
              : 1 /* pre-release */);
          }
          //
          // For a major/minor snapshot we assume that all the packages are
          // developed in the lockstep and convert the constraint range to
          // represent this "snapshot series".
          //
          else
          {
            assert (dv.snapshot () && dv.patch () == 0);

            uint16_t pr (*dv.pre_release ());

            min_version = standard_version (dv.epoch,
                                            dv.major (),
                                            dv.minor (),
                                            0 /* patch */,
                                            pr,
                                            1 /* snapshot_sn */,
                                            "" /* snapshot_id */);

            max_version = standard_version (dv.epoch,
                                            dv.major (),
                                            dv.minor (),
                                            0 /* patch */,
                                            pr + 1);
          }
        }
        else // Version is specified literally.
        {
          // Can throw.
          //
          min_version = standard_version (cv,
                                          standard_version::allow_earliest);
        }

        // If the max version is not set for the lockstep (see above), then we
        // set it normally.
        //
        if (max_version.empty ())
          max_version = shortcut_max_version (
            c, min_version, false /* ignore_overflow */); // Can throw.
      }
      catch (const invalid_argument& e)
      {
        bail (string ("invalid version: ") + e.what ());
      }

      try
      {
        return standard_version_constraint (
          move (min_version), false /* min_open */,
          move (max_version), true  /* max_open */);
      }
      catch (const invalid_argument&)
      {
        // There shouldn't be a reason for standard_version_constraint()
        // to throw.
        //
        assert (false);
      }
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

      standard_version cv;

      try
      {
        string cver (s, p);

        cv = v != nullptr && cver == "$"
          ? dependent_version ()
          : standard_version (cver,
                              operation != comparison::eq
                              ? standard_version::allow_earliest
                              : standard_version::none);
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
        return standard_version_constraint (cv);
      case comparison::lt:
        return standard_version_constraint (nullopt, true, move (cv), true);
      case comparison::le:
        return standard_version_constraint (nullopt, true, move (cv), false);
      case comparison::gt:
        return standard_version_constraint (move (cv), true, nullopt, true);
      case comparison::ge:
        return standard_version_constraint (move (cv), false, nullopt, true);
      }
    }

    // Can't be here.
    //
    assert (false);
    return standard_version_constraint ();
  }

  standard_version_constraint::
  standard_version_constraint (const std::string& s)
  {
    *this = parse_constraint (s, nullptr /* dependent_version */);
  }

  standard_version_constraint::
  standard_version_constraint (const std::string& s,
                               const standard_version& dependent_version)
  {
    *this = parse_constraint (s, &dependent_version);
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

      // Version should be non-empty and not a stub.
      //
      (!min_version || (!min_version->empty () && !min_version->stub ())) &&
      (!max_version || (!max_version->empty () && !max_version->stub ())) &&

      // Absent version endpoint (infinity) should be open.
      //
      (min_version || min_open) && (max_version || max_open));

    if (min_version && max_version)
    {
      if (*min_version > *max_version)
        throw invalid_argument ("min version is greater than max version");

      if (*min_version == *max_version)
      {
        if (min_open || max_open)
          throw invalid_argument ("equal version endpoints not closed");

        if (min_version->earliest ())
          throw invalid_argument ("equal version endpoints are earliest");
      }
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

    if (!min_open && max_open)
    {
      // We try the '^' shortcut first as prefer ^0.2.3 syntax over ~0.2.3.
      //
      // In the case of overflow shortcut_max_version() function will return
      // an empty version, that will never match the max_version.
      //
      if (shortcut_max_version ('^', *min_version, true) == *max_version)
        return '^' + min_version->string ();

      if (shortcut_max_version ('~', *min_version, true) == *max_version)
        return '~' + min_version->string ();
    }

    return (min_open ? '(' : '[') + min_version->string () + ' ' +
      max_version->string () + (max_open ? ')' : ']');
  }

  bool standard_version_constraint::
  satisfies (const standard_version& v) const noexcept
  {
    bool s (true);

    if (min_version)
    {
      int i (v.compare (*min_version));
      s = min_open ? i > 0 : i >= 0;
    }

    if (s && max_version)
    {
      int i (v.compare (*max_version));
      s = max_open ? i < 0 : i <= 0;
    }

    return s;
  }
}
