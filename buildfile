# file      : buildfile
# license   : MIT; see accompanying LICENSE file

./: {*/ -build/ -upstream/}                                   \
    doc{INSTALL NEWS README} legal{LICENSE COPYRIGHT AUTHORS} \
    manifest

# Don't install tests or the INSTALL file.
#
tests/:          install = false
doc{INSTALL}@./: install = false
