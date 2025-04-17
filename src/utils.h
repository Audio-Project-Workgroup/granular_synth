// common utilities and miscellaneous functions

#ifdef __clang__
#define ASSERT(cond) if(!(cond)) __builtin_trap();
#else
#define ASSERT(cond) if(!(cond)) *(int *)0 = 0;
#endif

#define UNUSED(var) (void)var

#define KILOBYTES(count) (1024LL*count)
#define MEGABYTES(count) (1024LL*KILOBYTES(count))
#define GIGABYTES(count) (1024LL*MEGABYTES(count))

#define THOUSAND(count) (1000LL*count)
#define MILLION(count) (1000LL*THOUSAND(count))
#define BILLION(count) (1000LL*MILLION(count))

#define MIN(a, b) ((a) > (b)) ? (b) : (a)
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define ARRAY_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))
#define OFFSET_OF(type, member) ((usz)&(((type *)0)->member))

#define IS_MULTIPLE_OF_2(num) (((num) & ((num) - 1)) == (num))
#define ROUND_UP_TO_MULTIPLE_OF_2(num) (((num) + 1) & (~1))
#define ALIGN_POW_2(num, alignment) (((num) + ((alignment)-1LL)) & (~((alignment)-1LL)))
#define ALIGN_DOWN_POW_2(num, alignment) ((num) & ~((alignment) - 1LL))
#define ROUND_UP_TO_MULTIPLE(num, length) (((num) % (length)) ? ((num) + (length) - ((num) % (length))) : (num))

#define CYCLIC_ARRAY_INDEX(i, len) ((i < len) ? ((i < 0) ? (i + len) : i) : (i - len))

// NOTE: linked-list utilities
#define STACK_PUSH(head, node) do {(node)->next = (head); (head) = (node);} while(0)
#define STACK_POP(head) do {(head) = (head)->next;} while(0)

#define QUEUE_PUSH(f, l, n) do {(f) ? ((l->next=(n),(l)=(n)),(n)->next=0) : (f)=(l)=(n);} while(0)
#define QUEUE_PUSH_FRONT(f, l, n) do {(f) ? ((n)->next=(f),(f)=(n)) : ((f)=(l)=(n),(n)->next=0);} while(0)
#define QUEUE_POP(f, l) do {(((f)==(l)) ? ((f)=0, (l)=0) : ((f)=(f)->next));} while(0)

#define DLL_PUSH_BACK(f, l, n) do {((f)==0) ? ((f)=(l)=(n), (n)->next=(n)->prev=0):((n)->prev=(l), (l)->next=(n), (l)=(n), (n)->next=0);} while(0)
#define DLL_REMOVE(f, l, d) do {((f)==(d) ? ((f)==(l) ? ((f)=(l)=(0)) : ((f)=(f)->next, (f)->prev=0)) : (l)==(d) ? ((l)=(l)->prev, (l)->next=0) : ((d)->next->prev=(d)->prev, (d)->prev->next=(d)->next));} while(0)

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

#define DLINKED_LIST_REMOVE(sentinel, dead) do {	\
    (dead)->prev->next = (dead)->next;			\
    (dead)->next->prev = (dead)->prev;			\
  } while(0)

#define ZERO_SIZE(ptr, size) do {		\
    u8 *p = (u8 *)(ptr);				\
    for(u32 i = 0; i < (size); ++i) *p++ = 0;		\
  } while(0)

/* #define ZERO_STRUCT(s) do {					\ */
/*     usz size = sizeof(s);					\ */
/*     u8 *data = (u8 *)&(s);					\ */
/*     for(u32 byte = 0; byte < (size); ++byte) data[byte] = 0;	\ */
/*   } while(0) */
#define ZERO_STRUCT(s) ZERO_SIZE((s), sizeof(*s))

#define ZERO_ARRAY(ptr, count, type) ZERO_SIZE((ptr), (count)*sizeof(type))

#define COPY_SIZE(dest, src, size) do {			\
    u8 *destP = (u8 *)(dest);				\
    u8 *srcP = (u8 *)(src);				\
    for(u32 i = 0; i < (size); ++i) *destP++ = *srcP++;	\
  } while(0)

#define COPY_ARRAY(dest, src, count, type) COPY_SIZE(dest, src, (count)*sizeof(type))

inline u32
safeTruncateU64(u64 val)
{
  u32 result = MIN(val, (u64)U32_MAX);

  return(result);
}

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

inline bool
stringsAreEqual(u8 *s1, u8 *s2)
{
  bool result = true;
  
  u8 *atS1 = s1;
  u8 *atS2 = s2;
  while(*atS1 && *atS2)
    {
      if(*atS1 != *atS2)
	{
	  result = false;
	  break;
	}

      ++atS1;
      ++atS2;
    }

  if(result)
    {
      result = !(*atS1 || *atS2);
    }

  return(result);
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

inline u32
roundR32ToU32(r32 num)
{
  u32 result = (u32)num;
  if((num - result) >= 0.5f) ++result;

  return(result);
}

inline v4
colorV4FromU32(u32 c)
{
  v4 result=  {(r32)((c & (0xFF << 3*8)) >> 3*8)/255.f,
	       (r32)((c & (0xFF << 2*8)) >> 2*8)/255.f,
	       (r32)((c & (0xFF << 1*8)) >> 1*8)/255.f,
	       (r32)((c & (0xFF << 0*8)) >> 0*8)/255.f};

  return(result);
}

inline u32
colorU32FromV4(v4 c)
{
  u32 result = ((roundR32ToU32(c.a*255.f) << 3*8) |
		(roundR32ToU32(c.b*255.f) << 2*8) |
		(roundR32ToU32(c.g*255.f) << 1*8) |
		(roundR32ToU32(c.r*255.f) << 0*8));

  return(result);
}

