// file      : libbutl/project-name.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/project-name.hxx>

#include <string>
#include <vector>
#include <utility>   // move()
#include <algorithm> // find()
#include <stdexcept> // invalid_argument

#include <libbutl/path.hxx>    // path::traits
#include <libbutl/utility.hxx> // alpha(), alnum()

using namespace std;

namespace butl
{
  // project_name
  //
  static const vector<string> illegal_prj_names ({
      "build",
      "con", "prn", "aux", "nul",
      "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
      "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"});

  static const string legal_prj_chars ("_+-.");

  project_name::
  project_name (std::string&& nm)
  {
    if (nm.size () < 2)
      throw invalid_argument ("length is less than two characters");

    if (find (illegal_prj_names.begin (), illegal_prj_names.end (), nm) !=
        illegal_prj_names.end ())
      throw invalid_argument ("illegal name");

    if (!alpha (nm.front ()))
      throw invalid_argument ("illegal first character (must be alphabetic)");

    // Here we rely on the fact that the name length >= 2.
    //
    for (auto i (nm.cbegin () + 1), e (nm.cend () - 1); i != e; ++i)
    {
      char c (*i);

      if (!(alnum(c) || legal_prj_chars.find (c) != string::npos))
        throw invalid_argument ("illegal character");
    }

    if (!alnum (nm.back ()) && nm.back () != '+')
      throw invalid_argument (
        "illegal last character (must be alphabetic, digit, or plus)");

    value_ = move (nm);
  }

  string project_name::
  base (const char* e) const
  {
    using std::string;

    size_t p (path::traits_type::find_extension (value_));

    if (e != nullptr                            &&
        p != string::npos                       &&
        icasecmp (value_.c_str () + p + 1, e) != 0)
      p = string::npos;

    return string (value_, 0, p);
  }

  string project_name::
  extension () const
  {
    using std::string;

    size_t p (path::traits_type::find_extension (value_));
    return p != string::npos ? string (value_, p + 1) : string ();
  }
}
