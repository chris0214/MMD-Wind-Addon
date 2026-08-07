# Version-Locked MMD ABI Headers

This directory contains the minimal interface and memory-layout declarations
required to build WindTool for MikuMikuDance 9.31 x64. They are maintained as
part of WindTool and are covered by the repository MIT license.

The headers contain declarations, constants and layouts only. They do not
contain MikuMikuDance or MikuMikuEffect executable code, assets or binaries.

These declarations are version-specific. Do not reuse them for another MMD
build without independently verifying every address, size, offset and calling
convention, then adding a separate compatibility profile and tests.
