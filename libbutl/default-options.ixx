// file      : libbutl/default-options.ixx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

namespace butl
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

  template <typename O, typename AS>
  inline AS
  merge_default_arguments (const default_options<O>& def_ops,
                           const AS& cmd_args)
  {
    return merge_default_arguments (
      def_ops,
      cmd_args,
      [] (const default_options_entry<O>&, const AS&) {});
  }
}
