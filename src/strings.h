// structures/functions for making string processing and amalgamation convenient
//#include <stdio.h>
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_DECORATE(name) gs_##name
#include "stb_sprintf.h"

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
  while(*at) {++at; ++result;}

  return(result);
}

#define STR8_CSTR(cstr) makeString8((u8 *)cstr, getCStringLength(cstr))
#define STR8_LIT(lit) makeString8((u8 *)lit, sizeof(lit)-1)

inline String8
arenaPushString(Arena *arena, String8 string)
{
  String8 result = {};
  result.size = string.size;
  result.str = arenaPushSize(arena, result.size + 1);
  COPY_SIZE(result.str, string.str, result.size);
  result.str[result.size] = 0;

  return(result);
}

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
  node->string = arenaPushString(arena, str);

  stringListPushNode(list, node);
}

inline void
stringListPushFront(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = arenaPushStruct(arena, String8Node);
  node->string = arenaPushString(arena, str);

  stringListPushNodeFront(list, node);
}

inline void
stringListPushFormat(Arena *arena, String8List *list, char *fmt, ...)
{
  va_list varArgs;
  va_start(varArgs, fmt);

  char buffer[256] = {};
  int len = gs_vsnprintf(buffer, ARRAY_COUNT(buffer), fmt, varArgs);

  String8 string = makeString8((u8 *)buffer, len);
  stringListPush(arena, list, string);
}

inline void
stringListPushFormatV(Arena *arena, String8List *list, char *fmt, va_list vaArgs)
{
  char buffer[256] = {};
  int len = gs_vsnprintf(buffer, ARRAY_COUNT(buffer), fmt, vaArgs);

  String8 string = makeString8((u8 *)buffer, len);
  stringListPush(arena, list, string);
}

// string functions
inline bool
stringsAreEqual(String8 s0, String8 s1)
{
  bool result = s0.size == s1.size;
  if(result)
    {
      u8 *s0At = s0.str;
      u8 *s1At = s1.str;
      for(u32 i = 0; result && i < s0.size; ++i, ++s0At, ++s1At)
	{
	  result = *s0At == *s1At;
	}
    }

  return(result);
}
  
inline String8
concatenateStrings(Arena *allocator, String8 s0, String8 s1)
{
  String8 result = {};
  result.size = s0.size + s1.size;
  result.str = arenaPushArray(allocator, result.size + 1, u8, arenaFlagsZeroNoAlign()); // NOTE: null-terminate
  COPY_ARRAY(result.str, s0.str, s0.size, u8);
  COPY_ARRAY(result.str + s0.size, s1.str, s1.size, u8);

  return(result);
}

inline String8
stringGetParentPath(String8 s)
{
  u32 lastSlashIndex = 0;
  for(u32 i = 0; i < s.size; ++i)
    {
      u8 c = s.str[i];
      if(c == '/') lastSlashIndex = i;
    }

  String8 result = makeString8(s.str, lastSlashIndex);
  return(result);
}
