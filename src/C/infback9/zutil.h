/* zutil.h -- xzip: the few zlib internals infback9 uses.
 *
 * infback9 is zlib's contrib/infback9 (zlib v1.3.1, zlib license, unmodified). It is written to be
 * built inside the zlib tree and includes zlib's private zutil.h, which the Apple SDKs do not ship.
 * This stands in for exactly what infback9.c and inftree9.c reference, on top of the public zlib.h.
 */

#ifndef XZIP_INFBACK9_ZUTIL_H
#define XZIP_INFBACK9_ZUTIL_H

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define local static

#define ZALLOC(strm, items, size) (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr) (*((strm)->zfree))((strm)->opaque, (voidpf)(addr))
#define zmemcpy memcpy

#define Tracev(x)
#define Tracevv(x)

static inline voidpf zcalloc(voidpf opaque, unsigned items, unsigned size) {
    (void)opaque;
    return calloc(items, size);
}

static inline void zcfree(voidpf opaque, voidpf ptr) {
    (void)opaque;
    free(ptr);
}

#endif
