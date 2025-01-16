#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#define M_TAU (2.0*M_PI)

//
// v2
//

static inline v2
V2(r32 x, r32 y)
{
  v2 result = {x, y};

  return(result);
}

static inline v2
operator-(v2 v)
{
  v2 result = V2(-v.x, -v.y);

  return(result);
}

static inline v2
operator+(v2 v, v2 w)
{
  v2 result = V2(v.x + w.x, v.y + w.y);

  return(result);
}

static inline v2 &
operator+=(v2 &v, v2 w)
{
  v = v + w;

  return(v);
}

static inline v2
operator-(v2 v, v2 w)
{
  v2 result = v + (-w);

  return(result);
}

static inline v2 &
operator-=(v2 &v, v2 w)
{
  v = v - w;

  return(v);
}

static inline v2
operator*(r32 a, v2 v)
{
  v2 result = V2(a*v.x, a*v.y);

  return(result);
}

static inline v2
operator*(v2 v, r32 a)
{
  v2 result = a*v;

  return(result);
}

static inline v2 &
operator*=(v2 &v, r32 a)
{
  v = v * a;

  return(v);
}

static inline v2
operator/(v2 v, r32 a)
{
  v2 result = v * (1.f/a);

  return(result);
}

static inline v2 &
operator/=(v2 &v, r32 a)
{
  v = v/a;

  return(v);
}

static inline v2
hadamard(v2 v, v2 w)
{
  v2 result = V2(v.x*w.x, v.y*w.y);

  return(result);
}

//
// v3
//

static inline v3
V3(r32 x, r32 y, r32 z)
{
  v3 result = {x, y, z};

  return(result);
}

//
// v4
// 

static inline v4
V4(r32 x, r32 y, r32 z, r32 w)
{
  v4 result = {x, y, z, w};

  return(result);
}

//
// Rect2
//

static inline Rect2
rectMinMax(v2 min, v2 max)  
{
  Rect2 result = {};
  result.min = min;
  result.max = max;

  return(result);
}

static inline Rect2
rectMinDim(v2 min, v2 dim)
{
  Rect2 result = rectMinMax(min, min + dim);
    
  return(result);
}

static inline Rect2
rectCenterHalfDim(v2 center, v2 halfDim)
{
  Rect2 result = {};
  result.min = center - halfDim;
  result.max = center + halfDim;

  return(result);
}

static inline Rect2
rectCenterDim(v2 center, v2 dim)
{
  Rect2 result = rectCenterHalfDim(center, 0.5f*dim);

  return(result);
}

static inline v2
getDim(Rect2 rect)
{
  v2 result = rect.max - rect.min;

  return(result);
}

static inline v2
getCenter(Rect2 rect)
{
  v2 result = 0.5f*getDim(rect);

  return(result);
}

static inline bool
isInRectangle(Rect2 r, v2 v)
{
  bool result = (r.min.x <= v.x &&
		 r.min.y <= v.y &&
		 v.x <= r.max.x &&
		 v.y <= r.max.y);
  return(result);
}
