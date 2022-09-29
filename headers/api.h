struct debug_state;
struct debug_profile_scope;
struct debug_thread_state;

struct input;
struct memory_arena;
struct mutex;


typedef void                 (*debug_clear_framebuffers_proc)          ();
typedef void                 (*debug_frame_end_proc)                   (v2 *MouseP, v2 *MouseDP, v2 ScreenDim, input *Input, r32 dt);
typedef void                 (*debug_frame_begin_proc)                 (b32, b32);
typedef void                 (*debug_register_arena_proc)              (const char*, memory_arena*);
typedef void                 (*debug_worker_thread_advance_data_system)(void);
typedef void                 (*debug_main_thread_advance_data_system)  (r64);

typedef void                 (*debug_mutex_waiting_proc)               (mutex*);
typedef void                 (*debug_mutex_aquired_proc)               (mutex*);
typedef void                 (*debug_mutex_released_proc)              (mutex*);

typedef debug_profile_scope* (*debug_get_profile_scope_proc)           ();
typedef void*                (*debug_allocate_proc)                    (memory_arena*, umm, umm, const char*, s32 , const char*, umm, b32);
typedef void                 (*debug_register_thread_proc)             (u32);
typedef void                 (*debug_clear_meta_records_proc)          (memory_arena*);
typedef void                 (*debug_track_draw_call_proc)             (const char*, u32);
typedef debug_thread_state*  (*debug_get_thread_local_state)           (void);
typedef void                 (*debug_value)                            (r32, const char*);
typedef void                 (*debug_dump_scope_tree_data_to_console)  ();
typedef void                 (*debug_open_window_and_let_us_do_stuff)  ();


typedef debug_state*         (*get_debug_state_proc)  ();
typedef get_debug_state_proc (*init_debug_system_proc)();

#if DEBUG_LIB_INTERNAL_BUILD
  link_export debug_state* GetDebugState();

#else
  link_internal get_debug_state_proc GetDebugState;

#endif

#define DebugLibName_InitDebugSystem "InitDebugSystem"
#define DebugLibName_GetDebugState "GetDebugState"

#define DEFAULT_DEBUG_LIB "./bin/lib_debug_system" PLATFORM_RUNTIME_LIB_EXTENSION



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

struct debug_scope_tree
{
  debug_profile_scope *Root;

  debug_profile_scope **WriteScope;
  debug_profile_scope *ParentOfNextScope;

  u64 FrameRecorded;
};

struct debug_thread_state
{
  memory_arena *Memory;
  memory_arena *MemoryFor_debug_profile_scope; // Specifically for allocationg debug_profile_scope structs
  push_metadata *MetaTable;

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

enum debug_ui_type
{
  DebugUIType_None = 0,

  /* DebugUIType_PickedChunks          = (1 << 0), */
  DebugUIType_CallGraph             = (1 << 1),
  DebugUIType_CollatedFunctionCalls = (1 << 2),
  DebugUIType_Memory                = (1 << 3),
  DebugUIType_Graphics              = (1 << 4),
  DebugUIType_Network               = (1 << 5),
  DebugUIType_DrawCalls             = (1 << 6)
};

struct registered_memory_arena
{
  memory_arena *Arena;
  const char* Name;
  b32 Expanded;
};

struct selected_memory_arena
{
  umm ArenaHash;
  umm HeadArenaHash;
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

#define REGISTERED_MEMORY_ARENA_COUNT (128)
#define DEBUG_FRAMES_TRACKED (128)

struct debug_state
{
  b32 Initialized;
  u32 UIType = DebugUIType_None;

  u64 BytesBufferedToCard;
  b32 DebugDoScopeProfiling = True;

  debug_scope_tree* GetReadScopeTree(u32 ThreadIndex)
  {
    debug_scope_tree *RootScope = &this->ThreadStates[ThreadIndex].ScopeTrees[this->ReadScopeIndex];
    return RootScope;
  }

  debug_scope_tree* GetWriteScopeTree()
  {
    debug_scope_tree* Result = 0;

    debug_thread_state* ThreadState = GetDebugState()->GetThreadLocalState();
    if (ThreadState)
    {
      Result = ThreadState->ScopeTrees + (ThreadState->WriteIndex % DEBUG_FRAMES_TRACKED);
    }

    return Result;
  }

  debug_clear_framebuffers_proc             ClearFramebuffers;
  debug_frame_end_proc                      FrameEnd;
  debug_frame_begin_proc                    FrameBegin;
  debug_register_arena_proc                 RegisterArena;
  debug_worker_thread_advance_data_system   WorkerThreadAdvanceDebugSystem;
  debug_main_thread_advance_data_system     MainThreadAdvanceDebugSystem;

  debug_mutex_waiting_proc                  MutexWait;
  debug_mutex_aquired_proc                  MutexAquired;
  debug_mutex_released_proc                 MutexReleased;

  debug_get_profile_scope_proc              GetProfileScope;
  debug_allocate_proc                       Debug_Allocate;
  debug_register_thread_proc                RegisterThread;
  debug_clear_meta_records_proc             ClearMetaRecordsFor;
  debug_track_draw_call_proc                TrackDrawCall;
  debug_get_thread_local_state              GetThreadLocalState;
  /* debug_pick_chunk                          PickChunk; */
  /* debug_compute_pick_ray                    ComputePickRay; */
  debug_value                               DebugValue;
  debug_dump_scope_tree_data_to_console     DumpScopeTreeDataToConsole;
  debug_open_window_and_let_us_do_stuff     OpenDebugWindowAndLetUsDoStuff;

  // TODO(Jesse): Put this into some sort of debug_render struct such that
  // users of the library (externally) don't have to include all the rendering
  // code that the library relies on.
  //
  // NOTE(Jesse): This stuff has to be "hidden" at the end of the struct so the
  // external ABI is the same as the internal ABI until this point
#if DEBUG_LIB_INTERNAL_BUILD
  debug_ui_render_group UiGroup;

  untextured_3d_geometry_buffer LineMesh;

  // For the GameGeo
  camera *Camera;

  framebuffer GameGeoFBO;
  shader GameGeoShader;
  m4 ViewProjection;
  gpu_mapped_element_buffer GameGeo;
  shader DebugGameGeoTextureShader;

  selected_arenas *SelectedArenas;

  b32 TriggerRuntimeBreak;
  b32 DisplayDebugMenu;

  debug_profile_scope* HotFunction;

  debug_profile_scope FreeScopeSentinel;
  debug_thread_state *ThreadStates;

  frame_stats Frames[DEBUG_FRAMES_TRACKED];

  u32 ReadScopeIndex;
  s32 FreeScopeCount;
  u64 NumScopes;

  registered_memory_arena RegisteredMemoryArenas[REGISTERED_MEMORY_ARENA_COUNT];

#endif

};

struct debug_timed_function
{
  debug_profile_scope *Scope;
  debug_scope_tree *Tree;

  debug_timed_function(const char *Name)
  {
    Clear(this);

    // This if doesn't have to be here in internal builds, because we know
    // statically that it'll be present
#if !DEBUG_LIB_INTERNAL_BUILD
    if (!GetDebugState) return;
#endif

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
    // This if doesn't have to be here in internal builds, because we know
    // statically that it'll be present
#if !DEBUG_LIB_INTERNAL_BUILD
    if (!GetDebugState) return;
#endif

    debug_state *DebugState = GetDebugState();
    if (DebugState)
    {
      if (!DebugState->DebugDoScopeProfiling) return;
      if (!this->Scope) return;

      this->Scope->EndingCycle = __rdtsc(); // Intentionally first;

      Assert(this->Scope->EndingCycle > this->Scope->StartingCycle);

      // 'Pop' the scope stack
      this->Tree->WriteScope = &this->Scope->Sibling;
      this->Tree->ParentOfNextScope = this->Scope->Parent;
    }
  }

};

#define TIMED_FUNCTION() debug_timed_function FunctionTimer(__func__)
#define TIMED_NAMED_BLOCK(BlockName) debug_timed_function BlockTimer(BlockName)

#define TIMED_BLOCK(BlockName) { debug_timed_function BlockTimer(BlockName)
#define END_BLOCK(BlockName) } do {} while (0)

#define DEBUG_VALUE(Pointer) if (GetDebugState) {GetDebugState()->DebugValue(Pointer, #Pointer);}

#define DEBUG_FRAME_RECORD(...) DoDebugFrameRecord(__VA_ARGS__)
#define DEBUG_FRAME_END(a, b, c, d, e) if (GetDebugState) {GetDebugState()->FrameEnd(a, b, c, d, e);}
#define DEBUG_FRAME_BEGIN(bToggleMenu, bToggleProfile) if (GetDebugState) {GetDebugState()->FrameBegin(bToggleMenu, bToggleProfile);}

#if 1
void DebugTimedMutexWaiting(mutex *Mut);
void DebugTimedMutexAquired(mutex *Mut);
void DebugTimedMutexReleased(mutex *Mut);

#define TIMED_MUTEX_WAITING(Mut)  if (GetDebugState) {GetDebugState()->MutexWait(Mut);}
#define TIMED_MUTEX_AQUIRED(Mut)  if (GetDebugState) {GetDebugState()->MutexAquired(Mut);}
#define TIMED_MUTEX_RELEASED(Mut) if (GetDebugState) {GetDebugState()->MutexReleased(Mut);}
#endif

#define MAIN_THREAD_ADVANCE_DEBUG_SYSTEM(dt)               if (GetDebugState) {GetDebugState()->MainThreadAdvanceDebugSystem(dt);}
#define WORKER_THREAD_ADVANCE_DEBUG_SYSTEM()               if (GetDebugState) {GetDebugState()->WorkerThreadAdvanceDebugSystem();}

#define DEBUG_CLEAR_META_RECORDS_FOR(Arena)                if (GetDebugState) {GetDebugState()->ClearMetaRecordsFor(Arena);}
#define DEBUG_TRACK_DRAW_CALL(CallingFunction, VertCount)  if (GetDebugState) {GetDebugState()->TrackDrawCall(CallingFunction, VertCount);}

#define DEBUG_REGISTER_VIEW_PROJECTION_MATRIX(ViewProjPtr) if (GetDebugState) {GetDebugState()->ViewProjection = ViewProjPtr;}
#define DEBUG_COMPUTE_PICK_RAY(ViewProjPtr)          if (GetDebugState) {GetDebugState()->ComputePickRay(ViewProjPtr);}
#define DEBUG_PICK_CHUNK(Chunk, ChunkAABB)                 if (GetDebugState) {GetDebugState()->PickChunk(Chunk, ChunkAABB);}
