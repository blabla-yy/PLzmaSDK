// BZip2Decoder.h -- xzip: 7z method 040202, decoded by the system libbz2

#ifndef ZIP7_INC_COMPRESS_BZIP2_DECODER_H
#define ZIP7_INC_COMPRESS_BZIP2_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

namespace NCompress {
namespace NBZip2 {

Z7_CLASS_IMP_COM_1(
  CDecoder
  , ICompressCoder
)
};

}}

#endif
