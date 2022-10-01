#define BONSAI_DEBUG_SYSTEM_API 1
#define DEBUG_LIB_INTERNAL_BUILD 1
#define PLATFORM_GL_IMPLEMENTATIONS 1
#define PLATFORM_LIBRARY_AND_WINDOW_IMPLEMENTATIONS 1

#include <bonsai_stdlib/bonsai_stdlib.h>
#include <bonsai_stdlib/bonsai_stdlib.cpp>

#include <engine/engine.h>
#include <engine/engine.cpp>

#include <bonsai_debug/debug_data_system.cpp>
#include <bonsai_debug/interactable.cpp>
#include <bonsai_debug/debug_render_system.cpp>

debug_state *Global_DebugStatePointer;

global_variable os Os = {};
global_variable platform Plat = {};
global_variable hotkeys Hotkeys = {};

link_internal void
DebugFrameEnd(v2 *MouseP, v2 *MouseDP, v2 ScreenDim, input *Input, r32 dt)
{
  TIMED_FUNCTION();

  GL.Disable(GL_CULL_FACE);

  debug_state *DebugState = GetDebugState();

  min_max_avg_dt Dt = {};

  debug_ui_render_group *UiGroup = &DebugState->UiGroup;

  UiGroup->GameGeo               = &DebugState->GameGeo;
  UiGroup->GameGeoShader         = &DebugState->GameGeoShader;
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
       DrawCountIndex < Global_DrawCallArrayLength;
       ++ DrawCountIndex)
  {
    debug_draw_call *Call = &Global_DrawCalls[DrawCountIndex];
    if (Call->Caller)
    {
      TotalDrawCalls += Call->Calls;
    }
  }

  Dt = ComputeMinMaxAvgDt();

  v4 Padding = V4(0,0,50,0);
  ui_style Style = UiStyleFromLightestColor(V3(1));

  ui_element_reference DtTable = PushTableStart(UiGroup);
    StartColumn(UiGroup, &Style, Padding);
      Text(UiGroup, CS("+"));
      Text(UiGroup, CS(Dt.Max - Dt.Avg));
    EndColumn(UiGroup);

    PushNewRow(UiGroup);

    PushColumn(UiGroup, CS(Dt.Avg), &Style, Padding);
    PushColumn(UiGroup, CS(dt*1000.0f));

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

    PushColumn(UiGroup, CS(TotalStats.Allocations), &Style, Padding);
    PushColumn(UiGroup, CS(TotalStats.Pushes));
    PushColumn(UiGroup, CS(TotalDrawCalls));
    PushNewRow(UiGroup);
  PushTableEnd(UiGroup);

  END_BLOCK("Status Bar");


  if (DebugState->DisplayDebugMenu)
  {
    v4 Padding = V4(25);
    ui_style Style =  UiStyleFromLightestColor(V3(1));

    PushTableStart(UiGroup);

    /* if (Button(UiGroup, CS("PickedChunks"), (umm)"PickedChunks", &Style, Padding)) */
    /* { */
    /*   ToggleBitfieldValue(DebugState->UIType, DebugUIType_PickedChunks); */
    /* } */

    if (Button(UiGroup, CS("Graphics"), (umm)"Graphics", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_Graphics);
    }

    if (Button(UiGroup, CS("Network"), (umm)"Network", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_Network);
    }

    if (Button(UiGroup, CS("Functions"), (umm)"Functions", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_CollatedFunctionCalls);
    }

    if (Button(UiGroup, CS("Callgraph"), (umm)"Callgraph", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_CallGraph);
    }

    if (Button(UiGroup, CS("Memory"), (umm)"Memory", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_Memory);
    }

    if (Button(UiGroup, CS("DrawCalls"), (umm)"DrawCalls", &Style, Padding))
    {
      ToggleBitfieldValue(DebugState->UIType, DebugUIType_DrawCalls);
    }
    PushTableEnd(UiGroup);




#if 0
    if (DebugState->UIType & DebugUIType_PickedChunks)
    {
      DrawPickedChunks(UiGroup);
    }
#endif

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

  }

  UiGroup->HighestWindow = GetHighestWindow(UiGroup, UiGroup->CommandBuffer);
  FlushCommandBuffer(UiGroup, UiGroup->CommandBuffer);

  FlushBuffers(UiGroup, UiGroup->ScreenDim);

  DebugState->BytesBufferedToCard = 0;

  for( u32 DrawCountIndex = 0;
       DrawCountIndex < Global_DrawCallArrayLength;
       ++ DrawCountIndex)
  {
     Global_DrawCalls[DrawCountIndex] = NullDrawCall;
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

  GL.Enable(GL_CULL_FACE);

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
}

link_export u64
QueryMemoryRequirements()
{
  return sizeof(debug_state);
}

link_internal b32
OpenAndInitializeDebugWindow()
{
  Assert(GetDebugState());
  Assert(GetDebugState()->Initialized);
  GetDebugState()->DebugDoScopeProfiling = True;

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
  ClearClickedFlags(&Plat.Input);
  Clear(&Hotkeys);

  v2 LastMouseP = Plat.MouseP;
  while ( ProcessOsMessages(&Os, &Plat) );
  Plat.MouseDP = LastMouseP - Plat.MouseP;

  Assert(Plat.WindowWidth && Plat.WindowHeight);

  BindHotkeysToInput(&Hotkeys, &Plat.Input);

  DebugFrameBegin(Hotkeys.Debug_ToggleMenu, Hotkeys.Debug_ToggleProfiling);
  DebugFrameEnd(&Plat.MouseP, &Plat.MouseDP, V2(Plat.WindowWidth, Plat.WindowHeight), &Plat.Input, Plat.dt);

  RewindArena(TranArena);

  BonsaiSwapBuffers(&Os);

  GetDebugState()->ClearFramebuffers();

  GL.BindFramebuffer(GL_FRAMEBUFFER, 0);
  GL.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return Os.ContinueRunning;
}

link_export b32
InitDebugState(debug_state *DebugState, u64 AllocationSize)
{
  Assert(AllocationSize >= QueryMemoryRequirements());
  Assert(DebugState->Initialized == False);

  LastMs = GetHighPrecisionClock();

  DebugState->Frames[1].StartingCycle = GetCycleCount();

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
  DebugState->DebugValue                      = DebugValue;
  DebugState->DumpScopeTreeDataToConsole      = DumpScopeTreeDataToConsole;
  DebugState->GetReadScopeTree                = GetReadScopeTree;
  DebugState->GetWriteScopeTree               = GetWriteScopeTree;

  DebugState->WriteMemoryRecord               = WriteMemoryRecord;
  DebugState->ClearMemoryRecordsFor           = ClearMetaRecordsFor;

  DebugState->OpenAndInitializeDebugWindow    = OpenAndInitializeDebugWindow;
  DebugState->ProcessInputAndRedrawWindow     = ProcessInputAndRedrawWindow;
  DebugState->InitializeRenderSystem          = InitDebugRenderSystem;

  DebugState->Initialized = True;

  Global_DebugStatePointer = DebugState;

  InitDebugDataSystem(DebugState);

  b32 Result = True;
  return Result;
}

