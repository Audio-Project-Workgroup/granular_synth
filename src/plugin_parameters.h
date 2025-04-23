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
  currentValue.asInt = globalPlatform.atomicLoad(&param->currentValue.asInt);

  return(currentValue.asFloat);
}

inline void
pluginSetFloatParameter(PluginFloatParameter *param, r32 value, r32 changeTimeMS = 10)
{
  ParameterValue targetValue = {};
  targetValue.asFloat = clampToRange(value, param->range);
  globalPlatform.atomicStore(&param->targetValue.asInt, targetValue.asInt);

  ParameterValue currentValue = {};
  currentValue.asInt = globalPlatform.atomicLoad(&param->currentValue.asInt);   

  ParameterValue dValue = {};
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  dValue.asFloat = (targetValue.asFloat - currentValue.asFloat)/changeTimeSamples;  
  globalPlatform.atomicStore(&param->dValue.asInt, dValue.asInt);
}

inline void
pluginOffsetFloatParameter(PluginFloatParameter *param, r32 inc, r32 changeTimeMS = 10)
{
  ParameterValue currentValue = {};
  currentValue.asInt = globalPlatform.atomicLoad(&param->currentValue.asInt);   

  ParameterValue targetValue = {};
  targetValue.asFloat = clampToRange(currentValue.asFloat + inc, param->range);
  globalPlatform.atomicStore(&param->targetValue.asInt, targetValue.asInt);

  ParameterValue dValue = {};
  r32 changeTimeSamples = 0.001f*changeTimeMS*(r32)INTERNAL_SAMPLE_RATE;
  dValue.asFloat = (targetValue.asFloat - currentValue.asFloat)/changeTimeSamples;  
  globalPlatform.atomicStore(&param->dValue.asInt, dValue.asInt);
}

inline void
pluginUpdateFloatParameter(PluginFloatParameter *param)
{
  // NOTE: should be called once per sample, per parameter
  r32 rangeLen = getLength(param->range);
  r32 err = 0.001f;

  ParameterValue targetValue = {};
  ParameterValue currentValue = {};
  targetValue.asInt = globalPlatform.atomicLoad(&param->targetValue.asInt);
  currentValue.asInt = globalPlatform.atomicLoad(&param->currentValue.asInt);

  if(Abs(targetValue.asFloat - currentValue.asFloat)/rangeLen > err)
    {
      ParameterValue dValue = {};
      dValue.asInt = globalPlatform.atomicLoad(&param->dValue.asInt);

      ParameterValue newValue = {};      
      newValue.asFloat = currentValue.asFloat + dValue.asFloat;
      
      if(globalPlatform.atomicCompareAndSwap(&param->currentValue.asInt, currentValue.asInt, newValue.asInt) !=
	 currentValue.asInt)
	{
	  logString("WARNING: atomicCAS failed to modify parameter!");
	}
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
