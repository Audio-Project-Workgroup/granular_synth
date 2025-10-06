// structures/functions for making string processing and amalgamation convenient
//#include <stdio.h>
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_DECORATE(name) gs_##name
#include "stb_sprintf.h"

#include <stdarg.h>

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

inline String8
arenaPushStringFormatV(Arena *arena, char *fmt, va_list vaArgs)
{
  int bufferCount = 1024;
  u8 *buffer = arenaPushArray(arena, bufferCount, u8, arenaFlagsZeroNoAlign());
  int count = gs_vsnprintf((char*)buffer, bufferCount, fmt, vaArgs);
  if(count < bufferCount)
    {
      arenaPopSize(arena, bufferCount - count - 1);
    }
  else
    {
      arenaPopSize(arena, bufferCount);
      buffer = arenaPushArray(arena, count + 1, u8, arenaFlagsZeroNoAlign());
      count = gs_vsnprintf((char*)buffer, count + 1, fmt, vaArgs);
    }

  String8 result = {};
  result.str = buffer;
  result.size = count;

  return(result);
}

inline String8
arenaPushStringFormat(Arena *arena, char *fmt, ...)
{
  va_list vaArgs;
  va_start(vaArgs, fmt);

  String8 result = arenaPushStringFormatV(arena, fmt, vaArgs);

  va_end(vaArgs);

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

inline String8
stringListPush(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = arenaPushStruct(arena, String8Node, arenaFlagsZeroNoAlign());
  node->string = arenaPushString(arena, str);

  stringListPushNode(list, node);
  
  String8 result = node->string;
  return(result);
}

inline String8
stringListPushFront(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = arenaPushStruct(arena, String8Node, arenaFlagsZeroNoAlign());
  node->string = arenaPushString(arena, str);

  stringListPushNodeFront(list, node);

  String8 result = node->string;
  return(result);
}

inline String8
stringListPushFormatV(Arena *arena, String8List *list, char *fmt, va_list vaArgs)
{
  /* char buffer[1024] = {}; */
  /* int len = gs_vsnprintf(buffer, ARRAY_COUNT(buffer), fmt, vaArgs); */
  /* String8 string = makeString8((u8 *)buffer, len);  */
  TemporaryMemory scratch = arenaGetScratch(&arena, 1);
  String8 stringTemp = arenaPushStringFormatV(scratch.arena, fmt, vaArgs);
  String8 result = stringListPush(arena, list, stringTemp);
  arenaReleaseScratch(scratch);
  return(result);
}

inline String8
stringListPushFormat(Arena *arena, String8List *list, char *fmt, ...)
{
  va_list varArgs;
  va_start(varArgs, fmt);

  /* char buffer[256] = {}; */
  /* int len = gs_vsnprintf(buffer, ARRAY_COUNT(buffer), fmt, varArgs); */
  /* String8 string = makeString8((u8 *)buffer, len); */
  String8 result = stringListPushFormatV(arena, list, fmt, varArgs);
  va_end(varArgs);
  return(result);
}

inline String8
stringListJoin(Arena *arena, String8List *list)
{
  String8 result = {};
  result.str = arenaPushArray(arena, list->totalSize, u8);
  result.size = list->totalSize;

  u8 *stringAt = result.str;
  for(String8Node *node = list->first; node; node = node->next)
    {
      usz nodeStringSize = node->string.size;
      COPY_ARRAY(stringAt, node->string.str, nodeStringSize, u8);
      stringAt += nodeStringSize;
    }

  return(result);
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
