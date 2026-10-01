// BZip2Decoder.cpp -- xzip: 7z method 040202, decoded by the system libbz2

#include "StdAfx.h"

#include <string.h>

#include <bzlib.h>

#include "BZip2Decoder.h"
#include "PullDecoderLoop.h"

namespace NCompress {
namespace NBZip2 {

namespace {

struct CBackend
{
  bz_stream Stream;
  bool Active;

  CBackend(): Active(false) {}

  bool Init()
  {
    memset(&Stream, 0, sizeof(Stream));
    Active = (BZ2_bzDecompressInit(&Stream, 0, 0) == BZ_OK);
    return Active;
  }

  void End()
  {
    if (Active)
      BZ2_bzDecompressEnd(&Stream);
    Active = false;
  }

  NPullDecoder::EStep Step(const Byte *&in, size_t &inAvail, Byte *&out, size_t &outAvail)
  {
    Stream.next_in = (char *)(const void *)in;
    Stream.avail_in = (unsigned)inAvail;
    Stream.next_out = (char *)(void *)out;
    Stream.avail_out = (unsigned)outAvail;
    const int ret = BZ2_bzDecompress(&Stream);
    in = (const Byte *)(const void *)Stream.next_in;
    inAvail = Stream.avail_in;
    out = (Byte *)(void *)Stream.next_out;
    outAvail = Stream.avail_out;
    switch (ret)
    {
      case BZ_OK: return NPullDecoder::kStepOk;
      case BZ_STREAM_END: return NPullDecoder::kStepStreamEnd;
      case BZ_MEM_ERROR: return NPullDecoder::kStepMemError;
      default: return NPullDecoder::kStepDataError;
    }
  }
};

}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  CBackend backend;
  return NPullDecoder::Decode(backend, inStream, outStream, outSize, progress);
}

}}
