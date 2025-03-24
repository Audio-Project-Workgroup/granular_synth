// structures/functions for making string processing and amalgamation convenient
#include <stdarg.h>

// base string types
struct String8
{
  u8 *str;
  u64 size;
};

struct String16
{
  u16 *str;
  u64 size;
};

struct String32
{
  u32 *str;
  u64 size;
};

// many-string structures
struct String8Node
{
  String8Node *next;
  String8 string;
};

struct String8List
{
  String8Node *first;
  String8Node *last;
  
  u64 nodeCount;
  u64 totalSize;
};

struct String8Array
{
  u64 count;
  String8 *strings;
};

// constructors/helpers
inline String8
makeString8(u8 *str, u64 size)
{
  String8 result = {str, size};

  return(result);
}

inline u64
getCStringLength(char *cstr)
{
  u64 result = 0;
  char *at = cstr;
  while(*at) ++at; ++result;

  return(result);
}

#define STR8_CSTR(cstr) makeString8((u8 *)cstr, getCStringLength(cstr))
#define STR8_LIT(lit) makeString8((u8 *)lit, sizeof(lit)-1)

// string list functions
inline void
stringListPushNode(String8List *list, String8Node *node)
{
  QUEUE_PUSH(list->first, list->last, node);
  ++list->nodeCount;
  list->totalSize += node->string.size;
}

inline void
stringListPushNodeFront(String8List *list, String8Node *node)
{
  QUEUE_PUSH_FRONT(list->first, list->last, node);
  ++list->nodeCount;
  list->totalSize += node->string.size;
}

inline void
stringListPush(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = arenaPushStruct(arena, String8Node);
  node->string = str;

  stringListPushNode(list, node);
}

inline void
stringListPushFront(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = arenaPushStruct(arena, String8Node);
  node->string = str;

  stringListPushNodeFront(list, node);
}

inline void
stringListPushFormat(Arena *arena, String8List *list, char *fmt, ...)
{
  va_list varArgs;
  va_start(varArgs, fmt);

  char buffer[256] = {};
  int len = vsnprintf(buffer, ARRAY_COUNT(buffer), fmt, varArgs);

  String8 string = makeString8((u8 *)buffer, len);
  stringListPush(arena, list, string);
}




  

