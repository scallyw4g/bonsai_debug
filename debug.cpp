#define DEBUG_SYSTEM_API 1
#define DEBUG_SYSTEM_INTERNAL_BUILD 1

#define PLATFORM_GL_IMPLEMENTATIONS 1
#define PLATFORM_LIBRARY_AND_WINDOW_IMPLEMENTATIONS 1

#include <bonsai_stdlib/bonsai_stdlib.h>
#include <bonsai_stdlib/bonsai_stdlib.cpp>


#include <engine/engine.h>
#include <engine/engine.cpp>

#include <bonsai_debug/debug_data_system.cpp>
#include <bonsai_debug/debug_render_system.cpp>

#if BONSAI_WIN32
#include <bonsai_debug/headers/win32_etw.cpp>
/* #include <bonsai_debug/headers/win32_pmc.cpp> */
#endif

/* debug_state *Global_DebugStatePointer; */

global_variable os Os = {};
global_variable platform Plat = {};
global_variable hotkeys Hotkeys = {};

link_internal void
DebugFrameEnd(v2 *MouseP, v2 *MouseDP, v2 ScreenDim, input *Input, r32 dt, picked_world_chunk_static_buffer *PickedChunks)
{
  TIMED_FUNCTION();

  GL.Disable(GL_CULL_FACE);

  debug_state *DebugState = GetDebugState();

  min_max_avg_dt Dt = {};

  debug_ui_render_group *UiGroup = &DebugState->UiGroup;

  /* UiGroup->GameGeo               = &DebugState->GameGeo; */
  /* UiGroup->GameGeoShader         = &DebugState->GameGeoShader; */
  UiGroup->Input                 = Input;
  UiGroup->ScreenDim             = ScreenDim;
  UiGroup->MouseP                = MouseP;
  UiGroup->MouseDP               = MouseDP;

  if ( ! (Input->LMB.Pressed || Input->RMB.Pressed) )
  {
    UiGroup->PressedInteractionId = 0;
  }

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

  Dt = ComputeMinMaxAvgDt();

  v4 Padding = V4(25,0,25,0);
  ui_style Style = UiStyleFromLightestColor(V3(1));

  ui_element_reference DtTable = PushTableStart(UiGroup);
    StartColumn(UiGroup, &Style, Padding);
      Text(UiGroup, CS("+"));
      Text(UiGroup, CS(Dt.Max - Dt.Avg));
    EndColumn(UiGroup);

    PushNewRow(UiGroup);

    PushColumn(UiGroup, CS(Dt.Avg), &Style, Padding);
    PushColumn(UiGroup, CS(dt*1000.0f), &Style, Padding);

    PushNewRow(UiGroup);

    StartColumn(UiGroup, &Style, Padding);
      Text(UiGroup, CS("-"));
      Text(UiGroup, CS(Dt.Avg - Dt.Min));
    EndColumn(UiGroup);

  PushTableEnd(UiGroup);

  PushTableStart(UiGroup, Position_RightOf, DtTable);
    StartColumn(UiGroup, &Style, Padding);
      Text(UiGroup, CS("Allocations"));
    EndColumn(UiGroup);

    PushColumn(UiGroup, CS("Pushes"));
    PushColumn(UiGroup, CS("Draw Calls"));
    PushNewRow(UiGroup);

#if 1
    PushColumn(UiGroup, CS(TotalStats.Allocations), &Style, Padding);
    PushColumn(UiGroup, CS(TotalStats.Pushes));
    PushColumn(UiGroup, CS(TotalDrawCalls));
    PushNewRow(UiGroup);
#endif

  PushTableEnd(UiGroup);

  /* PushNewRow(UiGroup); // TODO(Jesse): This probably shouldn't have to be here. */
  END_BLOCK("Draw Status Bar");

  if (DebugState->DisplayDebugMenu)
  {
    TIMED_BLOCK("Draw Debug Menu");

    PushTableStart(UiGroup);

    {
      TIMED_NAMED_BLOCK("Draw Toggle Buttons");
      ui_style *Style = (DebugState->UIType & DebugUIType_PickedChunks) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(UiGroup, CS("PickedChunks"), (umm)"PickedChunks", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_PickedChunks);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_Graphics) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(UiGroup, CS("Graphics"), (umm)"Graphics", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Graphics);
      }
    }

    {
      /* ui_style *Style = &DefaultStyle; */
      /* if ( DebugState->UIType & DebugUIType_Network)  *1/ */ /* { */ /*   Style = &DefaultSelectedStyle; */ /* } */
      /* if (Button(UiGroup, CS("Network"), (umm)"Network", Style)) */
      /* { */
      /*   ToggleBitfieldValue(DebugState->UIType, DebugUIType_Network); */
      /* } */
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CollatedFunctionCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(UiGroup, CS("Functions"), (umm)"Functions", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CollatedFunctionCalls);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CallGraph) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(UiGroup, CS("Callgraph"), (umm)"Callgraph", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CallGraph);
      }
    }

    {
      ui_style *Style = &DefaultStyle;
      if ( DebugState->UIType & DebugUIType_Memory ) { Style = &DefaultSelectedStyle; }
      if (Button(UiGroup, CS("Memory"), (umm)"Memory", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Memory);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_DrawCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(UiGroup, CS("DrawCalls"), (umm)"DrawCalls", Style))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_DrawCalls);
      }
    }

    PushTableEnd(UiGroup);




    if (DebugState->UIType & DebugUIType_PickedChunks)
    {
      DebugState->PickedChunk = DrawPickedChunks(UiGroup, &DebugState->PickedChunksRenderGroup, PickedChunks, DebugState->PickedChunk);
    }

    if (DebugState->UIType & DebugUIType_Graphics)
    {
      DebugDrawGraphicsHud(UiGroup, DebugState);
    }

    /* if (DebugState->UIType & DebugUIType_Network) */
    /* { */
    /*   DebugDrawNetworkHud(UiGroup, &Plat->Network, ServerState); */
    /* } */

    if (DebugState->UIType & DebugUIType_CollatedFunctionCalls)
    {
      DebugDrawCollatedFunctionCalls(UiGroup, DebugState);
    }

    if (DebugState->UIType & DebugUIType_CallGraph)
    {
      DebugDrawCallGraph(UiGroup, DebugState, Dt.Max);
    }

    if (DebugState->UIType & DebugUIType_Memory)
    {
      DebugDrawMemoryHud(UiGroup, DebugState);
    }

    if (DebugState->UIType & DebugUIType_DrawCalls)
    {
      DebugDrawDrawCalls(UiGroup);
    }

    END_BLOCK("Draw Debug Menu");
  }

  UiGroup->HighestWindow = GetHighestWindow(UiGroup, UiGroup->CommandBuffer);

  if (UiGroup->HighestWindow)
  {
    UiGroup->HighestWindow->Scroll.y += Input->MouseWheelDelta * 5;
  }

  FlushCommandBuffer(UiGroup, UiGroup->CommandBuffer);

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

  if (UiGroup->PressedInteractionId == 0 &&
      (Input->LMB.Pressed || Input->RMB.Pressed))
  {
    UiGroup->PressedInteractionId = StringHash("GameViewport");
  }

#if 0
  if (DebugState->DoChunkPicking && Input->LMB.Clicked)
  {
    DebugState->DoChunkPicking = False;
  }
#endif

  /* GL.Enable(GL_CULL_FACE); */

  Ensure( RewindArena(TranArena) );

  return;
}

link_internal void
DebugFrameBegin(b32 ToggleMenu, b32 ToggleProfiling)
{
  debug_state *State = GetDebugState();
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
  b32 Result = InitDebugRenderSystem(GetDebugState(), &Heap);

  return Result;
}

link_internal b32
ProcessInputAndRedrawWindow()
{
  ResetInputForFrameStart(&Plat.Input, &Hotkeys);

  v2 LastMouseP = Plat.MouseP;
  while ( ProcessOsMessages(&Os, &Plat) );
  Plat.MouseDP = LastMouseP - Plat.MouseP;

  Assert(Plat.WindowWidth && Plat.WindowHeight);

  BindHotkeysToInput(&Hotkeys, &Plat.Input);

  DebugFrameBegin(Hotkeys.Debug_ToggleMenu, Hotkeys.Debug_ToggleProfiling);
  DebugFrameEnd(&Plat.MouseP, &Plat.MouseDP, V2(Plat.WindowWidth, Plat.WindowHeight), &Plat.Input, Plat.dt, 0);

  BonsaiSwapBuffers(&Os);

  GetDebugState()->ClearFramebuffers(&GetDebugState()->PickedChunksRenderGroup);

  GL.BindFramebuffer(GL_FRAMEBUFFER, 0);
  GL.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return Os.ContinueRunning;
}



// DLL API

link_export u64
QueryMemoryRequirements()
{
  return sizeof(debug_state);
}

link_export void
BonsaiDebug_OnLoad(debug_state *DebugState)
{
  InitializeOpenglFunctions();

  Global_DebugStatePointer = DebugState;

  DebugState->ClearFramebuffers               = ClearFramebuffers;
  DebugState->FrameEnd                        = DebugFrameEnd;
  DebugState->FrameBegin                      = DebugFrameBegin;
  DebugState->RegisterArena                   = RegisterArena;
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
}

link_export b32
InitDebugState(debug_state *DebugState, u64 AllocationSize)
{
  Assert(AllocationSize >= QueryMemoryRequirements());
  Assert(DebugState->Initialized == False);

  LastMs = GetHighPrecisionClock();

  DebugState->Frames[1].StartingCycle = GetCycleCount();

  DebugState->Initialized = True;

  Global_DebugStatePointer = DebugState;

  InitDebugDataSystem(DebugState);

  DEBUG_REGISTER_NAMED_ARENA(TranArena, 0, "debug_lib TranArena");

  /* Platform_EnableContextSwitchTracing(); */

  DebugState->DebugDoScopeProfiling = True;

  b32 Result = True;
  return Result;
}

