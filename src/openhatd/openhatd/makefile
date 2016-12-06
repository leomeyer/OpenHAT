# openhatd makefile
# Creates a release file that includes plugins.
# Pass DEBUG=1 to enable a debug build.

MAKEFILE = $(firstword $(MAKEFILE_LIST))

# build specific settings

# DEBUG = 1 will produce very large binaries!
DEBUG = 0

# The compiler to be used.
CC = g++

# Compiler flags.
CFLAGS = -static-libstdc++

# include POCO libraries with package
INCLUDE_POCO_SO = 1

include common.mk


