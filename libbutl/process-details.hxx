// file      : libbutl/process-details.hxx -*- C++ -*-
// license   : MIT; see accompanying LICENSE file

#pragma once

#include <libbutl/mutex.hxx>

namespace butl
{
  // Mutex that is acquired to make a sequence of operations atomic in regards
  // to child process spawning. Must be aquired for exclusive access for child
  // process startup, and for shared access otherwise. Defined in process.cxx.
  //
  extern shared_mutex process_spawn_mutex;
}
