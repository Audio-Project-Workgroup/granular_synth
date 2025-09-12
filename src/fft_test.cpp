static bool
fftTest(Arena *allocator)
{
  u32 testSignalLength = 16;
  r32 freq = 4;
  r32 nFreq = freq*GS_TAU/(r32)testSignalLength;
  r32 *testSignal = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4*sizeof(r32)));
  for(u32 i = 0; i < testSignalLength; ++i)
    {
      testSignal[i] = Sin((r32)i*nFreq);
    }

  r32 *testDestRe = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4*sizeof(r32)));
  r32 *testDestIm = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4*sizeof(r32)));
  c64 *testDestComplex = arenaPushArray(allocator, testSignalLength, c64, arenaFlagsZeroAlign(4*sizeof(r32)));

  fft(testDestComplex, testSignal, testSignalLength);
  //fft_real_permute(testDestRe, testDestIm, testSignal, testSignalLength);
  fft_real_noPermute(testDestRe, testDestIm, testSignal, testSignalLength);

  r32 *testIfftResult = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4*sizeof(r32)));
  r32 *testIfftTemp = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4*sizeof(r32)));
  
  //ifft(testIfftResult, testIfftTemp, testDestComplex, testSignalLength);
  //ifft_real_permute(testIfftResult, testIfftTemp, testDestRe, testDestIm, testSignalLength);
  ifft_real_noPermute(testIfftResult, testIfftTemp, testDestRe, testDestIm, testSignalLength);

  r32 maxErr = 0.f;
  r32 errTol = 0.001f;
  for(u32 i = 0; i < testSignalLength; ++i)
    {
      r32 err = Abs(testSignal[i] - testIfftResult[i]);
      maxErr = MAX(maxErr, err);
    }

  bool result = (maxErr <= errTol);
  return(result);
}
