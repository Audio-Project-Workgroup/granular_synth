// TODO: investigate different fft radices

#define REAL_FFT_FUNCTION(name) void (name)(r32 *destRe, r32 *destIm, r32 *src, u32 length)
#define REAL_IFFT_FUNCTION(name) void (name)(r32 *dest, r32 *destImTemp, r32 *srcRe, r32 *srcIm, u32 length)

static REAL_FFT_FUNCTION(fft_real_noPermute);
static REAL_IFFT_FUNCTION(ifft_real_noPermute);

static REAL_FFT_FUNCTION(fft_real_permute);
static REAL_IFFT_FUNCTION(ifft_real_permute);

static REAL_FFT_FUNCTION(fft_real_permute)
{
  ASSERT(isPowerOf2(length));
  u32 logLength = log2(length);
  u32 simdWidth = 4;

  // TODO: simd-ize this
  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      destRe[index] = src[reversedIndex];
    }

  u32 mask = 0xFFFFFFFF;
  //r32 maskF = *(r32 *)&mask;

  for(u32 m = 2; m <= length; m <<= 1)
    {
      r32 angle = -GS_TAU/(r32)m;
      
      // TODO: make trig ops fast
      c64 wm = C64Polar(1.f, angle);
      // TODO: maybe do these multiplies wide as well?
      c64 wmSq = wm*wm;
      c64 wmCu = wm*wmSq;
      c64 wmQu = wm*wmCu;
      WideFloat wmQuRe = wideSetConstantFloats(wmQu.re);
      WideFloat wmQuIm = wideSetConstantFloats(wmQu.im);

      for(u32 k = 0; k < length; k += m)
	{	  
	  WideFloat wRe = wideSetFloats(1.f, wm.re, wmSq.re, wmCu.re);
	  WideFloat wIm = wideSetFloats(0.f, wm.im, wmSq.im, wmCu.im);
	  	      
	  r32 *dest0Re = destRe + k;
	  r32 *dest0Im = destIm + k;
	  r32 *dest1Re = destRe + k + m/2;
	  r32 *dest1Im = destIm + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      WideInt storeMask = wideSetConstantInts(0);
	      //WideFloat storeMaskInv = wideSetConstantFloats(0.f);
	      for(u32 lane = 0; lane < simdWidth; ++lane)
		{
		  //r32 val = (j + lane < m/2) ? maskF : 0;
		  //r32 valInv = (j + lane < m/2) ? 0 : maskF;
		  u32 val = (j + lane < m/2) ? mask : 0;
		  
		  wideSetLaneInts(&storeMask, val, lane);
		  //wideSetLaneFloats(&storeMaskInv, valInv, lane);
		}

	      // NOTE: loads
	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);

	      // NOTE: butterflies and twiddles
	      WideFloat productRe = wideSubFloats(wideMulFloats(wRe, in1Re),
						  wideMulFloats(wIm, in1Im));
	      WideFloat productIm = wideAddFloats(wideMulFloats(wRe, in1Im),
						  wideMulFloats(wIm, in1Re));
	      WideFloat out0Re = wideAddFloats(in0Re, productRe);
	      WideFloat out0Im = wideAddFloats(in0Im, productIm);
	      WideFloat out1Re = wideSubFloats(in0Re, productRe);
	      WideFloat out1Im = wideSubFloats(in0Im, productIm);

	      // NOTE: mask out what not to store
	      /* WideFloat store0Re = wideAddFloats(wideMaskFloats(out0Re, storeMask), */
	      /* 					 wideMaskFloats(in0Re, storeMaskInv)); */
	      /* WideFloat store0Im = wideAddFloats(wideMaskFloats(out0Im, storeMask), */
	      /* 					 wideMaskFloats(in0Im, storeMaskInv)); */
	      /* WideFloat store1Re = wideAddFloats(wideMaskFloats(out1Re, storeMask), */
	      /* 					 wideMaskFloats(in1Re, storeMaskInv)); */
	      /* WideFloat store1Im = wideAddFloats(wideMaskFloats(out1Im, storeMask), */
	      /* 					 wideMaskFloats(in1Im, storeMaskInv)); */

	      WideFloat store0Re = wideMaskFloats(out0Re, in0Re, storeMask);
	      WideFloat store0Im = wideMaskFloats(out0Im, in0Im, storeMask);
	      WideFloat store1Re = wideMaskFloats(out1Re, in1Re, storeMask);
	      WideFloat store1Im = wideMaskFloats(out1Im, in1Im, storeMask);

	      // NOTE: do the store
	      wideStoreFloats(dest0Re, store0Re);
	      wideStoreFloats(dest0Im, store0Im);
	      wideStoreFloats(dest1Re, store1Re);
	      wideStoreFloats(dest1Im, store1Im);

	      WideFloat wReOld = wRe;
	      WideFloat wImOld = wIm;
	      wRe = wideSubFloats(wideMulFloats(wReOld, wmQuRe),
				  wideMulFloats(wImOld, wmQuIm));
	      wIm = wideAddFloats(wideMulFloats(wReOld, wmQuIm),
				  wideMulFloats(wImOld, wmQuRe));

	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static REAL_IFFT_FUNCTION(ifft_real_permute)
{
  ASSERT(isPowerOf2(length));
  u32 logLength = log2(length);
  r32 invLength = 1.f/(r32)length;
  u32 simdWidth = 4;

  // TODO: simd-ize this
  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      dest[index] = invLength*srcRe[reversedIndex];
      destImTemp[index] = invLength*srcIm[reversedIndex];
    }

  u32 mask = 0xFFFFFFFF;
  //  r32 maskF = *(r32 *)&mask;

  for(u32 m = 2; m <= length; m <<= 1)
    {
      r32 angle = GS_TAU/(r32)m;
      
      // TODO: make trig ops fast
      c64 wm = C64Polar(1.f, angle);
      // TODO: maybe do these multiplies wide as well?
      c64 wmSq = wm*wm;
      c64 wmCu = wm*wmSq;
      c64 wmQu = wm*wmCu;
      WideFloat wmQuRe = wideSetConstantFloats(wmQu.re);
      WideFloat wmQuIm = wideSetConstantFloats(wmQu.im);

      for(u32 k = 0; k < length; k += m)
	{	  
	  WideFloat wRe = wideSetFloats(1.f, wm.re, wmSq.re, wmCu.re);
	  WideFloat wIm = wideSetFloats(0.f, wm.im, wmSq.im, wmCu.im);
	  	      
	  r32 *dest0Re = dest + k;
	  r32 *dest0Im = destImTemp + k;
	  r32 *dest1Re = dest + k + m/2;
	  r32 *dest1Im = destImTemp + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      WideInt storeMask = wideSetConstantInts(0.f);
	      //WideFloat storeMaskInv = wideSetConstantFloats(0.f);
	      for(u32 lane = 0; lane < simdWidth; ++lane)
		{
		  //r32 val = (j + lane < m/2) ? maskF : 0;
		  //r32 valInv = (j + lane < m/2) ? 0 : maskF;
		  u32 val = (j + lane < m/2) ? mask : 0;
		  
		  wideSetLaneInts(&storeMask, val, lane);
		  //wideSetLaneFloats(&storeMaskInv, valInv, lane);
		}

	      // NOTE: loads
	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);

	      // NOTE: butterflies and twiddles
	      WideFloat productRe = wideSubFloats(wideMulFloats(wRe, in1Re),
						  wideMulFloats(wIm, in1Im));
	      WideFloat productIm = wideAddFloats(wideMulFloats(wRe, in1Im),
						  wideMulFloats(wIm, in1Re));
	      WideFloat out0Re = wideAddFloats(in0Re, productRe);
	      WideFloat out0Im = wideAddFloats(in0Im, productIm);
	      WideFloat out1Re = wideSubFloats(in0Re, productRe);
	      WideFloat out1Im = wideSubFloats(in0Im, productIm);

	      // NOTE: mask out what not to store
	      /* WideFloat store0Re = wideAddFloats(wideMaskFloats(out0Re, storeMask), */
	      /* 					 wideMaskFloats(in0Re, storeMaskInv)); */
	      /* WideFloat store0Im = wideAddFloats(wideMaskFloats(out0Im, storeMask), */
	      /* 					 wideMaskFloats(in0Im, storeMaskInv)); */
	      /* WideFloat store1Re = wideAddFloats(wideMaskFloats(out1Re, storeMask), */
	      /* 					 wideMaskFloats(in1Re, storeMaskInv)); */
	      /* WideFloat store1Im = wideAddFloats(wideMaskFloats(out1Im, storeMask), */
	      /* 					 wideMaskFloats(in1Im, storeMaskInv)); */

	      WideFloat store0Re = wideMaskFloats(out0Re, in0Re, storeMask);
	      WideFloat store0Im = wideMaskFloats(out0Im, in0Im, storeMask);
	      WideFloat store1Re = wideMaskFloats(out1Re, in1Re, storeMask);
	      WideFloat store1Im = wideMaskFloats(out1Im, in1Im, storeMask);

	      // NOTE: do the store
	      wideStoreFloats(dest0Re, store0Re);
	      wideStoreFloats(dest0Im, store0Im);
	      wideStoreFloats(dest1Re, store1Re);
	      wideStoreFloats(dest1Im, store1Im);

	      WideFloat wReOld = wRe;
	      WideFloat wImOld = wIm;
	      wRe = wideSubFloats(wideMulFloats(wReOld, wmQuRe),
				  wideMulFloats(wImOld, wmQuIm));
	      wIm = wideAddFloats(wideMulFloats(wReOld, wmQuIm),
				  wideMulFloats(wImOld, wmQuRe));

	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static REAL_FFT_FUNCTION(fft_real_noPermute)
{
  // NOTE: radix-2 dif fft
  ASSERT(isPowerOf2(length));
  //u32 logLength = log2(length);
  u32 simdWidth = 4;
  
  // NOTE: copy input
  r32 *read = src;
  r32 *write = destRe;
  for(u32 i = 0; i < length; i += simdWidth)
    {
      //WideFloat in = wideLoadFloats(src);
      WideFloat in = wideLoadFloats(read);
      wideStoreFloats(write, in);      

      read += simdWidth;
      write += simdWidth;
    }

  //u32 mask = 0xFFFFFFFF;
  //r32 maskF = *(r32 *)&mask;
  //for(u32 m = length; m >= 2; m >>= 1)
  for(u32 m = length; m >= 2*simdWidth; m >>= 1)
    {
      r32 angle = -GS_TAU/(r32)m;

      c64 wm = C64Polar(1.f, angle);
      c64 wmSq = wm*wm;
      c64 wmCu = wm*wmSq;
      c64 wmQu = wm*wmCu;
      WideFloat wmQuRe = wideSetConstantFloats(wmQu.re);
      WideFloat wmQuIm = wideSetConstantFloats(wmQu.im);

      for(u32 k = 0; k < length; k += m)
	{
	  WideFloat wRe = wideSetFloats(1.f, wm.re, wmSq.re, wmCu.re);
	  WideFloat wIm = wideSetFloats(0.f, wm.im, wmSq.im, wmCu.im);

	  r32 *dest0Re = destRe + k;
	  r32 *dest0Im = destIm + k;
	  r32 *dest1Re = destRe + k + m/2;
	  r32 *dest1Im = destIm + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      /* WideFloat storeMask = wideSetConstantFloats(0.f); */
	      /* WideFloat storeMaskInv = wideSetConstantFloats(0.f); */
	      /* for(u32 lane = 0; lane < simdWidth; ++lane) */
	      /* 	{ */
	      /* 	  r32 val = (j + lane < m/2) ? maskF : 0.f; */
	      /* 	  r32 valInv = (j + lane < m/2) ? 0.f : maskF; */

	      /* 	  wideSetLaneFloats(&storeMask, val, lane); */
	      /* 	  wideSetLaneFloats(&storeMaskInv, valInv, lane); */
	      /* 	} */

	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);

	      // NOTE: butterflies/twiddles
	      WideFloat out0Re = wideAddFloats(in0Re, in1Re);
	      WideFloat out0Im = wideAddFloats(in0Im, in1Im);
	      WideFloat diffRe = wideSubFloats(in0Re, in1Re);
	      WideFloat diffIm = wideSubFloats(in0Im, in1Im);
	      WideFloat out1Re = wideSubFloats(wideMulFloats(diffRe, wRe),
					       wideMulFloats(diffIm, wIm));
	      WideFloat out1Im = wideAddFloats(wideMulFloats(diffRe, wIm),
					       wideMulFloats(diffIm, wRe));

	      /* WideFloat store0Re = wideAddFloats(wideMaskFloats(out0Re, storeMask), */
	      /* 					 wideMaskFloats(in0Re, storeMaskInv)); */
	      /* WideFloat store0Im = wideAddFloats(wideMaskFloats(out0Im, storeMask), */
	      /* 					 wideMaskFloats(in0Im, storeMaskInv)); */
	      /* WideFloat store1Re = wideAddFloats(wideMaskFloats(out1Re, storeMask), */
	      /* 					 wideMaskFloats(in1Re, storeMaskInv)); */
	      /* WideFloat store1Im = wideAddFloats(wideMaskFloats(out1Im, storeMask), */
	      /* 					 wideMaskFloats(in1Im, storeMaskInv)); */

	      /* wideStoreFloats(dest0Re, store0Re); */
	      /* wideStoreFloats(dest0Im, store0Im); */
	      /* wideStoreFloats(dest1Re, store1Re); */
	      /* wideStoreFloats(dest1Im, store1Im); */
	      wideStoreFloats(dest0Re, out0Re);
	      wideStoreFloats(dest0Im, out0Im);
	      wideStoreFloats(dest1Re, out1Re);
	      wideStoreFloats(dest1Im, out1Im);

	      WideFloat wReOld = wRe;
	      WideFloat wImOld = wIm;
	      wRe = wideSubFloats(wideMulFloats(wReOld, wmQuRe),
				  wideMulFloats(wImOld, wmQuIm));
	      wIm = wideAddFloats(wideMulFloats(wReOld, wmQuIm),
				  wideMulFloats(wImOld, wmQuRe));
		
	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }  
  for(u32 m = simdWidth; m >= 2; m >>= 1)
    {
      r32 angle = -GS_TAU/(r32)m;
      c64 wm = C64Polar(1.f, angle);
      r32 wmRe = wm.re;
      r32 wmIm = wm.im;
      
      for(u32 k = 0; k < length; k += m)
	{
	  r32 wRe = 1.f;
	  r32 wIm = 0.f;

	  r32 *dest0Re = destRe + k;
	  r32 *dest0Im = destIm + k;
	  r32 *dest1Re = destRe + k + m/2;
	  r32 *dest1Im = destIm + k + m/2;
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      r32 in0Re = *dest0Re;
	      r32 in0Im = *dest0Im;
	      r32 in1Re = *dest1Re;
	      r32 in1Im = *dest1Im;

	      r32 out0Re = in0Re + in1Re;
	      r32 out0Im = in0Im + in1Im;
	      r32 diffRe = in0Re - in1Re;
	      r32 diffIm = in0Im - in1Im;
	      r32 out1Re = diffRe*wRe - diffIm*wIm;
	      r32 out1Im = diffRe*wIm + diffIm*wRe;

	      *dest0Re++ = out0Re;
	      *dest0Im++ = out0Im;
	      *dest1Re++ = out1Re;
	      *dest1Im++ = out1Im;

	      r32 wReOld = wRe;
	      wRe = wReOld*wmRe - wIm*wmIm;
	      wIm = wReOld*wmIm + wIm*wmRe;
	    }
	}
    }
}

static REAL_IFFT_FUNCTION(ifft_real_noPermute)
{
  // NOTE: radix-2 dit ifft, without bit-reverse input permutation
  ASSERT(isPowerOf2(length));
  WideFloat invLength = wideSetConstantFloats(1.f/(r32)length);
  u32 simdWidth = 4;
  
  // NOTE: copy input
  r32 *readRe = srcRe;
  r32 *readIm = srcIm;
  r32 *writeRe = dest;
  r32 *writeIm = destImTemp;
  for(u32 i = 0; i < length; i += simdWidth)
    {
      WideFloat inRe = wideLoadFloats(readRe);
      WideFloat inIm = wideLoadFloats(readIm);
      
      wideStoreFloats(writeRe, wideMulFloats(invLength, inRe));
      wideStoreFloats(writeIm, wideMulFloats(invLength, inIm));

      readRe += simdWidth;
      readIm += simdWidth;
      writeRe += simdWidth;
      writeIm += simdWidth;
    }

  //u32 mask = 0xFFFFFFFF;
  //r32 maskF = *(r32 *)&mask;
  for(u32 m = 2; m <= simdWidth; m <<= 1)
    {
      r32 angle = GS_TAU/(r32)m;
      c64 wm = C64Polar(1.f, angle);
      r32 wmRe = wm.re;
      r32 wmIm = wm.im;

      for(u32 k = 0; k < length; k += m)
	{
	  r32 wRe = 1.f;
	  r32 wIm = 0.f;

	  r32 *dest0Re = dest + k;
	  r32 *dest0Im = destImTemp + k;
	  r32 *dest1Re = dest + k + m/2;
	  r32 *dest1Im = destImTemp + k + m/2;
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      r32 in0Re = *dest0Re;
	      r32 in0Im = *dest0Im;
	      r32 in1Re = *dest1Re;
	      r32 in1Im = *dest1Im;

	      r32 productRe = in1Re*wRe - in1Im*wIm;
	      r32 productIm = in1Re*wIm + in1Im*wRe;
	      r32 out0Re = in0Re + productRe;
	      r32 out0Im = in0Im + productIm;
	      r32 out1Re = in0Re - productRe;
	      r32 out1Im = in0Im - productIm;

	      *dest0Re++ = out0Re;
	      *dest0Im++ = out0Im;
	      *dest1Re++ = out1Re;
	      *dest1Im++ = out1Im;

	      r32 wReOld = wRe;
	      wRe = wReOld*wmRe - wIm*wmIm;
	      wIm = wReOld*wmIm + wIm*wmRe;
	    }
	}
    }
  for(u32 m = 2*simdWidth; m <= length; m <<= 1)
    {
      r32 angle = GS_TAU/(r32)m;
      
      // TODO: make trig ops fast
      c64 wm = C64Polar(1.f, angle);
      // TODO: maybe do these multiplies wide as well?
      c64 wmSq = wm*wm;
      c64 wmCu = wm*wmSq;
      c64 wmQu = wm*wmCu;
      WideFloat wmQuRe = wideSetConstantFloats(wmQu.re);
      WideFloat wmQuIm = wideSetConstantFloats(wmQu.im);

      for(u32 k = 0; k < length; k += m)
	{	  
	  WideFloat wRe = wideSetFloats(1.f, wm.re, wmSq.re, wmCu.re);
	  WideFloat wIm = wideSetFloats(0.f, wm.im, wmSq.im, wmCu.im);
	  	      
	  r32 *dest0Re = dest + k;
	  r32 *dest0Im = destImTemp + k;
	  r32 *dest1Re = dest + k + m/2;
	  r32 *dest1Im = destImTemp + k + m/2;
	  for(u32 j = 0; j < m/2; j += simdWidth)
	    {
	      /* WideFloat storeMask = wideSetConstantFloats(0.f); */
	      /* WideFloat storeMaskInv = wideSetConstantFloats(0.f); */
	      /* for(u32 lane = 0; lane < simdWidth; ++lane) */
	      /* 	{ */
	      /* 	  r32 val = (j + lane < m/2) ? maskF : 0; */
	      /* 	  r32 valInv = (j + lane < m/2) ? 0 : maskF; */
		  
	      /* 	  wideSetLaneFloats(&storeMask, val, lane); */
	      /* 	  wideSetLaneFloats(&storeMaskInv, valInv, lane); */
	      /* 	} */

	      // NOTE: loads
	      WideFloat in0Re = wideLoadFloats(dest0Re);
	      WideFloat in0Im = wideLoadFloats(dest0Im);
	      WideFloat in1Re = wideLoadFloats(dest1Re);
	      WideFloat in1Im = wideLoadFloats(dest1Im);

	      // NOTE: butterflies and twiddles
	      WideFloat productRe = wideSubFloats(wideMulFloats(wRe, in1Re),
						  wideMulFloats(wIm, in1Im));
	      WideFloat productIm = wideAddFloats(wideMulFloats(wRe, in1Im),
						  wideMulFloats(wIm, in1Re));
	      WideFloat out0Re = wideAddFloats(in0Re, productRe);
	      WideFloat out0Im = wideAddFloats(in0Im, productIm);
	      WideFloat out1Re = wideSubFloats(in0Re, productRe);
	      WideFloat out1Im = wideSubFloats(in0Im, productIm);

	      // NOTE: mask out what not to store
	      /* WideFloat store0Re = wideAddFloats(wideMaskFloats(out0Re, storeMask), */
	      /* 					 wideMaskFloats(in0Re, storeMaskInv)); */
	      /* WideFloat store0Im = wideAddFloats(wideMaskFloats(out0Im, storeMask), */
	      /* 					 wideMaskFloats(in0Im, storeMaskInv)); */
	      /* WideFloat store1Re = wideAddFloats(wideMaskFloats(out1Re, storeMask), */
	      /* 					 wideMaskFloats(in1Re, storeMaskInv)); */
	      /* WideFloat store1Im = wideAddFloats(wideMaskFloats(out1Im, storeMask), */
	      /* 					 wideMaskFloats(in1Im, storeMaskInv)); */

	      // NOTE: do the store
	      /* wideStoreFloats(dest0Re, store0Re); */
	      /* wideStoreFloats(dest0Im, store0Im); */
	      /* wideStoreFloats(dest1Re, store1Re); */
	      /* wideStoreFloats(dest1Im, store1Im); */
	      wideStoreFloats(dest0Re, out0Re);
	      wideStoreFloats(dest0Im, out0Im);
	      wideStoreFloats(dest1Re, out1Re);
	      wideStoreFloats(dest1Im, out1Im);


	      WideFloat wReOld = wRe;
	      WideFloat wImOld = wIm;
	      wRe = wideSubFloats(wideMulFloats(wReOld, wmQuRe),
				  wideMulFloats(wImOld, wmQuIm));
	      wIm = wideAddFloats(wideMulFloats(wReOld, wmQuIm),
				  wideMulFloats(wImOld, wmQuRe));

	      dest0Re += simdWidth;
	      dest0Im += simdWidth;
	      dest1Re += simdWidth;
	      dest1Im += simdWidth;
	    }
	}
    }
}

static void
convolve(r32 *dest, r32 *src0, r32 *src1, u32 length, Arena *tempAllocator)
{
  // NOTE: fast convolution with ffts  
  ASSERT(isPowerOf2(length));
  u32 simdWidth = 4;
  
  r32 *temp0Re = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp0Im = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp1Re = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  r32 *temp1Im = arenaPushArray(tempAllocator, length, r32, arenaFlagsZeroAlign(simdWidth));
  
  fft_real_noPermute(temp0Re, temp0Im, src0, length);
  fft_real_noPermute(temp1Re, temp1Im, src1, length);

  r32 *tempDestRe = temp0Re;
  r32 *tempDestIm = temp0Im;
  r32 *tempSrc0Re = temp0Re;
  r32 *tempSrc0Im = temp0Im;
  r32 *tempSrc1Re = temp1Re;
  r32 *tempSrc1Im = temp1Im;
  ASSERT((length & 0x3) == 0);
  for(u32 i = 0; i < length; i += simdWidth)
    {
      WideFloat in0Re = wideLoadFloats(tempSrc0Re);
      WideFloat in0Im = wideLoadFloats(tempSrc0Im);
      WideFloat in1Re = wideLoadFloats(tempSrc1Re);
      WideFloat in1Im = wideLoadFloats(tempSrc1Im);

      WideFloat outRe = wideSubFloats(wideMulFloats(in0Re, in1Re), wideMulFloats(in0Im, in1Re));
      WideFloat outIm = wideAddFloats(wideMulFloats(in0Re, in1Im), wideMulFloats(in0Im, in1Re));
      
      wideStoreFloats(tempDestRe, outRe);
      wideStoreFloats(tempDestIm, outIm);

      tempDestRe += simdWidth;
      tempDestIm += simdWidth;
      
      tempSrc0Re += simdWidth;
      tempSrc0Im += simdWidth;
      tempSrc1Re += simdWidth;
      tempSrc1Im += simdWidth;
    }

  ifft_real_noPermute(dest, temp1Im, temp0Re, temp0Im, length);
}

inline void
fft(c64 *destBuffer, r32 *sourceBuffer, u32 lengthInit, u32 stride = 1)
{
#if 0
  u32 length = lengthInit;
  if(length == 1)
    {
      (*destBuffer).re = *sourceBuffer;
    }
  else
    {
      fft(destBuffer, sourceBuffer, length/2, 2*stride);
      fft(destBuffer + length/2, sourceBuffer + stride, length/2, 2*stride);

      for(u32 k = 0; k < length/2; ++k)
	{
	  c64 w = C64Polar(1, -GS_TAU*k/(r32)length);
	  c64 in0 = destBuffer[k];
	  c64 in1 = destBuffer[k + length/2];

	  destBuffer[k] = in0 + w*in1;
	  destBuffer[k + length/2] = in0 - w*in1;
	}
    }
#else 
  // NOTE: iterative radix-2 DIT fft
  // TODO: radix-4
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(length);
  u32 logLength = log2(length);  

  // NOTE: bit-reverse copy
  // TODO: optimize
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;
      ASSERT(reversedIndex < length);
      
      destBuffer[index].re = sourceBuffer[reversedIndex];
    }  

  // NOTE: twiddles
  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = (1 << level);
      r32 angle = -GS_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);
      
      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = destBuffer + k;
	  c64 *dest1 = destBuffer + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: optimize this math:
	      //       use SIMD intrinsics, and take advantage of the fact that the input data is real
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }  
#endif
}

inline void
ifft(r32 *destBuffer, r32 *destImScratch, c64 *sourceBuffer, u32 lengthInit, u32 stride = 1)
{
#if 0
  if(length == 1)
    {
      *destBuffer = (*sourceBuffer).re;
    }
  else
    {
      ifft(destBuffer, sourceBuffer, length/2, 2*stride);
      ifft(destBuffer + length/2, sourceBuffer + stride, length/2, 2*stride);

      for(u32 k = 0; k < length/2; ++k)
	{
	  c64 w = C64Polar(1, GS_TAU*k/(r32)length);
	  c64 in0 = destBuffer[k];
	  c64 in1 = destBuffer[k + length/2];

	  destBuffer[k] = in0 + w*in1;
	  destBuffer[k + length/2] = in0 - w*in1;
	}
    }
#else
  // TODO: optimize
  //ASSERT(!(length & 1));
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 logLength = log2(length);
  // NOTE: assuming power-of-two size
  // TODO: support non-power-of-two sizes, don't require zero-padding input
  ASSERT(length == (u32)(1 << logLength));
  r32 invLength = 1.f/(r32)length;

  u32 lengthSizeInBits = 8*sizeof(length);
  for(u32 index = 0; index < length; ++index)
    {
      u32 reversedIndex = reverseBits(index);
      reversedIndex >>= lengthSizeInBits - logLength;

      c64 source = invLength*sourceBuffer[reversedIndex];
      destBuffer[index] = source.re;
      destImScratch[index] = source.im;
    }

  for(u32 level = 1; level <= logLength; ++level)
    {
      u32 m = 1 << level;
      r32 angle = GS_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  r32 *dest0 = destBuffer + k;
	  r32 *dest0Im = destImScratch + k;
	  r32 *dest1 = destBuffer + k + m/2;
	  r32 *dest1Im = destImScratch + k + m/2;
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = C64(*dest0, *dest0Im);
	      c64 in1 = C64(*dest1, *dest1Im);

	      c64 result0 = in0 + w*in1;
	      c64 result1 = in0 - w*in1;
	      
	      *dest0++ = result0.re;
	      *dest1++ = result1.re;
	      *dest0Im++ = result0.im;
	      *dest1Im++ = result1.im;

	      w *= wm;
	    }
	}
    }
#endif
}

inline void
fft(c64 *output, c64 *input, u32 lengthInit)
{
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(lengthInit);
  u32 logLength = log2(length);
  
  for(u32 index = 0; index < length; ++index)
    {
      u32 reverseIndex = reverseBits(index);
      reverseIndex >>= lengthSizeInBits - logLength;
      output[index] = input[reverseIndex];
    }

  for(u32 s = 1; s <= logLength; ++s)
    {
      u32 m = 1 << s;
      r32 angle = -GS_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = output + k;
	  c64 *dest1 = output + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: SIMD	 
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }
}

inline void
ifft(c64 *output, c64 *input, u32 lengthInit)
{
  u32 length = ROUND_UP_TO_POWER_OF_2(lengthInit);
  u32 lengthSizeInBits = 8*sizeof(lengthInit);
  u32 logLength = log2(length);
  r32 invLength = 1.f/(r32)length;
  
  for(u32 index = 0; index < length; ++index)
    {
      u32 reverseIndex = reverseBits(index);
      reverseIndex >>= lengthSizeInBits - logLength;
      output[index] = invLength*input[reverseIndex];
    }

  for(u32 s = 1; s <= logLength; ++s)
    {
      u32 m = 1 << s;
      r32 angle = GS_TAU/(r32)m;
      c64 wm = C64Polar(1, angle);

      for(u32 k = 0; k < length; k += m)
	{
	  c64 w = C64(1, 0);
	  
	  c64 *dest0 = output + k;
	  c64 *dest1 = output + k + m/2;	  
	  for(u32 j = 0; j < m/2; ++j)
	    {
	      c64 in0 = *dest0;
	      c64 in1 = *dest1;     

	      // TODO: SIMD	 
	      *dest0++ = in0 + w*in1;
	      *dest1++ = in0 - w*in1;

	      w *= wm;
	    }
	}
    }
}

inline void
czt(c64 *output, r32 *input, u32 length, Arena *scratchAllocator)
{
  // NOTE: chirp z transform.
  //       for taking ffts of arbitrary length signals
  
  u32 chirpLength = 2*length - 1;
  u32 fftLength = ROUND_UP_TO_POWER_OF_2(chirpLength);

  r32 angle = -GS_PI/(r32)length;
  c64 wBase = C64Polar(1, angle);  
  c64 *chirp = arenaPushArray(scratchAllocator, chirpLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *reciprocalChirp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *scaledInput = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *wTemp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  chirp[length - 1] = C64(1, 0);
  reciprocalChirp[length - 1] = C64(1, 0);
  wTemp[0] = C64(1, 0);
  scaledInput[0] = input[0]*chirp[length];
  for(u32 k = 1; k < length; ++k)
    {
      // NOTE: generate chirp, multiply the input
      c64 chirpVal = chirp[length - 1 + k - 1] * wTemp[k - 1] * wBase;
      c64 conjChirpVal = conjugateC64(chirpVal);
      chirp[length - 1 + k] = chirpVal;
      chirp[length - 1 - k] = chirpVal;
      reciprocalChirp[length - 1 + k] = conjChirpVal;
      reciprocalChirp[length - 1 - k] = conjChirpVal;
      scaledInput[k] = input[k] * chirp[length + k - 1 ];

      // TODO: don't compute more of these values than necessary. may be a source of significant numerical error
      //if(k*k >= 2*length) 
      wTemp[k] = wTemp[k - 1] * wBase * wBase;
    } 

  // NOTE: convolve the scaled input with the reciprocal chirp
  // TODO: these fft calls are not optimized for doing convolutions:
  //       we do an (order 2) input permutation at the beginning of each fft and ifft so that our output coefficients
  //       are in order of increasing frequency. That is not necessary to do here, and a huge burden on performance
  c64 *fftScaledInput = wTemp;
  fft(fftScaledInput, scaledInput, fftLength);

  c64 *fftReciprocalChirp = scaledInput;
  fft(fftReciprocalChirp, reciprocalChirp, fftLength);
  
  for(u32 index = 0; index < fftLength; ++index)
    {
      fftScaledInput[index] *= fftReciprocalChirp[index];
    }

  c64 *ifftOutput = reciprocalChirp;
  ifft(ifftOutput, fftScaledInput, fftLength);

  // NOTE: write out, scaling by the chirp
  for(u32 index = 0; index < length; ++index)
    {
      output[index] = ifftOutput[length + index - 1] * chirp[length + index - 1];
    }
}

inline void
iczt(r32 *outputReal, r32 *outputImag, c64 *input, u32 length, Arena *scratchAllocator)
{
  r32 invLength = 1.f/(r32)length;
  u32 chirpLength = 2*length - 1;
  u32 fftLength = ROUND_UP_TO_POWER_OF_2(chirpLength);

  r32 angle = GS_PI/(r32)length;
  c64 wBase = C64Polar(1, angle);  
  c64 *chirp = arenaPushArray(scratchAllocator, chirpLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *reciprocalChirp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *scaledInput = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  c64 *wTemp = arenaPushArray(scratchAllocator, fftLength, c64);//, arenaFlagsZeroNoAlign());
  chirp[length - 1] = C64(1, 0);
  reciprocalChirp[length - 1] = C64(1, 0);
  wTemp[0] = C64(1, 0);
  scaledInput[0] = input[0]*chirp[length];
  for(u32 k = 1; k < length; ++k)
    {      
      c64 chirpVal = chirp[length - 1 + k - 1] * wTemp[k - 1] * wBase;
      c64 conjChirpVal = conjugateC64(chirpVal);
      chirp[length - 1 + k] = chirpVal;
      chirp[length - 1 - k] = chirpVal;
      reciprocalChirp[length - 1 + k] = conjChirpVal;
      reciprocalChirp[length - 1 - k] = conjChirpVal;
      scaledInput[k] = input[k] * chirp[length + k - 1 ];
      
      wTemp[k] = wTemp[k - 1] * wBase * wBase;
    } 

  c64 *fftScaledInput = wTemp;
  fft(fftScaledInput, scaledInput, fftLength);

  c64 *fftReciprocalChirp = scaledInput;
  fft(fftReciprocalChirp, reciprocalChirp, fftLength);
  
  for(u32 index = 0; index < fftLength; ++index)
    {
      fftScaledInput[index] *= fftReciprocalChirp[index];
    }

  c64 *ifftOutput = reciprocalChirp;
  ifft(ifftOutput, fftScaledInput, fftLength);

  for(u32 index = 0; index < length; ++index)
    {
      c64 outVal = invLength * ifftOutput[length + index - 1] * chirp[length + index - 1];
      outputReal[index] = outVal.re;
      outputImag[index] = outVal.im;
    }
}
