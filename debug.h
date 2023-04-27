
#if DEBUG_SYSTEM_API

#define DEFAULT_DEBUG_LIB "./bin/lib_debug_system" PLATFORM_RUNTIME_LIB_EXTENSION

inline umm
HashArenaBlock(memory_arena *Arena)
{
  umm Result = (umm)Arena;
  return Result;
}

inline umm
HashArena(memory_arena *Arena)
{
  umm Result = (umm)Arena->Start;
  return Result;
}


#if 0
// TODO(Jesse, id: 161, tags: back_burner, debug_recording): Reinstate this!
enum debug_recording_mode
{
  RecordingMode_Clear,
  RecordingMode_Record,
  RecordingMode_Playback,

  RecordingMode_Count,
};

#define DEBUG_RECORD_INPUT_SIZE 3600
struct debug_recording_state
{
  s32 FramesRecorded;
  s32 FramesPlayedBack;
  debug_recording_mode Mode;

  memory_arena RecordedMainMemory;

  hotkeys Inputs[DEBUG_RECORD_INPUT_SIZE];
};
#endif


struct memory_record;
typedef b32 (*meta_comparator)(memory_record*, memory_record*);

struct called_function
{
  const char* Name;
  u32 CallCount;
};

struct debug_draw_call
{
  const char * Caller;
  u32 N;
  u32 Calls;
};

struct min_max_avg_dt
{
  r64 Min;
  r64 Max;
  r64 Avg;
};

struct cycle_range
{
  u64 StartCycle;
  u64 TotalCycles;
};

struct memory_arena_stats
{
  u64 Allocations;
  u64 Pushes;

  u64 TotalAllocated;
  u64 Remaining;
};
poof(are_equal(memory_arena_stats))
#include <generated/are_equal_memory_arena_stats.h>

struct debug_profile_scope;
struct debug_scope_tree;

enum debug_context_switch_type
{
  ContextSwitch_Undefined,

  ContextSwitch_On,
  ContextSwitch_Off
};

struct context_switch_event;
struct debug_context_switch_event
{
  debug_context_switch_type Type;
  u32 ProcessorNumber;
  u64 CycleCount;

  /* context_switch_event *SystemEvent; */
};

#define MAX_CONTEXT_SWITCH_EVENTS ((u32)Kilobytes(1))

struct debug_context_switch_event_buffer
{
  u32 At;
  u32 End; // one-past-last
  debug_context_switch_event *Events;
};

// TODO(Jesse): Metaprogram this now that we have more flexible parameters
struct debug_context_switch_event_buffer_stream_block
{
  umm MinCycles;
  umm MaxCycles;
  debug_context_switch_event_buffer Buffer;
  debug_context_switch_event_buffer_stream_block *Next;
};

struct debug_context_switch_event_buffer_stream
{
  debug_context_switch_event_buffer_stream_block *FirstBlock;
  debug_context_switch_event_buffer_stream_block *CurrentBlock;

  debug_context_switch_event_buffer_stream_block *FirstFreeBlock;
};


template <typename T> b32 BufferHasRoomFor(T *Buffer, u32 VertsToPush);

link_internal debug_context_switch_event*
GetLatest(debug_context_switch_event_buffer *Buf)
{
  debug_context_switch_event *Result = {};
  if (Buf->At) { Result = Buf->Events + (Buf->At-1); }
  return Result;
}

link_internal debug_context_switch_event*
GetLatest(debug_context_switch_event_buffer_stream_block *Block)
{
  debug_context_switch_event *Result = GetLatest(&Block->Buffer);
  return Result;
}


link_internal debug_context_switch_event*
GetLatest(debug_context_switch_event_buffer_stream *Stream)
{
  debug_context_switch_event *Result = GetLatest(Stream->CurrentBlock);
  return Result;
}


struct debug_thread_state
{
  memory_arena *Memory;
  memory_arena *MemoryFor_debug_profile_scope; // Specifically for allocationg debug_profile_scope structs
  memory_record *MetaTable;

  debug_scope_tree *ScopeTrees;
  debug_profile_scope *FirstFreeScope;

  mutex_op_array *MutexOps;

  // Note(Jesse): This must not straddle a cache line;
  // on x86, reads are defined to be atomic of they do not straddle a cache line
  // multiple threads read from the main threads copy of this
  //
  // TODO(Jesse): What about other chips? Should we just use atomic read instructions?
  volatile u32 WriteIndex;
  u32 ThreadId;

  debug_context_switch_event_buffer_stream *ContextSwitches;


  // TODO(Jesse): Make a 32-bit define instead
#if EMCC
  u8 Pad[36];
#else
  /* u8 Pad[12]; */
#endif
};
CAssert(sizeof(debug_thread_state) == CACHE_LINE_SIZE);

struct unique_debug_profile_scope
{
  const char* Name;
  u32 CallCount;
  u64 TotalCycles;
  u64 MinCycles = u64_MAX;
  u64 MaxCycles;

  debug_profile_scope* Scope;
  unique_debug_profile_scope* NextUnique;
};

struct selected_memory_arena
{
  umm ArenaAddress;
  umm ArenaBlockAddress;
};

#define MAX_SELECTED_ARENAS 128
struct selected_arenas
{
  u32 Count;
  selected_memory_arena Arenas[MAX_SELECTED_ARENAS];
};

struct frame_stats
{
  u64 TotalCycles;
  u64 StartingCycle;
  r64 FrameMs;
};

struct registered_memory_arena
{
  memory_arena *Arena;
  const char* Name;
  s32 ThreadId;
  b32 Expanded;
};

enum event_tracing_status
{
  EventTracingStatus_Unstarted,

  EventTracingStatus_Starting,
  EventTracingStatus_Running,
  EventTracingStatus_Error,
};

global_variable volatile event_tracing_status Global_EventTracingStatus = {};



/* #include <bonsai_debug/headers/api.h> */
/* global_variable debug_profile_scope NullDebugProfileScope = {}; */

#endif

