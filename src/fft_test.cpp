static bool
fftTest(Arena *allocator)
{
  u32 testSignalLength = 16;
  r32 freq = 4;
  r32 nFreq = freq*M_TAU/(r32)testSignalLength;
  r32 *testSignal = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4));
  for(u32 i = 0; i < testSignalLength; ++i)
    {
      testSignal[i] = Sin((r32)i*nFreq);
    }

  r32 *testDestRe = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4));
  r32 *testDestIm = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4));
  c64 *testDestComplex = arenaPushArray(allocator, testSignalLength, c64, arenaFlagsZeroAlign(4));

  fft(testDestComplex, testSignal, testSignalLength);
  fft_real_permute(testDestRe, testDestIm, testSignal, testSignalLength);  

  r32 *testIfftResult = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4));
  r32 *testIfftTemp = arenaPushArray(allocator, testSignalLength, r32, arenaFlagsZeroAlign(4));
  ifft_real_permute(testIfftResult, testIfftTemp, testDestRe, testDestIm, testSignalLength);

  bool result = true;
  for(u32 i = 0; result && i < testSignalLength; ++i)
    {
      result = (testSignal[i] == testIfftResult[i]);
    }

  return(result);
}
