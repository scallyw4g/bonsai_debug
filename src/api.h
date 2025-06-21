struct debug_state;
struct selected_arenas;
typedef debug_state*         (*get_debug_state_proc)  ();
typedef u64                  (*query_memory_requirements_proc)();
typedef get_debug_state_proc (*init_debug_system_proc)(debug_state *);
typedef void                 (*patch_debug_lib_pointers_proc)(debug_state *, thread_local_state *, u32);

struct bonsai_debug_system
{
  debug_state *DebugState;
  b32 Initialized;
};

#if BONSAI_DEBUG_SYSTEM_API

struct debug_state;
struct debug_scope_tree;
struct debug_profile_scope;
struct debug_thread_state;

struct input;
struct memory_arena;
struct mutex;
struct heap_allocator;

struct render_entity_to_texture_group;
struct picked_world_chunk_static_buffer;
struct picked_world_chunk;
struct thread_local_state;



#define BONSAI_NO_ARENA (0xFFFFFFFFFFFFFFFF)
struct memory_record
{
  const char* Name;
  umm ArenaAddress;     // If this is set to BONSAI_NO_ARENA you can set "ArenaMemoryBlock" to a null-terminated string identifying the allocation site
  umm ArenaMemoryBlock; // @ArenaMemoryBlock-as-char-pointer
  umm StructSize;
  umm StructCount;
  s32 ThreadId;

  u32 PushCount;
};

#if 1

typedef debug_scope_tree*    (*get_read_scope_tree_proc)(u32);
typedef debug_scope_tree*    (*get_write_scope_tree_proc)();
/* typedef void                 (*debug_clear_framebuffers_proc)          (); */
typedef void                 (*debug_frame_end_proc)                   (r32);
typedef void                 (*debug_frame_begin_proc)                 (renderer_2d*, r32, b32, b32);
typedef void                 (*debug_register_arena_proc)              (const char*, memory_arena*, s32);
typedef void                 (*debug_unregister_arena_proc)            (memory_arena*);
typedef void                 (*debug_worker_thread_advance_data_system)(void);
typedef void                 (*debug_main_thread_advance_data_system)  (f32);

typedef void                 (*debug_mutex_waiting_proc)               (mutex*);
typedef void                 (*debug_mutex_aquired_proc)               (mutex*);
typedef void                 (*debug_mutex_released_proc)              (mutex*);

typedef debug_profile_scope* (*debug_get_profile_scope_proc)           ();
typedef void*                (*debug_arena_allocate_proc)              (memory_arena*,   umm, umm, const char*, s32 , const char*, umm, b32);
typedef void*                (*debug_heap_allocate_proc)               (heap_allocator*, umm, umm, const char*, s32 , const char*, umm, b32);
typedef void                 (*debug_register_thread_proc)             (thread_local_state*);
typedef void                 (*debug_track_draw_call_proc)             (const char*, u32);
typedef debug_thread_state*  (*debug_get_thread_local_state)           (void);
typedef void                 (*debug_value_r32_proc)                   (r32, const char*);
typedef void                 (*debug_value_u32_proc)                   (u32, const char*);
typedef void                 (*debug_value_u64_proc)                   (u64, const char*);
typedef void                 (*debug_dump_scope_tree_data_to_console)  ();

typedef void                 (*debug_clear_memory_records_proc)        (memory_arena*);
typedef void                 (*debug_write_memory_record_proc)         (memory_record*);

typedef b32                  (*debug_open_window_proc)                 ();
typedef void                 (*debug_redraw_window_proc)               ();
#endif



struct debug_profile_scope
{
  u64 StartingCycle;
  u64 EndingCycle;
  const char* Name;

  b32 Expanded;

  debug_profile_scope* Sibling;
  debug_profile_scope* Child;
  debug_profile_scope* Parent;

  /* b64 Pad; */
};
// NOTE(Jesse): I thought maybe this would increase perf .. it had a negligible
// effect These structs are per-thread so there's no sense in having them
// cache-line sized.  Something that would probably speed this up is not storing
// the sibling/child/parent pointers and instead store a one-past-last index into
// a statically allocated buffer (instead of dynamically allocating new scopes
// every time we need another one)
/* CAssert(sizeof(debug_profile_scope) == CACHE_LINE_SIZE); */

poof(are_equal(debug_profile_scope))
#include <generated/are_equal_debug_profile_scope.h>

poof(generate_cursor(debug_profile_scope))
#include <generated/generate_cursor_debug_profile_scope.h>

struct debug_scope_tree
{
  debug_profile_scope *Root;

  debug_profile_scope **WriteScope;
  debug_profile_scope *ParentOfNextScope;
  u64 FrameRecorded;
};

enum debug_ui_type
{
  DebugUIType_None = 0,

  DebugUIType_CallGraph             = (1 << 1),
  DebugUIType_CollatedFunctionCalls = (1 << 2),
  DebugUIType_Memory                = (1 << 3),
  DebugUIType_Graphics              = (1 << 4),
  DebugUIType_Network               = (1 << 5),
  DebugUIType_DrawCalls             = (1 << 6),
};

struct debug_timed_function;

struct debug_state
{
  b32 Initialized;
  u32 UIType = DebugUIType_None;

  u64 BytesBufferedToCard;
  b32 DebugDoScopeProfiling;

  u64 NumScopes;

  u32 DrawCallCountLastFrame;
  u32 VertexCountLastFrame;

#if 1
  debug_frame_end_proc                      FrameEnd;
  debug_frame_begin_proc                    FrameBegin;
  debug_register_arena_proc                 RegisterArena;
  debug_unregister_arena_proc               UnregisterArena;
  debug_worker_thread_advance_data_system   WorkerThreadAdvanceDebugSystem;
  debug_main_thread_advance_data_system     MainThreadAdvanceDebugSystem;

  debug_mutex_waiting_proc                  MutexWait;
  debug_mutex_aquired_proc                  MutexAquired;
  debug_mutex_released_proc                 MutexReleased;

  debug_get_profile_scope_proc              GetProfileScope;
  debug_arena_allocate_proc                 Debug_Allocate;
  debug_register_thread_proc                RegisterThread;

  debug_write_memory_record_proc            WriteMemoryRecord;
  debug_clear_memory_records_proc           ClearMemoryRecordsFor;

  debug_track_draw_call_proc                TrackDrawCall;
  debug_get_thread_local_state              GetThreadLocalState;
  debug_value_r32_proc                      DebugValue_r32;
  debug_value_u32_proc                      DebugValue_u32;
  debug_value_u64_proc                      DebugValue_u64;
  debug_dump_scope_tree_data_to_console     DumpScopeTreeDataToConsole;

  debug_open_window_proc                    OpenAndInitializeDebugWindow;
  debug_redraw_window_proc                  ProcessInputAndRedrawWindow;

  b32  (*InitializeRenderSystem)(heap_allocator*, memory_arena*);
  void (*SetRenderer)(renderer_2d*);
  void (*PushHistogramDataPoint)(u64);

  get_read_scope_tree_proc GetReadScopeTree;
  get_write_scope_tree_proc GetWriteScopeTree;

  // TODO(Jesse): Remove these.  Need to expose the UI drawing code to the user
  // of the library.
  picked_world_chunk *PickedChunk;
  picked_world_chunk *HoverChunk;
#endif

  // TODO(Jesse): Can this go away ..?
  debug_thread_state *ThreadStates;

  // @meta_table_allocation_name_copy
  memory_arena *MetaTableNameStringsArena;

  // TODO(Jesse): Put this into some sort of debug_render struct such that
  // users of the library (externally) don't have to include all the rendering
  // code that the library relies on.
  //
  // NOTE(Jesse): This stuff has to be "hidden" at the end of the struct so the
  // external ABI is the same as the internal ABI until this point

  // NOTE(Jesse): This has to be a pointer such that we can initialize it and
  // pass it to the library, or initialize it and stand on our own two feet.
  renderer_2d *UiGroup;

  untextured_3d_geometry_buffer LineMesh;

  selected_arenas *SelectedArenas;

  b32 DisplayDebugMenu;

  debug_profile_scope *HotFunction;

#define DEBUG_HISTOGRAM_MAX_SAMPLES (1024*32) // TODO(Jesse): ??
  u64_cursor HistogramSamples;
  bonsai_futex HistogramFutex;

  debug_profile_scope FreeScopeSentinel;

  volatile umm MinCycles; // span of start/end frame cycles
  volatile umm MaxCycles;

#define DEBUG_FRAMES_TRACKED (128)
  frame_stats Frames[DEBUG_FRAMES_TRACKED];

  u32 ReadScopeIndex;
  s32 FreeScopeCount;

#define REGISTERED_MEMORY_ARENA_COUNT (1024)
  registered_memory_arena RegisteredMemoryArenas[REGISTERED_MEMORY_ARENA_COUNT];

#define TRACKED_DRAW_CALLS_MAX (4096)
  debug_draw_call TrackedDrawCalls[TRACKED_DRAW_CALLS_MAX];
};

struct debug_timed_function
{
  debug_profile_scope *Scope;
  debug_scope_tree *Tree;

  debug_timed_function(const char *Name);
  ~debug_timed_function();
};

struct debug_histogram_function : debug_timed_function
{
  debug_histogram_function(const char* Name) : debug_timed_function(Name) {}
  ~debug_histogram_function();
};

link_export b32 InitDebugState(debug_state *DebugState);

#define TIMED_FUNCTION() debug_timed_function FunctionTimer(__func__)
#define TIMED_NAMED_BLOCK(BlockName) debug_timed_function BlockName##_block_timer(STRINGIZE(BlockName))
#define HISTOGRAM_FUNCTION() debug_histogram_function FunctionTimer(__func__)

#define TIMED_BLOCK(BlockName) { debug_timed_function BlockTimer0(BlockName)
#define END_BLOCK(BlockName) } do {} while (0)

#define DEBUG_VALUE_r32(Pointer) do {GetDebugState()->DebugValue_r32((Pointer), #Pointer);} while (false)
#define DEBUG_VALUE_u32(Pointer) do {GetDebugState()->DebugValue_u32((Pointer), #Pointer);} while (false)
#define DEBUG_VALUE(Pointer) do {DebugValue((Pointer), #Pointer);} while (false)

#define DEBUG_FRAME_RECORD(...) DoDebugFrameRecord(__VA_ARGS__)
#define DEBUG_FRAME_END(a) do {GetDebugState()->FrameEnd(a);} while (false)
#define DEBUG_FRAME_BEGIN(Ui, PrevDt, bToggleMenu, bToggleProfile) do {GetDebugState()->FrameBegin(Ui, PrevDt, bToggleMenu, bToggleProfile);} while (false)

void DebugTimedMutexWaiting(mutex *Mut);
void DebugTimedMutexAquired(mutex *Mut);
void DebugTimedMutexReleased(mutex *Mut);

#define TIMED_MUTEX_WAITING(Mut)  do {GetDebugState()->MutexWait(Mut);} while (false)
#define TIMED_MUTEX_AQUIRED(Mut)  do {GetDebugState()->MutexAquired(Mut);} while (false)
#define TIMED_MUTEX_RELEASED(Mut) do {GetDebugState()->MutexReleased(Mut);} while (false)

#define MAIN_THREAD_ADVANCE_DEBUG_SYSTEM(dt)               do {GetDebugState()->MainThreadAdvanceDebugSystem(dt);} while (false)
#define WORKER_THREAD_ADVANCE_DEBUG_SYSTEM()               do {GetDebugState()->WorkerThreadAdvanceDebugSystem();} while (false)

#define DEBUG_CLEAR_MEMORY_RECORDS_FOR(Arena)              do {GetDebugState()->ClearMemoryRecordsFor(Arena);} while (false)
#define DEBUG_TRACK_DRAW_CALL(CallingFunction, VertCount)  do {GetDebugState()->TrackDrawCall(CallingFunction, VertCount);} while (false)

#else // BONSAI_DEBUG_SYSTEM_API

#define TIMED_FUNCTION(...)
#define TIMED_NAMED_BLOCK(...)
#define HISTOGRAM_FUNCTION(...)

#define TIMED_BLOCK(...)
#define END_BLOCK(...)

#define DEBUG_VALUE(...)
#define DEBUG_VALUE_r32(...)
#define DEBUG_VALUE_u32(...)

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


#endif //  BONSAI_DEBUG_SYSTEM_API
