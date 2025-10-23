// operating-system-dependent functions/types
#pragma once

#define BASE_THREAD_PROC(name) void (name)(void *data)
typedef BASE_THREAD_PROC(BaseThreadProc);

static void
baseThreadEntry(BaseThreadProc *func, void *data)
{  
  func(data);
}

struct OSThread
{
  void *handle;
  void *id;
  
  BaseThreadProc *func;
  void *data;

  volatile u32 lock;
  u32 cancel;
  u32 finished;
};

static void threadCreate(OSThread *thread, BaseThreadProc *func, void *data);
//static void threadStart(OSThread thread);

static u32 atomicLoad(volatile u32 *src);
static u32 atomicStore(volatile u32 *dest, u32 value);
static u32 atomicAdd(volatile u32 *addend, u32 value);
static u32 atomicCompareAndSwap(volatile u32 *value, u32 oldval, u32 newval);
static void *atomicCompareAndSwapPointers(volatile void *value, void *oldval, void *newval);

#if OS_WINDOWS

#include <windows.h>

#define FORMAT_ERROR_AS_STRING(code, string)				\
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, \
		 NULL, (code), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		 (LPSTR)&(string), 0, NULL)

inline u64
u64FromFILETIME(FILETIME filetime)
{
  u64 result = (u64)filetime.dwHighDateTime << 32;
  result |= filetime.dwLowDateTime;

  return(result);
}

inline u64
s64FromLARGE_INTEGER(LARGE_INTEGER largeInt)
{
  s64 result = (s64)largeInt.HighPart << 32;
  result |= largeInt.LowPart;

  return(result);
}

inline u64
getOSTimerFreq(void)
{
  LARGE_INTEGER timerFreq;
  QueryPerformanceFrequency(&timerFreq);

  return(timerFreq.QuadPart);
}

inline u64
readOSTimer(void)
{
  LARGE_INTEGER cycleCount;
  QueryPerformanceCounter(&cycleCount);

  return(cycleCount.QuadPart);
}

inline FILETIME
getLastWriteTime(char *filename)
{
  FILETIME result = {};
  
  WIN32_FIND_DATA findData;
  HANDLE findHandle = FindFirstFileA(filename, &findData);
  if(findHandle != INVALID_HANDLE_VALUE)
    {
      result = findData.ftLastWriteTime;
      FindClose(findHandle);
    }

  return(result);
}

inline u64
getLastWriteTimeU64(char *filename)
{
  FILETIME filetime = getLastWriteTime(filename);
  u64 result = u64FromFILETIME(filetime);

  return(result);
}

inline void
msecWait(u32 msecsToWait)
{
  Sleep(msecsToWait);
}

//
// thread 
//

static DWORD WINAPI
win32ThreadEntry(void *data)
{
  OSThread *thread = (OSThread *)data;
  baseThreadEntry(thread->func, thread->data);
  
  return(0);
}

static void
threadCreate(OSThread *thread, BaseThreadProc *func, void *threadData)
{
  thread->lock = 0;
  thread->cancel = 0;
  thread->finished = 0;

  thread->func = func;
  thread->data = threadData;

  DWORD id;
  //thread->handle = CreateThread(0, 0, win32ThreadEntry, thread, CREATE_SUSPENDED, &id);
  thread->handle = CreateThread(0, 0, win32ThreadEntry, thread, 0, &id);
  if(thread->handle == NULL)
    {
      DWORD errorCode = GetLastError();
      char *errorMessage;
      FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
      fprintf(stderr, "failed to get plugin code: %s\n", errorMessage);
    }
  else
    {
      thread->id = (void *)(uintptr_t)id;
    } 
}
#if 0
static void
threadStart(OSThread thread)
{  
  if(ResumeThread(thread.handle) == (DWORD)-1)
    {
      DWORD errorCode = GetLastError();
      char *errorMessage;
      FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
      fprintf(stderr, "failed to get plugin code: %s\n", errorMessage);
    }
}
#endif

// TODO: test on windows
static void
threadDestroy(OSThread *thread)
{
  // NOTE: take lock, set cancel, release lock
  while(atomicCompareAndSwap(&thread->lock, 0, 1) != 0)
    {
      msecWait(1);
    }
  thread->cancel = 1;
  atomicStore(&thread->lock, 0);

  // NOTE: take lock, check finished, if not finished, release lock and loop, else return
  for(;;)
    {
      while(atomicCompareAndSwap(&thread->lock, 0, 1) != 0)
	{
	  msecWait(1);
	}
      if(thread->finished == 1)
	{
	  return;
	}
      else
	{
	  atomicStore(&thread->lock, 0);
	  msecWait(1);
	}
    }
}

//
// memory
//

static void*
platformAllocateMemory(usz size)
{
  void *result = VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  return(result);
}

static void
platformFreeMemory(void *memory, usz size)
{
  VirtualFree(memory, size, MEM_RELEASE);
}

//
// atomic operations
//

static u32
atomicLoad(volatile u32 *src)
{
  u32 result = InterlockedExchangeAdd(src, 0);
  
  return(result);
}

#if 0
static void *
atomicLoadPointer(volatile void **src)
{
  void *result = InterlockedCompareExchangePointer((volatile PVOID *)src, 0, 0);

  return(result);
}
#endif

static u32
atomicStore(volatile u32 *dest, u32 value)
{ 
  u32 result = InterlockedExchange(dest, value);

  return(result);
}

static u32
atomicAdd(volatile u32 *addend, u32 value)
{
  u32 result = atomicLoad(addend);
  InterlockedAdd((volatile LONG *)addend, value);

  return(result);
}

static u32
atomicCompareAndSwap(volatile u32 *value, u32 oldval, u32 newval)
{
  return(InterlockedCompareExchange(value, newval, oldval));
}

static void *
atomicCompareAndSwapPointers(volatile void **value, void *oldval, void *newval)
{
  return(InterlockedCompareExchangePointer((volatile PVOID *)value, newval, oldval));
}

//
// plugin code loading
//

struct PluginCode
{
  bool isValid;

  u64 lastWriteTime;

  HMODULE pluginCode;
  PluginAPI pluginAPI;
};

static PluginCode
loadPluginCode(char *filename)
{
  PluginCode result = {};
  
  FILETIME filetime = getLastWriteTime(filename);
  result.lastWriteTime = u64FromFILETIME(filetime);
  if(result.lastWriteTime)
    {
      char filepath[32] = {};
      char extension[8] = {};
      separateExtensionAndFilepath(filename, filepath, extension);

      char tempFilename[64] = {};
      snprintf(tempFilename, ARRAY_COUNT(tempFilename), "%s_temp.%s", filepath, extension);
      CopyFile(filename, tempFilename, FALSE);
      result.pluginCode = LoadLibraryA(tempFilename);
      if(result.pluginCode)
	{
	  result.pluginAPI.gsRenderNewFrame =
	    (GS_RenderNewFrame *)GetProcAddress(result.pluginCode, "gsRenderNewFrame");
	  result.pluginAPI.gsAudioProcess =
	    (GS_AudioProcess *)GetProcAddress(result.pluginCode, "gsAudioProcess");
	  result.pluginAPI.gsInitializePluginState =
	    (GS_InitializePluginState *)GetProcAddress(result.pluginCode, "gsInitializePluginState");
	  result.isValid = (result.pluginAPI.gsRenderNewFrame &&
			    result.pluginAPI.gsAudioProcess &&
			    result.pluginAPI.gsInitializePluginState);
	}
      else
	{
	  DWORD errorCode = GetLastError();
	  char *errorMessage;
	  FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
	  fprintf(stderr, "failed to get plugin code: %s\n", errorMessage);
	}

      if(!result.isValid)
	{
	  result.pluginAPI.gsRenderNewFrame = 0;
	  result.pluginAPI.gsAudioProcess = 0;
	  result.pluginAPI.gsInitializePluginState = 0;
	}
    }

  return(result);
}

static void
unloadPluginCode(PluginCode *code)
{
  if(code->pluginCode)
    {
      FreeLibrary(code->pluginCode);
      code->pluginCode = 0;
    }

  code->isValid = false;
  code->pluginAPI.gsRenderNewFrame = 0;
  code->pluginAPI.gsAudioProcess = 0;
  code->pluginAPI.gsInitializePluginState = 0;
}

//
// file operations
//

static Buffer
platformReadEntireFile(char *filename, Arena *allocator)
{
  Buffer result = {};
  
  HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,
				  OPEN_EXISTING, 0, 0);
  if(fileHandle != INVALID_HANDLE_VALUE)
    {
      LARGE_INTEGER fileSize;
      if(GetFileSizeEx(fileHandle, &fileSize))
	{
	  s64 fileSizeInt = s64FromLARGE_INTEGER(fileSize);
	  ASSERT(fileSizeInt > 0);
	  result.size = fileSizeInt;
	  result.contents = arenaPushSize(allocator, result.size + 1);
	  
	  u8 *dest = result.contents;
	  usz totalBytesToRead = result.size;
	  u32 bytesToRead = safeTruncateU64(totalBytesToRead);
	  while(totalBytesToRead)
	    {
	      DWORD bytesRead;
	      if(ReadFile(fileHandle, dest, bytesToRead, &bytesRead, 0))
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  DWORD errorCode = GetLastError();
		  char *errorMessage;
		  FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
		  fprintf(stderr, "ERROR: ReadFile failed: %s\n", errorMessage);
		  
		  result.contents = 0;
		  result.size = 0;
		  break;
		}

	      if(result.contents)
		{
		  result.contents[result.size] = 0; // NOTE: null termination
		}
	    }
	}
      else
	{
	  DWORD errorCode = GetLastError();
	  char *errorMessage;
	  FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
	  fprintf(stderr, "ERROR: GetFileSizeEx failed: %s\n", errorMessage);
	}

      CloseHandle(fileHandle);
    }
  else
    {
      DWORD errorCode = GetLastError();
      char *errorMessage;
      FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
      fprintf(stderr, "ERROR: CreateFileA failed: %s\n", errorMessage);
    }

  return(result);
}

static void
platformWriteEntireFile(char *filename, Buffer file)
{
  HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_WRITE, 0,
				  CREATE_ALWAYS, 0, 0);
  if(fileHandle != INVALID_HANDLE_VALUE)
    {
      void *fileMemory = file.contents;
      usz bytesRemaining = file.size;      
      while(bytesRemaining)
	{
	  u32 bytesToWrite = safeTruncateU64(bytesRemaining);
	  if(WriteFile(fileHandle, fileMemory, bytesToWrite, 0, 0))
	    {
	      bytesRemaining -= bytesToWrite;
	      fileMemory = (u8 *)fileMemory + bytesToWrite;
	    }
	  else
	    {
	      DWORD errorCode = GetLastError();
	      char *errorMessage;
	      FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
	      fprintf(stderr, "ERROR: WriteFile %s failed: %s\n", filename, errorMessage);
	    }
	}
      
      CloseHandle(fileHandle);
    }
}

static String8
platformGetPathToModule(void *handleToModule, void *functionInModule, Arena *allocator)
{
  String8 result = {};
  
  if(!handleToModule)
    {
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)functionInModule, (HMODULE *)&handleToModule);
    }

  char buffer[MAX_PATH];
  DWORD status = GetModuleFileNameA((HMODULE)handleToModule, buffer, MAX_PATH);
  if(status)
    {      
      result = arenaPushString(allocator, STR8_CSTR(buffer));
    }
  else
    {
      DWORD errorCode = GetLastError();
      char *errorMessage;
      FORMAT_ERROR_AS_STRING(errorCode, errorMessage);
      fprintf(stderr, "ERROR: GetModuleFilenameA failed: %s\n", errorMessage);
    }

  return(result);
}

#elif OS_LINUX || OS_MAC

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

inline u64
getOSTimerFreq(void)
{
  return(1000000);
}

inline u64
readOSTimer(void)
{
  struct timeval timeVal;
  gettimeofday(&timeVal, 0);
  
  u64 result = getOSTimerFreq()*(u64)timeVal.tv_sec + (u64)timeVal.tv_usec;
  return(result);
}

inline time_t
getLastWriteTime(char *filename)
{
  time_t result = {};

  struct stat statBuf;
  int statResult = stat(filename, &statBuf);
  if(statResult != -1)
    {
      result = statBuf.st_mtime;
    }

  return(result);
}

inline u64
getLastWriteTimeU64(char *filename)
{
  time_t writeTime = getLastWriteTime(filename);
  u64 result = (u64)writeTime;

  return(result);
}

inline void
msecWait(u32 msecsToWait)
{
  usleep(1000*msecsToWait);
}

//
// thread
//

#include <pthread.h>

static void *
posixThreadEntry(void *data)
{
  OSThread *thread = (OSThread *)data;
  baseThreadEntry(thread->func, thread->data);

  return(NULL);
}

static void
threadCreate(OSThread *thread, BaseThreadProc *func, void *data)
{
  thread->lock = 0;
  thread->cancel = 0;
  thread->finished = 0;

  thread->func = func;
  thread->data = data;

  pthread_t handle;
  //pthread_t *handleOut = (pthread_t *)&thread->handle;
  int result = pthread_create(&handle, NULL, posixThreadEntry, thread);
  if(result == 0)
    {
      thread->handle = (void *)(uintptr_t)handle;
    }
  else
    {
      const char *errorMessage = 0;
      switch(result)
	{	
	case EAGAIN: {errorMessage = "insufficient resources";} break;
	case EINVAL: {errorMessage = "invalid settings in attr";} break;
	case EPERM: {errorMessage = "no permission to set scheduling policy and parameters in attr";} break;
	}
      fprintf(stderr, "ERROR: threadCreate: pthread_create failed: %s\n", errorMessage);
    }
}

static void
threadDestroy(OSThread *thread)
{
  // NOTE: take lock, set cancel, release lock
  while(atomicCompareAndSwap(&thread->lock, 0, 1) != 0)
    {
      msecWait(1);
    }
  thread->cancel = 1;
  atomicStore(&thread->lock, 0);

  // NOTE: take lock, check finished, if not finished, release lock and loop, else return
  for(;;)
    {
      while(atomicCompareAndSwap(&thread->lock, 0, 1) != 0)
	{
	  msecWait(1);
	}
      if(thread->finished == 1)
	{
	  return;
	}
      else
	{
	  atomicStore(&thread->lock, 0);
	  msecWait(1);
	}
    }
}

//
// memory
//

#include <sys/mman.h>

static void*
platformAllocateMemory(usz size)
{
  void *result = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  return(result);
}

static void
platformFreeMemory(void *memory, usz size)
{
  munmap(memory, size);
}

//
// atomic operations
// 

#if COMPILER_GCC || COMPILER_CLANG

static u32
atomicLoad(volatile u32 *src)
{
  return(__atomic_load_n(src, __ATOMIC_ACQUIRE));
}

#if 0
static void *
atomicLoadPointer(volatile void **src)
{
  return(__atomic_load_n(src, __ATOMIC_ACQUIRE));
}
#endif

static u32
atomicStore(volatile u32 *dest, u32 value)
{
  u32 result = atomicLoad(dest);
  __atomic_store_n(dest, value, __ATOMIC_RELEASE);

  return(result);
}

static u32
atomicAdd(volatile u32 *addend, u32 value)
{
  return(__atomic_fetch_add(addend, value, __ATOMIC_ACQUIRE));
}

static u32
atomicCompareAndSwap(volatile u32 *value, u32 oldval, u32 newval)
{
  u32 *expectedPtr = &oldval;
  bool success = __atomic_compare_exchange_n(value, expectedPtr, newval, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);

  u32 result = success ? oldval : newval;
  return(result);
}

static void *
atomicCompareAndSwapPointers(volatile void **value, void *oldval, void *newval)
{
  usz *expectedPtr = (usz *)&oldval;
  bool success = __atomic_compare_exchange_n((volatile usz *)value, expectedPtr, (usz)newval, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);

  void *result = success ? oldval : newval;
  return(result);
}

#else
#error ERROR: unsupported compiler
#endif

//
// plugin code loading
//

struct PluginCode
{
  bool isValid;
  u64 lastWriteTime;
  
  void *pluginCode;
  PluginAPI pluginAPI;
};

static PluginCode
loadPluginCode(char *filename)
{
  PluginCode result = {};
  result.lastWriteTime = getLastWriteTime(filename);
  if(result.lastWriteTime)
    {
      result.pluginCode = dlopen(filename, RTLD_NOW);
      if(result.pluginCode)
	{
	  result.pluginAPI.gsRenderNewFrame
	    = (GS_RenderNewFrame*)dlsym(result.pluginCode, "gsRenderNewFrame");
	  result.pluginAPI.gsAudioProcess
	    = (GS_AudioProcess*)dlsym(result.pluginCode, "gsAudioProcess");
	  result.pluginAPI.gsInitializePluginState
	    = (GS_InitializePluginState*)dlsym(result.pluginCode, "gsInitializePluginState");
	  
	  result.isValid = (result.pluginAPI.gsRenderNewFrame &&
			    result.pluginAPI.gsAudioProcess &&
			    result.pluginAPI.gsInitializePluginState);
	}
      else
	{
	  fprintf(stderr, "failed to get plugin code: %s\n", dlerror());
	}
    }

  if(!result.isValid)
    {
      result.pluginAPI.gsRenderNewFrame	       = 0;
      result.pluginAPI.gsAudioProcess	       = 0;
      result.pluginAPI.gsInitializePluginState = 0;
    }

  return(result);
}

static void
unloadPluginCode(PluginCode *code)
{
  if(code->pluginCode)
    {
      if(dlclose(code->pluginCode) != 0)
	{
	  fprintf(stderr, "ERROR: dlclose failed: %s\n", dlerror());
	}
      code->pluginCode = 0;
    }

  code->isValid = false;
  code->pluginAPI.gsRenderNewFrame	  = 0;
  code->pluginAPI.gsAudioProcess	  = 0;
  code->pluginAPI.gsInitializePluginState = 0;
}

//
// file operations
//

typedef ssize_t ssz;
#define PLATFORM_MAX_READ_SIZE 0x7FFFF000

static Buffer
platformReadEntireFile(char *filename, Arena *allocator)
{
  Buffer result = {};

#if BUILD_DEBUG
  fprintf(stderr, "reading entire file: %s\n", filename);
#endif

  int fileHandle = open(filename, O_RDONLY);
  if(fileHandle != -1)
    {
      struct stat fileStatus;
      if(fstat(fileHandle, &fileStatus) != -1)
	{
	  result.size = fileStatus.st_size;
	  result.contents = arenaPushSize(allocator, result.size + 1);
	  
	  u8 *dest = result.contents;
	  usz totalBytesToRead = result.size;
	  usz bytesToRead = MIN(totalBytesToRead, PLATFORM_MAX_READ_SIZE);
	  while(totalBytesToRead)
	    {
	      ssz bytesRead = read(fileHandle, dest, bytesToRead);
	      if(bytesRead == bytesToRead)
		{
		  dest += bytesRead;
		  totalBytesToRead -= bytesRead;
		}
	      else
		{
		  fprintf(stderr, "ERROR: read failed: %s: %s\n", filename, strerror(errno));
		  result.contents = 0;
		  result.size = 0;
		  break;
		}

	      if(result.contents)
		{
		  result.contents[result.size] = 0; // NOTE: null termination
		}
	    }
	}
      else
	{
	  fprintf(stderr, "ERROR: fstat failed: %s: %s\n", filename, strerror(errno));
	}

      close(fileHandle);
    }
  else
    {
      fprintf(stderr, "ERROR: open failed: %s: %s\n", filename, strerror(errno));
    }

  return(result);
}

static void
platformWriteEntireFile(char *filename, Buffer file)
{
  int fileHandle = open(filename, O_CREAT | O_WRONLY | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(fileHandle != -1)
    {
      void *fileMemory = file.contents;
      usz bytesRemaining = file.size;
      while(bytesRemaining)
	{
	  u32 bytesToWrite = safeTruncateU64(bytesRemaining);
	  ssize_t bytesWritten = write(fileHandle, fileMemory, bytesToWrite);
	  if(bytesWritten == -1)
	    {
	      fprintf(stderr, "ERROR: write %s failed: %s\n", filename, strerror(errno));
	      break;
	    }
	  else
	    {
	      bytesRemaining -= bytesWritten;
	      fileMemory = (u8 *)fileMemory + bytesWritten;
	    }
	}

      close(fileHandle);
    }
  else
    {
      fprintf(stderr, "ERROR: open %s failed: %s\n", filename, strerror(errno));
    }
}

static String8
platformGetPathToModule(void *handleToModule, void *functionInModule, Arena *allocator)
{
  String8 result = {};
  Dl_info dlInfo = {};
  if(dladdr(functionInModule, &dlInfo))
    {
      if(dlInfo.dli_fname)
	{
	  result = arenaPushString(allocator, STR8_CSTR((char *)dlInfo.dli_fname));
	}
    }
  else
    {
      fprintf(stderr, "ERROR: could not match address to shared object: %p\n", functionInModule);
    }

  return(result);
}

#else
#error ERROR: unsupported OS
#endif 

static void
platformFreeFileMemory(Buffer file, Arena *allocator)
{
  arenaPopSize(allocator, file.size);
}

//static PLATFORM_GET_CURRENT_TIMESTAMP(platformGetCurrentTimestamp)
static u64
platformGetCurrentTimestamp(void)
{
  return(readOSTimer());
}

#if 0
inline u64
estimateCPUCyclesPerSecond(void)
{
  u64 millisecondsToWait = 100;
  u64 osFreq = getOSTimerFreq();
  u64 clocksToWait = millisecondsToWait*osFreq/1000;

  u64 clocksElapsed = 0;  
  u64 startClocks = readOSTimer();
  while(clocksElapsed < clocksToWait)
    {
      u64 clocks = readOSTimer();
      clocksElapsed = clocks - startClocks;
    }
}
#endif
