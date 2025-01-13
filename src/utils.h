#ifdef __clang__
#define ASSERT(cond) if(!(cond)) __builtin_trap();
#else
#define ASSERT(cond) if(!(cond)) *(int *)0 = 0;
#endif

#define KILOBYTES(count) (1024LL*count)
#define MEGABYTES(count) (1024LL*KILOBYTES(count))
#define GIGABYTES(count) (1024LL*MEGABYTES(count))

#define MIN(a, b) ((a) > (b)) ? (b) : (a)
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define ARRAY_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))
#define OFFSET_OF(type, member) ((usz)&(((type *)0)->member))

#define ROUND_UP_TO_MULTIPLE_OF_2(num) (((num) + 1) & (~1))

static inline u32
smallestNotInSortedArray(u32 *array, u32 length)
{
  u32 result = array[0];
  for(u32 i = 1; i < length; ++i)
    {
      u32 val = array[i];
      if(result == val)
	{
	  ++result;
	}
      else
	{
	  break;
	}
    }

  return(result);
}

static inline void
separateExtensionAndFilepath(char *filepath, char *resultPath, char *resultExtension)
{
  char *at = filepath;
  char *dest = resultPath;
  while(*at)
    {
      if(*at != '.')
	{
	  *dest++ = *at++;
	}
      else
	{
	  if(*(at + 1))
	    {
	      if(*(at + 1) == '.')
		{
		  *dest++ = *at++;
		  *dest++ = *at++;
		}
	      else
		{
		  dest = resultExtension;
		  ++at;
		}
	    }
	  else
	    {
	      break;
	    }
	}
    }
}

