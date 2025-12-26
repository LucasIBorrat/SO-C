# Libraries
LIBS=commons pthread readline m crypto

# Custom libraries' paths
SHARED_LIBPATHS=
STATIC_LIBPATHS=

# Compiler flags
CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always -D_POSIX_C_SOURCE=200809L
CRELEASE=-O3 -Wall -DNDEBUG -D_POSIX_C_SOURCE=200809L

# Source files (*.c) to be excluded from tests compilation
TEST_EXCLUDE=src/main.c
