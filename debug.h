
#if BONSAI_DEBUG_SYSTEM_API

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


struct debug_thread_state
{
  memory_arena *Memory;
  memory_arena *MemoryFor_debug_profile_scope; // Specifically for allocationg debug_profile_scope structs
  memory_record *MetaTable;

  debug_scope_tree *ScopeTrees;
  debug_profile_scope *FirstFreeScope;

  mutex_op_array *MutexOps;

  volatile u32 WriteIndex; // Note(Jesse): This must not straddle a cache line on x86 because multiple threads read from the main threads copy of this

#if EMCC // NOTE(Jesse): Uhh, wtf?  Oh, EMCC pointers are 32 bits..? FML
  u8 Pad[36];
#else
  u8 Pad[12];
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
  b32 Expanded;
};


#if DEBUG_LIB_INTERNAL_BUILD
#include <bonsai_debug/headers/debug_ui.h>
#include <bonsai_debug/headers/interactable.h>
#include <bonsai_debug/headers/debug_render.h>
#endif

#include <bonsai_debug/headers/api.h>
global_variable debug_profile_scope NullDebugProfileScope = {};




#else

#define TIMED_FUNCTION(...)
#define TIMED_BLOCK(...)
#define END_BLOCK(...)

#define DEBUG_VALUE(...)

#define TIMED_MUTEX_WAITING(...)
#define TIMED_MUTEX_AQUIRED(...)
#define TIMED_MUTEX_RELEASED(...)

#define DEBUG_FRAME_RECORD(...)
#define DEBUG_FRAME_END(...)
#define DEBUG_FRAME_BEGIN(...)

#define WORKER_THREAD_WAIT_FOR_DEBUG_SYSTEM(...)
#define MAIN_THREAD_ADVANCE_DEBUG_SYSTEM(...)
#define WORKER_THREAD_ADVANCE_DEBUG_SYSTEM()

#define DEBUG_CLEAR_META_RECORDS_FOR(...)
#define DEBUG_TRACK_DRAW_CALL(...)

#define DEBUG_REGISTER_VIEW_PROJECTION_MATRIX(...)

#define DEBUG_COMPUTE_PICK_RAY(...)
#define DEBUG_PICK_CHUNK(...)

#endif
