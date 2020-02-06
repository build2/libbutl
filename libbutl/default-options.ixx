// file      : libbutl/default-options.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

LIBBUTL_MODEXPORT namespace butl //@@ MOD Clang needs this for some reason.
{
  template <typename O>
  inline O
  merge_default_options (const default_options<O>& def_ops, const O& cmd_ops)
  {
    return merge_default_options (
      def_ops,
      cmd_ops,
      [] (const default_options_entry<O>&, const O&) {});
  }
}
