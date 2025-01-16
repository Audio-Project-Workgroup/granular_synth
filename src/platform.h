// operating-system-dependent functions/types

#ifdef _WIN32

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

inline s64
s64FromLARGE_INTEGER(LARGE_INTEGER largeInt)
{
  s64 result = (s64)largeInt.HighPart << 32;
  result |= largeInt.LowPart;

  return(result);
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

struct PluginCode
{
  bool isValid;

  u64 lastWriteTime;

  HMODULE pluginCode;
  RenderNewFrame *renderNewFrame;
  AudioProcess *audioProcess;
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
	  result.renderNewFrame = (RenderNewFrame *)GetProcAddress(result.pluginCode, "renderNewFrame");
	  result.audioProcess = (AudioProcess *)GetProcAddress(result.pluginCode, "audioProcess");
	  result.isValid = result.renderNewFrame && result.audioProcess;
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
	  result.renderNewFrame = 0;
	  result.audioProcess = 0;
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
  code->renderNewFrame = 0;
  code->audioProcess = 0;
}

PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile)
{
  ReadFileResult result = {};
  
  HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,
				  OPEN_EXISTING, 0, 0);
  if(fileHandle != INVALID_HANDLE_VALUE)
    {
      LARGE_INTEGER fileSize;
      if(GetFileSizeEx(fileHandle, &fileSize))
	{
	  s64 fileSizeInt = s64FromLARGE_INTEGER(fileSize);
	  ASSERT(fileSizeInt > 0);
	  result.contentsSize = fileSizeInt;
	  result.contents = arenaPushSize(allocator, result.contentsSize + 1);
	  
	  u8 *dest = result.contents;
	  usz totalBytesToRead = result.contentsSize;
	  u32 bytesToRead = MIN(totalBytesToRead, U32_MAX);
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
		  result.contentsSize = 0;
		  break;
		}

	      if(result.contents)
		{
		  result.contents[result.contentsSize++] = 0; // NOTE: null termination
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

#else

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

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

struct PluginCode
{
  bool isValid;
  u64 lastWriteTime;
  
  void *pluginCode;
  RenderNewFrame *renderNewFrame;
  AudioProcess *audioProcess;
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
	  result.renderNewFrame = (RenderNewFrame *)dlsym(result.pluginCode, "renderNewFrame");
	  result.audioProcess   = (AudioProcess *)dlsym(result.pluginCode, "audioProcess");
	  
	  result.isValid = result.renderNewFrame && result.audioProcess;
	}
      else
	{
	  fprintf(stderr, "failed to get plugin code: %s\n", dlerror());
	}
    }

  if(!result.isValid)
    {
      result.renderNewFrame = 0;
      result.audioProcess   = 0;
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
  code->renderNewFrame = 0;
  code->audioProcess = 0;
}

typedef ssize_t ssz;
#define PLATFORM_MAX_READ_SIZE 0x7FFFF000

PLATFORM_READ_ENTIRE_FILE(platformReadEntireFile)
{
  ReadFileResult result = {};
  
  int fileHandle = open(filename, O_RDONLY);
  if(fileHandle != -1)
    {
      struct stat fileStatus;
      if(fstat(fileHandle, &fileStatus) != -1)
	{
	  result.contentsSize = fileStatus.st_size;
	  result.contents = arenaPushSize(allocator, result.contentsSize + 1);
	  
	  u8 *dest = result.contents;
	  usz totalBytesToRead = result.contentsSize;
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
		  fprintf(stderr, "ERROR: read failed\n");
		  result.contents = 0;
		  result.contentsSize = 0;
		  break;
		}

	      if(result.contents)
		{
		  result.contents[result.contentsSize++] = 0; // NOTE: null termination
		}
	    }
	}
      else
	{
	  fprintf(stderr, "ERROR: fstat failed\n");
	}

      close(fileHandle);
    }
  else
    {
      fprintf(stderr, "ERROR: open failed\n");
    }

  return(result);
}

#endif 

PLATFORM_FREE_FILE_MEMORY(platformFreeFileMemory)
{
  arenaPopSize(allocator, file.contentsSize);
}
