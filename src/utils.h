#define ASSERT(cond) if(!(cond)) __builtin_trap();

#define KILOBYTES(count) (1024LL*count)
#define MEGABYTES(count) (1024LL*KILOBYTES(count))
#define GIGABYTES(count) (1024LL*MEGABYTES(count))

#define MIN(a, b) ((a) > (b)) ? (b) : (a)
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define ARRAY_COUNT(arr) (sizeof(arr)/sizeof(arr[0]))
#define OFFSET_OF(type, member) ((usz)&(((type *)0)->member))

#define ROUND_UP_TO_MULTIPLE_OF_2(num) (((num) + 1) & (~1))

