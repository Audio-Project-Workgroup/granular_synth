struct AudioRingBuffer
{
  r32 *samples[2];
  u32 capacity;
  u32 writeIndex;
  u32 readIndex;
};

struct SharedRingBuffer
{
  u8 *entries;
  u32 capacity;
  volatile u32 writeIndex;
  volatile u32 readIndex;
  volatile u32 queuedCount;
};

// NOTE: we can't use this for ring buffer reading/writing everywhere because juce doesn't give us simd-aligned
//       input buffers. Maybe it's on them, maybe it's on daw people. But come on. SSE came out over 25 years ago.
//       What gives? Aligning allocated memory is super easy.
inline void
copyFloatArrayWide(r32 *destInit, r32 *srcInit, u32 count)
{
  u32 simdWidth = 4;
  u32 mask = 0xFFFFFFFF;
  u32 maskInv = ~mask;
  r32 maskF = *(r32 *)&mask;
  r32 maskInvF = *(r32 *)&maskInv;

  r32 *dest = destInit;
  r32 *src = srcInit;
  ASSERT(!((usz)dest & (simdWidth - 1)));
  ASSERT(!((usz)src & (simdWidth - 1)));
  
  for(u32 i = 0; i < count; i += simdWidth)
    {
      WideFloat storeMask = wideSetConstantFloats(maskF);
      WideFloat storeMaskInv = wideSetConstantFloats(maskInvF);
      for(u32 lane = 0; lane < simdWidth; ++lane)
	{
	  if((i + lane) >= count)
	    {
	      wideSetLaneFloats(&storeMask, maskInvF, lane);
	      wideSetLaneFloats(&storeMaskInv, maskF, lane);
	    }
	}
      
      WideFloat srcW = wideLoadFloats(src);
      WideFloat destW = wideLoadFloats(dest);
      WideFloat storeW = wideAddFloats(wideMaskFloats(srcW, storeMask),
				       wideMaskFloats(destW, storeMaskInv));
      wideStoreFloats(dest, storeW);

      dest += simdWidth;
      src += simdWidth;
    }
}

inline void
writeSamplesToAudioRingBuffer(AudioRingBuffer *rb, r32 *srcL, r32 *srcR, u32 count, bool increment = true)
{
  u32 samplesToBufferEnd = rb->capacity - rb->writeIndex;
  u32 samplesToCopy = MIN(samplesToBufferEnd, count);
  /* COPY_ARRAY(rb->samples[0] + rb->writeIndex, srcL, samplesToCopy, r32); */
  /* COPY_ARRAY(rb->samples[1] + rb->writeIndex, srcR, samplesToCopy, r32); */
  copyFloatArrayWide(rb->samples[0] + rb->writeIndex, srcL, samplesToCopy);
  copyFloatArrayWide(rb->samples[1] + rb->writeIndex, srcR, samplesToCopy);

  if(samplesToCopy < count)
    {
      u32 samplesRemaining = count - samplesToCopy;
      /* COPY_ARRAY(rb->samples[0], srcL + samplesToCopy, samplesRemaining, r32); */
      /* COPY_ARRAY(rb->samples[1], srcR + samplesToCopy, samplesRemaining, r32); */
      copyFloatArrayWide(rb->samples[0], srcL + samplesToCopy, samplesRemaining);
      copyFloatArrayWide(rb->samples[1], srcR + samplesToCopy, samplesRemaining);
    }

  if(increment)
    {
      rb->writeIndex = (rb->writeIndex + count) % rb->capacity;
    }
}

inline void
readSamplesFromAudioRingBuffer(AudioRingBuffer *rb, r32 *destL, r32 *destR, u32 count, bool increment = true)
{
  u32 samplesToBufferEnd = rb->capacity - rb->readIndex;
  u32 samplesToCopy = MIN(samplesToBufferEnd, count);
  /* COPY_ARRAY(destL, rb->samples[0] + rb->readIndex, samplesToCopy, r32); */
  /* COPY_ARRAY(destR, rb->samples[1] + rb->readIndex, samplesToCopy, r32); */
  copyFloatArrayWide(destL, rb->samples[0] + rb->readIndex, samplesToCopy);
  copyFloatArrayWide(destR, rb->samples[1] + rb->readIndex, samplesToCopy);

  if(samplesToCopy < count)
    {
      u32 samplesRemaining = count - samplesToCopy;
      /* COPY_ARRAY(destL + samplesToCopy, rb->samples[0], samplesRemaining, r32); */
      /* COPY_ARRAY(destR + samplesToCopy, rb->samples[1], samplesRemaining, r32); */
      copyFloatArrayWide(destL + samplesToCopy, rb->samples[0], samplesRemaining);
      copyFloatArrayWide(destR + samplesToCopy, rb->samples[1], samplesRemaining);
    }

  if(increment)
    {
      rb->readIndex = (rb->readIndex + count) % rb->capacity;
    }
}

inline void
queueSharedRingBufferEntry_(SharedRingBuffer *rb, void *entry, usz entrySize)
{  
  u8 *newEntry = rb->entries + entrySize*rb->writeIndex;
  COPY_SIZE(newEntry, entry, entrySize);

  rb->writeIndex = (rb->writeIndex + 1) % rb->capacity;
  u32 queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
  while(globalPlatform.atomicCompareAndSwap(&rb->queuedCount, queuedCount, queuedCount + 1) != queuedCount)
    {
      queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
    }
}

#define dequeueSharedRingBufferEntry(rb, type) (type *)dequeueSharedRingBufferEntry_((rb), sizeof(type))
#define dequeueAllSharedRingBufferEntries(dest, rb, type) dequeueAllSharedRingBufferEntry_(dest, (rb), sizeof(type))

inline u8 *
dequeueSharedRingBufferEntry_(SharedRingBuffer *rb, usz entrySize)
{
  u8 *entry = rb->entries + entrySize*rb->readIndex;

  rb->readIndex = (rb->readIndex + 1) % rb->capacity;
  u32 queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
  ASSERT(queuedCount);
  while(globalPlatform.atomicCompareAndSwap(&rb->queuedCount, queuedCount, queuedCount - 1) != queuedCount)
    {
      queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
    }

  return(entry);
}

inline void
dequeueAllSharedRingBufferEntries_(void *destInit, SharedRingBuffer *rb, usz entrySize)
{
  u8 *dest = (u8 *)destInit;
  u32 queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
  for(u32 entryIndex = 0; entryIndex < queuedCount; ++entryIndex)
    {
      u32 readIndex = (rb->readIndex + entryIndex) % rb->capacity;
      u8 *entry = rb->entries + readIndex*entrySize;
      COPY_SIZE(dest, entry, entrySize);

      dest += entrySize;
    }

  rb->readIndex = (rb->readIndex + queuedCount) % rb->capacity;
  u32 oldQueuedCount = queuedCount;
  while(globalPlatform. atomicCompareAndSwap(&rb->queuedCount,
					     queuedCount, queuedCount - oldQueuedCount) != queuedCount)
    {
      queuedCount = globalPlatform.atomicLoad(&rb->queuedCount);
    }
}
