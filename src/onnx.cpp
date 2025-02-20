#include "onnx.h"

OnnxState onnxState = {};

static OnnxState
onnxInitializeState(const ORTCHAR_T *modelPath)
{
  OnnxState result = {};
  result.apiBase = OrtGetApiBase();
  ASSERT(result.apiBase);
  result.api = result.apiBase->GetApi(ORT_API_VERSION);
  ASSERT(result.api);
  OrtStatus *ortStatus = 0;

  ortStatus = result.api->GetAllocatorWithDefaultOptions(&result.sessionAllocator);
  ortPrintError(ortStatus);

  ortStatus = result.api->CreateEnv(ORT_LOGGING_LEVEL_VERBOSE, "GranularSynthTestLog", &result.env);
  ortPrintError(ortStatus);

  OrtSessionOptions *ortSessionOptions = 0;
  ortStatus = result.api->CreateSessionOptions(&ortSessionOptions);
  ortPrintError(ortStatus);
  
  ortStatus = result.api->CreateSession(result.env, modelPath, ortSessionOptions, &result.session);
  ortPrintError(ortStatus);
  result.api->ReleaseSessionOptions(ortSessionOptions);

  return(result);
}

PLATFORM_RUN_MODEL(platformRunModel)
{
  char *inputName, *outputName;
  onnxState.api->SessionGetInputName(onnxState.session, 0, onnxState.sessionAllocator, &inputName);
  onnxState.api->SessionGetOutputName(onnxState.session, 0, onnxState.sessionAllocator, &outputName);

  const char *inputNames[] = {inputName};
  const char *outputNames[] = {outputName};
  
  OrtStatus *status = 0;
  OrtMemoryInfo *inputMemInfo = 0;
  status = onnxState.api->CreateCpuMemoryInfo(OrtArenaAllocator, (OrtMemType)0, &inputMemInfo);
  ortPrintError(status);

  OrtValue *inputTensor = 0;
  usz inputLengthInBytes = 2*inputLength*sizeof(r32);
  const s64 inputShape[] = {1, 2, inputLength};
  usz inputShapeLen = ARRAY_COUNT(inputShape);
  ONNXTensorElementDataType inputDataType = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;  
  status = onnxState.api->CreateTensorWithDataAsOrtValue(inputMemInfo, inputData, inputLengthInBytes,
						  inputShape, inputShapeLen, inputDataType, &inputTensor);
  ortPrintError(status);

  const OrtValue *inputTensors[] = {inputTensor};
  OrtValue *outputTensor = 0;
  status = onnxState.api->Run(onnxState.session, 0, inputNames, inputTensors, 1, outputNames, 1, &outputTensor);
  ortPrintError(status);

  void *outputData = 0; // TODO: do we need to free this thing?
  status = onnxState.api->GetTensorMutableData(outputTensor, &outputData);
  ortPrintError(status);

  onnxState.api->ReleaseMemoryInfo(inputMemInfo);
  onnxState.api->ReleaseValue(inputTensor);
  onnxState.api->ReleaseValue(outputTensor);
  onnxState.api->AllocatorFree(onnxState.sessionAllocator, inputName);
  onnxState.api->AllocatorFree(onnxState.sessionAllocator, outputName);

  return(outputData);
}

