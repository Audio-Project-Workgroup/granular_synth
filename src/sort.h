#define SWAP(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))

// NOTE: result is low to high
static inline void
insertionSort(u32 *array, u32 length)
{
  for(u32 i = 0; i < length; ++i)
    {
      for(u32 j = i; j > 1; --j)
	{
	  u32 v1 = array[j];
	  u32 v2 = array[j - 1];
	  if(v1 < v2)
	    {
	      SWAP(v1, v2);
	    }
	  else
	    {
	      break;
	    }
	}
    }
}

static inline void
bubbleSort(u32 *array, u32 length)
{
  for(u32 i = 0; i < length - 1; ++i)
    {
      bool swapped = false;
      for(u32 j = 0; j < length - i - 1; ++j)
	{
	  u32 v1 = array[j];
	  u32 v2 = array[j + 1];
	  if(v1 > v2)
	    {
	      SWAP(v1, v2);
	      swapped = true;
	    }
	}

      if(swapped) break;
    }
}

static inline void
mergeSort(u32 *arrary, u32 length, Arena *allocator)
{
}
