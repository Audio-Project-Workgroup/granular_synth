struct FloatBuffer
{
  usz count;
  r32 *vals;
};

struct ComplexBuffer
{
  usz count;
  r32 *reVals;
  r32 *imVals;
};

static inline Buffer
bufferMake(void *data, usz size)
{
  Buffer result = {};
  result.size = size;
  result.contents = (u8*)data;
  return(result);
}

static inline FloatBuffer
bufferMakeFloat(r32 *vals, usz count)
{
  FloatBuffer result = {};
  result.count = count;
  result.vals = vals;
  return(result);
}

static inline ComplexBuffer
bufferMakeComplex(r32 *reVals, r32 *imVals, usz count)
{
  ComplexBuffer result = {};
  result.count = count;
  result.reVals = reVals;
  result.imVals = imVals;
  return(result);
}

#define bufferReadStruct(buf, type) (type*)bufferReadSize_(buf, sizeof(type))
#define bufferReadArray(buf, count, type) (type*)bufferReadSize_(buf, (count)*sizeof(type))
static inline u8*
bufferReadSize_(Buffer *buf, usz size)
{
  u8 *result = 0;
  ASSERT(size <= buf->size);
  if(size <= buf->size)
    {
      result = buf->contents;
      buf->size -= size;
      buf->contents += size;
    }
  return(result);
}
