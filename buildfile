# file      : buildfile
# copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
# license   : MIT; see accompanying LICENSE file

d = butl/ tests/
./: $d doc{INSTALL LICENSE NEWS README version} file{manifest}
include $d

# Don't install tests or the INSTALL file.
#
dir{tests/}: install = false
doc{INSTALL}@./: install = false
