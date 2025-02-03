// common utilities and miscellaneous functions

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

#define CYCLIC_ARRAY_INDEX(i, len) ((i < len) ? ((i < 0) ? (i + len) : i) : (i - len))

#define LINKED_LIST_APPEND(head, new) do {	\
    new->next = head;				\
    head = new;					\
  } while(0)

#define DLINKED_LIST_APPEND(sentinel, new) do { \
    new->next = sentinel;			\
    new->prev = sentinel->prev;			\
    new->next->prev = new;			\
    new->prev->next = new;			\
  } while(0)

#define ZERO_SIZE(ptr, size) do {		\
    u8 *p = (u8 *)ptr;				\
    for(u32 i = 0; i < size; ++i) *p++ = 0;	\
  } while(0)

#define ZERO_STRUCT(s) do {					\
    usz size = sizeof(s);					\
    u8 *data = (u8 *)&s;					\
    for(u32 byte = 0; byte < size; ++byte) data[byte] = 0;	\
  } while(0)

#define COPY_SIZE(dest, src, size) do {			\
    u8 *destP = (u8 *)dest;				\
    u8 *srcP = (u8 *)src;				\
    for(u32 i = 0; i < size; ++i) *destP++ = *srcP++;	\
  } while(0)

static inline u32
lowestOrderBit(u32 num)
{
  u32 result = 0;  
  u32 lowestBit = num & -num;
  while(lowestBit > 1)
    {
      lowestBit >>= 1;
      ++result;
    }

  return(result);
}

inline u32
reverseBits(u32 num)
{
  u32 v = num;
  v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
  v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
  v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
  v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
  v = (v >> 16) | (v << 16);

  return(v);
}

inline u64
reverseBits(u64 num)
{
  u64 v = num;
  v = ((v >> 1) & 0x5555555555555555) | ((v & 0x5555555555555555) << 1);
  v = ((v >> 2) & 0x3333333333333333) | ((v & 0x3333333333333333) << 2);
  v = ((v >> 4) & 0x0F0F0F0F0F0F0F0F) | ((v & 0x0F0F0F0F0F0F0F0F) << 4);
  v = ((v >> 8) & 0x00FF00FF00FF00FF) | ((v & 0x00FF00FF00FF00FF) << 8);
  v = ((v >> 16) & 0x0000FFFF0000FFFF) | ((v & 0x0000FFFF0000FFFF) << 16);
  v = (v >> 32) | (v << 32);

  return(v);
}

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

