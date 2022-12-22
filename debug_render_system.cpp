/****************************                       **************************/
/****************************  Mutex Introspection  **************************/
/****************************                       **************************/



#if 0
link_internal void
DrawWaitingBar(mutex_op_record *WaitRecord, mutex_op_record *AquiredRecord, mutex_op_record *ReleasedRecord,
               debug_ui_render_group *Group, layout *Layout, u64 FrameStartingCycle, u64 FrameTotalCycles, r32 TotalGraphWidth, r32 Z, v2 MaxClip)
{
  Assert(WaitRecord->Op == MutexOp_Waiting);
  Assert(AquiredRecord->Op == MutexOp_Aquired);
  Assert(ReleasedRecord->Op == MutexOp_Released);

  Assert(AquiredRecord->Mutex == WaitRecord->Mutex);
  Assert(ReleasedRecord->Mutex == WaitRecord->Mutex);

  u64 WaitCycleCount = AquiredRecord->Cycle - WaitRecord->Cycle;
  u64 AquiredCycleCount = ReleasedRecord->Cycle - AquiredRecord->Cycle;

  untextured_2d_geometry_buffer *Geo = &Group->Geo;
  cycle_range FrameRange = {FrameStartingCycle, FrameTotalCycles};

  cycle_range WaitRange = {WaitRecord->Cycle, WaitCycleCount};
  DrawCycleBar( &WaitRange, &FrameRange, TotalGraphWidth, 0, V3(1, 0, 0), Group, Geo, Layout, Z, MaxClip, 0);

  cycle_range AquiredRange = {AquiredRecord->Cycle, AquiredCycleCount};
  DrawCycleBar( &AquiredRange, &FrameRange, TotalGraphWidth, 0, V3(0, 1, 0), Group, Geo, Layout, Z, MaxClip, 0);

  return;
}
#endif



/****************************                 ********************************/
/****************************  Picked Chunks  ********************************/
/****************************                 ********************************/



#if 0
link_internal void
PushChunkView(debug_ui_render_group* Group, world_chunk* Chunk, window_layout* Window)
{
  debug_state* DebugState = GetDebugState();
  PushWindowStart(Group, Window);
    PushTableStart(Group);
      b32 DebugButtonPressed = False;

      interactable_handle PrevButton = PushButtonStart(Group, (umm)"PrevButton");
        PushColumn(Group, CSz("<"));
      PushButtonEnd(Group);

      if (Clicked(Group, &PrevButton))
      {
        Chunk->PointsToLeaveRemaining = Min(Chunk->PointsToLeaveRemaining+1, Chunk->EdgeBoundaryVoxelCount);
        DebugButtonPressed = True;
      }


      interactable_handle NextButton = PushButtonStart(Group, (umm)"NextButton");
        PushColumn(Group, CSz(">"));
      PushButtonEnd(Group);

      if (Clicked(Group, &NextButton))
      {
        Chunk->PointsToLeaveRemaining = Max(Chunk->PointsToLeaveRemaining-1, 0);
        DebugButtonPressed = True;
      }

      counted_string ButtonText = Chunk->DrawBoundingVoxels ? CSz("|") : CSz("O");

      interactable_handle ToggleBoundingVoxelsButton = PushButtonStart(Group, (umm)"ToggleBoundingVoxelsButton");
        PushColumn(Group, ButtonText);
      PushButtonEnd(Group);

      if (Clicked(Group, &ToggleBoundingVoxelsButton))
      {
        Chunk->DrawBoundingVoxels = !Chunk->DrawBoundingVoxels;
        DebugButtonPressed = True;
      }

      if (DebugButtonPressed)
      {
        Chunk->LodMesh_Complete = False;
        Chunk->LodMesh->At = 0;
        Chunk->Mesh = 0;
        Chunk->FilledCount = 0;
        Chunk->Data->Flags = Chunk_Uninitialized;
        NotImplemented;
        /* QueueChunkForInit( &DebugState->Plat->HighPriority, Chunk); */
      }

      PushNewRow(Group);
    PushTableEnd(Group);

    PushTableStart(Group);
      interactable_handle ViewportButton = PushButtonStart(Group, (umm)"ViewportButton");
        PushTexturedQuad(Group, DebugTextureArraySlice_Viewport, zDepth_Text);
      PushButtonEnd(Group);
    PushTableEnd(Group);

  PushWindowEnd(Group, Window);

  input* WindowInput = 0;
  if (Pressed(Group, &ViewportButton))
    { WindowInput = Group->Input; }
  UpdateGameCamera( -0.005f*(*Group->MouseDP), WindowInput, Canonical_Position(0), DebugState->Camera, Chunk_Dimension(32,32,8));
}

link_internal void
PushChunkDetails(debug_ui_render_group* Group, world_chunk* Chunk, window_layout* Window)
{
  PushWindowStart(Group, Window);
  PushTableStart(Group);
    PushColumn(Group, CSz("WorldP"));
    PushColumn(Group, CS(Chunk->WorldP.x));
    PushColumn(Group, CS(Chunk->WorldP.y));
    PushColumn(Group, CS(Chunk->WorldP.z));
    PushNewRow(Group);

    PushColumn(Group, CSz("PointsToLeaveRemaining"));
    PushColumn(Group, CS(Chunk->PointsToLeaveRemaining));
    PushNewRow(Group);

    PushColumn(Group, CSz("BoundaryVoxels Count"));
    PushColumn(Group, CS(Chunk->EdgeBoundaryVoxelCount));
    PushNewRow(Group);

    PushColumn(Group, CSz("Triangles"));
    PushColumn(Group, CS(Chunk->TriCount));
    PushNewRow(Group);
  PushTableEnd(Group);
  PushWindowEnd(Group, Window);
}

link_internal world_chunk*
DrawPickedChunks(debug_ui_render_group* Group, world_chunk_static_buffer *PickedChunks, world_chunk *HotChunk)
{
  debug_state* DebugState = GetDebugState();
  DebugState->HoverChunk = 0;

  MapGpuElementBuffer(&DebugState->GameGeo);

  v2 ListingWindowBasis = V2(20, 350);
  local_persist window_layout ListingWindow = WindowLayout("Picked Chunks", ListingWindowBasis, V2(400, 1600));

  PushWindowStart(Group, &ListingWindow);
  PushTableStart(Group);

  for (u32 ChunkIndex = 0;
      ChunkIndex < PickedChunks->At;
      ++ChunkIndex)
  {
    world_chunk *Chunk = PickedChunks->E[ChunkIndex];

    interactable_handle PositionButton = PushButtonStart(Group, (umm)"PositionButton"^(umm)Chunk);
      ui_style Style = Chunk == DebugState->PickedChunk ? DefaultSelectedStyle : DefaultStyle;
      PushColumn(Group, CS(Chunk->WorldP.x), &Style);
      PushColumn(Group, CS(Chunk->WorldP.y), &Style);
      PushColumn(Group, CS(Chunk->WorldP.z), &Style);
    PushButtonEnd(Group);

    if (Clicked(Group, &PositionButton)) { HotChunk = Chunk; }
    if (Hover(Group, &PositionButton)) { DebugState->HoverChunk = Chunk; }

    interactable_handle CloseButton = PushButtonStart(Group, (umm)"CloseButton"^(umm)Chunk);
      PushColumn(Group, CSz("X"));
    PushButtonEnd(Group);

    if ( Clicked(Group, &CloseButton) )
    {
      world_chunk** SwapChunk = PickedChunks->E + ChunkIndex;
      if (*SwapChunk == HotChunk) { HotChunk = 0; }
      *SwapChunk = PickedChunks->E[--PickedChunks->At];
    }

    PushNewRow(Group);
  }

  PushTableEnd(Group);
  PushWindowEnd(Group, &ListingWindow);

  if (HotChunk)
  {
    v3 Basis = -0.5f*V3(ChunkDimension(HotChunk));
    untextured_3d_geometry_buffer* Src = HotChunk->LodMesh;
    untextured_3d_geometry_buffer* Dest = &Group->GameGeo->Buffer;
    BufferVertsChecked(Src, Dest, Basis, V3(1.0f));
  }

  { // Draw hotchunk to the GameGeo FBO
    GL.BindFramebuffer(GL_FRAMEBUFFER, DebugState->GameGeoFBO.ID);
    FlushBuffersToCard(&DebugState->GameGeo);

    DebugState->ViewProjection =
      ProjectionMatrix(DebugState->Camera, DEBUG_TEXTURE_DIM, DEBUG_TEXTURE_DIM) *
      ViewMatrix(ChunkDimension(HotChunk), DebugState->Camera);

    GL.UseProgram(Group->GameGeoShader->ID);

    SetViewport(V2(DEBUG_TEXTURE_DIM, DEBUG_TEXTURE_DIM));

    BindShaderUniforms(Group->GameGeoShader);

    Draw(DebugState->GameGeo.Buffer.At);
    DebugState->GameGeo.Buffer.At = 0;
  }

  if (HotChunk)
  {
    local_persist window_layout ChunkDetailWindow = WindowLayout("Chunk Details", BasisRightOf(&ListingWindow),     V2(1100.0f, 400.0f));
    local_persist window_layout ChunkViewWindow   = WindowLayout("Chunk View",    BasisRightOf(&ChunkDetailWindow), V2(800.0f));

    PushChunkDetails(Group, HotChunk, &ChunkDetailWindow);
    PushChunkView(Group, HotChunk, &ChunkViewWindow);
  }

  return HotChunk;
}
#endif



/************************                        *****************************/
/************************  Thread Perf Bargraph  *****************************/
/************************                        *****************************/



link_internal counted_string
BuildNameStringFor(char Prefix, counted_string Name, u32 DepthAdvance)
{
  counted_string Result = FormatCountedString(TranArena, CSz("%*c%S"), DepthAdvance, Prefix, Name);
  return Result;
}

link_internal void
BufferScopeTreeEntry(debug_ui_render_group *Group, debug_profile_scope *Scope,
                     u64 TotalCycles, u64 TotalFrameCycles, u64 CallCount, u32 Depth)
{
  Assert(TotalFrameCycles);

  r32 Percentage = 100.0f * (r32)SafeDivide0((r64)TotalCycles, (r64)TotalFrameCycles);
  u64 AvgCycles = (u64)SafeDivide0(TotalCycles, CallCount);

  PushColumn(Group, CS(Percentage));
  PushColumn(Group, CS(AvgCycles));
  PushColumn(Group, CS(CallCount));

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
  PushColumn(Group, NameString, &DefaultStyle, DefaultColumnPadding, ColumnRenderParam_LeftAlign);

  return;
}

/* #if 1 */
/* bonsai_function scope_stats */
/* GetStatsFor( debug_profile_scope *Target, debug_profile_scope *Root) */
/* { */
/*   scope_stats Result = {}; */

/*   debug_profile_scope *Current = Root; */
/*   if (Target->Parent) Current = Target->Parent->Child; // Selects first sibling */

/*   while (Current) */
/*   { */
/*     if (Current == Target) // Find Ourselves */
/*     { */
/*       if (Result.Calls == 0) // We're first */
/*       { */
/*         Result.IsFirst = True; */
/*       } */
/*       else */
/*       { */
/*         break; */
/*       } */
/*     } */

/*     // These are compile-time string constants, so we can compare pointers to */
/*     // find equality */
/*     if (Current->Name == Target->Name) */
/*     { */
/*       ++Result.Calls; */
/*       Result.CumulativeCycles += Current->CycleCount; */

/*       if (!Result.MinScope || Current->CycleCount < Result.MinScope->CycleCount) */
/*         Result.MinScope = Current; */

/*       if (!Result.MaxScope || Current->CycleCount > Result.MaxScope->CycleCount) */
/*         Result.MaxScope = Current; */
/*     } */

/*     Current = Current->Sibling; */
/*   } */

/*   return Result; */
/* } */
/* #endif */

link_internal void
PushCycleBar(debug_ui_render_group* Group, cycle_range* Range, cycle_range* Frame, r32 TotalGraphWidth, r32 BarHeight, r32 yOffset, ui_style *Style, v4 Padding = V4(0), cs Name = CSz(""))
{
  Assert(Frame->StartCycle <= Range->StartCycle);

  r32 FramePerc = (r32)Range->TotalCycles / (r32)Frame->TotalCycles;

  r32 BarWidth = FramePerc*TotalGraphWidth;

  if (BarWidth > 0.15f)
  {
    v2 BarDim = V2(BarWidth, BarHeight);

    u64 StartCycleOffset = Range->StartCycle - Frame->StartCycle;
    r32 xOffset = GetXOffsetForHorizontalBar(StartCycleOffset, Frame->TotalCycles, TotalGraphWidth);

    v2 Offset = V2(xOffset, yOffset);

    PushUntexturedQuad(Group, Offset, BarDim, zDepth_Text, Style, Padding, QuadRenderParam_AdvanceClip);
    ui_style NameStyle = DefaultStyle;
    NameStyle.Font = Global_SmallFont;
    if (Name.Count) { Text(Group, Name, &NameStyle, TextRenderParam_NoAdvanceLayout, Offset, RectMinDim({}, BarDim)); }
  }

  return;
}

link_internal u8
GetByte(u32 ByteIndex, u64 Source)
{
  u32 ShiftWidth = ByteIndex * 8;
  Assert(ShiftWidth < (sizeof(Source)*8));

  u64 Mask = (u64)(0xff << ShiftWidth);
  u8 Result = (u8)((Source & Mask) >> ShiftWidth);
  return Result;
}

link_internal v3
ColorFromHash(u64 HashValue)
{
  u8 R = GetByte(0, HashValue);
  u8 G = GetByte(1, HashValue);
  u8 B = GetByte(2, HashValue);

  v3 Color = V3( R / 255.0f, G / 255.0f, B / 255.0f );
  return Color;
}

global_variable r32 Global_CoreBarHeight = 3.f;

link_internal void
PushScopeBarsRecursive( debug_ui_render_group *Group,
                        debug_profile_scope *Scope,
                        cycle_range *Frame,
                        r32 TotalGraphWidth,
                        random_series *Entropy,
                        u32 Depth = 0)
{
  while (Scope)
  {
    cycle_range Range = {Scope->StartingCycle, GetCycleCount(Scope)};

    /* umm NameHash = Hash(CS(Scope->Name)) ^ Hash(CS(Scope->Location)); */
    umm NameHash = Hash(CS(Scope->Name));

    random_series ScopeSeries = {.Seed = (u64)Scope};
    r32 Tint = RandomBetween(0.5f, &ScopeSeries, 1.0f);

    v3 FunctionColor = ColorFromHash(NameHash) * Tint;
    ui_style FunctionStyle = UiStyleFromLightestColor(FunctionColor);
    r32 yOffsetFunction = Depth * (Global_Font.Size.y);

    {
      cs ScopeName = CS(Scope->Name);
      interactable_handle Bar = PushButtonStart(Group, (umm)"CycleBarHoverInteraction"^(umm)Scope);
        PushCycleBar(Group, &Range, Frame, TotalGraphWidth, (r32)Global_Font.Size.y, yOffsetFunction, &FunctionStyle, V4(0), ScopeName);
      PushButtonEnd(Group);
      if (Hover(Group, &Bar)) { PushTooltip(Group, ScopeName); }
      if (Clicked(Group, &Bar)) { Scope->Expanded = !Scope->Expanded; }
    }

    if (Scope->Expanded) { PushScopeBarsRecursive(Group, Scope->Child, Frame, TotalGraphWidth, Entropy, Depth+1); }
    Scope = Scope->Sibling;
  }

  return;
}

link_internal void
DrawThreadsWindow(debug_ui_render_group *Group, debug_state *SharedState, v2 BasisP)
{
  TIMED_FUNCTION();

  random_series Entropy = {};
  r32 TotalGraphWidth = 1500.0f;
  local_persist window_layout CycleGraphWindow = WindowLayout("Threads", BasisP);

  PushWindowStart(Group, &CycleGraphWindow);

  cs ETStatusString = CSz("");
  switch (Global_EventTracingStatus)
  {
    case EventTracingStatus_Unstarted:
    {
      ETStatusString = CSz("Tracing: Unstarted");
    } break;
    case EventTracingStatus_Starting:
    {
      ETStatusString = CSz("Tracing: Starting");
    } break;
    case EventTracingStatus_Running:
    {
      ETStatusString = CSz("Tracing: Running");
    } break;
    case EventTracingStatus_Error:
    {
      ETStatusString = CSz("Tracing: Error");
    } break;
  }

  Text(Group, ETStatusString);
  PushNewRow(Group);
  PushNewRow(Group);

  u32 TotalThreadCount                 = GetTotalThreadCount();
  frame_stats *FrameStats              = SharedState->Frames + SharedState->ReadScopeIndex;
  cycle_range FrameCycles              = {FrameStats->StartingCycle, FrameStats->TotalCycles};

  debug_thread_state *MainThreadState  = GetThreadLocalStateFor(0);
  debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + SharedState->ReadScopeIndex;

  /* PushTableStart(Group); */
  for ( u32 ThreadIndex = 0;
        ThreadIndex < TotalThreadCount;
        ++ThreadIndex)
  {
    TIMED_NAMED_BLOCK("Thread Loop");

    PushColumn(Group, FormatCountedString(TranArena, CSz("Thread %u"), ThreadIndex));
    PushNewRow(Group);

    debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
    Assert(ThreadState->ThreadId);

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
            v3 CoreColor = Group->DebugColors[LastCSwitchEvt->ProcessorNumber];
            ui_style Style = UiStyleFromLightestColor(CoreColor);
            PushCycleBar(Group, &Range, &FrameCycles, TotalGraphWidth, Global_CoreBarHeight, 0, &Style, V4(0, 0, 0, Global_CoreBarHeight));
          }
        }

        LastCSwitchEvt = CSwitch;
      }

      PrevBlock = CurrentBlock;
      CurrentBlock = CurrentBlock->Next;
    }

    PushNewRow(Group);

    /* if (ThreadIndex == 0) */
    /* { */
    /*   DebugLine("----"); */
    /* } */

    debug_scope_tree *ReadTree = ThreadState->ScopeTrees + SharedState->ReadScopeIndex;
    if (MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded)
    {
      debug_timed_function BlockTimer2("Push Scope Bars");
      PushScopeBarsRecursive(Group, ReadTree->Root, &FrameCycles, TotalGraphWidth, &Entropy);
    }
    PushNewRow(Group);
  }
  /* PushTableEnd(Group); */

#if 0
  r32 TotalMs = (r32)FrameStats->FrameMs;

  if (TotalMs > 0.0f)
  {
    r32 MarkerWidth = 3.0f;

    {
      r32 FramePerc = 16.66666f/TotalMs;
      r32 xOffset = FramePerc*TotalGraphWidth;
      v2 MinP16ms = Layout->Basis + V2( xOffset, MinY );
      v2 MaxP16ms = Layout->Basis + V2( xOffset+MarkerWidth, Layout->At.y );
      v2 Dim = MaxP16ms - MinP16ms;
      PushUntexturedQuad(Group, MinP16ms, Dim, V3(0,1,0));
    }
    {
      r32 FramePerc = 33.333333f/TotalMs;
      r32 xOffset = FramePerc*TotalGraphWidth;
      v2 MinP16ms = Layout->Basis + V2( xOffset, MinY );
      v2 MaxP16ms = Layout->Basis + V2( xOffset+MarkerWidth, Layout->At.y );
      v2 Dim = MaxP16ms - MinP16ms;
      PushUntexturedQuad(Group, MinP16ms, Dim, V3(0,1,1));
    }
  }
#endif

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
          r32 yOffset = ThreadIndex * Group->Font.LineHeight;
          Layout->At.y += yOffset;
          DrawWaitingBar(CurrentRecord, Aquired, Released, Group, Layout, &Group->Font, FrameStartingCycle, FrameTotalCycles, TotalGraphWidth);
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

  PushWindowEnd(Group, &CycleGraphWindow);

  return;
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
      GotUniqueScope = AllocateProtection(unique_debug_profile_scope, TranArena, 1, False);
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
    DebugLine("Avg: %f\n", (r32)UniqueScopes->TotalCycles / (r32)UniqueScopes->CallCount);

    DumpScopeTreeDataToConsole_Internal(UniqueScopes->Scope->Child, TreeRoot, Memory);
    UniqueScopes = UniqueScopes->NextUnique;
  }

  return;
}

link_internal void
BufferFirstCallToEach(debug_ui_render_group *Group,
                      debug_profile_scope *Scope_in, debug_profile_scope *TreeRoot,
                      memory_arena *Memory, window_layout* Window, u64 TotalFrameCycles, u32 Depth)
{
  unique_debug_profile_scope* UniqueScopes = {};

  debug_profile_scope* CurrentUniqueScopeQuery = Scope_in;
  while (CurrentUniqueScopeQuery)
  {
    unique_debug_profile_scope* GotUniqueScope = ListContainsScope(UniqueScopes, CurrentUniqueScopeQuery);
    if (!GotUniqueScope )
    {
      GotUniqueScope = AllocateProtection(unique_debug_profile_scope, TranArena, 1, False);
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
    interactable_handle ScopeTextInteraction = PushButtonStart(Group, (umm)UniqueScopes->Scope);
      BufferScopeTreeEntry(Group, UniqueScopes->Scope, UniqueScopes->TotalCycles, TotalFrameCycles, UniqueScopes->CallCount, Depth);
    PushButtonEnd(Group);
    PushNewRow(Group);

    if (UniqueScopes->Scope->Expanded)
      BufferFirstCallToEach(Group, UniqueScopes->Scope->Child, TreeRoot, Memory, Window, TotalFrameCycles, Depth+1);

    if (Clicked(Group, &ScopeTextInteraction))
    {
      GetDebugState()->HotFunction = UniqueScopes->Scope;
      UniqueScopes->Scope->Expanded = !UniqueScopes->Scope->Expanded;
    }

    UniqueScopes = UniqueScopes->NextUnique;
  }

  return;
}

link_internal void
DrawFrameTicker(debug_ui_render_group *Group, debug_state *DebugState, r64 MaxMs)
{
  TIMED_FUNCTION();

  PushTableStart(Group);

    v4 Pad = V4(1, 0, 1, 0);
    v2 MaxBarDim = V2(15.0f, 80.0f);
    r32 HorizontalAdvance = (MaxBarDim.x+Pad.Left+Pad.Right);
    r32 VerticalAdvance = MaxBarDim.y;

    v2 LineDim = V2( HorizontalAdvance * DEBUG_FRAMES_TRACKED, 2.0f);
    {
      ui_style Style = UiStyleFromLightestColor( V3(1.f, 0.75f, 0.f) );
      r32 MsPerc = (r32)SafeDivide0(33.333, MaxMs);
      r32 MinPOffset = MaxBarDim.y * MsPerc;
      v2 MinP = { 0.0f, MaxBarDim.y - MinPOffset };
      PushUntexturedQuad(Group, MinP, LineDim, zDepth_Text, &Style, V4(0), QuadRenderParam_NoAdvance);
    }

    {
      ui_style Style = UiStyleFromLightestColor( V3(0.f, 1.f, 0.f) );
      r32 MsPerc = (r32)SafeDivide0(16.666, MaxMs);
      r32 MinPOffset = MaxBarDim.y * MsPerc;
      v2 MinP = { 0.0f, MaxBarDim.y - MinPOffset };
      PushUntexturedQuad(Group, MinP, LineDim, zDepth_Text, &Style, V4(0), QuadRenderParam_NoAdvance);
    }

    volatile umm MinCycles = u64_MAX;
    volatile umm MaxCycles = 0;

    for ( u32 FrameIndex = 0;
              FrameIndex < DEBUG_FRAMES_TRACKED;
            ++FrameIndex )
    {
      frame_stats *Frame = DebugState->Frames + FrameIndex;
      r32 Perc = (r32)SafeDivide0(Frame->FrameMs, MaxMs);

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

      interactable_handle B = PushButtonStart(Group, (umm)"FrameTickerHoverInteraction"+(umm)FrameIndex);
        PushUntexturedQuad(Group, V2(Pad.x, 0), MaxBarDim, zDepth_Background, &BackgroundStyle, {}, QuadRenderParam_NoAdvance);
        PushUntexturedQuad(Group, Offset, QuadDim, zDepth_Background, &Style, Pad);
      PushButtonEnd(Group);

      if (Clicked(Group, &B)) { DebugState->ReadScopeIndex = FrameIndex; }
    }

    DebugState->MaxCycles = MaxCycles;
    DebugState->MinCycles = MinCycles;



  PushTableEnd(Group);

  frame_stats *Frame = DebugState->Frames + DebugState->ReadScopeIndex;

  u32 TotalMutexOps = GetTotalMutexOpsForReadFrame();
  PushTableStart(Group);
    PushColumn(Group, CS(Frame->FrameMs));
    PushColumn(Group, CS(Frame->TotalCycles));
    PushColumn(Group, CS(TotalMutexOps));
    PushNewRow(Group);
  PushTableEnd(Group);

  /* DebugState->DebugValue_u64(DebugState->MinCycles, "MinCycles"); */
  /* DebugState->DebugValue_u64(DebugState->MaxCycles, "MaxCycles"); */

  return;
}

link_internal v2
DefaultWindowBasis(v2 ScreenDim)
{
  v2 Basis = V2(20, ScreenDim.y - DefaultWindowSize.y - 20);
  return Basis;
}

link_internal void
DebugDrawCallGraph(debug_ui_render_group *Group, debug_state *DebugState, r64 MaxMs)
{
  TIMED_FUNCTION();

  DrawFrameTicker(Group, DebugState, Max(33.3, MaxMs));

  u32 TotalThreadCount = GetWorkerThreadCount() + 1;

  debug_thread_state *MainThreadState  = GetThreadLocalStateFor(0);
  debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + DebugState->ReadScopeIndex;

  v2 Basis = DefaultWindowBasis(Group->ScreenDim);
  local_persist window_layout CallgraphWindow = WindowLayout("Callgraph", Basis);

  TIMED_BLOCK("Call Graph");

    PushWindowStart(Group, &CallgraphWindow);

    PushTableStart(Group);

    PushColumn(Group, CSz("Frame %"));
    PushColumn(Group, CSz("Cycles"));
    PushColumn(Group, CSz("Calls"));
    PushColumn(Group, CSz("Name"));
    PushNewRow(Group);

    for ( u32 ThreadIndex = 0;
        ThreadIndex < TotalThreadCount;
        ++ThreadIndex)
    {
      debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
      debug_scope_tree *ReadTree = ThreadState->ScopeTrees + DebugState->ReadScopeIndex;
      frame_stats *Frame = DebugState->Frames + DebugState->ReadScopeIndex;

      if (Frame->TotalCycles && MainThreadReadTree->FrameRecorded == ReadTree->FrameRecorded)
      {
        debug_timed_function BlockTimer2("Buffer First Call To Each");
        BufferFirstCallToEach(Group, ReadTree->Root, ReadTree->Root, ThreadsafeDebugMemoryAllocator(), &CallgraphWindow, Frame->TotalCycles, 0);
      }
    }
    PushTableEnd(Group);
    PushWindowEnd(Group, &CallgraphWindow);

  END_BLOCK("Call Graph");

  DrawThreadsWindow(Group, DebugState, BasisRightOf(&CallgraphWindow));

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
PushCallgraphRecursive(debug_ui_render_group *Group, debug_profile_scope* At)
{
  if (At)
  {
    u64 CycleCount = GetCycleCount(At);
    PushColumn(Group, CS(At->Name));
    PushColumn(Group, CS(CycleCount));
    PushNewRow(Group);

    if (At->Child)
    {
      PushCallgraphRecursive(Group, At->Child);
    }

    if (At->Sibling)
    {
      PushCallgraphRecursive(Group, At->Sibling);
    }
  }

  return;
}

link_internal void
DumpCallgraphRecursive(debug_ui_render_group *Group, debug_profile_scope* At, u32 Depth = 0)
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
    DumpCallgraphRecursive(Group, At->Child, Depth+1);
  }

  if (At->Sibling)
  {
    DumpCallgraphRecursive(Group, At->Sibling, Depth);
  }

  return;
}

link_internal void
DebugDrawCollatedFunctionCalls(debug_ui_render_group *Group, debug_state *DebugState)
{
  TIMED_FUNCTION();
#if 0
  debug_thread_state *MainThreadState = GetThreadLocalStateFor(0);
  debug_scope_tree *MainThreadReadTree = MainThreadState->ScopeTrees + DebugState->ReadScopeIndex;
  TIMED_BLOCK("Collated Function Calls");
    local_persist window_layout FunctionCallWindow = WindowLayout("Functions", V2(0, 200));
    CollateAllFunctionCalls(MainThreadReadTree->Root);
    PushWindowStart(Group, &FunctionCallWindow);
    PushTableStart(Group);
    for ( u32 FunctionIndex = 0;
        FunctionIndex < MAX_RECORDED_FUNCTION_CALLS;
        ++FunctionIndex)
    {
      called_function *Func = ProgramFunctionCalls + FunctionIndex;
      if (Func->Name)
      {
        PushColumn(Group, CS(Func->Name));
        PushColumn(Group, CS(Func->CallCount));
        PushNewRow(Group);
      }
    }
    PushTableEnd(Group);
    PushWindowEnd(Group, &FunctionCallWindow);
  END_BLOCK("Collated Function Calls");
#endif

  TIMED_BLOCK("Hot Function Stuff");
    local_persist window_layout HotFunctionWindow = WindowLayout("Hot Function Window?", V2(400, 200));
    PushWindowStart(Group, &HotFunctionWindow);
    PushTableStart(Group);


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

      sort_key* SortBuffer = Allocate(sort_key, TranArena, SortKeyCount);

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
        for (u32 SortIndex = 0;
             SortIndex < Min(100u, SortKeyCount);
             ++SortIndex)
        {
          debug_profile_scope* CurrentScope = (debug_profile_scope*)SortBuffer[SortIndex].Index;

#define TEXT_OUTPUT_FOR_FUNCTION_CALLS 0
#if TEXT_OUTPUT_FOR_FUNCTION_CALLS
          Print(CS(CurrentScope->Name));
          DebugChars("\n");
          Print(CS(GetCycleCount(CurrentScope)));
          DebugChars("\n");
          DumpCallgraphRecursive(Group, CurrentScope->Child);
          DebugChars("\n");
#else
          PushColumn(Group, CS(CurrentScope->Name));
          PushNewRow(Group);
          PushColumn(Group, CS(GetCycleCount(CurrentScope)));
          PushNewRow(Group);
          PushCallgraphRecursive(Group, CurrentScope->Child);
#endif

        }
      }
#if TEXT_OUTPUT_FOR_FUNCTION_CALLS
      exit(0);
#endif


    }

    PushTableEnd(Group);
    PushWindowEnd(Group, &HotFunctionWindow);
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
DebugDrawDrawCalls(debug_ui_render_group *Group)
{
  TIMED_FUNCTION();

  local_persist window_layout DrawCallWindow = WindowLayout("Draw Calls", V2(0));
  PushWindowStart(Group, &DrawCallWindow);

  PushTableStart(Group);

  PushColumn(Group, CSz("Caller"));
  PushColumn(Group, CSz("Calls"));
  PushColumn(Group, CSz("Bytes"));
  PushNewRow(Group);

  for( u32 DrawCountIndex = 0;
       DrawCountIndex < TRACKED_DRAW_CALLS_MAX;
       ++ DrawCountIndex)
  {
     debug_draw_call *DrawCall = &GetDebugState()->TrackedDrawCalls[DrawCountIndex];
     if (DrawCall->Caller)
     {
       PushColumn(Group, CS(DrawCall->Caller));
       PushColumn(Group, CS(DrawCall->Calls));
       PushColumn(Group, CS(DrawCall->N));
       PushNewRow(Group);
     }
  }

  PushTableEnd(Group);

  PushWindowEnd(Group, &DrawCallWindow);
  return;
}



/*******************************            **********************************/
/*******************************   Memory   **********************************/
/*******************************            **********************************/


link_internal void
PushBargraph(debug_ui_render_group *Group, r32 PercFilled, v3 FColor, v3 BColor, r32 BarWidth, r32 BarHeight = Global_Font.Size.y)
{
  v2 BackgroundQuadDim = V2(BarWidth, BarHeight);
  v2 ShadedQuadDim = BackgroundQuadDim * V2(PercFilled, 1);

  v2 UnshadedQuadDim = V2(BackgroundQuadDim.x - ShadedQuadDim.x, BackgroundQuadDim.y);

  ui_style Style = UiStyleFromLightestColor(FColor);
  /* PushUntexturedQuad(Group, V2(0), ShadedQuadDim, zDepth_TitleBar, &Style, V4(0), QuadRenderParam_NoAdvance); */
  PushUntexturedQuad(Group, V2(0), ShadedQuadDim, zDepth_TitleBar, &Style);

  Style = UiStyleFromLightestColor(BColor);
  /* PushUntexturedQuad(Group, V2(0), UnshadedQuadDim, zDepth_TitleBar, &Style, V4(0), QuadRenderParam_NoAdvance); */
  PushUntexturedQuad(Group, V2(0), UnshadedQuadDim, zDepth_TitleBar, &Style);

  /* PushForceAdvance(Group, BackgroundQuadDim); */
  /* PushForceAdvance(Group, V2(BackgroundQuadDim.x, 0)); */

  return;
}

link_internal interactable_handle
PushArenaBargraph(debug_ui_render_group *Group, v3 FColor, v3 BColor, umm TotalUsed, r32 TotalPerc, umm Remaining, umm InteractionId, r32 BarHeight)
{
  counted_string StatsString = FormatCountedString(TranArena, CSz("%S / %S (%S)"), MemorySize(TotalUsed), MemorySize(TotalUsed + Remaining), MemorySize(Remaining));
  PushColumn(Group, StatsString);
  PushNewRow(Group);

  r32 BargraphWidth = 800.f;

  interactable_handle Handle = PushButtonStart(Group, InteractionId);
    PushBargraph(Group, TotalPerc, FColor, BColor, BargraphWidth, BarHeight);
  PushButtonEnd(Group);

  PushNewRow(Group);

  return Handle;
}

link_internal void
PushMemoryBargraphTable(debug_ui_render_group *Group, selected_arenas *SelectedArenas, memory_arena_stats MemStats, umm TotalUsed, memory_arena *HeadArena)
{
  PushNewRow(Group);
  v3 DefaultForegroundColor =  V3(.25f, .1f, .35f);
  v3 DefaultBackgroundColor =  V3(.5f);

  r32 TotalPerc = (r32)SafeDivide0(TotalUsed, MemStats.TotalAllocated);
  // TODO(Jesse, id: 110, tags: ui, semantic): Should we do something special when interacting with this thing instead of Ignored-ing it?
  PushArenaBargraph(Group, DefaultForegroundColor, DefaultBackgroundColor, TotalUsed, TotalPerc, MemStats.Remaining, (umm)"Ignored", Global_Font.Size.y);
  PushNewRow(Group);

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

    u64 CurrentUsed = TotalSize(CurrentArena) - Remaining(CurrentArena);
    r32 CurrentPerc = (r32)SafeDivide0(CurrentUsed, TotalSize(CurrentArena));

    interactable_handle Handle = PushArenaBargraph(Group, FColor, BColor, CurrentUsed, CurrentPerc, Remaining(CurrentArena), HashArena(CurrentArena), Global_Font.Size.y*.5f);
    if (Clicked(Group, &Handle))
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
PackSortAndBufferMemoryRecords(debug_ui_render_group *Group, memory_record *Records, u64 RecordCount)
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
      PushColumn(Group,  CS(Collated->ThreadId));

      if (Collated->ArenaAddress == BONSAI_NO_ARENA && Collated->ArenaMemoryBlock)
      {
        // @ArenaMemoryBlock-as-char-pointer
        PushColumn(Group,  CS((char*)Collated->ArenaMemoryBlock), &ArenaStyle);
      }
      else
      {
        PushColumn(Group,  CS((u16)HashValue), &ArenaStyle);
      }

      PushColumn(Group,  MemorySize(AllocationSize));
      PushColumn(Group,  FormatThousands(Collated->StructCount));
      PushColumn(Group,  FormatThousands(Collated->PushCount));
      PushColumn(Group, CS(Collated->Name));
      PushNewRow(Group);
    }

    continue;
  }
}

link_internal void
DebugMetadataHeading(debug_ui_render_group *Group)
{
  PushColumn(Group, CSz("Thread"));
  PushColumn(Group, CSz("Arena"));
  PushColumn(Group, CSz("Memory"));
  PushColumn(Group, CSz("Structs"));
  PushColumn(Group, CSz("Pushes"));
  PushColumn(Group, CSz("Name"));
  PushNewRow(Group);

}

link_internal void
PushDebugPushMetaData(debug_ui_render_group *Group, selected_arenas *SelectedArenas, umm CurrentMemoryBlock)
{
  memory_record CollatedMetaTable[META_TABLE_SIZE] = {};

  DebugMetadataHeading(Group);

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

  PackSortAndBufferMemoryRecords(Group, CollatedMetaTable, META_TABLE_SIZE);

  return;
}

link_internal void
DebugDrawMemoryHud(debug_ui_render_group *Group, debug_state *DebugState)
{
  TIMED_FUNCTION();

  local_persist b32 UntrackedAllocationsExpanded = {};
  b32 FoundUntrackedAllocations = False;
  memory_record UnknownRecordTable[META_TABLE_SIZE] = {};
  {
    u32 TotalThreadCount = GetWorkerThreadCount() + 1;
    for ( u32 ThreadIndex = 0;
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

          if (!FoundRecordOwner)
          {
            if (Meta->ArenaAddress == BONSAI_NO_ARENA)
            {
              FoundUntrackedAllocations = True;
              WriteToMetaTable(Meta, UnknownRecordTable, PushesMatchExactly);
            }
            else
            {
              const char* Name = GetNullTerminated(CS(Meta->ArenaMemoryBlock), &Global_PermMemory);
              RegisterArena(Name, (memory_arena*)Meta->ArenaMemoryBlock, ThreadIndex);
            }
          }
        }
      }
    }
  }









  v2 Basis = V2(20, 300);
  local_persist window_layout MemoryArenaWindowInstance = WindowLayout("Memory Arena List", Basis, DefaultWindowSize + V2(500, 150));
  window_layout* MemoryArenaList = &MemoryArenaWindowInstance;


  PushWindowStart(Group, MemoryArenaList);
  PushTableStart(Group);

  /* v3 TitleColor = V3(.5f); */
  v3 TitleColor = V3(1.f, 1.f, 1.f);
  ui_style TitleStyle = UiStyleFromLightestColor(TitleColor);


  PushColumn(Group, CSz("Name"),   &TitleStyle);
  PushColumn(Group, CSz("Size"),   &TitleStyle);
  PushColumn(Group, CSz("Pushes"), &TitleStyle);
  PushColumn(Group, CSz("Thread"), &TitleStyle);
  PushNewRow(Group);

  if (FoundUntrackedAllocations)
  {
    ui_style UnnamedStyle = UntrackedAllocationsExpanded ? DefaultSelectedStyle : DefaultStyle;

    interactable_handle UnknownAllocationsExpandInteraction =
    PushButtonStart(Group, (umm)"unnamed MemoryWindowExpandInteraction");
      PushColumn(Group, CSz("?"), &UnnamedStyle);
      PushColumn(Group, CSz("?"), &UnnamedStyle);
      PushColumn(Group, CSz("?"), &UnnamedStyle);
      PushColumn(Group, CSz("Untracked Allocations"), &UnnamedStyle);
      PushNewRow(Group);
    PushButtonEnd(Group);
    if (Clicked(Group, &UnknownAllocationsExpandInteraction))
    {
      UntrackedAllocationsExpanded = !UntrackedAllocationsExpanded;
    }
  }


  selected_arenas *SelectedArenas = GetDebugState()->SelectedArenas;



  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (!Current->Arena) continue;

    memory_arena_stats MemStats = GetMemoryArenaStats(Current->Arena);
    u64 TotalUsed = MemStats.TotalAllocated - MemStats.Remaining;

    ui_style Style = Current->Expanded? DefaultSelectedStyle : DefaultStyle;

    interactable_handle ExpandInteraction =
    PushButtonStart(Group, (umm)"MemoryWindowExpandInteraction"^(umm)Current);
      PushColumn(Group, CS(Current->Name),                   &Style);
      PushColumn(Group, MemorySize(MemStats.TotalAllocated), &Style);
      PushColumn(Group, CS(MemStats.Pushes),                 &Style);
      PushColumn(Group, CS(Current->ThreadId),               &Style);
      PushNewRow(Group);
    PushButtonEnd(Group);

    if (Clicked(Group, &ExpandInteraction))
    {
      Current->Expanded = !Current->Expanded;
    }
  }

  PushTableEnd(Group);
  PushWindowEnd(Group, MemoryArenaList);








  Basis = BasisBelow(MemoryArenaList);
  local_persist window_layout MemoryArenaDetailsInstance = WindowLayout("Memory Arena Details", Basis, DefaultWindowSize * V2(2.f, 1.f));
  window_layout* MemoryArenaDetails = &MemoryArenaDetailsInstance;

  PushWindowStart(Group, MemoryArenaDetails);

  if (FoundUntrackedAllocations && UntrackedAllocationsExpanded)
  {
    PushTableStart(Group);
    PushNewRow(Group);
    DebugMetadataHeading(Group);
    PackSortAndBufferMemoryRecords(Group, UnknownRecordTable, META_TABLE_SIZE);
    PushTableEnd(Group);
  }

  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (!Current->Arena) continue;

    memory_arena_stats MemStats = GetMemoryArenaStats(Current->Arena);
    u64 TotalUsed = MemStats.TotalAllocated - MemStats.Remaining;

    if (Current->Expanded)
    {

      counted_string TitleStats = FormatCountedString( TranArena,
                                                       CSz("Allocs(%lu) Pushes(%lu) TotalSize(%S) Used(%S) Remaining(%S)"),
                                                       MemStats.Allocations,
                                                       MemStats.Pushes,
                                                       MemorySize(MemStats.TotalAllocated),
                                                       MemorySize(MemStats.TotalAllocated - MemStats.Remaining),
                                                       MemorySize(MemStats.Remaining)
                                                     );

      /* counted_string TitleStats = FormatCountedString( TranArena, */
      /*                                                  CSz("%s :: Allocs(%lu) Pushes(%lu) TotalSize(%lu) Used(%lu) Remaining(%lu)"), */
      /*                                                  Current->Name, */
      /*                                                  MemStats.Allocations, */
      /*                                                  MemStats.Pushes, */
      /*                                                  MemStats.TotalAllocated, */
      /*                                                  MemStats.TotalAllocated - MemStats.Remaining, */
      /*                                                  MemStats.Remaining */
      /*                                                ); */

      /* PushTableStart(Group); */
        /* PushNewRow(Group); */
        PushColumn(Group, CS(Current->Name));
        PushNewRow(Group);
        PushColumn(Group, TitleStats);
        PushNewRow(Group);
      /* PushTableEnd(Group); */

      ui_element_reference BargraphTable = PushTableStart(Group);
        PushMemoryBargraphTable(Group, SelectedArenas, MemStats, TotalUsed, Current->Arena);
      PushTableEnd(Group);

      PushTableStart(Group);
        PushDebugPushMetaData(Group, SelectedArenas, HashArenaBlock(Current->Arena));
      PushTableEnd(Group);

      /* PushNewRow(Group); */
    }

    continue;
  }

  PushWindowEnd(Group, MemoryArenaDetails);

  return;
}



/*******************************              ********************************/
/*******************************  Network UI  ********************************/
/*******************************              ********************************/



#if BONSAI_NETWORK_IMPLEMENTATION
link_internal void
DebugDrawNetworkHud(debug_ui_render_group *Group, network_connection *Network, server_state *ServerState)
{
  local_persist window_layout NetworkWindow = WindowLayout("Network", V2(0));

#if (!EMCC)
  if (!ServerState) return;

  PushWindowStart(Group, &NetworkWindow);

  PushTableStart(Group);
  if (IsConnected(Network))
  {
    PushColumn(Group, CSz("O"));

    if (Network->Client)
    {
      PushColumn(Group, CSz("ClientId"));
      PushColumn(Group, CS(Network->Client->Id));
      PushNewRow(Group);
    }

    for (s32 ClientIndex = 0;
        ClientIndex < MAX_CLIENTS;
        ++ClientIndex)
    {
      client_state *Client = &ServerState->Clients[ClientIndex];

      u32 Color = WHITE;

      if (Network->Client->Id == ClientIndex)
        Color = GREEN;

      PushColumn(Group, CSz("Id:"));
      PushColumn(Group, CS( Client->Id));
      PushColumn(Group, CS(Client->Counter));
      PushNewRow(Group);
    }

  }
  else
  {
    PushColumn(Group, CSz("X"));
    PushNewRow(Group);
  }
  PushTableEnd(Group);
  PushWindowEnd(Group, &NetworkWindow);
#endif

  return;
}
#endif



/******************************               ********************************/
/******************************  Graphics UI  ********************************/
/******************************               ********************************/



link_internal void
DebugDrawGraphicsHud(debug_ui_render_group *Group, debug_state *DebugState)
{
  TIMED_FUNCTION();
  PushTableStart(Group);
  PushColumn(Group, CS(DebugState->BytesBufferedToCard));
  PushTableEnd(Group);
  return;
}



/******************************              *********************************/
/******************************  Initialize  *********************************/
/******************************              *********************************/



link_internal void
DebugValue_r32(r32 Value, const char* Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Group = &DebugState->UiGroup;

  PushTableStart(Group);
    PushColumn(Group, CS(Name));
    PushColumn(Group, CS(Value));
    PushNewRow(Group);
  PushTableEnd(Group);
}

link_internal void
DebugValue_u32(u32 Value, const char* Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Group = &DebugState->UiGroup;

  PushTableStart(Group);
    PushColumn(Group, CS(Name));
    PushColumn(Group, CS(Value));
    PushNewRow(Group);
  PushTableEnd(Group);
}

link_internal void
DebugValue_u64(u64 Value, const char* Name)
{
  debug_state* DebugState = GetDebugState();
  debug_ui_render_group* Group = &DebugState->UiGroup;

  PushTableStart(Group);
    PushColumn(Group, CS(Name));
    PushColumn(Group, CS(Value));
    PushNewRow(Group);
  PushTableEnd(Group);
}


link_internal void
FramebufferTextureLayer(framebuffer *FBO, texture *Tex, debug_texture_array_slice Layer)
{
  u32 Attachment = FBO->Attachments++;
  GL.FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + Attachment, Tex->ID, 0, Layer);
  return;
}

link_internal void
AllocateAndInitGeoBuffer(textured_2d_geometry_buffer *Geo, u32 ElementCount, memory_arena *DebugArena)
{
  Geo->Verts  = Allocate(v3, DebugArena, ElementCount);
  Geo->Colors = Allocate(v3, DebugArena, ElementCount);
  Geo->UVs    = Allocate(v3, DebugArena, ElementCount);

  Geo->End = ElementCount;
  Geo->At = 0;
}

link_internal void
AllocateAndInitGeoBuffer(untextured_2d_geometry_buffer *Geo, u32 ElementCount, memory_arena *DebugArena)
{
  Geo->Verts = Allocate(v3, DebugArena, ElementCount);
  Geo->Colors = Allocate(v3, DebugArena, ElementCount);

  Geo->End = ElementCount;
  Geo->At = 0;
  return;
}

link_internal shader
MakeRenderToTextureShader(memory_arena *Memory, m4 *ViewProjection)
{
  shader Shader = LoadShaders( CSz("RenderToTexture.vertexshader"), CSz("RenderToTexture.fragmentshader") );

  shader_uniform **Current = &Shader.FirstUniform;

  *Current = GetUniform(Memory, &Shader, ViewProjection, "ViewProjection");
  Current = &(*Current)->Next;

  return Shader;
}

void
InitRenderToTextureGroup(debug_state *DebugState, render_entity_to_texture_group *Group)
{
  AllocateGpuElementBuffer(&Group->GameGeo, (u32)Megabytes(4));

  Group->GameGeoFBO = GenFramebuffer();
  GL.BindFramebuffer(GL_FRAMEBUFFER, Group->GameGeoFBO.ID);

  FramebufferTextureLayer(&Group->GameGeoFBO, DebugState->UiGroup.TextGroup->DebugTextureArray, DebugTextureArraySlice_Viewport);
  SetDrawBuffers(&Group->GameGeoFBO);
  Group->GameGeoShader = MakeRenderToTextureShader(ThreadsafeDebugMemoryAllocator(), &Group->ViewProjection);
  Group->Camera = Allocate(camera, ThreadsafeDebugMemoryAllocator(), 1);
  StandardCamera(Group->Camera, 1000.0f, 100.0f, {});

  GL.ClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  GL.ClearDepth(1.0f);
}

link_internal b32
InitDebugRenderSystem(debug_state *DebugState, heap_allocator *Heap)
{
  AllocateMesh(&DebugState->LineMesh, 1024, Heap);

  DebugState->UiGroup.TextGroup     = Allocate(debug_text_render_group, ThreadsafeDebugMemoryAllocator(), 1);
  DebugState->UiGroup.CommandBuffer = Allocate(ui_render_command_buffer, ThreadsafeDebugMemoryAllocator(), 1);
  DebugState->SelectedArenas        = Allocate(selected_arenas, ThreadsafeDebugMemoryAllocator(), 1);

  // TODO(Jesse, memory): Instead of allocate insanely massive buffers (these are ~400x overkill)
  // we should have a system that streams blocks of memory in as-necessary
  // @streaming_ui_render_memory
  u32 ElementCount = (u32)Megabytes(2);
  AllocateAndInitGeoBuffer(&DebugState->UiGroup.TextGroup->Geo, ElementCount, ThreadsafeDebugMemoryAllocator());
  AllocateAndInitGeoBuffer(&DebugState->UiGroup.Geo, ElementCount, ThreadsafeDebugMemoryAllocator());


  auto TextGroup = DebugState->UiGroup.TextGroup;
  TextGroup->DebugTextureArray = LoadBitmap("texture_atlas_0.bmp", ThreadsafeDebugMemoryAllocator(), DebugTextureArraySlice_Count);
  GL.GenBuffers(1, &TextGroup->SolidUIVertexBuffer);
  GL.GenBuffers(1, &TextGroup->SolidUIColorBuffer);
  GL.GenBuffers(1, &TextGroup->SolidUIUVBuffer);
  TextGroup->Text2DShader = LoadShaders( CSz("TextVertexShader.vertexshader"), CSz("TextVertexShader.fragmentshader") );
  TextGroup->TextTextureUniform = GL.GetUniformLocation(TextGroup->Text2DShader.ID, "TextTextureSampler");
  DebugState->UiGroup.TextGroup->SolidUIShader = LoadShaders( CSz("SimpleColor.vertexshader"), CSz("SimpleColor.fragmentshader") );


  InitRenderToTextureGroup(DebugState, &DebugState->PickedChunksRenderGroup);


  v2i TextureDim = V2i(DEBUG_TEXTURE_DIM, DEBUG_TEXTURE_DIM);
  texture *DepthTexture = MakeDepthTexture( TextureDim, ThreadsafeDebugMemoryAllocator() );
  FramebufferDepthTexture(DepthTexture);

  b32 Result = CheckAndClearFramebuffer();
  Assert(Result);

  GL.BindFramebuffer(GL_FRAMEBUFFER, 0);
  GL.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  AssertNoGlErrors;

  random_series Entropy = {54623153};
  for (u32 ColorIndex = 0; ColorIndex < RANDOM_COLOR_COUNT; ++ColorIndex)
  {
    DebugState->UiGroup.DebugColors[ColorIndex] = RandomV3(&Entropy);
  }

  return Result;
}

