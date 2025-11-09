#define PLUGIN_PARAMETER_TOOLTIPS 1

inline bool
pluginReadBooleanParameter(PluginBooleanParameter *param)
{
  return(param->value);
}

inline void
pluginSetBooleanParameter(PluginBooleanParameter *param, bool value)
{
  param->value = value;
}

inline void
initializeFloatParameter(PluginFloatParameter *param, PluginParameterInitData initData,		
			 ParameterTransform *transform = defaultTransform)
{
  param->currentValue.asFloat = param->targetValue.asFloat = initData.init;
  param->range = makeRange(initData.min, initData.max);
  param->processingTransform = transform;
}

inline r32
pluginReadFloatParameter(PluginFloatParameter *param)
{
  ParameterValue currentValue = {};
  currentValue.asInt = gsAtomicLoad(&param->currentValue.asInt);

  return(currentValue.asFloat);
}

static inline r32
pluginReadFloatParameterProcessing(PluginFloatParameter *param)
{
  r32 resultRaw = pluginReadFloatParameter(param);
  r32 result = param->processingTransform(resultRaw);
  return(result);
}

inline void
pluginSetFloatParameter(PluginFloatParameter *param, r32 value, r32 changeTimeMS = 10)
{
  ParameterValue targetValue = {};
  targetValue.asFloat = clampToRange(value, param->range);
  gsAtomicStore(&param->targetValue.asInt, targetValue.asInt);

  ParameterValue currentValue = {};
  currentValue.asInt = gsAtomicLoad(&param->currentValue.asInt);   

  ParameterValue dValue = {};
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  dValue.asFloat = (targetValue.asFloat - currentValue.asFloat)/changeTimeSamples;  
  gsAtomicStore(&param->dValue.asInt, dValue.asInt);
}

inline void
pluginOffsetFloatParameter(PluginFloatParameter *param, r32 inc, r32 changeTimeMS = 10)
{
  ParameterValue currentValue = {};
  currentValue.asInt = gsAtomicLoad(&param->currentValue.asInt);   

  ParameterValue targetValue = {};
  targetValue.asFloat = clampToRange(currentValue.asFloat + inc, param->range);
  gsAtomicStore(&param->targetValue.asInt, targetValue.asInt);

  ParameterValue dValue = {};
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  dValue.asFloat = (targetValue.asFloat - currentValue.asFloat)/changeTimeSamples;  
  gsAtomicStore(&param->dValue.asInt, dValue.asInt);
}

inline r32
pluginUpdateFloatParameter(PluginFloatParameter *param)
{
  // NOTE: should be called once per sample, per parameter
  r32 rangeLen = getLength(param->range);
  r32 err = 0.001f;

  ParameterValue targetValue = {};
  ParameterValue currentValue = {};
  targetValue.asInt = gsAtomicLoad(&param->targetValue.asInt);
  currentValue.asInt = gsAtomicLoad(&param->currentValue.asInt);

  r32 resultRaw = currentValue.asFloat;

  if(gsAbs(targetValue.asFloat - currentValue.asFloat)/rangeLen > err)
    {
      ParameterValue dValue = {};
      dValue.asInt = gsAtomicLoad(&param->dValue.asInt);

      ParameterValue newValue = {};      
      newValue.asFloat = currentValue.asFloat + dValue.asFloat;
      resultRaw = newValue.asFloat;
      
      if(gsAtomicCompareAndSwap(&param->currentValue.asInt, currentValue.asInt, newValue.asInt) !=
	 currentValue.asInt)
	{
	  logString("WARNING: Atomic CAS failed to modify parameter!");
	}
    }

  r32 result = param->processingTransform(resultRaw);
  return(result);
}

enum PluginParameterType
  {
    PluginParameterType_float,
    PluginParamterType_bool,
  };

struct PluginParameter
{
  PluginParameterType type;

  union
  {
    PluginFloatParameter floatParam;
    PluginBooleanParameter boolParam;
  };
};
