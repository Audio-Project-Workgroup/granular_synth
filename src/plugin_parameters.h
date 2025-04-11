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
  param->currentValue = param->targetValue = initData.init;
  param->range = makeRange(initData.min, initData.max);
  param->processingTransform = transform;
}

inline r32
pluginReadFloatParameter(PluginFloatParameter *param)
{
  u32 paramCurrentValue_AsInt = globalPlatform.atomicLoad(&param->currentValue_AsInt);
  r32 paramCurrentValue = *(r32 *)&paramCurrentValue_AsInt;
  //return(param->currentValue);
  return(paramCurrentValue);
}

inline void
pluginSetFloatParameter(PluginFloatParameter *param, r32 value, r32 changeTimeMS = 10)
{
  r32 paramTargetValue = clampToRange(value, param->range);
  u32 paramTargetValue_AsInt = *(u32 *)&paramTargetValue;
  globalPlatform.atomicStore(&param->targetValue_AsInt, paramTargetValue_AsInt);

  u32 paramCurrentValue_AsInt = globalPlatform.atomicLoad(&param->currentValue_AsInt);
  r32 paramCurrentValue = *(r32 *)&paramCurrentValue_AsInt;
  
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  r32 dValue = (paramTargetValue - paramCurrentValue)/changeTimeSamples;
  u32 dValue_AsInt = *(u32 *)&dValue;
  globalPlatform.atomicStore(&param->dValue_AsInt, dValue_AsInt);
}

inline void
pluginUpdateFloatParameter(PluginFloatParameter *param)
{
  // NOTE: should be called once per sample, per parameter
  r32 err = 0.001f;

  u32 paramTargetValue_AsInt = globalPlatform.atomicLoad(&param->targetValue_AsInt);
  u32 paramCurrentValue_AsInt = globalPlatform.atomicLoad(&param->currentValue_AsInt);
  r32 paramTargetValue = *(r32 *)&paramTargetValue_AsInt;
  r32 paramCurrentValue = *(r32 *)&paramCurrentValue_AsInt;
  if(Abs(paramTargetValue - paramCurrentValue) > err)
    {
      //param->currentValue += param->dValue;
      u32 dValue_AsInt = globalPlatform.atomicLoad(&param->dValue_AsInt);
      r32 dValue = *(r32 *)&dValue_AsInt;
      
      r32 newValue = paramCurrentValue + dValue;
      u32 newValue_AsInt = *(u32 *)&newValue;
      //globalPlatform.atomicStore(&param->currentValue_AsInt, newValue_AsInt);
      globalPlatform.atomicCompareAndSwap(&param->currentValue_AsInt, paramCurrentValue_AsInt, newValue_AsInt);
    }
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
