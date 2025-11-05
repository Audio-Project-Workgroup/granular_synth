#define PV_WINDOW_SAMPLE_COUNT 512
#define PV_ANALYSIS_HOP_DIVISOR 4
STATIC_ASSERT(IS_POWER_OF_2(PV_WINDOW_SAMPLE_COUNT), pvWindowSampleCountCheck);

struct PhaseVocoder
{
  AudioRingBuffer *inputBuffer;
  AudioRingBuffer *outputBuffer;

  // NOTE: persistent arrays
  r32 *freq;
  r32 *window;

  // NOTE: arrays that are modified each loop
  r32 *phaseInL[2];
  r32 *phaseInR[2];
  r32 *phaseAdjL[2];
  r32 *phaseAdjR[2];
  r32 *magL;
  r32 *magR;
  ComplexBuffer lastModifiedSpectrumL;
  ComplexBuffer lastModifiedSpectrumR;
  FloatBuffer lastOutputSignalL;
  FloatBuffer lastOutputSignalR;
  TemporaryMemory lastScratch;
  u32 currentMemoryIndex;
};

static inline PhaseVocoder
initializePhaseVocoder(Arena *arena, AudioRingBuffer *inputBuffer, AudioRingBuffer *outputBuffer)
{
  u32 pvArrayCount = PV_WINDOW_SAMPLE_COUNT / 2;
  r32 *freq = arenaPushArray(arena, pvArrayCount, r32);  
  for(u32 binIndex = 0; binIndex < pvArrayCount; ++binIndex)
  {
    freq[binIndex] = GS_TAU * (r32)binIndex / (r32)(pvArrayCount + 1);
  }

  // TODO: since the window is symmetric, we could only store half the window,
  // and apply the window to each half of the input signal in separate loops
  r32 *window = arenaPushArray(arena, PV_WINDOW_SAMPLE_COUNT, r32);
  {
    r32 amp = 1.f / (r32)PV_ANALYSIS_HOP_DIVISOR;
    for(u32 binIndex = 0; binIndex < PV_WINDOW_SAMPLE_COUNT; ++binIndex)
    {
      // NOTE: hann window (for now)
      window[binIndex] = amp*(1.f - gsCos((GS_TAU * (r32)binIndex)/(r32)PV_WINDOW_SAMPLE_COUNT));
    }
  }
  
  r32 *phaseInL0 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseInR0 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseAdjL0 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseAdjR0 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *magL = arenaPushArray(arena, pvArrayCount, r32);
  r32 *magR = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseInL1 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseInR1 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseAdjL1 = arenaPushArray(arena, pvArrayCount, r32);
  r32 *phaseAdjR1 = arenaPushArray(arena, pvArrayCount, r32);

  PhaseVocoder result = {};
  result.inputBuffer = inputBuffer;
  result.outputBuffer = outputBuffer;
  result.freq = freq;
  result.phaseInL[0] = phaseInL0;
  result.phaseInL[1] = phaseInL1;
  result.phaseInR[0] = phaseInR0;
  result.phaseInR[1] = phaseInR1;
  result.phaseAdjL[0] = phaseAdjL0;
  result.phaseAdjL[1] = phaseAdjL1;
  result.phaseAdjR[0] = phaseAdjR0;
  result.phaseAdjR[1] = phaseAdjR1;
  result.magL = magL;
  result.magR = magR;
  return(result);
}

static inline void
phaseVocoderProcess(PhaseVocoder *pv, r32 *outputL, r32 *outputR, u32 processBlockSamples,
		    r32 timeStretchFactor = 1.f)
{
  u32 inHopSize = PV_WINDOW_SAMPLE_COUNT / PV_ANALYSIS_HOP_DIVISOR;
  u32 outHopSize = (u32)(timeStretchFactor * (r32)inHopSize);

  // TODO: check if there are enough samples in the input buffer to fill the
  // output buffer with the required amount of samples, given the time stretch
  // factor
  u32 samplesInOutputBuffer = getAudioRingBufferOffset(pv->outputBuffer);
  u32 windowsToWrite = 1 + (processBlockSamples - samplesInOutputBuffer) / outHopSize;
  u32 samplesToWrite = windowsToWrite * PV_WINDOW_SAMPLE_COUNT;
  
  u32 currentMemoryIndex = pv->currentMemoryIndex;
  u32 lastMemoryIndex = !currentMemoryIndex;
  r32 *phaseInL = pv->phaseInL[currentMemoryIndex];
  r32 *phaseInR = pv->phaseInR[currentMemoryIndex];
  r32 *phaseAdjL = pv->phaseAdjL[currentMemoryIndex];
  r32 *phaseAdjR = pv->phaseAdjR[currentMemoryIndex];
  r32 *lastPhaseInL = pv->phaseInL[lastMemoryIndex];
  r32 *lastPhaseInR = pv->phaseInR[lastMemoryIndex];
  r32 *lastPhaseAdjL = pv->phaseAdjL[lastMemoryIndex];
  r32 *lastPhaseAdjR = pv->phaseAdjR[lastMemoryIndex];
  r32 *magL = pv->magL;
  r32 *magR = pv->magR;
  r32 *freq = pv->freq;
  r32 *window = pv->window;
  for(u32 windowIndex = 0; windowIndex < windowsToWrite; ++windowIndex)
  {
    TemporaryMemory scratch = arenaGetScratch(&pv->lastScratch.arena, 1);

    // NOTE: read samples from input
    r32 *pvInL = arenaPushArray(scratch.arena, PV_WINDOW_SAMPLE_COUNT, r32);
    r32 *pvInR = arenaPushArray(scratch.arena, PV_WINDOW_SAMPLE_COUNT, r32);
    readSamplesFromAudioRingBuffer(pv->inputBuffer, pvInL, pvInR, PV_WINDOW_SAMPLE_COUNT);

    // TODO: apply window
    for(u32 binIndex = 0; binIndex < PV_WINDOW_SAMPLE_COUNT; ++binIndex)
    {
      pvInL[binIndex] *= window[binIndex];
      pvInR[binIndex] *= window[binIndex];
    }

    // NOTE: compute spectrum
    FloatBuffer fftInL = bufferMakeFloat(pvInL, PV_WINDOW_SAMPLE_COUNT);
    ComplexBuffer fftL = fft(scratch.arena, fftInL);

    FloatBuffer fftInR = bufferMakeFloat(pvInR, PV_WINDOW_SAMPLE_COUNT);
    ComplexBuffer fftR = fft(scratch.arena, fftInR);

    // NOTE: compute magnitudes and phases
    for(u32 binIndex = 0; binIndex < PV_WINDOW_SAMPLE_COUNT / 2; ++binIndex)
    {
      r32 reL = fftL.reVals[binIndex];
      r32 imL = fftL.imVals[binIndex];
      r32 reR = fftR.reVals[binIndex];
      r32 imR = fftR.imVals[binIndex];
      magL[binIndex] = gsSqrt(reL*reL + imL*imL);
      magR[binIndex] = gsSqrt(reR*reR + imR*imR);

      r32 binPhaseL = gsAtan(reL/imL);
      r32 binPhaseR = gsAtan(reR/imR);
      r32 lastBinPhaseL = lastPhaseInL[binIndex];
      r32 lastBinPhaseR = lastPhaseInR[binIndex];

      r32 binFreq = freq[binIndex];
      r32 freqEstL =
	(binFreq +
	 principalPhaseAngle(binPhaseL - lastBinPhaseL - binFreq*(r32)inHopSize)/(r32)inHopSize);
      r32 freqEstR =
	(binFreq +
	 principalPhaseAngle(binPhaseL - lastBinPhaseR - binFreq*(r32)inHopSize)/(r32)inHopSize);

      phaseInL[binIndex] = binPhaseL;
      phaseInR[binIndex] = binPhaseR;
      phaseAdjL[binIndex] = freqEstL * (r32)outHopSize;
      phaseAdjR[binIndex] = freqEstR * (r32)outHopSize;
    }

    // NOTE: modify spectrum with adjusted phase
    for(u32 binIndex = 0; binIndex < PV_WINDOW_SAMPLE_COUNT / 2; ++binIndex)
    {
      r32 binMagL = magL[binIndex];
      r32 binMagR = magR[binIndex];
      r32 binPhaseL = phaseAdjL[binIndex];
      r32 binPhaseR = phaseAdjR[binIndex];

      r32 binReL = binMagL*gsCos(binPhaseL);
      r32 binImL = binMagL*gsSin(binPhaseL);
      r32 binReR = binMagR*gsCos(binPhaseR);
      r32 binImR = binMagR*gsSin(binPhaseR);

      fftL.reVals[binIndex] = binReL;
      fftL.imVals[binIndex] = binImL;
      fftR.reVals[binIndex] = binReR;
      fftR.imVals[binIndex] = binImR;
      // TODO: conjugate-symmetry
    }

    // NOTE: synthesize output samples from modified spectrum
    FloatBuffer outL = ifft(scratch.arena, fftL);
    FloatBuffer outR = ifft(scratch.arena, fftR);

    // TODO: write output samples to output buffer
    {
      r32 *destL = outputL + windowIndex*outHopSize;
      r32 *destR = outputR + windowIndex*outHopSize;
      r32 *srcL = outL.vals;
      r32 *srcR = outR.vals;
      for(u32 binIndex = 0; binIndex < PV_WINDOW_SAMPLE_COUNT; ++binIndex)
      {
	destL[binIndex] += srcL[binIndex];
	destR[binIndex] += srcR[binIndex];
      }
    }
    // NOTE: update pointers/index
    r32 *phaseInLTemp = phaseInL;
    phaseInL = lastPhaseInL;
    lastPhaseInL = phaseInLTemp;

    r32 *phaseInRTemp = phaseInR;
    phaseInR = lastPhaseInR;
    lastPhaseInR = phaseInRTemp;

    r32 *phaseAdjLTemp = phaseAdjL;
    phaseAdjL = lastPhaseAdjL;
    lastPhaseAdjL = phaseAdjLTemp;

    r32 *phaseAdjRTemp = phaseAdjR;
    phaseAdjR = lastPhaseAdjR;
    lastPhaseAdjR = phaseAdjRTemp;

    u32 indexTemp = currentMemoryIndex;
    currentMemoryIndex = lastMemoryIndex;
    lastMemoryIndex = indexTemp;
    pv->currentMemoryIndex = currentMemoryIndex;

    // NOTE: update buffers
    pv->lastModifiedSpectrumL = fftL;
    pv->lastModifiedSpectrumR = fftR;
    pv->lastOutputSignalL = outL;
    pv->lastOutputSignalR = outR;

    // NOTE: update scratch
    arenaReleaseScratch(pv->lastScratch);
    pv->lastScratch = scratch;
  }

  writeSamplesToAudioRingBuffer(pv->outputBuffer, outputL, outputR, processBlockSamples);
}
