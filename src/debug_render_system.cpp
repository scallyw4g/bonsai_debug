/****************************                       **************************/
/****************************  Mutex Introspection  **************************/
/****************************                       **************************/



#if 0
link_internal void
DrawWaitingBar(mutex_op_record *WaitRecord, mutex_op_record *AquiredRecord, mutex_op_record *ReleasedRecord,
               renderer_2d *Ui, layout *Layout, u64 FrameStartingCycle, u64 FrameTotalCycles, r32 TotalGraphWidth, r32 Z, v2 MaxClip)
{
  Assert(WaitRecord->Op == MutexOp_Waiting);
  Assert(AquiredRecord->Op == MutexOp_Aquired);
  Assert(ReleasedRecord->Op == MutexOp_Released);

  Assert(AquiredRecord->Mutex == WaitRecord->Mutex);
  Assert(ReleasedRecord->Mutex == WaitRecord->Mutex);

  u64 WaitCycleCount = AquiredRecord->Cycle - WaitRecord->Cycle;
  u64 AquiredCycleCount = ReleasedRecord->Cycle - AquiredRecord->Cycle;

  untextured_2d_geometry_buffer *Geo = &Ui->Geo;
  cycle_range FrameRange = {FrameStartingCycle, FrameTotalCycles};

  cycle_range WaitRange = {WaitRecord->Cycle, WaitCycleCount};
  DrawCycleBar( &WaitRange, &FrameRange, TotalGraphWidth, 0, V3(1, 0, 0), Ui, Geo, Layout, Z, MaxClip, 0);

  cycle_range AquiredRange = {AquiredRecord->Cycle, AquiredCycleCount};
  DrawCycleBar( &AquiredRange, &FrameRange, TotalGraphWidth, 0, V3(0, 1, 0), Ui, Geo, Layout, Z, MaxClip, 0);

  return;
}
#endif



/************************                        *****************************/
/************************  Thread Perf Bargraph  *****************************/
/************************                        *****************************/



link_internal counted_string
BuildNameStringFor(char Prefix, counted_string Name, u32 DepthAdvance)
{
  counted_string Result = FormatCountedString(GetTranArena(), CSz("%*c%S"), DepthAdvance, Prefix, Name);
  return Result;
}

link_internal void
BufferScopeTreeEntry(renderer_2d *Ui, debug_profile_scope *Scope,
                     u64 TotalCycles, u64 TotalFrameCycles, u64 CallCount, u32 Depth)
{
  Assert(TotalFrameCycles);

  r32 Percentage = 100.0f * (r32)SafeDivide0((r64)TotalCycles, (r64)TotalFrameCycles);
  u64 AvgCycles = (u64)SafeDivide0(TotalCycles, CallCount);

  PushColumn(Ui, CS(Percentage));
  PushColumn(Ui, CS(AvgCycles));
  PushColumn(Ui, CS(CallCount));

  char Prefix = ' ';
  if (Scope->Expanded && Scope->Child)
  {
    Prefix = '-';
  }
  else if (Scope->Child)
  {
    Prefix = '+';
  }

  u32 DepthSpaces = (Depth*2)+1;
  counted_string NameString = BuildNameStringFor(Prefix, CS(Scope->Name), DepthSpaces);
  PushColumn(Ui, NameString, &DefaultStyle, DefaultColumnPadding, UiElementAlignmentFlag_LeftAlign);

  return;
}

global_variable r32 Global_CoreBarHeight = 3.f;
global_variable r32 Global_CoreBarPadding = 3.f;

link_internal void
PushScopeBarsRecursive( renderer_2d *Ui,
                        window_layout *Window,
                        debug_profile_scope *Scope,
                        cycle_range *Frame,
                        r32 TotalGraphWidth,
                        r32 BarHeight,
                        random_series *Entropy,
                        u32 Depth = 0 )
{
  while (Scope)
  {
    cycle_range Range = {Scope->StartingCycle, GetCycleCount(Scope)};

    /* umm NameHash = Hash(CS(Scope->Name)) ^ Hash(CS(Scope->Location)); */

    random_series ScopeSeries = {.Seed = (u64)Scope};
    r32 Tint = RandomBetween(0.5f, &ScopeSeries, 1.0f);

    umm NameHash = Hash(CS(Scope->Name));
    v3 FunctionColor = ColorFromHash(NameHash) * Tint;
    ui_style FunctionStyle = UiStyleFromLightestColor(FunctionColor);
    r32 yOffsetFunction = Depth * BarHeight;

    {
      cs ScopeName = CS(Scope->Name);
      interactable_handle Bar = PushButtonStart(Ui, UiId(Window, "CycleBarHoverInteraction", Scope));
        PushCycleBar(Ui, &Range, Frame, TotalGraphWidth, BarHeight, yOffsetFunction, &FunctionStyle, V4(0), ScopeName);
      PushButtonEnd(Ui);
      if (Hover(Ui, &Bar)) { PushTooltip(Ui, ScopeName); }
      if (Clicked(Ui, &Bar)) { Scope->Expanded = !Scope->Expanded; }
    }

    if (Scope->Expanded) { PushScopeBarsRecursive(Ui, Window, Scope->Child, Frame, TotalGraphWidth, BarHeight, Entropy, Depth+1); }
    Scope = Scope->Sibling;
  }

  return;
}

link_internal void
DrawThreadsWindow(renderer_2d *Ui, debug_state *SharedState)
{
  TIMED_FUNCTION();

  random_series Entropy = {};
  r32 TotalGraphWidth = 1500.0f;
  window_layout_flags Flags =  Cast(window_layout_flags, WindowLayoutFlag_Align_Bottom|WindowLayoutFlag_StartupSize_InferHeight);
  local_persist window_layout CycleGraphWindow = WindowLayout("Thread View", {}, V2(TotalGraphWidth+150.f, 0.f), Flags);

  PushWindowStart(Ui, &CycleGraphWindow);

  auto DebugState = GetDebugState();
  ui_toggle_button_group ViewMode =
    RadioButtonGroup_callgraph_window_view_mode( Ui,
                                                &CycleGraphWindow,
                                                  CSz(""),
                                                &DebugState->CallgraphWindowViewMode);

  switch (callgraph_window_view_mode(*ViewMode.EnumStorage))
  {
    case CallgraphWindowViewMode_Frame:
    {
      cs ETStatusString = CSz("");
      switch (Global_EventTracingStatus)
      {
        case EventTracingStatus_Unstarted:
        {
          ETStatusString = CSz("CSwitch Tracing: Unstarted");
        } break;
        case EventTracingStatus_Starting:
        {
          ETStatusString = CSz("CSwitch Tracing: Starting");
        } break;
        case EventTracingStatus_Running:
        {
          ETStatusString = CSz("CSwitch Tracing: Running");
        } break;
        case EventTracingStatus_Error:
        {
          ETStatusString = CSz("CSwitch Tracing: Error");
        } break;
      }

      Text(Ui, ETStatusString);
      PushNewRow(Ui);
      PushNewRow(Ui);


      /* PushTableStart(Ui); */

      s32 TotalThreadCount                 = (s32)GetTotalThreadCount();
      frame_stats *FrameStats              = SharedState->Frames + SharedState->ReadScopeIndex;
      cycle_range FrameCycles              = {FrameStats->StartingCycle, FrameStats->TotalCycles};

      r32 BarHeight = (r32)Global_Font.Size.y;

#if 1
      /* r32 TotalMs = Max(33.333333f, (r32)FrameStats->FrameMs); */
      r32 TotalMs = (r32)FrameStats->FrameMs;

      if (TotalMs > 0.0f)
      {
        r32 MarkerWidth = 1.f;
        r32 MinY = 0.f;
        r32 TotalGraphHeight = TotalThreadCount * (Global_CoreBarHeight + (Global_CoreBarPadding*2.f) + BarHeight);

        {
          r32 FramePerc = 16.666666f/TotalMs;
          r32 xOffset = FramePerc*TotalGraphWidth;
          PushUntexturedQuad(Ui, V2(xOffset, 0.f), V2(MarkerWidth, TotalGraphHeight), zDepth_Border, &Global_DefaultSuccessStyle, V4(0), UiElementLayoutFlag_NoAdvance);
        }
        {
          r32 FramePerc = 33.333333f/TotalMs;
          r32 xOffset = FramePerc*TotalGraphWidth;
          PushUntexturedQuad(Ui, V2(xOffset, 0.f), V2(MarkerWidth, TotalGraphHeight), zDepth_Border, &Global_DefaultWarnStyle, V4(0), UiElementLayoutFlag_NoAdvance);
        }
      }
#endif


      debug_thread_state *MainThreadState  = GetThreadLocalStateFor(0);
      debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + SharedState->ReadScopeIndex;

      /* PushTableStart(Ui); */
      for ( s32 ThreadIndex = 0;
                ThreadIndex < TotalThreadCount;
              ++ThreadIndex)
      {
        TIMED_NAMED_BLOCK(Thread_Loop);

        PushColumn(Ui, FormatCountedString(GetTranArena(), CSz("T %u "), ThreadIndex));
        debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);

        if (ThreadState->ThreadId)
        {
          u32 StartIndex = StartColumn(Ui);

#if 1
          debug_context_switch_event_buffer_stream *ContextSwitchStream = ThreadState->ContextSwitches;
          debug_context_switch_event_buffer_stream_block *PrevBlock = 0;
          debug_context_switch_event_buffer_stream_block *CurrentBlock = ContextSwitchStream->FirstBlock;
          while (CurrentBlock)
          {
            debug_context_switch_event_buffer *ContextSwitches = &CurrentBlock->Buffer;
            debug_context_switch_event *LastCSwitchEvt = ContextSwitches->Events;

            /* if (ThreadIndex == 0) */
            /* { */
            /*   DebugLine("%u", ContextSwitches->At); */
            /* } */

            b32 FoundOutOfOrderEvent = False;
            for ( u32 ContextSwitchEventIndex = 1;
                      ContextSwitchEventIndex < ContextSwitches->At;
                    ++ContextSwitchEventIndex )
            {
              debug_context_switch_event *CSwitch = ContextSwitches->Events + ContextSwitchEventIndex;

              if ( RangeContains(FrameStats->StartingCycle, CSwitch->CycleCount, FrameStats->StartingCycle+FrameStats->TotalCycles) ||
                   RangeContains(FrameStats->StartingCycle, LastCSwitchEvt->CycleCount, FrameStats->StartingCycle+FrameStats->TotalCycles) )
              {
                cycle_range Range = {
                  .StartCycle = Max(FrameStats->StartingCycle, LastCSwitchEvt->CycleCount),
                  .TotalCycles = CSwitch->CycleCount-LastCSwitchEvt->CycleCount
                };

                if (LastCSwitchEvt->Type == ContextSwitch_On)
                {
                  v3 CoreColor = Ui->DebugColors[LastCSwitchEvt->ProcessorNumber];
                  ui_style Style = UiStyleFromLightestColor(CoreColor);
                  PushCycleBar(Ui, &Range, &FrameCycles, TotalGraphWidth, Global_CoreBarHeight, 0, &Style, V4(0, 0, 0, Global_CoreBarHeight));
                }
              }

              LastCSwitchEvt = CSwitch;
            }

            PrevBlock = CurrentBlock;
            CurrentBlock = CurrentBlock->Next;
          }

          PushForceAdvance(Ui, V2(0, Global_CoreBarHeight + Global_CoreBarPadding*2));
#endif


          {
            debug_scope_tree *ReadTree = ThreadState->ScopeTrees + SharedState->ReadScopeIndex;
            /* if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded) */
            {
              debug_timed_function BlockTimer2("Push Scope Bars");
              PushScopeBarsRecursive(Ui, &CycleGraphWindow, ReadTree->Root, &FrameCycles, TotalGraphWidth, BarHeight, &Entropy);
            }
          }

          {
            debug_scope_tree *ReadTree = ThreadState->ScopeTrees + ((SharedState->ReadScopeIndex + 1) % DEBUG_FRAMES_TRACKED);
            /* if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded) */
            {
              debug_timed_function BlockTimer2("Push Scope Bars");
              PushScopeBarsRecursive(Ui, &CycleGraphWindow, ReadTree->Root, &FrameCycles, TotalGraphWidth, BarHeight, &Entropy);
            }
          }

          {
            debug_scope_tree *ReadTree = ThreadState->ScopeTrees + ((SharedState->ReadScopeIndex + 2) % DEBUG_FRAMES_TRACKED);
            /* if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded) */
            {
              debug_timed_function BlockTimer2("Push Scope Bars");
              PushScopeBarsRecursive(Ui, &CycleGraphWindow, ReadTree->Root, &FrameCycles, TotalGraphWidth, BarHeight, &Entropy);
            }
          }

          {
            debug_scope_tree *ReadTree = ThreadState->ScopeTrees + ((SharedState->ReadScopeIndex - 1) % DEBUG_FRAMES_TRACKED);
            /* if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded) */
            {
              debug_timed_function BlockTimer2("Push Scope Bars");
              PushScopeBarsRecursive(Ui, &CycleGraphWindow, ReadTree->Root, &FrameCycles, TotalGraphWidth, BarHeight, &Entropy);
            }
          }

          {
            debug_scope_tree *ReadTree = ThreadState->ScopeTrees + ((SharedState->ReadScopeIndex - 2) % DEBUG_FRAMES_TRACKED);
            /* if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded) */
            {
              debug_timed_function BlockTimer2("Push Scope Bars");
              PushScopeBarsRecursive(Ui, &CycleGraphWindow, ReadTree->Root, &FrameCycles, TotalGraphWidth, BarHeight, &Entropy);
            }
          }



          EndColumn(Ui, StartIndex);

          PushNewRow(Ui);
        }
        else
        {
          PushColumn(Ui, CSz(" --- Thread Not Registered ---"));
          PushNewRow(Ui);
        }
      }

      /* PushTableEnd(Ui); */

#if 0
      u32 UnclosedMutexRecords = 0;
      u32 TotalMutexRecords = 0;
      TIMED_BLOCK("Mutex Record Collation");
      for ( u32 ThreadIndex = 0;
            ThreadIndex < TotalThreadCount;
            ++ThreadIndex)
      {
        debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
        mutex_op_array *MutexOps = ThreadState->MutexOps + SharedState->ReadScopeIndex;
        mutex_op_record *FinalRecord = MutexOps->Records + MutexOps->NextRecord;

        for (u32 OpRecordIndex = 0;
            OpRecordIndex < MutexOps->NextRecord;
            ++OpRecordIndex)
        {
          mutex_op_record *CurrentRecord = MutexOps->Records + OpRecordIndex;
          if (CurrentRecord->Op == MutexOp_Waiting)
          {
            mutex_op_record *Aquired = FindRecord(CurrentRecord, FinalRecord, MutexOp_Aquired);
            mutex_op_record *Released = FindRecord(CurrentRecord, FinalRecord, MutexOp_Released);
            if (Aquired && Released)
            {
              r32 yOffset = ThreadIndex * Ui->Font.LineHeight;
              Layout->At.y += yOffset;
              DrawWaitingBar(CurrentRecord, Aquired, Released, Ui, Layout, &Ui->Font, FrameStartingCycle, FrameTotalCycles, TotalGraphWidth);
              Layout->At.y -= yOffset;
            }
            else
            {
              Warn("Unclosed Mutex Record at %u on thread %u", OpRecordIndex, ThreadIndex);
            }
          }
        }
      }
      END_BLOCK("Mutex Record Collation");
#endif
    } break;

    case CallgraphWindowViewMode_Jobs:
    {
      /* IterateOver(); */
    } break;
  }

  PushWindowEnd(Ui, &CycleGraphWindow);
}

link_internal interactable_handle
DrawHistogramCell(renderer_2d *Ui, window_layout *Window, u32 ThingIndex, v2 MaxCellDim, r32 Perc, v3 Foreground, v3 Background, v4 Pad)
{
  r32 VerticalAdvance = MaxCellDim.y;

  v2 QuadDim = MaxCellDim * V2(1.0f, Perc);
  v2 Offset = V2(0.f, VerticalAdvance-QuadDim.y);

  ui_style Style = UiStyleFromLightestColor(Foreground);
  ui_style BackgroundStyle = UiStyleFromLightestColor(Background);

  interactable_handle B = PushButtonStart(Ui, UiId(Window, "HistogramInteraction", ThingIndex) );
    PushUntexturedQuad(Ui, V2(Pad.x, 0), MaxCellDim, zDepth_Background, &BackgroundStyle, {}, UiElementLayoutFlag_NoAdvance);
    PushUntexturedQuad(Ui, Offset, QuadDim, zDepth_Background, &Style, Pad);
  PushButtonEnd(Ui);

  return B;
}

link_internal void
DrawHistogram(debug_ui_render_group *Ui, debug_state *SharedState)
{
  TIMED_FUNCTION();

  random_series Entropy = {};
  window_layout_flags Flags =  Cast(window_layout_flags, WindowLayoutFlag_StartupSize_InferHeight);
  window_layout *Window = GetOrCreateWindow(Ui, "Histogram", Flags);

  r32 GraphHeight = 200.f;
  PushWindowStart(Ui, Window);

  if (Button(Ui, CSz("Reset"), UiId(Window, "Reset", 0u)))
  {
    for (u32 SampleIndex = 0; SampleIndex < DEBUG_HISTOGRAM_MAX_SAMPLES; ++SampleIndex)
    {
      SharedState->HistogramSamples.Start[SampleIndex] = 0;
    }
  }
  PushNewRow(Ui);


  u64 MinCycles = u64_MAX;
  u64 MaxCycles = 0;
  u64 TotalCycles = 0;

  u64 Elements = 0;
  /* const char *Name = 0; */
  {
    for (u32 SampleIndex = 0; SampleIndex < DEBUG_HISTOGRAM_MAX_SAMPLES; ++SampleIndex)
    {
      u64 Sample = SharedState->HistogramSamples.Start[SampleIndex];

      MaxCycles = Max(MaxCycles, Sample);
      if (Sample)
      {
        MinCycles = Min(MinCycles, Sample);
        TotalCycles += Sample;
       ++Elements;
       /* Name = Sample->Name; */ 
      }
    }
  }

/*   if (Name) */
/*   { */
/*     Window.Title = FSz("Histogram : %s", Name); */
/*   } */


  u64 AvgCycles = 0;
  if (Elements) { AvgCycles = TotalCycles / Elements; }

  PushColumn(Ui, CSz("Max Cycles"));
  PushColumn(Ui, CSz("Min Cycles"));
  PushColumn(Ui, CSz("Avg Cycles"));
  PushNewRow(Ui);

  PushColumn(Ui, CS(MaxCycles));
  PushColumn(Ui, CS(MinCycles));
  PushColumn(Ui, CS(AvgCycles));
  PushNewRow(Ui);

  {
    for (u32 SampleIndex = 0; SampleIndex < DEBUG_HISTOGRAM_MAX_SAMPLES; ++SampleIndex)
    {
      u64 Sample = SharedState->HistogramSamples.Start[SampleIndex];
      if (Sample > 0)
      {
        r32 Perc = r32(r64(Sample)/r64(MaxCycles));

        interactable_handle B = DrawHistogramCell(Ui, Window, u32(SampleIndex), V2(1.f, GraphHeight), Perc, V3(1.f), V3(0.3f), V4(0.f));

        if (Hover(Ui, &B))
        {
          PushTooltip(Ui, FSz("%lu / %lu (%.2f)%%", Sample, MaxCycles, r64(Perc*100.f)));
        }

        if ((SampleIndex+1) % 1024 == 0)
        {
          PushNewRow(Ui);
        }
      }
    }
  }

  PushWindowEnd(Ui, Window);
}


/******************************              *********************************/
/******************************  Call Graph  *********************************/
/******************************              *********************************/



#define MAX_RECORDED_FUNCTION_CALLS 256
static called_function ProgramFunctionCalls[MAX_RECORDED_FUNCTION_CALLS];
static called_function NullFunctionCall = {};

link_internal void
CollateAllFunctionCalls(debug_profile_scope* Current)
{
  if (!Current || !Current->Name)
    return;

  called_function* Prev = 0;
  for ( u32 FunctionIndex = 0;
        FunctionIndex < MAX_RECORDED_FUNCTION_CALLS;
        ++FunctionIndex)
  {
    called_function* Func = ProgramFunctionCalls + FunctionIndex;

    if (Func->Name == Current->Name || !Func->Name)
    {
      Func->Name = Current->Name;
      Func->CallCount++;
      s32 SwapIndex = MAX_RECORDED_FUNCTION_CALLS;
      for (s32 PrevIndex = (s32)FunctionIndex -1;
          PrevIndex >= 0;
          --PrevIndex)
      {
        Prev = ProgramFunctionCalls + PrevIndex;
        if (Prev->CallCount < Func->CallCount)
        {
          SwapIndex = PrevIndex;
        }
        else
          break;
      }

      if(SwapIndex < MAX_RECORDED_FUNCTION_CALLS)
      {
        called_function* Swap = ProgramFunctionCalls + SwapIndex;
        called_function Temp = *Swap;
        *Swap = *Func;
        *Func = Temp;
      }

      break;
    }

    Prev = Func;

    if (FunctionIndex == MAX_RECORDED_FUNCTION_CALLS-1)
    {
      Warn("MAX_RECORDED_FUNCTION_CALLS limit reached");
    }
  }

  if (Current->Sibling)
  {
    CollateAllFunctionCalls(Current->Sibling);
  }

  if (Current->Child)
  {
    CollateAllFunctionCalls(Current->Child);
  }

  return;
}

link_internal unique_debug_profile_scope *
ListContainsScope(unique_debug_profile_scope* List, debug_profile_scope* Query)
{
  unique_debug_profile_scope* Result = 0;
  while (List)
  {
    if (StringsMatch(List->Name, Query->Name))
    {
      Result = List;
      break;
    }
    List = List->NextUnique;
  }

  return Result;
}

link_internal void
DumpScopeTreeDataToConsole_Internal(debug_profile_scope *Scope_in, debug_profile_scope *TreeRoot, memory_arena *Memory)
{
  unique_debug_profile_scope* UniqueScopes = {};

  debug_profile_scope* CurrentUniqueScopeQuery = Scope_in;
  while (CurrentUniqueScopeQuery)
  {
    unique_debug_profile_scope* GotUniqueScope = ListContainsScope(UniqueScopes, CurrentUniqueScopeQuery);
    if (!GotUniqueScope )
    {
      GotUniqueScope = AllocateProtection(unique_debug_profile_scope, GetTranArena(), 1, False);
      GotUniqueScope->NextUnique = UniqueScopes;
      UniqueScopes = GotUniqueScope;
    }

    GotUniqueScope->Name = CurrentUniqueScopeQuery->Name;
    GotUniqueScope->CallCount++;
    u64 CycleCount = GetCycleCount(CurrentUniqueScopeQuery);
    GotUniqueScope->TotalCycles += CycleCount;
    GotUniqueScope->MinCycles = Min(CycleCount, GotUniqueScope->MinCycles);
    GotUniqueScope->MaxCycles = Max(CycleCount, GotUniqueScope->MaxCycles);
    GotUniqueScope->Scope = CurrentUniqueScopeQuery;

    CurrentUniqueScopeQuery = CurrentUniqueScopeQuery->Sibling;
  }

  while (UniqueScopes)
  {

    DebugLine("\n------------------------\n");
    DebugLine("%s\n", UniqueScopes->Name);
    DebugLine("%u\n", UniqueScopes->CallCount);
    Assert(UniqueScopes->CallCount);

    DebugLine("Total: %lu\n", UniqueScopes->TotalCycles);
    DebugLine("Min: %lu\n", UniqueScopes->MinCycles);
    DebugLine("Max: %lu\n", UniqueScopes->MaxCycles);
    DebugLine("Avg: %f\n", r64(UniqueScopes->TotalCycles / UniqueScopes->CallCount));

    DumpScopeTreeDataToConsole_Internal(UniqueScopes->Scope->Child, TreeRoot, Memory);
    UniqueScopes = UniqueScopes->NextUnique;
  }

  return;
}

link_internal void
BufferFirstCallToEach(renderer_2d *Ui,
                      debug_profile_scope *Scope_in,
                      debug_profile_scope *TreeRoot,
                      memory_arena *Memory,
                      window_layout* Window,
                      u64 TotalFrameCycles,
                      u32 Depth)
{
  unique_debug_profile_scope* UniqueScopes = {};

  debug_profile_scope* CurrentUniqueScopeQuery = Scope_in;
  while (CurrentUniqueScopeQuery)
  {
    unique_debug_profile_scope* GotUniqueScope = ListContainsScope(UniqueScopes, CurrentUniqueScopeQuery);
    if (!GotUniqueScope )
    {
      GotUniqueScope = AllocateProtection(unique_debug_profile_scope, GetTranArena(), 1, False);
      GotUniqueScope->NextUnique = UniqueScopes;
      UniqueScopes = GotUniqueScope;
      GotUniqueScope->Name = CurrentUniqueScopeQuery->Name;
      GotUniqueScope->Scope = CurrentUniqueScopeQuery;
    }

    GotUniqueScope->CallCount++;

    u64 CycleCount = GetCycleCount(CurrentUniqueScopeQuery);
    GotUniqueScope->TotalCycles += CycleCount;
    GotUniqueScope->MinCycles = Min(CycleCount, GotUniqueScope->MinCycles);
    GotUniqueScope->MaxCycles = Max(CycleCount, GotUniqueScope->MaxCycles);

    CurrentUniqueScopeQuery = CurrentUniqueScopeQuery->Sibling;
  }

  while (UniqueScopes)
  {
    interactable_handle ScopeTextInteraction = PushButtonStart(Ui, UiId(Window, "profile_scope", UniqueScopes->Scope) );
      BufferScopeTreeEntry(Ui, UniqueScopes->Scope, UniqueScopes->TotalCycles, TotalFrameCycles, UniqueScopes->CallCount, Depth);
    PushButtonEnd(Ui);
    PushNewRow(Ui);

    if (UniqueScopes->Scope->Expanded)
      BufferFirstCallToEach(Ui, UniqueScopes->Scope->Child, TreeRoot, Memory, Window, TotalFrameCycles, Depth+1);

    if (Clicked(Ui, &ScopeTextInteraction))
    {
      GetDebugState()->HotFunction = UniqueScopes->Scope;
      UniqueScopes->Scope->Expanded = !UniqueScopes->Scope->Expanded;
    }

    UniqueScopes = UniqueScopes->NextUnique;
  }

  return;
}

link_internal void
DrawFrameTicker(renderer_2d *Ui, window_layout *Window, debug_state *DebugState, r32 MaxMs)
{
  TIMED_FUNCTION();

  PushTableStart(Ui);

    v4 Pad = V4(1, 0, 1, 0);
    v2 MaxBarDim = V2(15.0f, 80.0f);
    r32 HorizontalAdvance = (MaxBarDim.x+Pad.Left+Pad.Right);
    r32 VerticalAdvance = MaxBarDim.y;

    v2 LineDim = V2( HorizontalAdvance * DEBUG_FRAMES_TRACKED, 2.0f);
    {
      r32 MsPerc = SafeDivide0(33.333f, MaxMs);
      r32 MinPOffset = MaxBarDim.y * MsPerc;
      v2 MinP = {{ 0.0f, MaxBarDim.y - MinPOffset }};
      PushUntexturedQuad(Ui, MinP, LineDim, zDepth_Text, &Global_DefaultWarnStyle, V4(0), UiElementLayoutFlag_NoAdvance);
    }

    {
      r32 MsPerc = (r32)SafeDivide0(16.666f, MaxMs);
      r32 MinPOffset = MaxBarDim.y * MsPerc;
      v2 MinP = {{ 0.0f, MaxBarDim.y - MinPOffset }};
      PushUntexturedQuad(Ui, MinP, LineDim, zDepth_Text, &Global_DefaultSuccessStyle, V4(0), UiElementLayoutFlag_NoAdvance);
    }

    volatile umm MinCycles = umm_MAX;
    volatile umm MaxCycles = 0;

    for ( u32 FrameIndex = 0;
              FrameIndex < DEBUG_FRAMES_TRACKED;
            ++FrameIndex )
    {
      frame_stats *Frame = DebugState->Frames + FrameIndex;
      r32 Perc = SafeDivide0(Frame->FrameMs, MaxMs);

      if (Frame->StartingCycle)
      {
        MinCycles = Min(MinCycles, Frame->StartingCycle);
      }

      if (Frame->StartingCycle && Frame->TotalCycles)
      {
        MaxCycles = Max(MaxCycles, Frame->StartingCycle + Frame->TotalCycles);
      }

      v2 QuadDim = MaxBarDim * V2(1.0f, Perc);
      v2 Offset = V2(0.f, VerticalAdvance-QuadDim.y);

      r32 Brightness = 0.35f;

      ui_style Style =
        FrameIndex == DebugState->ReadScopeIndex ?
        UiStyleFromLightestColor(V3(Brightness,       0.0f, Brightness)) :
        UiStyleFromLightestColor(V3(Brightness, Brightness,       0.0f));

      ui_style BackgroundStyle = FrameIndex == DebugState->ReadScopeIndex ?
         UiStyleFromLightestColor(V3(Brightness, Brightness, Brightness)) :
         DefaultBlurredStyle;

      interactable_handle B = PushButtonStart(Ui, UiId(Window, "FrameTickerHoverInteraction", FrameIndex) );
        PushUntexturedQuad(Ui, V2(Pad.x, 0), MaxBarDim, zDepth_Background, &BackgroundStyle, {}, UiElementLayoutFlag_NoAdvance);
        PushUntexturedQuad(Ui, Offset, QuadDim, zDepth_Background, &Style, Pad);
      PushButtonEnd(Ui);

      if (Clicked(Ui, &B)) { DebugState->ReadScopeIndex = FrameIndex; }
    }

    DebugState->MaxCycles = MaxCycles;
    DebugState->MinCycles = MinCycles;



  PushTableEnd(Ui);

  frame_stats *Frame = DebugState->Frames + DebugState->ReadScopeIndex;

  u32 TotalMutexOps = GetTotalMutexOpsForReadFrame();
  PushTableStart(Ui);
    PushColumn(Ui, CS(Frame->FrameMs));
    PushColumn(Ui, CS(Frame->TotalCycles));
    PushColumn(Ui, CS(TotalMutexOps));
    PushNewRow(Ui);
  PushTableEnd(Ui);

  /* DebugState->DebugValue_u64(DebugState->MinCycles, "MinCycles"); */
  /* DebugState->DebugValue_u64(DebugState->MaxCycles, "MaxCycles"); */

  return;
}

link_internal void
DebugCallgraphWindow(renderer_2d *Ui, debug_state *DebugState, r32 MaxMs)
{
  TIMED_FUNCTION();

  DrawFrameTicker(Ui, 0, DebugState, Max(33.3f, MaxMs));

  DrawThreadsWindow(Ui, DebugState);
  DrawHistogram(Ui, DebugState);

  debug_thread_state *MainThreadState  = GetThreadLocalStateFor(0);
  debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + DebugState->ReadScopeIndex;

  TIMED_BLOCK("Call Graph");
    local_persist window_layout FunctionTreeWindow = WindowLayout("Function Tree", WindowLayoutFlag_Align_Right);

    PushWindowStart(Ui, &FunctionTreeWindow);
      PushTableStart(Ui);
        PushColumn(Ui, CSz("Frame %"));
        PushColumn(Ui, CSz("Cycles"));
        PushColumn(Ui, CSz("Calls"));
        PushColumn(Ui, CSz("Name"));
        PushNewRow(Ui);

        s32 TotalThreadCount = (s32)GetTotalThreadCount();
        for ( s32 ThreadIndex = 0;
                  ThreadIndex < TotalThreadCount;
                ++ThreadIndex )
        {
          debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
          debug_scope_tree *ReadTree = ThreadState->ScopeTrees + DebugState->ReadScopeIndex;
          frame_stats *Frame = DebugState->Frames + DebugState->ReadScopeIndex;

          if (Frame->TotalCycles && MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded)
          {
            debug_timed_function BlockTimer2("Buffer First Call To Each");
            BufferFirstCallToEach(Ui, ReadTree->Root, ReadTree->Root, ThreadsafeDebugMemoryAllocator(), &FunctionTreeWindow, Frame->TotalCycles, 0);
          }
        }
      PushTableEnd(Ui);
    PushWindowEnd(Ui, &FunctionTreeWindow);

  END_BLOCK("Call Graph");


  return;
}


#if 0
debug_global hotkeys HotkeyThing;
link_internal void
OpenDebugWindowAndLetUsDoStuff()
{
#if 0
  debug_state* DebugState = GetDebugState();

  DEBUG_FRAME_BEGIN(&HotkeyThing);
  DEBUG_FRAME_END(&Plat->MouseP, &Plat->MouseDP, V2(Plat->WindowWidth, Plat->WindowHeight), &Plat->Input, Plat->dt);
  RewindArena(TranArena);
#endif
}
#endif

link_export void
DumpScopeTreeDataToConsole()
{
  memory_arena* Temp = AllocateArena();
  debug_state* DebugState = GetDebugState();

  /* Print("Starting debug data dump"); */

  /* debug_thread_state *ThreadState = GetThreadLocalStateFor(0); */
  debug_scope_tree *ReadTree = DebugState->GetWriteScopeTree();
  DumpScopeTreeDataToConsole_Internal(ReadTree->Root, ReadTree->Root, Temp);

  /* Print("Ending debug data dump"); */

  return;
}



/*************************                      ******************************/
/*************************  Collated Fun Calls  ******************************/
/*************************                      ******************************/



link_internal void
PushCallgraphRecursive(debug_ui_render_group *Ui, window_layout *Window, debug_profile_scope* At)
{
  if (At)
  {
    u64 CycleCount = GetCycleCount(At);
    cs Name = FSz("%s %lu", At->Name, CycleCount);

    if (At->Child)
    {
      if (ToggleButton(Ui, Name, Name, UiId(Window, "callgraph scope toggle_button", At), &DefaultStyle, &DefaultBackgroundStyle, DefaultButtonPadding, UiElementAlignmentFlag_LeftAlign))
      {
        PushNewRow(Ui);
        OPEN_INDENT_FOR_TOGGLEABLE_REGION();
        PushCallgraphRecursive(Ui, Window, At->Child);
        CLOSE_INDENT_FOR_TOGGLEABLE_REGION();
      }
      else
      {
        PushNewRow(Ui);
      }
    }
    else
    {
      PushColumn(Ui, Name, UiElementAlignmentFlag_LeftAlign);
      PushNewRow(Ui);
    }

    if (At->Sibling)
    {
      PushCallgraphRecursive(Ui, Window, At->Sibling);
    }
  }
}

link_internal void
DumpCallgraphRecursive(renderer_2d *Ui, debug_profile_scope* At, u32 Depth = 0)
{
  for (u32 DepthIndex = 0;
      DepthIndex < Depth;
      ++DepthIndex)
  {
    DebugChars("  ");
  }

  u64 CycleCount = GetCycleCount(At);
  DebugChars("%s (%lu) \n", At->Name, CycleCount);

  if (At->Child)
  {
    DumpCallgraphRecursive(Ui, At->Child, Depth+1);
  }

  if (At->Sibling)
  {
    DumpCallgraphRecursive(Ui, At->Sibling, Depth);
  }

  return;
}

link_internal void
DebugDrawCollatedFunctionCalls(renderer_2d *Ui, debug_state *DebugState)
{
  TIMED_FUNCTION();
#if 0
  debug_thread_state *MainThreadState = GetThreadLocalStateFor(0);
  debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + DebugState->ReadScopeIndex;
  TIMED_BLOCK("Collated Function Calls");
    window_layout FunctionCallWindow = GetOrCreateWindow(Ui, "Functions", V2(0, 200));
    CollateAllFunctionCalls(MainThreadReadTree->Root);
    PushWindowStart(Ui, &FunctionCallWindow);
    PushTableStart(Ui);
    for ( u32 FunctionIndex = 0;
        FunctionIndex < MAX_RECORDED_FUNCTION_CALLS;
        ++FunctionIndex)
    {
      called_function *Func = ProgramFunctionCalls + FunctionIndex;
      if (Func->Name)
      {
        PushColumn(Ui, CS(Func->Name));
        PushColumn(Ui, CS(Func->CallCount));
        PushNewRow(Ui);
      }
    }
    PushTableEnd(Ui);
    PushWindowEnd(Ui, &FunctionCallWindow);
  END_BLOCK("Collated Function Calls");
#endif

  TIMED_BLOCK("Hot Function Stuff");

    if (DebugState->HotFunction)
    {
      const char* NameOfHotFunction = DebugState->HotFunction->Name;

      u32 SortKeyCount = 0;
      {
        debug_profile_scope* CurrentScope = DebugState->HotFunction;
        while (CurrentScope)
        {
          if (StringsMatch(CurrentScope->Name, NameOfHotFunction))
          {
            ++SortKeyCount;
          }
          CurrentScope = CurrentScope->Sibling;
        }
      }

      sort_key* SortBuffer = Allocate(sort_key, GetTranArena(), SortKeyCount);

      {
        u32 CurrentSortKeyIndex = 0;
        debug_profile_scope* CurrentScope = DebugState->HotFunction;
        while (CurrentScope)
        {
          if (StringsMatch(CurrentScope->Name, NameOfHotFunction))
          {
            Assert(CurrentSortKeyIndex < SortKeyCount);
            SortBuffer[CurrentSortKeyIndex].Index = (u64)CurrentScope;
            SortBuffer[CurrentSortKeyIndex].Value = GetCycleCount(CurrentScope);
            ++CurrentSortKeyIndex;
          }
          CurrentScope = CurrentScope->Sibling;
        }
        Assert(CurrentSortKeyIndex == SortKeyCount);
      }


      BubbleSort(SortBuffer, SortKeyCount);


      {
        local_persist window_layout HotFunctionWindow = WindowLayout("Hot Function Window", V2(400, 200));
        PushWindowStart(Ui, &HotFunctionWindow);
        PushTableStart(Ui);

        for (u32 SortIndex = 0;
                 SortIndex < SortKeyCount;
               ++SortIndex)
        {
          debug_profile_scope* CurrentScope = (debug_profile_scope*)SortBuffer[SortIndex].Index;

#define TEXT_OUTPUT_FOR_FUNCTION_CALLS 0
#if TEXT_OUTPUT_FOR_FUNCTION_CALLS
          Print(CS(CurrentScope->Name));
          DebugChars("\n");
          Print(CS(GetCycleCount(CurrentScope)));
          DebugChars("\n");
          DumpCallgraphRecursive(Ui, CurrentScope->Child);
          DebugChars("\n");
#else
          PushColumn(Ui, CS(CurrentScope->Name));
          PushColumn(Ui, CS(GetCycleCount(CurrentScope)));
          PushNewRow(Ui);
          PushNewRow(Ui);
          PushCallgraphRecursive(Ui, &HotFunctionWindow, CurrentScope->Child);
          PushNewRow(Ui);
          PushNewRow(Ui);
#endif

        }

        PushTableEnd(Ui);
        PushWindowEnd(Ui, &HotFunctionWindow);
      }
#if TEXT_OUTPUT_FOR_FUNCTION_CALLS
      exit(0);
#endif


    }

  END_BLOCK();


}



/******************************              *********************************/
/******************************  Draw Calls  *********************************/
/******************************              *********************************/



debug_global debug_draw_call NullDrawCall = {};

link_internal void
TrackDrawCall(const char* Caller, u32 VertexCount)
{
  u64 Index = ((u64)Caller) % TRACKED_DRAW_CALLS_MAX;

  debug_draw_call *DrawCall = GetDebugState()->TrackedDrawCalls + Index;

  if (DrawCall->Caller)
  {
    debug_draw_call* First = DrawCall;
    while( DrawCall->Caller &&
           !(StringsMatch(DrawCall->Caller, Caller) && DrawCall->N == VertexCount)
         )
    {
      ++Index;
      Index = Index % TRACKED_DRAW_CALLS_MAX;
      DrawCall = GetDebugState()->TrackedDrawCalls + Index;
      if (DrawCall == First)
      {
        Error("Draw Call table full!");
        break;
      }
    }
  }

  DrawCall->Caller = Caller;
  DrawCall->N = VertexCount;
  DrawCall->Calls++;

  return;
}

link_internal void
DebugDrawDrawCalls(renderer_2d *Ui)
{
  TIMED_FUNCTION();

  window_layout *DrawCallWindow = GetOrCreateWindow(Ui, "Draw Calls");
  PushWindowStart(Ui, DrawCallWindow);

  PushTableStart(Ui);

  PushColumn(Ui, CSz("Caller"));
  PushColumn(Ui, CSz("Calls"));
  PushColumn(Ui, CSz("Bytes"));
  PushNewRow(Ui);

  for( u32 DrawCountIndex = 0;
       DrawCountIndex < TRACKED_DRAW_CALLS_MAX;
       ++ DrawCountIndex)
  {
     debug_draw_call *DrawCall = &GetDebugState()->TrackedDrawCalls[DrawCountIndex];
     if (DrawCall->Caller)
     {
       PushColumn(Ui, CS(DrawCall->Caller));
       PushColumn(Ui, CS(DrawCall->Calls));
       PushColumn(Ui, CS(DrawCall->N));
       PushNewRow(Ui);
     }
  }

  PushTableEnd(Ui);

  PushWindowEnd(Ui, DrawCallWindow);
  return;
}



/*******************************            **********************************/
/*******************************   Memory   **********************************/
/*******************************            **********************************/


link_internal interactable_handle
PushArenaBargraph(renderer_2d *Ui, v3 FColor, v3 BColor, umm TotalUsed, r32 TotalPerc, umm Remaining, ui_id InteractionId, r32 BarHeight)
{
  counted_string StatsString = FormatCountedString(GetTranArena(), CSz("%S / %S (%S)"), MemorySize(TotalUsed), MemorySize(TotalUsed + Remaining), MemorySize(Remaining));
  PushColumn(Ui, StatsString);
  PushNewRow(Ui);

  r32 BargraphWidth = 800.f;

  interactable_handle Handle = PushButtonStart(Ui, InteractionId);
    PushBargraph(Ui, TotalPerc, FColor, BColor, BargraphWidth, &BarHeight);
  PushButtonEnd(Ui);

  PushNewRow(Ui);

  return Handle;
}

link_internal void
PushMemoryBargraphTable(renderer_2d *Ui, window_layout *Window, selected_arenas *SelectedArenas, memory_arena_stats MemStats, umm TotalUsed, memory_arena *HeadArena)
{
  PushNewRow(Ui);
  v3 DefaultForegroundColor =  V3(.25f, .1f, .35f);
  v3 DefaultBackgroundColor =  V3(.5f);

  r32 TotalPerc = (r32)SafeDivide0(TotalUsed, MemStats.TotalAllocated);
  // TODO(Jesse, id: 110, tags: ui, semantic): Should we do something special when interacting with this thing instead of Ignored-ing it?
  ui_id Ignored = {{1,2,3,4}};
  PushArenaBargraph(Ui, DefaultForegroundColor, DefaultBackgroundColor, TotalUsed, TotalPerc, MemStats.Remaining, Ignored, Global_Font.Size.y);
  PushNewRow(Ui);

  memory_arena *CurrentArena = HeadArena;
  while (CurrentArena && CurrentArena->Start)
  {
    v3 FColor = DefaultForegroundColor;
    v3 BColor = DefaultBackgroundColor;
    for (u32 ArenaIndex = 0;
        ArenaIndex < SelectedArenas->Count;
        ++ArenaIndex)
    {
      selected_memory_arena *Selected = &SelectedArenas->Arenas[ArenaIndex];
      if (Selected->ArenaAddress == HashArena(CurrentArena))
      {
        FColor = DefaultForegroundColor * 1.8f;
        BColor = DefaultBackgroundColor * 1.8f;
      }
    }

    umm CurrentUsed = TotalSize(CurrentArena) - Remaining(CurrentArena);
    r32 CurrentPerc = (r32)SafeDivide0(CurrentUsed, TotalSize(CurrentArena));

    interactable_handle Handle = PushArenaBargraph(Ui, FColor, BColor, CurrentUsed, CurrentPerc, Remaining(CurrentArena), UiId(Window, "arena_bargraph", HashArena(CurrentArena)), Global_Font.Size.y*.5f);
    if (Clicked(Ui, &Handle))
    {
      selected_memory_arena *Found = 0;
      for (u32 ArenaIndex = 0;
          ArenaIndex < SelectedArenas->Count;
          ++ArenaIndex)
      {
        selected_memory_arena *Selected = &SelectedArenas->Arenas[ArenaIndex];
        if (Selected->ArenaAddress == HashArena(CurrentArena))
        {
          Found = Selected;
          break;
        }
      }
      if (Found)
      {
        *Found = SelectedArenas->Arenas[--SelectedArenas->Count];
      }
      else
      {
        selected_memory_arena *Selected = &SelectedArenas->Arenas[SelectedArenas->Count++];
        Selected->ArenaAddress = HashArena(CurrentArena);
        Selected->ArenaBlockAddress = HashArenaBlock(CurrentArena);
      }

    }

    CurrentArena = CurrentArena->Prev;
  }

  return;
}

link_internal void
PackSortAndBufferMemoryRecords(renderer_2d *Ui, memory_record *Records, u64 RecordCount)
{
  // Densely pack collated records
  u32 PackedRecords = 0;
  for ( u32 MetaIndex = 0;
        MetaIndex < RecordCount;
        ++MetaIndex )
  {
    memory_record *Record = Records+MetaIndex;
    if (Record->Name)
    {
      Records[PackedRecords++] = *Record;
    }
  }

  // Sort collation table
  for ( u32 MetaIndex = 0;
      MetaIndex < PackedRecords;
      ++MetaIndex)
  {
    memory_record *SortValue = Records + MetaIndex;
    for ( u32 TestMetaIndex = 0;
          TestMetaIndex < PackedRecords;
          ++TestMetaIndex )
    {
      memory_record *TestValue = Records + TestMetaIndex;

      if ( GetAllocationSize(SortValue) > GetAllocationSize(TestValue) )
      {
        memory_record Temp = *SortValue;
        *SortValue = *TestValue;
        *TestValue = Temp;
      }
    }
  }



  // Buffer collation table text
  for ( u32 MetaIndex = 0;
      MetaIndex < PackedRecords;
      ++MetaIndex)
  {
    memory_record *Collated = Records + MetaIndex;
    u64 HashValue = (Collated->ArenaMemoryBlock) * 2654435761;
    v3 ArenaColor = ColorFromHash(HashValue) * 1.2f;
    ui_style ArenaStyle = UiStyleFromLightestColor(ArenaColor);
    if (Collated->Name)
    {
      umm AllocationSize = GetAllocationSize(Collated);
      PushColumn(Ui,  CS(Collated->ThreadId));

      if (Collated->ArenaAddress == BONSAI_NO_ARENA && Collated->ArenaMemoryBlock)
      {
        // @ArenaMemoryBlock-as-char-pointer
        PushColumn(Ui,  CS((char*)Collated->ArenaMemoryBlock), &ArenaStyle);
      }
      else
      {
        PushColumn(Ui,  CS((u16)HashValue), &ArenaStyle);
      }

      PushColumn(Ui,  MemorySize(AllocationSize));
      PushColumn(Ui,  FormatThousands(Collated->StructCount));
      PushColumn(Ui,  FormatThousands(Collated->PushCount));
      PushColumn(Ui, CS(Collated->Name));
      PushNewRow(Ui);
    }

    continue;
  }
}

link_internal void
DebugMetadataHeading(renderer_2d *Ui)
{
  PushColumn(Ui, CSz("Thread"));
  PushColumn(Ui, CSz("Arena"));
  PushColumn(Ui, CSz("Memory"));
  PushColumn(Ui, CSz("Structs"));
  PushColumn(Ui, CSz("Pushes"));
  PushColumn(Ui, CSz("Name"));
  PushNewRow(Ui);

}

link_internal void
PushDebugPushMetaData(renderer_2d *Ui, selected_arenas *SelectedArenas, umm CurrentMemoryBlock)
{
  memory_record CollatedMetaTable[META_TABLE_SIZE] = {};

  DebugMetadataHeading(Ui);

  // Pick out relevant metadata and write to collation table
  u32 TotalThreadCount = GetWorkerThreadCount() + 1;


  for ( u32 ThreadIndex = 0;
            ThreadIndex < TotalThreadCount;
          ++ThreadIndex)
  {
    for ( u32 MetaIndex = 0;
              MetaIndex < META_TABLE_SIZE;
            ++MetaIndex)
    {
      memory_record *Meta = &GetDebugState()->ThreadStates[ThreadIndex].MetaTable[MetaIndex];

      for (u32 ArenaIndex = 0;
          ArenaIndex < SelectedArenas->Count;
          ++ArenaIndex)
      {
        selected_memory_arena *Selected = &SelectedArenas->Arenas[ArenaIndex];
        if ( Meta->ArenaMemoryBlock == CurrentMemoryBlock &&
             Meta->ArenaAddress     == Selected->ArenaAddress )
        {
          CollateMetadata(Meta, CollatedMetaTable);
        }
      }
    }
  }

  PackSortAndBufferMemoryRecords(Ui, CollatedMetaTable, META_TABLE_SIZE);

  return;
}


link_internal void
DebugDrawMemoryHud(renderer_2d *Ui, debug_state *DebugState)
{
  TIMED_FUNCTION();

  debug_global memory_arena Global_PermDebugMemory;

  local_persist b32 UntrackedAllocationsExpanded = {};
  b32 FoundUntrackedAllocations = False;
  memory_record UnknownRecordTable[META_TABLE_SIZE] = {};
  {
    s32 TotalThreadCount = (s32)GetTotalThreadCount();
    for ( s32 ThreadIndex = 0;
              ThreadIndex < TotalThreadCount;
            ++ThreadIndex)
    {
      for ( u32 MetaIndex = 0;
          MetaIndex < META_TABLE_SIZE;
          ++MetaIndex)
      {
        memory_record *Meta = GetDebugState()->ThreadStates[ThreadIndex].MetaTable + MetaIndex;

        b32 FoundRecordOwner = False;
        if (Meta->Name)
        {
          for ( u32 ArenaIndex = 0;
                ArenaIndex < REGISTERED_MEMORY_ARENA_COUNT;
                ++ArenaIndex )
          {
            registered_memory_arena *Current = DebugState->RegisteredMemoryArenas + ArenaIndex;
            if (Current->Arena)
            {
              if (Meta->ArenaMemoryBlock == HashArenaBlock(Current->Arena))
              {
                FoundRecordOwner = True;
              }
            }
          }

          if (FoundRecordOwner == False)
          {
            if (Meta->ArenaAddress == BONSAI_NO_ARENA)
            {
              FoundUntrackedAllocations = True;
              WriteToMetaTable(Meta, UnknownRecordTable, PushesMatchExactly);
            }
            else
            {
              const char *Name = GetNullTerminated(FSz("memory_location(%p)", Meta->ArenaMemoryBlock), &Global_PermDebugMemory);
              DebugRegisterArena("Unknown Source Location", (memory_arena*)Meta->ArenaMemoryBlock, INVALID_THREAD_LOCAL_THREAD_INDEX);
              DebugRegisterArenaName(Name, (memory_arena*)Meta->ArenaMemoryBlock);
            }
          }
        }
      }
    }
  }









  v2 Basis = V2(20, 300);
  window_layout *MemoryArenaList = GetOrCreateWindow(Ui, "Memory Arena List", Basis);


  PushWindowStart(Ui, MemoryArenaList);
  PushTableStart(Ui);

  /* v3 TitleColor = V3(.5f); */
  v3 TitleColor = V3(1.f, 1.f, 1.f);
  ui_style TitleStyle = UiStyleFromLightestColor(TitleColor);


  PushColumn(Ui, CSz("SourceLocation"),   &TitleStyle);
  PushColumn(Ui, CSz("Name"),   &TitleStyle);
  PushColumn(Ui, CSz("Size"),   &TitleStyle);
  PushColumn(Ui, CSz("Pushes"), &TitleStyle);
  PushColumn(Ui, CSz("Thread"), &TitleStyle);
  PushNewRow(Ui);

  if (FoundUntrackedAllocations)
  {
    ui_style UnnamedStyle = UntrackedAllocationsExpanded ? DefaultSelectedStyle : DefaultStyle;

    interactable_handle UnknownAllocationsExpandInteraction =
    PushButtonStart(Ui, UiId(MemoryArenaList, "unnamed arenas memory_window_expand_interaction", 0ull));
      PushColumn(Ui, CSz("?"), &UnnamedStyle);
      PushColumn(Ui, CSz("?"), &UnnamedStyle);
      PushColumn(Ui, CSz("?"), &UnnamedStyle);
      PushColumn(Ui, CSz("Untracked Allocations"), &UnnamedStyle);
      PushNewRow(Ui);
    PushButtonEnd(Ui);
    if (Clicked(Ui, &UnknownAllocationsExpandInteraction))
    {
      UntrackedAllocationsExpanded = !UntrackedAllocationsExpanded;
    }
  }


  selected_arenas *SelectedArenas = GetDebugState()->SelectedArenas;



  //
  // Draw active arenas
  //
  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (!Current->Arena) continue;


    ui_style Style = Current->Expanded? DefaultSelectedStyle : DefaultStyle;
    interactable_handle ExpandInteraction;

    if (Current->Tombstone)
    {
    }
    else
    {
      memory_arena_stats MemStats = GetMemoryArenaStats(Current->Arena);
      u64 TotalUsed = MemStats.TotalAllocated - MemStats.Remaining;

      ExpandInteraction =
      PushButtonStart(Ui, UiId(MemoryArenaList, "MemoryWindowExpandInteraction", (void*)Current));
        PushColumn(Ui, CS(Current->SourceLocation),                   &Style);
        PushColumn(Ui, CS(Current->UserSuppliedName),                   &Style);
        PushColumn(Ui, MemorySize(MemStats.TotalAllocated), &Style);
        PushColumn(Ui, CS(MemStats.Pushes),                 &Style);
        PushColumn(Ui, CS(Current->ThreadId),               &Style);
        PushNewRow(Ui);
      PushButtonEnd(Ui);
    }

    if (Clicked(Ui, &ExpandInteraction))
    {
      Current->Expanded = !Current->Expanded;
    }
  }

  //
  // Draw tombstoned arenas
  //
  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (!Current->Arena) continue;


    ui_style Style = Current->Expanded? DefaultSelectedStyle : DefaultStyle;
    interactable_handle ExpandInteraction;

    // TODO(Jesse): Improve this behavior?
    if (Current->Tombstone)
    {
      Style = DefaultDisabledStyle;
      ExpandInteraction =
      PushButtonStart(Ui, UiId(MemoryArenaList, "MemoryWindowExpandInteraction", (void*)Current));
        PushColumn(Ui, CS(Current->SourceLocation),     &Style);
        PushColumn(Ui, CS(Current->UserSuppliedName),     &Style);
        PushColumn(Ui, CSz("- TOMBSTONED -"), &Style);
        PushColumn(Ui, CS(Current->ThreadId), &Style);
        PushNewRow(Ui);
      PushButtonEnd(Ui);
    }
  }

  PushTableEnd(Ui);
  PushWindowEnd(Ui, MemoryArenaList);








  Basis = BasisBelow(MemoryArenaList);
  window_layout *MemoryArenaDetails = GetOrCreateWindow(Ui, "Memory Arena Details", DefaultWindowSize * V2(2.f, 1.f), Basis);

  PushWindowStart(Ui, MemoryArenaDetails);

  if (FoundUntrackedAllocations && UntrackedAllocationsExpanded)
  {
    PushTableStart(Ui);
    PushNewRow(Ui);
    DebugMetadataHeading(Ui);
    PackSortAndBufferMemoryRecords(Ui, UnknownRecordTable, META_TABLE_SIZE);
    PushTableEnd(Ui);
  }

  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (!Current->Arena || Current->Tombstone) continue;

    memory_arena_stats MemStats = GetMemoryArenaStats(Current->Arena);
    u64 TotalUsed = MemStats.TotalAllocated - MemStats.Remaining;

    if (Current->Expanded)
    {

      counted_string TitleStats = FormatCountedString( GetTranArena(),
                                                       CSz("Allocs(%lu) Pushes(%lu) TotalSize(%S) Used(%S) Remaining(%S)"),
                                                       MemStats.Allocations,
                                                       MemStats.Pushes,
                                                       MemorySize(MemStats.TotalAllocated),
                                                       MemorySize(MemStats.TotalAllocated - MemStats.Remaining),
                                                       MemorySize(MemStats.Remaining)
                                                     );

      /* counted_string TitleStats = FormatCountedString( GetTranArena(), */
      /*                                                  CSz("%s :: Allocs(%lu) Pushes(%lu) TotalSize(%lu) Used(%lu) Remaining(%lu)"), */
      /*                                                  Current->Name, */
      /*                                                  MemStats.Allocations, */
      /*                                                  MemStats.Pushes, */
      /*                                                  MemStats.TotalAllocated, */
      /*                                                  MemStats.TotalAllocated - MemStats.Remaining, */
      /*                                                  MemStats.Remaining */
      /*                                                ); */

      /* PushTableStart(Ui); */
        /* PushNewRow(Ui); */
        PushColumn(Ui, CS(Current->SourceLocation));
        PushColumn(Ui, CS(Current->UserSuppliedName));
        PushNewRow(Ui);
        PushColumn(Ui, TitleStats);
        PushNewRow(Ui);
      /* PushTableEnd(Ui); */

      ui_element_reference BargraphTable = PushTableStart(Ui);
        PushMemoryBargraphTable(Ui, MemoryArenaDetails, SelectedArenas, MemStats, TotalUsed, Current->Arena);
      PushTableEnd(Ui);

      PushTableStart(Ui);
        PushDebugPushMetaData(Ui, SelectedArenas, HashArenaBlock(Current->Arena));
      PushTableEnd(Ui);

      /* PushNewRow(Ui); */
    }

    continue;
  }

  PushWindowEnd(Ui, MemoryArenaDetails);

  return;
}



/*******************************              ********************************/
/*******************************  Network UI  ********************************/
/*******************************              ********************************/



#if BONSAI_NETWORK_IMPLEMENTATION
link_internal void
DebugDrawNetworkHud(renderer_2d *Ui, network_connection *Network, server_state *ServerState)
{
  window_layout NetworkWindow = GetOrCreateWindow(Ui, "Network", V2(0));

#if (!EMCC)
  if (!ServerState) return;

  PushWindowStart(Ui, &NetworkWindow);

  PushTableStart(Ui);
  if (IsConnected(Network))
  {
    PushColumn(Ui, CSz("O"));

    if (Network->Client)
    {
      PushColumn(Ui, CSz("ClientId"));
      PushColumn(Ui, CS(Network->Client->Id));
      PushNewRow(Ui);
    }

    for (s32 ClientIndex = 0;
        ClientIndex < MAX_CLIENTS;
        ++ClientIndex)
    {
      client_state *Client = &ServerState->Clients[ClientIndex];

      u32 Color = WHITE;

      if (Network->Client->Id == ClientIndex)
        Color = GREEN;

      PushColumn(Ui, CSz("Id:"));
      PushColumn(Ui, CS( Client->Id));
      PushColumn(Ui, CS(Client->Counter));
      PushNewRow(Ui);
    }

  }
  else
  {
    PushColumn(Ui, CSz("X"));
    PushNewRow(Ui);
  }
  PushTableEnd(Ui);
  PushWindowEnd(Ui, &NetworkWindow);
#endif

  return;
}
#endif



/******************************               ********************************/
/******************************  Graphics UI  ********************************/
/******************************               ********************************/



link_internal void
DebugDrawGraphicsHud(renderer_2d *Ui, debug_state *DebugState)
{
  TIMED_FUNCTION();
  PushTableStart(Ui);
  PushColumn(Ui, CS(DebugState->BytesBufferedToCard));
  PushTableEnd(Ui);
  return;
}



/******************************              *********************************/
/******************************  Initialize  *********************************/
/******************************              *********************************/


link_internal void
DebugValue(m4 *Value, cs Name)
{
  auto Ui = GetDebugState()->UiGroup;

  PushColumn(Ui, Name);
  PushNewRow(Ui);

  PushColumn(Ui, FSz("(%.6f %.6f %.6f %.6f)", r64(Value->E[0].x), r64(Value->E[0].y), r64(Value->E[0].z), r64(Value->E[0].w)));
  PushNewRow(Ui);

  PushColumn(Ui, FSz("(%.6f %.6f %.6f %.6f)", r64(Value->E[1].x), r64(Value->E[1].y), r64(Value->E[1].z), r64(Value->E[1].w)));
  PushNewRow(Ui);

  PushColumn(Ui, FSz("(%.6f %.6f %.6f %.6f)", r64(Value->E[2].x), r64(Value->E[2].y), r64(Value->E[2].z), r64(Value->E[2].w)));
  PushNewRow(Ui);

  PushColumn(Ui, FSz("(%.6f %.6f %.6f %.6f)", r64(Value->E[3].x), r64(Value->E[3].y), r64(Value->E[3].z), r64(Value->E[3].w)));
  PushNewRow(Ui);
}


link_internal void
DebugValue(v4 *Value, cs Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Ui = DebugState->UiGroup;

  PushColumn(Ui, Name);
  PushColumn(Ui, FSz("(%.6f %.6f %.6f %.6f)", r64(Value->x), r64(Value->y), r64(Value->z), r64(Value->w)));
  PushNewRow(Ui);
}

link_internal void
DebugValue(v3 *Value, cs Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Ui = DebugState->UiGroup;

  PushColumn(Ui, Name);
  PushColumn(Ui, FSz("(%.2f %.2f %.2f)", r64(Value->x), r64(Value->y), r64(Value->z)));
  PushNewRow(Ui);
}

link_internal void
DebugValue(r32 Value, cs Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Ui = DebugState->UiGroup;

  PushColumn(Ui, Name);
  PushColumn(Ui, CS(Value));
  PushNewRow(Ui);
}

link_internal void
DebugValue(u32 Value, cs Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Ui = DebugState->UiGroup;

  PushColumn(Ui, Name);
  PushColumn(Ui, CS(Value));
  PushNewRow(Ui);
}

link_internal void
DebugValue(u64 Value, cs Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Ui = DebugState->UiGroup;

    PushColumn(Ui, Name);
    PushColumn(Ui, CS(Value));
    PushNewRow(Ui);
}

link_internal b32
InitDebugRenderSystem(heap_allocator *Heap, memory_arena *Memory)
{
  debug_state *DebugState = GetDebugState();

  DebugState->SelectedArenas = Allocate(selected_arenas, ThreadsafeDebugMemoryAllocator(), 1);
  b32 Result = InitRenderer2D(DebugState->UiGroup, Heap, Memory, 0, 0, 0, 0);

  return Result;
}

