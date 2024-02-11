struct debug_state;
typedef debug_state*         (*get_debug_state_proc)  ();
typedef u64                  (*query_memory_requirements_proc)();
typedef get_debug_state_proc (*init_debug_system_proc)(debug_state *);
typedef void                 (*patch_debug_lib_pointers_proc)(debug_state *, thread_local_state *, u32);


struct bonsai_debug_api
{
  query_memory_requirements_proc QueryMemoryRequirements;
  init_debug_system_proc         InitDebugState;
  patch_debug_lib_pointers_proc  BonsaiDebug_OnLoad;
};

struct bonsai_debug_system
{
  shared_lib Lib;
  bonsai_debug_api Api;

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
typedef void*                (*debug_allocate_proc)                    (memory_arena*, umm, umm, const char*, s32 , const char*, umm, b32);
typedef void                 (*debug_register_thread_proc)             (thread_startup_params*);
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
  b32 DebugDoScopeProfiling = True;

  u64 NumScopes;

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
  debug_allocate_proc                       Debug_Allocate;
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
  void (*PushHistogramDataPoint)(debug_timed_function*);

  get_read_scope_tree_proc GetReadScopeTree;
  get_write_scope_tree_proc GetWriteScopeTree;

  // TODO(Jesse): Remove these.  Need to expose the UI drawing code to the user
  // of the library.
  picked_world_chunk *PickedChunk;
  picked_world_chunk *HoverChunk;

  debug_thread_state *ThreadStates;

#if BONSAI_DEBUG_SYSTEM_INTERNAL_BUILD
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

#define DEBUG_HISTOGRAM_MAX_SAMPLES (1024*10) // TODO(Jesse): ??
  debug_profile_scope_cursor HistogramSamples;

  debug_profile_scope FreeScopeSentinel;

  volatile umm MinCycles; // span of start/end frame cycles
  volatile umm MaxCycles;

#define DEBUG_FRAMES_TRACKED (128)
  frame_stats Frames[DEBUG_FRAMES_TRACKED];

  u32 ReadScopeIndex;
  s32 FreeScopeCount;

#define REGISTERED_MEMORY_ARENA_COUNT (1024)
  registered_memory_arena RegisteredMemoryArenas[REGISTERED_MEMORY_ARENA_COUNT];

#define TRACKED_DRAW_CALLS_MAX (128)
  debug_draw_call TrackedDrawCalls[TRACKED_DRAW_CALLS_MAX];
#endif
};


#define GetDebugState() Global_DebugStatePointer
global_variable debug_state *Global_DebugStatePointer;


struct debug_timed_function
{
  debug_profile_scope *Scope;
  debug_scope_tree *Tree;

  debug_timed_function(const char *Name)
  {
    this->Scope = 0;
    this->Tree = 0;

    debug_state *DebugState = GetDebugState();
    if (DebugState)
    {
      if (!DebugState->DebugDoScopeProfiling) return;

      ++DebugState->NumScopes;

      this->Scope = DebugState->GetProfileScope();
      this->Tree = DebugState->GetWriteScopeTree();

      if (this->Scope && this->Tree)
      {
        (*this->Tree->WriteScope) = this->Scope;
        this->Tree->WriteScope = &this->Scope->Child;

        this->Scope->Parent = this->Tree->ParentOfNextScope;
        this->Tree->ParentOfNextScope = this->Scope;

        this->Scope->Name = Name;
        this->Scope->StartingCycle = __rdtsc(); // Intentionally last
      }
    }

    return;
  }

  ~debug_timed_function()
  {
    debug_state *DebugState = GetDebugState();
    if (DebugState)
    {
      if (!DebugState->DebugDoScopeProfiling) return;
      if (!this->Scope) return;

      this->Scope->EndingCycle = __rdtsc(); // Intentionally first;

      Assert(this->Scope->EndingCycle > this->Scope->StartingCycle);
      Assert(this->Scope->Parent != this->Scope);
      Assert(this->Scope->Sibling != this->Scope);
      Assert(this->Scope->Child != this->Scope);

      // 'Pop' the scope stack
      this->Tree->WriteScope = &this->Scope->Sibling;
      this->Tree->ParentOfNextScope = this->Scope->Parent;
    }
  }

};

struct debug_histogram_function : debug_timed_function
{
  debug_histogram_function(const char* Name) : debug_timed_function(Name) {}

  ~debug_histogram_function()
  {
    debug_state *DebugState = GetDebugState();
    if (DebugState)
    {
      if (!DebugState->DebugDoScopeProfiling) return;
      if (!this->Scope) return;

      // NOTE(Jesse): Kinda henious hack because the constructor/destructor
      // ordering is the wrong way for this to work.  I couldn't think of a
      // better way to do this..
      this->Scope->EndingCycle = __rdtsc();
      DebugState->PushHistogramDataPoint(this);
    }
  }
};

#define TIMED_FUNCTION() debug_timed_function FunctionTimer(__func__)
#define TIMED_NAMED_BLOCK(BlockName) debug_timed_function BlockTimer1(BlockName)
#define HISTOGRAM_FUNCTION() debug_histogram_function FunctionTimer(__func__)

#define TIMED_BLOCK(BlockName) { debug_timed_function BlockTimer0(BlockName)
#define END_BLOCK(BlockName) } do {} while (0)

#define DEBUG_VALUE_r32(Pointer) do {GetDebugState()->DebugValue_r32(Pointer, #Pointer);} while (false)
#define DEBUG_VALUE_u32(Pointer) do {GetDebugState()->DebugValue_u32(Pointer, #Pointer);} while (false)

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

/* #include <dlfcn.h> */
#include <stdio.h>
#include <time.h>

#define BonsaiDebug_DefaultLibPath "lib_bonsai_debug/lib_bonsai_debug.so"
/* global_variable debug_state *Global_DebugStatePointer; */

global_variable r64 Global_LastDebugTime = 0;
r32 GetDt()
{
  r64 ThisTime = GetHighPrecisionClock();
  r64 Result = ThisTime - Global_LastDebugTime;
  Global_LastDebugTime = ThisTime;
  return r32(Result);
}

bool
InitializeBootstrapDebugApi(shared_lib DebugLib, bonsai_debug_api *Api)
{
  b32 Result = 1;

  Api->QueryMemoryRequirements = (query_memory_requirements_proc)GetProcFromLib(DebugLib, "QueryMemoryRequirements");
  Result &= (Api->QueryMemoryRequirements != 0);

  Api->InitDebugState = (init_debug_system_proc)GetProcFromLib(DebugLib, "InitDebugState");
  Result &= (Api->InitDebugState != 0);

  Api->BonsaiDebug_OnLoad = (patch_debug_lib_pointers_proc)GetProcFromLib(DebugLib, "BonsaiDebug_OnLoad");
  Result &= (Api->InitDebugState != 0);

  u64 BytesRequested = Api->QueryMemoryRequirements();
  Global_DebugStatePointer = (debug_state*)calloc(BytesRequested, 1);

  return Result;
}



bonsai_debug_system
InitializeBonsaiDebug(const char* DebugLibName, thread_local_state *ThreadStates)
{
  bonsai_debug_system Result = {};

  shared_lib DebugLib = OpenLibrary(DebugLibName); //, RTLD_NOW);

  if (DebugLib)
  {
    printf("Library (%s) loaded!\n", DebugLibName);

    bonsai_debug_api DebugApi = {};
    if (InitializeBootstrapDebugApi(DebugLib, &DebugApi))
    {
      DebugApi.BonsaiDebug_OnLoad(Global_DebugStatePointer, ThreadStates, BONSAI_INTERNAL);

      if (DebugApi.InitDebugState(Global_DebugStatePointer))
      {
        printf("Success initializing lib_bonsai_debug\n");
        Result.Lib = DebugLib;
        Result.Api = DebugApi;
        Result.Initialized = True;
      }
      else { printf("Error initializing lib_bonsai_debug\n"); DebugLib = 0; }
    }
    else { printf("Error initializing lib_bonsai_debug bootstrap API\n"); DebugLib = 0; }
  }
  else { printf("OpenLibrary Failed (%s)\n", ""); DebugLib = 0; }

  return Result;
}


#else // BONSAI_DEBUG_SYSTEM_API


#define TIMED_FUNCTION(...)
#define TIMED_NAMED_BLOCK(...)
#define HISTOGRAM_FUNCTION(...)

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


#endif //  BONSAI_DEBUG_SYSTEM_API
