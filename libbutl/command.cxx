// file      : libbutl/command.cxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#include <libbutl/command.hxx>

#include <ios>          // ios::failure
#include <vector>
#include <cassert>
#include <utility>      // move()
#include <stdexcept>    // invalid_argument
#include <system_error>

#include <libbutl/builtin.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/string-parser.hxx>

using namespace std;

namespace butl
{
  string
  command_substitute (const string& s, size_t sp,
                      const function<command_substitution_callback>& sc,
                      char open, char close)
  {
    string r;
    size_t p (0); // Current parsing position.

    for (size_t n (s.size ()); sp != string::npos; sp = s.find (open, ++p))
    {
      // Append the source string fraction preceding this substitution.
      //
      r.append (s, p, sp - p);

      // See if this is an escape of the opening substitution character
      // (adjacent characters).
      //
      if (++sp != n && s[sp] == open)
      {
        p = sp;
        r += open;
        continue;
      }

      // In sp we now have the start of the variable name. Find its end.
      //
      p = s.find (close, sp);

      // Verify that the variable name is properly terminated, not empty, and
      // doesn't contain whitespaces.
      //
      if (p == string::npos)
        throw invalid_argument (string ("unmatched substitution character '") +
                                open + '\'');

      if (p == sp)
        throw invalid_argument ("empty substitution variable");

      string vn (s, sp, p - sp);

      if (vn.find_first_of (" \t") != string::npos)
        throw invalid_argument ("whitespace in substitution variable '" +
                                vn + '\'');

      // Find the variable and append its value or fail if it's unknown.
      //
      if (!sc (vn, r))
        throw invalid_argument ("unknown substitution variable '" + vn + '\'');
    }

    // Append the source string tail following the last substitution.
    //
    r.append (s.begin () + p, s.end ());

    return r;
  }

  string
  command_substitute (const string& s, size_t sp,
                      const command_substitution_map& sm,
                      char o, char c)
  {
    return command_substitute (
      s,
      sp,
      [&sm] (const string& vn, string& r)
      {
        auto i (sm.find (vn));
        if (i == sm.end ())
          return false;
        r += i->second;
        return true;
      },
      o, c);
  }

  process_exit
  command_run (const string& cmd_str,
               const optional<process_env>& env,
               const optional<command_substitution_map>& substitutions,
               char subst,
               const function<command_callback>& callback)
  {
    // Split the command line into the program path, arguments, and redirects,
    // removing one level of quoting.
    //
    // Note: may throw invalid_argument.
    //
    vector<string> cmd (
      string_parser::parse_quoted (cmd_str, true /* unquote */));

    if (cmd.empty ())
      throw invalid_argument ("no program path specified");

    // Perform substitutions in a string. Throw invalid_argument for a
    // malformed substitution or an unknown variable name.
    //
    auto substitute = [&substitutions, subst] (string&& s)
    {
      size_t sp;
      if (substitutions && (sp = s.find (subst)) != string::npos)
        return command_substitute (s, sp, *substitutions, subst, subst);

      return move (s);
    };

    // Perform substitutions in the program path.
    //
    string prog (substitute (move (cmd.front ())));

    // Sort the remaining command line elements into the arguments and
    // redirects, performing the substitutions. Complete relative redirects
    // using CWD and use the rightmost redirect.
    //
    vector<string> args;

    optional<dir_path> redir;
    bool redir_append (false);

    const dir_path& cwd (env && env->cwd != nullptr ? *env->cwd : dir_path ());

    for (auto i (cmd.begin () + 1), e (cmd.end ()); i != e; ++i)
    {
      string a (move (*i));

      if (a[0] == '>') // Redirect.
      {
        redir_append = a[1] == '>';

        size_t n (redir_append ? 2 : 1);

        if (a.size () != n) // Strip the >/>> prefix.
        {
          a.erase (0, n);
        }
        else // Take the space-separated file path from the next element.
        {
          if (++i == e)
            throw invalid_argument ("no stdout redirect file specified");

          a = move (*i);
        }

        try
        {
          redir = dir_path (substitute (move (a)));
        }
        catch (const invalid_path& e)
        {
          throw invalid_argument ("invalid stdout redirect file path '" +
                                  e.path + '\'');
        }

        if (redir->empty ())
          throw invalid_argument ("empty stdout redirect file path");

        if (redir->relative () && !cwd.empty ())
          redir = cwd / *redir;
      }
      else             // Argument.
      {
        args.push_back (substitute (move (a)));
      }
    }

    // Open the redirect file descriptor, if specified.
    //
    // Intercept the exception to make the error description more informative.
    //
    auto_fd rd;

    if (redir)
    try
    {
      fdopen_mode m (fdopen_mode::out | fdopen_mode::create);
      m |= redir_append ? fdopen_mode::at_end : fdopen_mode::truncate;

      rd = fdopen (*redir, m);
    }
    catch (const ios::failure& e)
    {
      // @@ For libstdc++ the resulting exception description will be
      //    something like:
      //
      //    unable to open stdout redirect file ...: No such file or directory
      //
      //    Maybe we should improve our operator<<(ostream,exception) to
      //    lowercase the first word that follows ": " (code description) for
      //    exceptions derived from system_error.
      //
      string msg ("unable to open stdout redirect file '" + redir->string () +
                  "'");

      // For old versions of g++ (as of 4.9) ios_base::failure is not derived
      // from system_error and so we cannot recover the errno value. Lets use
      // EIO in this case. This is a temporary code after all.
      //
      const system_error* se (dynamic_cast<const system_error*> (&e));

      throw_generic_ios_failure (se != nullptr ? se->code ().value () : EIO,
                                 msg.c_str ());
    }

    const builtin_info* bi (builtins.find (prog));

    if (bi != nullptr && bi->function != nullptr) // Execute the builtin.
    {
      if (callback)
      {
        // Build the complete arguments list, appending the stdout redirect,
        // if present.
        //
        vector<const char*> elems ({prog.c_str ()});
        for (const auto& a: args)
          elems.push_back (a.c_str ());

        string rd;
        if (redir)
        {
          rd = (redir_append ? ">>" : ">");
          rd += redir->string ();

          elems.push_back (rd.c_str ());
        }

        elems.push_back (nullptr);

        callback (elems.data (), elems.size ());
      }

      // Finally, run the builtin.
      //
      uint8_t r; // Storage.
      builtin_callbacks cb;

      builtin b (bi->function (r,
                               args,
                               nullfd    /* stdin */,
                               move (rd) /* stdout */,
                               nullfd    /* stderr */,
                               cwd,
                               cb));

      return process_exit (b.wait ());
    }
    else                                          // Execute the program.
    {
      // Strip the potential leading `^`, indicating that this is an external
      // program rather than a builtin. Consider only simple paths and don't
      // end up with an empty path.
      //
      const char* p (prog.size () > 1 &&
                     prog[0] == '^'   &&
                     path::traits_type::find_separator (prog) == string::npos
                     ? prog.c_str () + 1
                     : prog.c_str ());

      // Prepare the process environment.
      //
      // Note: cwd passed to process_env() may not be a temporary object.
      //
      process_env pe (p, cwd, env ? env->vars : nullptr);

      // Finally, run the process.
      //
      // If the callback is specified, then intercept its call, appending the
      // stdout redirect to the arguments list, if present.
      //
      return process_run_callback (
        [&callback, &redir, redir_append] (const char* const args[], size_t n)
        {
          if (callback)
          {
            if (redir)
            {
              vector<const char*> elems (args, args + n);
              string rd ((redir_append ? ">>" : ">") + redir->string ());

              // Inject the redirect prior to the trailing NULL.
              //
              assert (n > 0);

              elems.insert (elems.begin () + n - 1, rd.c_str ());

              callback (elems.data (), elems.size ());
            }
            else
              callback (args, n);
          }
        },
        0                     /* stdin */,
        redir ? rd.get () : 1 /* stdout */,
        2                     /* stderr */,
        pe,
        args);
    }
  }
}
