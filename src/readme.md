# Inexor

This directory contains the sources – and libraries - for
the inexor core application written in C++.

At the moment this contains basically Cube 2: Sauerbraten
plus a few modifications and additions.

## Build

Not in this directory.
  
  ```shell
  cd ..
  git submodule init
  git submodule sync
  git submodule update
  mkdir build
  cd build
  cmake ..
  make run_tests
  make install
  cd ..
  ./inexor_unix
  ```

but see the [wiki](https://github.com/inexor-game/code/wiki/Build)
this for full instructions.

## Tree

**client** – Build config for the client with
graphics and sound features.

**server** – Build config for the server. The client can act
as a server too, but this binary lacks the graphics and
sound features and has therefor no dependency on SDL,
openal, opengl or X11

**test** – Test cases for inexor and the build config.

**util** – Utilities module: Small functions, little helpers
that fit nowhere else.

**net** – Network library module and abstraction for message
based connections. Note that this does NOT contain enet or
the actual ingame network stuff, but a generic library that
is already used by the rpc module. The build config pulls
asio.

**rpc** – Implementation for the Inexor RPC server and the
protoc files that describes the RPC protocol. The build
config pulls google protobuf.

**shared** – Basically the sauerbraten standard library.
Contains headers that include EVERYTHING, varous data
structures RNG and a tiny crypto lib. Note that this is
heavily intertwined with the other legacy modules engine/
and fpsgame/.  
**LEGALCY CODE: Use the standard library please; Patch, but
do not add new features here.**

**engine** – Various physics, graphics, cubescript, cube UI,
some engine related networking. Also contains the main
functions for the server and client. Note that this is
heavily intertwined with the other legacy modules shared/
and fpsgame/.  
**LEGALCY CODE: Use the standard library please; Patch, but
do not add new features here.**

**fpsgame** – Sauerbraten game logic plus a few extensions.
Note that this is heavily intertwined with the other legacy
modules shared/ and fpsgame/.  
**LEGALCY CODE: Use the standard library please; Patch, but
do not add new features here.**

**platform_windows** – Submodule that contains libraries and
build configs for windows/VS/mingw.

**platform_osx** – Submodule that contains libs and build
configs for Mac OS X. Currently unused.

**libraries** – Build config and submodules for several
libraries: muparser, enet, googletest.

