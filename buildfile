# file      : buildfile
# copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

d = butl/ tests/
./: $d doc{LICENSE version} file{manifest}
include $d
