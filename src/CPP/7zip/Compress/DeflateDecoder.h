// DeflateDecoder.h -- xzip: 7z methods 040108 (Deflate, system zlib) and 040109 (Deflate64, infback9)

#ifndef ZIP7_INC_COMPRESS_DEFLATE_DECODER_H
#define ZIP7_INC_COMPRESS_DEFLATE_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NDeflate {

Z7_CLASS_IMP_COM_1(
  CDecoder
  , ICompressCoder
)
};

Z7_CLASS_IMP_COM_1(
  CDecoder64
  , ICompressCoder
)
};

}}

#endif
