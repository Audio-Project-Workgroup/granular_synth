#define OLD_ARENA 0

struct Arena
{
#if OLD_ARENA
  u8 *base;
  usz capacity;
  usz used;
#else
  Arena *prev;
  Arena *current;
  u8 *base;
  usz capacity;
  usz used;
#endif
};

struct TemporaryMemory
{
#if OLD_ARENA
  u8 *base;
  usz capacity;
  usz used;

  Arena *parent;
#else
  Arena *arena;
  usz pos;
#endif
};

#if OLD_ARENA
#else
extern Arena* gs_arenaAcquire(usz capacity);
extern void   gs_arenaDiscard(Arena *arena);

#define ARENA_SCRATCH_POOL_COUNT 2
thread_var Arena *m__scratchPool[ARENA_SCRATCH_POOL_COUNT] = {};
#endif
//extern void*  gs_acquireMemory(usz size);
//extern void   gs_discardMemory(void *mem, usz size);

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

inline ArenaPushFlags
arenaFlagsNoZeroAlign(u32 alignment)
{
  ArenaPushFlags result = {};
  result.alignment = alignment;

  return(result);
}

#if OLD_ARENA
static inline Arena
arenaBegin(void *memory, usz capacity)
{
  Arena result = {};
  result.base = (u8 *)memory;
  result.capacity = capacity;

  return(result);
}
#endif

#define arenaPushSize(arena, size, ...) arenaPushSize_(arena, size, ##__VA_ARGS__)
#define arenaPushArray(arena, count, type, ...) (type *)arenaPushSize_(arena, count*sizeof(type), ##__VA_ARGS__)
#define arenaPushStruct(arena, type, ...) (type *)arenaPushSize_(arena, sizeof(type), ##__VA_ARGS__)

static inline u8 *
arenaPushSize_(Arena *arena, usz size, ArenaPushFlags flags = defaultArenaPushFlags())
{
#if OLD_ARENA
  usz resultInit = (usz)arena->base + arena->used;
  usz alignmentOffset = flags.alignment ? (ALIGN_POW_2(resultInit, flags.alignment) - resultInit) : 0; 

  ASSERT(size + arena->used + alignmentOffset < arena->capacity);
  u8 *result = arena->base + arena->used + alignmentOffset;
  arena->used += size + alignmentOffset;
#else
  u8 *result = 0;

  Arena *current = arena->current;
  usz alignedPos = flags.alignment ? ALIGN_POW_2(current->used, flags.alignment) : current->used;
  usz newPos = alignedPos + size;
  if(newPos > current->capacity)
  {
    usz allocSize = MAX(size, current->capacity);
    Arena *newBlock = gs_arenaAcquire(allocSize);
    newBlock->prev = current;
    arena->current = newBlock;
    
    current = arena->current;
    alignedPos = flags.alignment ? ALIGN_POW_2(current->used, flags.alignment) : current->used;
    newPos = alignedPos + size;
  }

  ASSERT(newPos <= current->capacity);
  result = current->base + alignedPos;
  current->used = newPos;
#endif

  if(flags.clearToZero)
    {
      ZERO_SIZE(result, size);
    }  

  return(result);
}

#if OLD_ARENA
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
#endif

static inline void
arenaPopSize(Arena *arena, usz size)
{
#if OLD_ARENA
  arena->used -= size;
#else
  Arena *current = arena->current;
  usz sizeToPop = size;
  while(sizeToPop > current->used)
    {
      sizeToPop -= current->used;
      arena->current = current->prev;
      gs_arenaDiscard(current);
    
      current = arena->current;
    }

  current->used -= sizeToPop;
#endif
}

#if OLD_ARENA
#else
static inline usz
arenaGetPos(Arena *arena)
{
  usz pos = INT_FROM_PTR(arena->current->base) + arena->current->used;
  return(pos);
}

static inline void
arenaPopTo(Arena *arena, usz pos)
{
  Arena *current = arena->current;
  usz base = INT_FROM_PTR(current->base);
  usz max = base + current->used;
  while(pos < base || pos > max)
    {
      arena->current = current->prev;
      gs_arenaDiscard(current);
      
      current = arena->current;
      base = INT_FROM_PTR(current->base);
      max = base + current->used;
    }

  ASSERT(pos >= base && pos <= max);
  current->used = pos - base;
}
#endif

#if OLD_ARENA
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
#endif

#if OLD_ARENA
inline TemporaryMemory
arenaBeginTemporaryMemory(Arena *arena, usz size)
{
  TemporaryMemory result = {};
  result.base = arenaPushSize(arena, size, arenaFlagsZeroNoAlign());
  result.capacity = size;
  result.parent = arena;

  return(result);
}
#else
static inline TemporaryMemory
arenaBeginTemporaryMemory(Arena *arena)
{
  TemporaryMemory result = {};
  result.arena = arena;
  result.pos = arenaGetPos(arena);

  return(result);
}
#endif

#if OLD_ARENA
inline void
arenaEndTemporaryMemory(TemporaryMemory *tempMem)
{
  tempMem->parent->used -= tempMem->capacity;
}
#else
static inline void
arenaEndTemporaryMemory(TemporaryMemory tempMem)
{
  arenaPopTo(tempMem.arena, tempMem.pos);
}
#endif

#if OLD_ARENA
static inline void
arenaEnd(Arena *arena, bool clearToZero = false)
{
  if(clearToZero)
    {
      ZERO_SIZE(arena->base, arena->used);
    }
  arena->used = 0;
}
#else
static inline void
arenaEnd(Arena *arena)
{
  arenaPopTo(arena, 0);
}
#endif

#if OLD_ARENA
#else

static inline TemporaryMemory
arenaGetScratch(Arena **conflicts, usz conflictsCount)
{
  // NOTE: initialize scratch pool
  if(m__scratchPool[0] == 0)
    {
      Arena **scratchSlot = m__scratchPool;
      for(usz scratchIdx = 0; scratchIdx < ARENA_SCRATCH_POOL_COUNT; ++scratchIdx, ++scratchSlot)
	{
	  *scratchSlot = gs_arenaAcquire(0);
	}
    }

  // NOTE: get non-conflicting arena
  TemporaryMemory result = {};
  Arena **scratchSlot = m__scratchPool;
  for(usz scratchIdx = 0; scratchIdx < ARENA_SCRATCH_POOL_COUNT; ++scratchIdx, ++scratchSlot)
    {
      b32 isNonConflict = true;
      Arena **conflictPtr = conflicts;
      for(usz conflictIdx = 0; conflictIdx < conflictsCount; ++conflictIdx, ++conflictPtr)
	{
	  if(*scratchSlot == *conflictPtr)
	    {
	      isNonConflict = false;
	      break;
	    }
	}

      if(isNonConflict)
	{
	  result = arenaBeginTemporaryMemory(*scratchSlot);
	  break;
	}
    }

  return(result);
}

#define arenaReleaseScratch(scratch) arenaEndTemporaryMemory(scratch)
#endif
