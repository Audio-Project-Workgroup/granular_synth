#include "onnxruntime_c_api.h"

struct OnnxState
{
  const OrtApiBase *apiBase;	 
  const OrtApi *api;
  OrtEnv *env;
  OrtSession *session;		 
  OrtAllocator *sessionAllocator;
};

extern OnnxState onnxState;

inline void
ortPrintError(OrtStatus *status)
{
  if(status)
    {
      OrtErrorCode errorCode = onnxState.api->GetErrorCode(status);
      const char *errorMessage = onnxState.api->GetErrorMessage(status);
      fprintf(stderr, "ERROR: ORT error: %d: %s\n", errorCode, errorMessage);
	      
      onnxState.api->ReleaseStatus(status);
    }
}
