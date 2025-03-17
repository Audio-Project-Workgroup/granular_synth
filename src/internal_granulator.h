struct GrainBuffer
{
    r32* samples[2];

    u32 writeIndex;
    u32 readIndex;

    u32 bufferSize;
};

enum WindowType {
    HANN,
    SINE,
    TRIANGLE,
    RECTANGULAR
};

struct Grain
{
    WindowType window;
    s32 samplesToPlay;
    u32 length;
    u32 rewrap_counter;
    u32 samplesTillRewrap;
    Grain* next;
    Grain* prev;
    bool onFreeList;
    r32* start[2];
};

struct GrainManager
{
    Arena* grainAllocator;
    Grain* grainPlayList;
    GrainBuffer* grainBuffer;
    u32 grainCount;

    r32 internal_clock;

    Grain* grainFreeList;
};


