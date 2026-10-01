// PullDecoderLoop.h -- xzip: drives a pull-style system decoder (libbz2, zlib) between 7-Zip streams

#ifndef ZIP7_INC_COMPRESS_PULL_DECODER_LOOP_H
#define ZIP7_INC_COMPRESS_PULL_DECODER_LOOP_H

#include "../../Common/MyBuffer2.h"

#include "../ICoder.h"
#include "../Common/StreamUtils.h"

namespace NCompress {
namespace NPullDecoder {

enum EStep
{
  kStepOk,
  kStepStreamEnd,
  kStepDataError,
  kStepMemError
};

/*
  Backend:
    bool Init();               fresh stream state; false on allocation failure
    void End();                safe to call when Init() failed or was never called
    EStep Step(const Byte *&in, size_t &inAvail, Byte *&out, size_t &outAvail);

  Returns ICompressCoder::Code's contract: S_OK, S_FALSE for any data error (damaged, truncated,
  or more or less output than *outSize), E_OUTOFMEMORY, or the first failing stream/progress HRESULT.
*/
template <class Backend>
HRESULT Decode(Backend &backend,
    ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *outSize, ICompressProgressInfo *progress,
    size_t inBufSize = (size_t)1 << 16, size_t outBufSize = (size_t)1 << 18)
{
  CMidBuffer inBuf;
  CMidBuffer outBuf;
  inBuf.Alloc(inBufSize);
  outBuf.Alloc(outBufSize);
  if (!inBuf.IsAllocated() || !outBuf.IsAllocated() || !backend.Init())
  {
    backend.End();
    return E_OUTOFMEMORY;
  }

  const Byte *in = inBuf;
  size_t inAvail = 0;
  bool inEnded = false;
  UInt64 inTotal = 0;
  UInt64 outTotal = 0;
  HRESULT res = S_OK;

  for (;;)
  {
    if (inAvail == 0 && !inEnded)
    {
      UInt32 got = 0;
      res = inStream->Read(inBuf, (UInt32)inBufSize, &got);
      if (res != S_OK)
        break;
      in = inBuf;
      inAvail = got;
      inTotal += got;
      inEnded = (got == 0);
    }

    Byte *out = outBuf;
    size_t outAvail = outBufSize;
    const size_t inBefore = inAvail;
    const EStep step = backend.Step(in, inAvail, out, outAvail);
    const size_t produced = outBufSize - outAvail;

    if (produced != 0)
    {
      if (outSize && produced > *outSize - outTotal)
      {
        res = S_FALSE;
        break;
      }
      res = WriteStream(outStream, outBuf, produced);
      if (res != S_OK)
        break;
      outTotal += produced;
    }

    if (step == kStepDataError)
    {
      res = S_FALSE;
      break;
    }
    if (step == kStepMemError)
    {
      res = E_OUTOFMEMORY;
      break;
    }
    // One stream per coder, as 7-Zip reads it: whatever follows the end is not decoded.
    if (step == kStepStreamEnd)
      break;
    if (produced == 0 && inAvail == inBefore && inEnded)
    {
      // The input is exhausted and the decoder cannot move: the stream was cut short.
      res = S_FALSE;
      break;
    }

    if (progress)
    {
      res = progress->SetRatioInfo(&inTotal, &outTotal);
      if (res != S_OK)
        break;
    }
  }

  backend.End();
  if (res == S_OK && outSize && outTotal != *outSize)
    res = S_FALSE;
  return res;
}

}}

#endif
