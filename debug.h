
#if BONSAI_DEBUG_SYSTEM_API


// TODO(Jesse, id: 161, tags: back_burner, debug_recording): Reinstate this!
/* enum debug_recording_mode */
/* { */
/*   RecordingMode_Clear, */
/*   RecordingMode_Record, */
/*   RecordingMode_Playback, */

/*   RecordingMode_Count, */
/* }; */

/* #define DEBUG_RECORD_INPUT_SIZE 3600 */
/* struct debug_recording_state */
/* { */
/*   s32 FramesRecorded; */
/*   s32 FramesPlayedBack; */
/*   debug_recording_mode Mode; */

/*   memory_arena RecordedMainMemory; */

/*   hotkeys Inputs[DEBUG_RECORD_INPUT_SIZE]; */
/* }; */


typedef b32 (*meta_comparator)(push_metadata*, push_metadata*);

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
