#define PLUGIN_PARAMETER_XLIST \
  X(none, 0.f, 0.f, 0.f) \
    X(volume, 0.f, 1.f, 0.8f)	       \
    X(density, 0.1f, 20.f, 1.f)	       \
    X(pan, 0.f, 1.f, 0.5f)		       \
    X(size, 0.f, 16000.f, 2600.f)		       \
    X(offset, 0.f, 0.f, 0.f)			       \
    X(window, 0.f, 0.f, 0.f)			       \
    X(pitch, 0.f, 0.f, 0.f)			       \
    X(streach, 0.f, 0.f, 0.f)			       \
    X(spread, 0.f, 1.0f, 0.5f)			       \
    X(mix, 0.f, 1.f, 0.5f)

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

struct PluginFloatParameter
{
  RangeR32 range;

  ParameterTransform *processingTransform;

  volatile u32 mutex;
  union
  {
    struct
    {
      r32 currentValue;
      r32 targetValue;
      r32 dValue;
    };
    struct
    {
      volatile u32 currentValue_AsInt;
      volatile u32 targetValue_AsInt;
      volatile u32 dValue_AsInt;
    };
  };   
};

static PARAMETER_TRANSFORM(defaultTransform)
{
  return(val);
}
