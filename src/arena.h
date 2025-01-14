struct Arena
{
  u8 *base;
  usz capacity;
  usz used;
};

static inline Arena
arenaBegin(void *memory, usz capacity)
{
  Arena result = {};
  result.base = (u8 *)memory;
  result.capacity = capacity;

  return(result);
}

#define arenaPushSize(arena, size) arenaPushSize_(arena, size)
#define arenaPushArray(arena, count, type) (type *)arenaPushSize_(arena, count*sizeof(type))
#define arenaPushStruct(arena, type) (type *)arenaPushSize_(arena, sizeof(type))

static inline u8 *
arenaPushSize_(Arena *arena, usz size)
{
  ASSERT(size + arena->used <=  arena->capacity);
  u8 *result = arena->base + arena->used;
  arena->used += size;

  return(result);
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

static inline void
arenaEnd(Arena *arena)
{
  arena->used = 0;
}
