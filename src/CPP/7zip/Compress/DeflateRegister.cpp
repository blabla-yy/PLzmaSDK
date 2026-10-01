// DeflateRegister.cpp -- xzip: decoders only; PLzmaSDK does not write Deflate or Deflate64

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "DeflateDecoder.h"

namespace NCompress {
namespace NDeflate {

REGISTER_CODEC_CREATE(CreateDec, CDecoder())
REGISTER_CODEC_CREATE(CreateDec64, CDecoder64())

REGISTER_CODEC_2(Deflate, CreateDec, NULL, 0x40108, "Deflate")
REGISTER_CODEC_2(Deflate64, CreateDec64, NULL, 0x40109, "Deflate64")

}}

#if defined(LIBPLZMA_USING_REGISTRATORS)
uint64_t plzma_registrator_19(void) {
    return static_cast<uint64_t>(NCompress::NDeflate::g_CodecInfo_Deflate.Id);
}
uint64_t plzma_registrator_20(void) {
    return static_cast<uint64_t>(NCompress::NDeflate::g_CodecInfo_Deflate64.Id);
}
#endif
