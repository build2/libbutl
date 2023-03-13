// file      : libbutl/host-os-release.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/host-os-release.hxx>

#include <sstream>
#include <stdexcept> // runtime_error

#include <libbutl/path.hxx>
#include <libbutl/path-io.hxx>
#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/filesystem.hxx>    // file_exists()
#include <libbutl/string-parser.hxx> // parse_quoted()

using namespace std;

namespace butl
{
  // Note: exported for access from the test.
  //
  LIBBUTL_SYMEXPORT os_release
  host_os_release_linux (path f = {})
  {
    os_release r;

    // According to os-release(5), we should use /etc/os-release and fallback
    // to /usr/lib/os-release if the former does not exist. It also lists the
    // fallback values for individual variables, in case some are not present.
    //
    auto exists = [] (const path& f)
    {
      try
      {
        return file_exists (f);
      }
      catch (const system_error& e)
      {
        ostringstream os;
        os << "unable to stat path " << f << ": " << e;
        throw runtime_error (os.str ());
      }
    };

    if (!f.empty ()
        ? exists (f)
        : (exists (f = path ("/etc/os-release")) ||
           exists (f = path ("/usr/lib/os-release"))))
    {
      try
      {
        ifdstream ifs (f, ifdstream::badbit);

        string l;
        for (uint64_t ln (1); !eof (getline (ifs, l)); ++ln)
        {
          trim (l);

          // Skip blanks lines and comments.
          //
          if (l.empty () || l[0] == '#')
            continue;

          // The variable assignments are in the "shell style" and so can be
          // quoted/escaped. For now we only handle quoting, which is what all
          // the instances seen in the wild seems to use.
          //
          size_t p (l.find ('='));
          if (p == string::npos)
            continue;

          string n (l, 0, p);
          l.erase (0, p + 1);

          using string_parser::parse_quoted;
          using string_parser::invalid_string;

          try
          {
            if (n == "ID_LIKE")
            {
              r.like_ids.clear ();

              vector<string> vs (parse_quoted (l, true /* unquote */));
              for (const string& v: vs)
              {
                for (size_t b (0), e (0); next_word (v, b, e); )
                {
                  r.like_ids.push_back (string (v, b, e - b));
                }
              }
            }
            else if (string* p = (n == "ID"               ?  &r.name_id :
                                  n == "VERSION_ID"       ?  &r.version_id :
                                  n == "VARIANT_ID"       ?  &r.variant_id :
                                  n == "NAME"             ?  &r.name :
                                  n == "VERSION_CODENAME" ?  &r.version_codename :
                                  n == "VARIANT"          ?  &r.variant :
                                  nullptr))
            {
              vector<string> vs (parse_quoted (l, true /* unquote */));
              switch (vs.size ())
              {
              case 0:  *p =  ""; break;
              case 1:  *p = move (vs.front ()); break;
              default: throw invalid_string (0, "multiple values");
              }
            }
          }
          catch (const invalid_string& e)
          {
            ostringstream os;
            os << "invalid " << n << " value in " << f << ':' << ln << ": "
               << e;
            throw runtime_error (os.str ());
          }
        }

        ifs.close ();
      }
      catch (const ios::failure& e)
      {
        ostringstream os;
        os << "unable to read from " << f << ": " << e;
        throw runtime_error (os.str ());
      }
    }

    // Assign fallback values.
    //
    if (r.name_id.empty ()) r.name_id = "linux";
    if (r.name.empty ())    r.name    = "Linux";

    return r;
  }

  static os_release
  host_os_release_macos ()
  {
    // Run sw_vers -productVersion to get Mac OS version.
    //
    try
    {
      process pr;
      try
      {
        fdpipe pipe (fdopen_pipe ());

        pr = process_start (0, pipe, 2, "sw_vers", "-productVersion");

        pipe.out.close ();
        ifdstream is (move (pipe.in), fdstream_mode::skip, ifdstream::badbit);

        // The output should be one line containing the version.
        //
        optional<string> v;
        for (string l; !eof (getline (is, l)); )
        {
          if (l.empty () || v)
          {
            v = nullopt;
            break;
          }

          v = move (l);
        }

        is.close (); // Detect errors.

        if (pr.wait ())
        {
          if (!v)
            throw runtime_error ("unexpected sw_vers -productVersion output");

          return os_release {"macos", {}, move (*v), "", "Mac OS", "", ""};
        }

      }
      catch (const ios::failure& e)
      {
        if (pr.wait ())
        {
          ostringstream os;
          os << "error reading sw_vers output: " << e;
          throw runtime_error (os.str ());
        }

        // Fall through.
      }

      // We should only get here if the child exited with an error status.
      //
      assert (!pr.wait ());
      throw runtime_error ("process sw_vers exited with non-zero code");
    }
    catch (const process_error& e)
    {
      ostringstream os;
      os << "unable to execute sw_vers: " << e;
      throw runtime_error (os.str ());
    }
  }

  optional<os_release>
  host_os_release (const target_triplet& h)
  {
    const string& c (h.class_);
    const string& s (h.system);

    if (c == "linux")
      return host_os_release_linux ();

    if (c == "macos")
      return host_os_release_macos ();

    if (c == "bsd")
    {
      // @@ TODO: ideally we would want to run uname and obtain the actual
      //    version we are runnig on rather than what we've been built for.
      //    (Think also how this will affect tests).
      //
      if (s == "freebsd")
        return os_release {"freebsd", {}, h.version, "", "FreeBSD", "", ""};

      if (s == "netbsd")
        return os_release {"netbsd", {}, h.version, "", "NetBSD", "", ""};

      if (s == "openbsd")
        return os_release {"openbsd", {}, h.version, "", "OpenBSD", "", ""};

      // Assume some other BSD.
      //
      return os_release {s, {}, h.version, "", s, "", ""};
    }

    return nullopt;
  }
}
