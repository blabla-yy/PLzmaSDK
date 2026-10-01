// DeflateDecoder.cpp -- xzip: 7z methods 040108 (Deflate, system zlib) and 040109 (Deflate64, infback9)

#include "StdAfx.h"

#include <string.h>

#include <zlib.h>

#include "../../../C/infback9/infback9.h"

#include "../../Common/MyBuffer2.h"

#include "../Common/StreamUtils.h"

#include "DeflateDecoder.h"
#include "PullDecoderLoop.h"

namespace NCompress {
namespace NDeflate {

namespace {

struct CBackend
{
  z_stream Stream;
  bool Active;

  CBackend(): Active(false) {}

  bool Init()
  {
    memset(&Stream, 0, sizeof(Stream));
    // Negative window bits: a raw deflate stream, which is what a 7z coder holds.
    Active = (inflateInit2(&Stream, -MAX_WBITS) == Z_OK);
    return Active;
  }

  void End()
  {
    if (Active)
      inflateEnd(&Stream);
    Active = false;
  }

  NPullDecoder::EStep Step(const Byte *&in, size_t &inAvail, Byte *&out, size_t &outAvail)
  {
    Stream.next_in = (Bytef *)(const void *)in;
    Stream.avail_in = (uInt)inAvail;
    Stream.next_out = (Bytef *)(void *)out;
    Stream.avail_out = (uInt)outAvail;
    const int ret = inflate(&Stream, Z_NO_FLUSH);
    in = (const Byte *)(const void *)Stream.next_in;
    inAvail = Stream.avail_in;
    out = (Byte *)(void *)Stream.next_out;
    outAvail = Stream.avail_out;
    switch (ret)
    {
      // Z_BUF_ERROR only means no progress was possible; the loop decides whether that is a truncation.
      case Z_OK:
      case Z_BUF_ERROR: return NPullDecoder::kStepOk;
      case Z_STREAM_END: return NPullDecoder::kStepStreamEnd;
      case Z_MEM_ERROR: return NPullDecoder::kStepMemError;
      default: return NPullDecoder::kStepDataError;
    }
  }
};

const size_t kInBufSize = (size_t)1 << 16;
const size_t kWindowSize = (size_t)1 << 16;

struct CDeflate64Context
{
  ISequentialInStream *InStream;
  ISequentialOutStream *OutStream;
  const UInt64 *OutSize;
  ICompressProgressInfo *Progress;
  Byte *InBuf;
  UInt64 InTotal;
  UInt64 OutTotal;
  HRESULT Res;
};

unsigned Deflate64In(void *desc, z_const unsigned char **buf)
{
  CDeflate64Context *ctx = (CDeflate64Context *)desc;
  UInt32 got = 0;
  const HRESULT res = ctx->InStream->Read(ctx->InBuf, (UInt32)kInBufSize, &got);
  if (res != S_OK)
  {
    ctx->Res = res;
    return 0;
  }
  ctx->InTotal += got;
  *buf = ctx->InBuf;
  return got;
}

int Deflate64Out(void *desc, unsigned char *buf, unsigned len)
{
  CDeflate64Context *ctx = (CDeflate64Context *)desc;
  if (ctx->OutSize && len > *ctx->OutSize - ctx->OutTotal)
  {
    ctx->Res = S_FALSE;
    return 1;
  }
  HRESULT res = WriteStream(ctx->OutStream, buf, len);
  if (res == S_OK)
  {
    ctx->OutTotal += len;
    if (ctx->Progress)
      res = ctx->Progress->SetRatioInfo(&ctx->InTotal, &ctx->OutTotal);
  }
  if (res != S_OK)
  {
    ctx->Res = res;
    return 1;
  }
  return 0;
}

}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  CBackend backend;
  return NPullDecoder::Decode(backend, inStream, outStream, outSize, progress);
}

Z7_COM7F_IMF(CDecoder64::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  CMidBuffer inBuf;
  CMidBuffer window;
  inBuf.Alloc(kInBufSize);
  window.Alloc(kWindowSize);
  if (!inBuf.IsAllocated() || !window.IsAllocated())
    return E_OUTOFMEMORY;

  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  if (inflateBack9Init(&stream, window) != Z_OK)
    return E_OUTOFMEMORY;

  CDeflate64Context ctx = { inStream, outStream, outSize, progress, inBuf, 0, 0, S_OK };
  const int ret = inflateBack9(&stream, Deflate64In, &ctx, Deflate64Out, &ctx);
  inflateBack9End(&stream);

  if (ctx.Res != S_OK)
    return ctx.Res;
  if (ret == Z_STREAM_END)
    return (outSize && ctx.OutTotal != *outSize) ? S_FALSE : S_OK;
  if (ret == Z_MEM_ERROR)
    return E_OUTOFMEMORY;
  // Z_DATA_ERROR, or Z_BUF_ERROR: the input ran out before the final block.
  return S_FALSE;
}

}}
