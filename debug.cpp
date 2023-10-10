#define DEBUG_SYSTEM_API 1
#define DEBUG_SYSTEM_INTERNAL_BUILD 1

#define PLATFORM_WINDOW_IMPLEMENTATIONS 1

#include <bonsai_stdlib/bonsai_stdlib.h>
#include <bonsai_stdlib/bonsai_stdlib.cpp>

/* #include <engine/engine.h> */
/* #include <engine/engine.cpp> */

#include <bonsai_debug/src/debug_data_system.cpp>
#include <bonsai_debug/src/debug_render_system.cpp>

#if BONSAI_WIN32
#include <bonsai_debug/src/platform/win32_etw.cpp>
/* #include <bonsai_debug/src/platform/win32_pmc.cpp> */
#endif

/* debug_state *Global_DebugStatePointer; */

global_variable os Os = {};
global_variable platform Plat = {};
global_variable hotkeys Hotkeys = {};

link_internal void
DebugFrameEnd(r32 dt)
{
  TIMED_FUNCTION();

  debug_state *DebugState = GetDebugState();

  debug_ui_render_group *UiGroup = DebugState->UiGroup;

  min_max_avg_dt Dt = ComputeMinMaxAvgDt();

  DebugState->BytesBufferedToCard = 0;

  for( u32 DrawCountIndex = 0;
       DrawCountIndex < TRACKED_DRAW_CALLS_MAX;
       ++ DrawCountIndex)
  {
    GetDebugState()->TrackedDrawCalls[DrawCountIndex] = NullDrawCall;
  }

  for ( u32 FunctionIndex = 0;
      FunctionIndex < MAX_RECORDED_FUNCTION_CALLS;
      ++FunctionIndex)
  {
    ProgramFunctionCalls[FunctionIndex] = NullFunctionCall;
  }

  return;
}

link_internal void
DebugFrameBegin(renderer_2d *Ui, r32 PrevDt, b32 ToggleMenu, b32 ToggleProfiling)
{
  debug_state *State = GetDebugState();
  State->UiGroup = Ui;

  if (ToggleMenu)
  {
    State->DisplayDebugMenu = !State->DisplayDebugMenu;
  }

  if (ToggleProfiling)
  {
    State->DebugDoScopeProfiling = !State->DebugDoScopeProfiling;
  }

  /* DebugLine("State->UiGroup.NumMinimizedWindowsDrawn (%u)", State->UiGroup.NumMinimizedWindowsDrawn ); */
  /* State->UiGroup.NumMinimizedWindowsDrawn = 0; */

  Ensure( RewindArena(GetTranArena()) );



  TIMED_BLOCK("Draw Status Bar");

  memory_arena_stats TotalStats = GetTotalMemoryArenaStats();

  u32 TotalDrawCalls = 0;

  for( u32 DrawCountIndex = 0;
       DrawCountIndex < TRACKED_DRAW_CALLS_MAX;
       ++ DrawCountIndex)
  {
    debug_draw_call *Call = &GetDebugState()->TrackedDrawCalls[DrawCountIndex];
    if (Call->Caller)
    {
      TotalDrawCalls += Call->Calls;
    }
  }


  min_max_avg_dt Dt = ComputeMinMaxAvgDt();

  {
    v4 Padding = V4(25,0,25,0);
    ui_style Style = UiStyleFromLightestColor(V3(1));

    PushTableStart(Ui);
      StartColumn(Ui, &Style, Padding);
        Text(Ui, FormatCountedString(GetTranArena(), CS("(%.1f) :: (+%.1f) (%.1f) (-%.1f) :: Allocations(%d) Pushes(%d) DrawCalls(%d)"),
          r64(PrevDt*1000.0f),
          r64(Dt.Max - Dt.Avg),
          r64(Dt.Avg),
          r64(Dt.Avg-Dt.Min),
          TotalStats.Allocations,
          TotalStats.Pushes,
          TotalDrawCalls
        ));
      EndColumn(Ui);
    PushTableEnd(Ui);
  }

  debug_state *DebugState = GetDebugState();
  if (DebugState->DisplayDebugMenu)
  {
    TIMED_NAMED_BLOCK("DrawDebugMenu");

    PushTableStart(Ui);

    v4 Padding = V4(15,0,15,0);

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_Graphics) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Graphics"), (umm)"Graphics", Style, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Graphics);
      }
    }

    {
      /* ui_style *Style = &DefaultStyle; */
      /* if ( DebugState->UIType & DebugUIType_Network)  *1/ */ /* { */ /*   Style = &DefaultSelectedStyle; */ /* } */
      /* if (Button(Ui, CS("Network"), (umm)"Network", Style)) */
      /* { */
      /*   ToggleBitfieldValue(DebugState->UIType, DebugUIType_Network); */
      /* } */
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CollatedFunctionCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Functions"), (umm)"Functions", Style, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CollatedFunctionCalls);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CallGraph) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Callgraph"), (umm)"Callgraph", Style, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CallGraph);
      }
    }

    {
      ui_style *Style = &DefaultStyle;
      if ( DebugState->UIType & DebugUIType_Memory ) { Style = &DefaultSelectedStyle; }
      if (Button(Ui, CS("Memory"), (umm)"Memory", Style, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Memory);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_DrawCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("DrawCalls"), (umm)"DrawCalls", Style, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_DrawCalls);
      }
    }

    PushTableEnd(Ui);




    if (DebugState->UIType & DebugUIType_Graphics)
    {
      DebugDrawGraphicsHud(Ui, DebugState);
    }

    /* if (DebugState->UIType & DebugUIType_Network) */
    /* { */
    /*   DebugDrawNetworkHud(Ui, &Plat->Network, ServerState); */
    /* } */

    if (DebugState->UIType & DebugUIType_CollatedFunctionCalls)
    {
      DebugDrawCollatedFunctionCalls(Ui, DebugState);
    }

    if (DebugState->UIType & DebugUIType_CallGraph)
    {
      DebugDrawCallGraph(Ui, DebugState, Dt.Max);
    }

    if (DebugState->UIType & DebugUIType_Memory)
    {
      DebugDrawMemoryHud(Ui, DebugState);
    }

    if (DebugState->UIType & DebugUIType_DrawCalls)
    {
      DebugDrawDrawCalls(Ui);
    }

  }

  
  END_BLOCK("Draw Status Bar");

}

link_internal void
SetRenderer(renderer_2d *Renderer)
{
  GetDebugState()->SelectedArenas = Allocate(selected_arenas, ThreadsafeDebugMemoryAllocator(), 1);
  GetDebugState()->UiGroup = Renderer;
}

link_internal b32
OpenAndInitializeDebugWindow()
{
  Assert(GetDebugState());
  Assert(GetDebugState()->Initialized);

  b32 WindowSuccess = OpenAndInitializeWindow(&Os, &Plat, 1);
  if (!WindowSuccess) { Error("Initializing Window :( "); return False; }
  Assert(Os.Window);

  heap_allocator Heap = InitHeap(Megabytes(128));
  memory_arena *GraphicsMemory2D = AllocateArena();
  b32 Result = InitDebugRenderSystem(&Heap, GraphicsMemory2D);

  return Result;
}

link_internal void
ProcessInputAndRedrawWindow()
{
  ResetInputForFrameStart(&Plat.Input, &Hotkeys);

  v2 LastMouseP = Plat.MouseP;
  while ( ProcessOsMessages(&Os, &Plat) );
  Plat.MouseDP = LastMouseP - Plat.MouseP;

  Assert(Plat.WindowWidth && Plat.WindowHeight);

  BindHotkeysToInput(&Hotkeys, &Plat.Input);

  DebugFrameBegin(GetDebugState()->UiGroup, Plat.dt, Hotkeys.Debug_ToggleMenu, Hotkeys.Debug_ToggleProfiling);
  DebugFrameEnd(Plat.dt);

  BonsaiSwapBuffers(&Os);

  GL.BindFramebuffer(GL_FRAMEBUFFER, 0);
  GL.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}



// DLL API

link_export u64
QueryMemoryRequirements()
{
  return sizeof(debug_state);
}

link_export void
BonsaiDebug_OnLoad(debug_state *DebugState, thread_local_state *ThreadStates, s32 CallerIsInternalBuild)
{
  s32 WeAreInternalBuild = BONSAI_INTERNAL;

  // NOTE(Jesse): This can't be an assert because they get compiled out if the debug lib is an external build!
  if (WeAreInternalBuild != CallerIsInternalBuild)
  {
    Error("Detected Loading unmatched interal/external build for bonsai debug lib.  CallerInternal(%d), DebugInternal(%d)", CallerIsInternalBuild, WeAreInternalBuild);
  }

  Assert(DebugState);
  Assert(ThreadStates);

  InitializeOpenglFunctions();

  Global_DebugStatePointer = DebugState;
  Global_ThreadStates = ThreadStates;

  SetThreadLocal_ThreadIndex(0);

  DebugState->FrameEnd                        = DebugFrameEnd;
  DebugState->FrameBegin                      = DebugFrameBegin;
  DebugState->RegisterArena                   = RegisterArena;
  DebugState->UnregisterArena                 = UnregisterArena;
  DebugState->WorkerThreadAdvanceDebugSystem  = WorkerThreadAdvanceDebugSystem;
  DebugState->MainThreadAdvanceDebugSystem    = MainThreadAdvanceDebugSystem;
  DebugState->MutexWait                       = MutexWait;
  DebugState->MutexAquired                    = MutexAquired;
  DebugState->MutexReleased                   = MutexReleased;
  DebugState->GetProfileScope                 = GetProfileScope;
  DebugState->Debug_Allocate                  = DEBUG_Allocate;
  DebugState->RegisterThread                  = RegisterThread;
  DebugState->TrackDrawCall                   = TrackDrawCall;
  DebugState->GetThreadLocalState             = GetThreadLocalState;
  DebugState->DebugValue_r32                  = DebugValue_r32;
  DebugState->DebugValue_u32                  = DebugValue_u32;
  DebugState->DebugValue_u64                  = DebugValue_u64;
  DebugState->DumpScopeTreeDataToConsole      = DumpScopeTreeDataToConsole;
  DebugState->GetReadScopeTree                = GetReadScopeTree;
  DebugState->GetWriteScopeTree               = GetWriteScopeTree;

  DebugState->WriteMemoryRecord               = WriteMemoryRecord;
  DebugState->ClearMemoryRecordsFor           = ClearMemoryRecordsFor;

  DebugState->OpenAndInitializeDebugWindow    = OpenAndInitializeDebugWindow;
  DebugState->ProcessInputAndRedrawWindow     = ProcessInputAndRedrawWindow;
  DebugState->InitializeRenderSystem          = InitDebugRenderSystem;
  DebugState->SetRenderer                     = SetRenderer;
}

link_export b32
InitDebugState(debug_state *DebugState)
{
  /* Assert(AllocationSize >= QueryMemoryRequirements()); */
  Assert(DebugState->Initialized == False);

  Assert(ThreadLocal_ThreadIndex == 0);

  LastMs = GetHighPrecisionClock();

  DebugState->Frames[1].StartingCycle = GetCycleCount();

  DebugState->Initialized = True;

  Global_DebugStatePointer = DebugState;

  InitDebugDataSystem(DebugState);

#if BONSAI_WIN32
  Platform_EnableContextSwitchTracing();
#endif

  DebugState->DebugDoScopeProfiling = True;

  b32 Result = True;
  return Result;
}

