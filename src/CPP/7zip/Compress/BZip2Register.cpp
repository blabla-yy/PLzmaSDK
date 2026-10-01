// BZip2Register.cpp -- xzip: decoder only; PLzmaSDK does not write BZip2

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "BZip2Decoder.h"

namespace NCompress {
namespace NBZip2 {

REGISTER_CODEC_CREATE(CreateDec, CDecoder())

REGISTER_CODEC_2(BZip2, CreateDec, NULL, 0x40202, "BZip2")

}}

#if defined(LIBPLZMA_USING_REGISTRATORS)
uint64_t plzma_registrator_18(void) {
    return static_cast<uint64_t>(NCompress::NBZip2::g_CodecInfo_BZip2.Id);
}
#endif
