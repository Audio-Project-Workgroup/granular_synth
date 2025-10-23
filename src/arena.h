// TODO: It's not nice having to do this. Some effort should be made separating
//       declarations and implementations. Maybe then we can also only compile
//       shared code once, and link it in both host and plugin
#if defined(HOST_LAYER)
struct Arena;
static Arena* gsArenaAcquire(usz capacity);
static void   gsArenaDiscard(Arena *arena);
//extern void*  gs_acquireMemory(usz size);
//extern void   gs_discardMemory(void *mem, usz size);
#endif

struct Arena
{
  Arena *current;
  Arena *prev;
  usz base;
  usz capacity;
  usz pos;
};

#define ARENA_HEADER_SIZE 64
STATIC_ASSERT(sizeof(Arena) <= ARENA_HEADER_SIZE, ARENA_HEADER_CHECK);

struct TemporaryMemory
{
  Arena *arena;
  usz pos;
};

#define ARENA_SCRATCH_POOL_COUNT 2
static thread_var Arena *m__scratchPool[ARENA_SCRATCH_POOL_COUNT] = {};

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

#define arenaPushSize(arena, size, ...) arenaPushSize_(arena, size, ##__VA_ARGS__)
#define arenaPushArray(arena, count, type, ...) (type *)arenaPushSize_(arena, count*sizeof(type), ##__VA_ARGS__)
#define arenaPushStruct(arena, type, ...) (type *)arenaPushSize_(arena, sizeof(type), ##__VA_ARGS__)

static inline u8 *
arenaPushSize_(Arena *arena, usz size, ArenaPushFlags flags = defaultArenaPushFlags())
{
  u8 *result = 0;

  Arena *current = arena->current;
  usz alignedPos = flags.alignment ? ALIGN_POW_2(current->pos, flags.alignment) : current->pos;
  usz newPos = alignedPos + size;
  if(newPos > current->capacity)
  {
    usz allocSize = MAX(size + ARENA_HEADER_SIZE, current->capacity);
    Arena *newBlock = gsArenaAcquire(allocSize);
    newBlock->base = current->base + current->pos;
    newBlock->prev = current;
    arena->current = newBlock;
    
    current = arena->current;
    alignedPos = flags.alignment ? ALIGN_POW_2(current->pos, flags.alignment) : current->pos;
    newPos = alignedPos + size;
  }

  ASSERT(newPos <= current->capacity);
  result = (u8*)current + alignedPos;
  current->pos = newPos;

  if(flags.clearToZero)
    {
      ZERO_SIZE(result, size);
    }  

  return(result);
}

static inline usz
arenaGetPos(Arena *arena)
{
  Arena *current = arena->current;
  usz result = current->base + current->pos;
  return(result);
}

static inline void
arenaPopTo(Arena *arena, usz pos)
{
  pos = MAX(pos, ARENA_HEADER_SIZE);
  Arena *current = arena->current;
  
  for(Arena *prev = 0; current->base >= pos; current = prev)
    {
      prev = current->prev;
      gsArenaDiscard(current);
    }

  arena->current = current;
  usz newPos = pos - current->base;
  ASSERT(newPos <= current->pos);
  current->pos = newPos;
}

static inline void
arenaPopSize(Arena *arena, usz size)
{
  usz pos = arenaGetPos(arena);
  ASSERT(size <= pos);
  usz newPos = pos - size;
  arenaPopTo(arena, newPos);
}

static inline TemporaryMemory
arenaBeginTemporaryMemory(Arena *arena)
{
  TemporaryMemory result = {};
  result.arena = arena;
  result.pos = arenaGetPos(arena);

  return(result);
}

static inline void
arenaEndTemporaryMemory(TemporaryMemory tempMem)
{
  arenaPopTo(tempMem.arena, tempMem.pos);
}

static inline void
arenaEnd(Arena *arena)
{
  arenaPopTo(arena, 0);
}

static inline TemporaryMemory
arenaGetScratch(Arena **conflicts, usz conflictsCount)
{
  // NOTE: initialize scratch pool
  if(m__scratchPool[0] == 0)
    {
      Arena **scratchSlot = m__scratchPool;
      for(usz scratchIdx = 0; scratchIdx < ARENA_SCRATCH_POOL_COUNT; ++scratchIdx, ++scratchSlot)
	{
	  *scratchSlot = gsArenaAcquire(KILOBYTES(128));
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
