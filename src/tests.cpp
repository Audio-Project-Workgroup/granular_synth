#define TEST_LAYER

#include "fft_test.cpp"

static void
testRun(void)
{
  TemporaryMemory scratch = arenaGetScratch(0, 0);

  String8List testLog = {};

  Buffer fftTestInputBuffer = gsReadEntireFile(DATA_PATH"test/fft_test_signal.float", scratch.arena);
  Buffer fftTestTargetBuffer = gsReadEntireFile(DATA_PATH"test/fft_test_target.float", scratch.arena);

  FloatBuffer fftTestInput = {};
  fftTestInput.count = fftTestInputBuffer.size / sizeof(r32);
  fftTestInput.vals = (r32*)fftTestInputBuffer.contents;
  
  ComplexBuffer fftTestTarget = {};
  fftTestTarget.count = fftTestTargetBuffer.size / (2 * sizeof(r32));
  fftTestTarget.reVals = arenaPushArray(scratch.arena, fftTestTarget.count, r32);
  fftTestTarget.imVals = arenaPushArray(scratch.arena, fftTestTarget.count, r32);
  {
    r32 *src = (r32*)fftTestTargetBuffer.contents;
    for(u32 i = 0; i < fftTestTarget.count; ++i)
      {
	fftTestTarget.reVals[i] = src[2*i];
	fftTestTarget.imVals[i] = src[2*i + 1];
      }
  }
  
  ComplexBuffer ifftTestInput = fftTestTarget;
  FloatBuffer ifftTestTarget = fftTestInput;

  profileBegin();  
  {
    for(u32 fftTestIdx = 0; fftTestIdx < ARRAY_COUNT(fftFunctions); ++fftTestIdx)
      {
	FFT_Function *fft = fftFunctions[fftTestIdx];
	FFT_TestResult fftResult = testFFTFunction(scratch.arena, fft, fftTestInput, fftTestTarget);
	if(fftResult.success)
	  {
	    stringListPush(scratch.arena, &testLog, STR8_LIT("fft function success"));
	  }
	else
	  {
	    String8 fftTestLogString = stringListJoin(scratch.arena, &fftResult.log, STR8_LIT("\n"));
	    stringListPush(scratch.arena, &testLog, fftTestLogString);
	  }
      }

    for(u32 ifftTestIdx = 0; ifftTestIdx < ARRAY_COUNT(ifftFunctions); ++ifftTestIdx)
      {
	IFFT_Function *ifft = ifftFunctions[ifftTestIdx];
	FFT_TestResult ifftResult = testIFFTFunction(scratch.arena, ifft, ifftTestInput, ifftTestTarget);
	if(ifftResult.success)
	  {
	    stringListPush(scratch.arena, &testLog, STR8_LIT("ifft function success"));
	  }
	else
	  {
	    String8 ifftTestLogString = stringListJoin(scratch.arena, &ifftResult.log, STR8_LIT("\n"));
	    stringListPush(scratch.arena, &testLog, ifftTestLogString);
	  }
      }
  }
  String8List profilerLog = profileEnd(scratch.arena);
  String8 profilerLogString = stringListJoin(scratch.arena, &profilerLog, STR8_LIT("\n"));

  stringListPush(scratch.arena, &testLog, profilerLogString);
  String8 testLogString = stringListJoin(scratch.arena, &testLog, STR8_LIT("\n\n"));

  Buffer fileBuffer = {};
  fileBuffer.size = testLogString.size;
  fileBuffer.contents = testLogString.str;
  gsWriteEntireFile(DATA_PATH"test/log.txt", fileBuffer);

  arenaReleaseScratch(scratch);
}
