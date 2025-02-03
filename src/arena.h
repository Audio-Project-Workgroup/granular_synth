struct Arena
{
  u8 *base;
  usz capacity;
  usz used;
};

struct TemporaryMemory
{
  u8 *base;
  usz capacity;
  usz used;

  Arena *parent;  
};

struct ArenaPushFlags
{
  bool clearToZero;
  u32 alignment;
};

inline ArenaPushFlags
defaultArenaPushFlags(void)
{
  ArenaPushFlags result = {};

  return(result);
}

inline ArenaPushFlags
arenaFlagsZeroNoAlign(void)
{
  ArenaPushFlags result = {};
  result.clearToZero = true;

  return(result);
}

inline ArenaPushFlags
arenaFlagsZeroAlign(u32 alignment)
{
  ArenaPushFlags result = {};
  result.clearToZero = true;
  result.alignment = alignment;

  return(result);
}

static inline Arena
arenaBegin(void *memory, usz capacity)
{
  Arena result = {};
  result.base = (u8 *)memory;
  result.capacity = capacity;

  return(result);
}

#define arenaPushSize(arena, size, ...) arenaPushSize_(arena, size, ##__VA_ARGS__)
#define arenaPushArray(arena, count, type, ...) (type *)arenaPushSize_(arena, count*sizeof(type), ##__VA_ARGS__)
#define arenaPushStruct(arena, type, ...) (type *)arenaPushSize_(arena, sizeof(type), ##__VA_ARGS__)

static inline u8 *
arenaPushSize_(Arena *arena, usz size, ArenaPushFlags flags = defaultArenaPushFlags())
{
  ASSERT(size + arena->used <=  arena->capacity);
  u8 *result = arena->base + arena->used;
  arena->used += size;

  if(flags.clearToZero)
    {
      ZERO_SIZE(result, size);
    }

  return(result);
}

inline u8 *
arenaPushString(Arena *arena, char *string)
{
  u8 *result = arena->base + arena->used;
  char *at = string;
  while(*at)
    {
      ASSERT(arena->used < arena->capacity);
      *(arena->base + arena->used++) = *at++;
    }
  ASSERT(arena->used < arena->capacity);
  *(arena->base + arena->used++) = 0; // NOTE: null termination

  return(result);
}

static inline void
arenaPopSize(Arena *arena, usz size)
{
  arena->used -= size;
}

static inline Arena
arenaSubArena(Arena *arena, usz size)
{
  Arena result = {};
  result.base = arenaPushSize(arena, size);
  result.capacity = size;

  return(result);
}

static inline void
arenaEndArena(Arena *parent, Arena child)
{
  parent->used -= child.capacity;
}

inline TemporaryMemory
arenaBeginTemporaryMemory(Arena *arena, usz size)
{
  TemporaryMemory result = {};
  result.base = arenaPushSize(arena, size, arenaFlagsZeroNoAlign());
  result.capacity = size;
  result.parent = arena;

  return(result);
}

inline void
arenaEndTemporaryMemory(TemporaryMemory *tempMem)
{
  tempMem->parent->used -= tempMem->capacity;
}

static inline void
arenaEnd(Arena *arena, bool clearToZero = false)
{
  if(clearToZero)
    {
      ZERO_SIZE(arena->base, arena->used);
    }
  arena->used = 0;
}
