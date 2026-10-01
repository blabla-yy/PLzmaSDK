# infback9 (Deflate64 decoder)

`infback9.c`, `infback9.h`, `inftree9.c`, `inftree9.h`, `inflate9.h`, `inffix9.h` are copied unmodified
from zlib v1.3.1 `contrib/infback9` (https://github.com/madler/zlib/tree/v1.3.1/contrib/infback9),
Copyright (C) 1995-2008 Mark Adler, under the zlib license (see the notice in `zlib.h`).

`zutil.h` is not zlib's: it supplies the handful of zlib-internal macros these files expect, on top
of the system `zlib.h`. Used by `CPP/7zip/Compress/DeflateDecoder.cpp` for 7z method 040109.
