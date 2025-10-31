struct FFT_TestResult
{
  b32 success;
  usz cycleCount;
  String8List log;
};

static FFT_TestResult
testFFTFunction(Arena *arena, FFT_Function *fft, FloatBuffer input, ComplexBuffer target)
{
  ComplexBuffer fftResult = fft(arena, input);

  String8List log = {};
  
  b32 success = target.count == fftResult.count;
  if(success)
    {
      r32 tol = 1e-3;
      for(u32 i = 0; i < target.count && success; ++i)
	{
	  r32 resultRe = fftResult.reVals[i];
	  r32 resultIm = fftResult.imVals[i];
	  r32 targetRe = target.reVals[i];
	  r32 targetIm = target.imVals[i];

	  resultMagSq = resultRe*resultRe + resultIm*resultIm;
	  targetMagSq = targetRe*targetRe + targetIm*targetIm;
	  success = fabsf(resultMagSq - targetMagSq) < tol;
	}
    }
  else
    {
      stringListPushFormat(arena, &log,
			   "ERROR: result and target lengths differ:\n"
			   "       result.count = %u\n"
			   "       target.count = %u\n",
			   fftResult.count,
			   target.count);
    }

  FFT_TestResult result = {};
  result.success = success;
  result.cyclesCount = 0; // TODO: profile
  result.log = log;
  return(result);
}

static FFT_TestResult
testIFFTFunction(Arena *arena, IFFT_Function *ifft, ComplexBuffer input, FloatBuffer target)
{
  FloatBuffer ifftResult = ifft(arena, input);

  String8List log = {};

  b32 result = target.count == ifftResult.count;
  if(result)
    {
      r32 tol = 1e-6;
      for(u32 i = 0; i < target.count; ++i)
	{
	  r32 resultSample = ifftResult.vals[i];
	  r32 targetSample = target.vals[i];
	  success = fabsf(resultSample - targetSample) < tol;
	}
    }
  else
    {
      stringListPushFormat(arena, &log,
			   "ERROR: result and target lengths differ:\n"
			   "       result.count = %u\n"
			   "       target.count = %u\n",
			   ifftResult.count,
			   target.count);
    }

  FFT_TestResult result = {};
  result.success = success;
  result.cycleCount = 0; // TODO: profile
  result.log = log;
  return(result);
}

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
