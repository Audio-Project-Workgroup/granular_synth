enum WindowType
{
  WindowShape_hann = 0,
  WindowShape_sine = 1,
  WindowShape_triangle = 2,
  WindowShape_rectangle = 3,
  WindowShape_count,
};

#define PLUGIN_PARAMETER_XLIST \
  X(none, 0.f, 0.f, 0.f) \
    X(volume, -60.f, 0.f, -1.f)	       \
    X(density, -10.f, 10.f, 0.f)	       \
    X(pan, -1.f, 1.f, 0.f)		       \
    X(size, 1024.f, 16000.f, 2600.f)		       \
    X(window, 0, WindowShape_count - 1, WindowShape_hann)			       \
    X(spread, 0.f, 1.0f, 0.5f)			       \
    X(mix, 0.f, 1.f, 0.5f) \
    X(offset, 1.f, 40000.f, 1024.f)			       \
    X(pitch, 0.f, 0.f, 0.f)			       \
    X(stretch, 0.f, 0.f, 0.f)			       

// Plugin Parameter enumeration to link with midi CC
enum PluginParameterEnum
{
#define X(name, min, max, init) PluginParameter_##name,
  PLUGIN_PARAMETER_XLIST
#undef X
  PluginParameter_count,
};

struct PluginParameterInitData
{
  const char *name;
  r32 min;
  r32 max;
  r32 init;
};

static PluginParameterInitData pluginParameterInitData[] =
  {
#define X(name, min, max, init) {#name, min, max, init},
    PLUGIN_PARAMETER_XLIST
#undef X
  };

struct PluginBooleanParameter
{
  bool value;
};

#define PARAMETER_TRANSFORM(name) r32 (name)(r32 val)
typedef PARAMETER_TRANSFORM(ParameterTransform);

union ParameterValue
{
  r32 asFloat;
  u32 asInt;
};

struct ParameterValueQueueEntry
{
  u32 index;
  ParameterValue value;
};

struct PluginFloatParameter
{
  RangeR32 range;  

  ParameterTransform *processingTransform;

  volatile u32 interacting;
  volatile ParameterValue currentValue;
  volatile ParameterValue targetValue;
  volatile ParameterValue dValue;
};

static PARAMETER_TRANSFORM(defaultTransform)
{
  return(val);
}

static PARAMETER_TRANSFORM(decibelsToAmplitude)
{
  r32 amplitude = gsPow(10.f, val/20.f);

  return(amplitude);
}

static PARAMETER_TRANSFORM(densityTransform)
{
  r32 grainsPlaying = gsPow(10.f, val/10.f);

  return(grainsPlaying);
}
