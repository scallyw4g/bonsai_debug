#include <bonsai_debug/src/debug_data_system.cpp>
#include <bonsai_debug/src/debug_render_system.cpp>

#if BONSAI_WIN32
#include <bonsai_debug/src/platform/win32_etw.cpp>
/* #include <bonsai_debug/src/platform/win32_pmc.cpp> */
#endif

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
         ++DrawCountIndex)
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

#if 0
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
#endif


  min_max_avg_dt Dt = ComputeMinMaxAvgDt();

  {
    v4 Padding = V4(25,0,25,0);
    ui_style Style = UiStyleFromLightestColor(V3(1));

    PushTableStart(Ui);
      u32 Start = StartColumn(Ui, &Style, Padding);
        Text(Ui, FormatCountedString(GetTranArena(), CS("(%.1f) :: (+%.1f) (%.1f) (-%.1f) :: Allocations(%d) Pushes(%d)"),
          r64(PrevDt*1000.0f),
          r64(Dt.Max - Dt.Avg),
          r64(Dt.Avg),
          r64(Dt.Avg-Dt.Min),
          TotalStats.Allocations,
          TotalStats.Pushes
        ));
      EndColumn(Ui, Start);
      PushNewRow(Ui);
    PushTableEnd(Ui);
  }

  debug_state *DebugState = GetDebugState();
  if (DebugState->DisplayDebugMenu)
  {
    TIMED_NAMED_BLOCK(DrawDebugMenu);

    PushTableStart(Ui);

    v4 Padding = V4(15,5,15,5);

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_Graphics) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Graphics"), UiId(0, "Graphics", u32(DebugState->UIType)), Style, &DefaultButtonBackgroundStyle, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Graphics);
      }
    }

    {
      /* ui_style *Style = &DefaultStyle; */
      /* if ( DebugState->UIType & DebugUIType_Network)  *1/ */ /* { */ /*   Style = &DefaultSelectedStyle; */ /* } */
      /* if (Button(Ui, CS("Network"), UiId(0, "Network", u32(DebugState->UIType)), Style)) */
      /* { */
      /*   ToggleBitfieldValue(DebugState->UIType, DebugUIType_Network); */
      /* } */
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CollatedFunctionCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Functions"), UiId(0, "Functions", u32(DebugState->UIType)), Style, &DefaultButtonBackgroundStyle, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CollatedFunctionCalls);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_CallGraph) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("Callgraph"), UiId(0, "Callgraph", u32(DebugState->UIType)), Style, &DefaultButtonBackgroundStyle, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_CallGraph);
      }
    }

    {
      ui_style *Style = &DefaultStyle;
      if ( DebugState->UIType & DebugUIType_Memory ) { Style = &DefaultSelectedStyle; }
      if (Button(Ui, CS("Memory"), UiId(0, "Memory", u32(DebugState->UIType)), Style, &DefaultButtonBackgroundStyle, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_Memory);
      }
    }

    {
      ui_style *Style = (DebugState->UIType & DebugUIType_DrawCalls) ? &DefaultSelectedStyle : &DefaultStyle;
      if (Button(Ui, CS("DrawCalls"), UiId(0, "DrawCalls", u32(DebugState->UIType)), Style, &DefaultButtonBackgroundStyle, Padding))
      {
        ToggleBitfieldValue(DebugState->UIType, DebugUIType_DrawCalls);
      }
    }

    PushNewRow(Ui);
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


link_export b32
InitDebugState(debug_state *DebugState)
{
  Assert(DebugState->Initialized == False);
  Assert(ThreadLocal_ThreadIndex == 0);

  DebugState->Frames[1].StartingCycle = GetCycleCount();

  // NOTE(Jesse): hook debug functions up before we make any function calls
  // such that we have a well-working state to go from
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
  /* DebugState->DebugValue_r32                  = DebugValue_r32; */
  /* DebugState->DebugValue_u32                  = DebugValue_u32; */
  /* DebugState->DebugValue_u64                  = DebugValue_u64; */
  DebugState->DumpScopeTreeDataToConsole      = DumpScopeTreeDataToConsole;
  DebugState->GetReadScopeTree                = GetReadScopeTree;
  DebugState->GetWriteScopeTree               = GetWriteScopeTree;

  DebugState->WriteMemoryRecord               = WriteMemoryRecord;
  DebugState->ClearMemoryRecordsFor           = ClearMemoryRecordsFor;
  DebugState->InitializeRenderSystem          = InitDebugRenderSystem;
  DebugState->SetRenderer                     = SetRenderer;
  DebugState->PushHistogramDataPoint          = PushHistogramDataPoint;

  DebugState->Initialized = True;

  InitDebugDataSystem(DebugState);

  DebugState->DebugDoScopeProfiling = True;

  b32 Result = True;
  return Result;
}




debug_timed_function::debug_timed_function(const char *Name)
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

#if !POOF_PREPROCESSOR
debug_timed_function::~debug_timed_function()
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


debug_histogram_function::~debug_histogram_function()
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
    DebugState->PushHistogramDataPoint(this->Scope->EndingCycle-this->Scope->StartingCycle);
  }
}
#endif
