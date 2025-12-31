struct BufferStream;
#define BUFFER_STREAM_REFILL_PROC(name) void (name) (BufferStream *stream)
typedef BUFFER_STREAM_REFILL_PROC(BufferStreamRefill);

struct BufferStream
{
  u8 *start;
  u8 *end;
  u8 *at;

  BufferStreamRefill *refill;
};
