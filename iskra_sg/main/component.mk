#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

EXTRA_COMPONENT_DIRS := cryptoauthlib

COMPONENT_ADD_INCLUDEDIRS += ../components/cryptoauthlib/lib/

COMPONENT_EMBED_FILES += upload_script.html