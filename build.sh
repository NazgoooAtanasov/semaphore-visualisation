set -xe

CFLAGS="`pkg-config --cflags raylib` -Wall -Wextra -pedantic -g"
LIBS="`pkg-config --libs raylib`"

gcc $CFLAGS main.c $LIBS -o semv
