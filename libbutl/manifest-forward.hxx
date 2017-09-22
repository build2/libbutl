// file      : libbutl/manifest-forward.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#pragma once

namespace butl
{
  class manifest_parser;
  class manifest_serializer;
  class manifest_name_value;

  // The way manifest implementation should proceed when unknown value name is
  // encountered during parsing.
  //
  enum class unknown_name_mode
  {
    skip,
    stop,
    fail
  };
}
