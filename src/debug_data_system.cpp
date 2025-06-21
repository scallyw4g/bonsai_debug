inline memory_arena *
ThreadsafeDebugMemoryAllocator();

debug_scope_tree* GetReadScopeTree(u32 ThreadIndex)
{
  debug_scope_tree *RootScope = &GetDebugState()->ThreadStates[ThreadIndex].ScopeTrees[GetDebugState()->ReadScopeIndex];
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



/*****************************                   *****************************/
/*****************************  Thread Metadata  *****************************/
/*****************************                   *****************************/



inline debug_thread_state*
GetThreadLocalStateFor(s32 ThreadIndex)
{
  debug_thread_state *Result = 0;

  if (ThreadIndex >= 0)
  {
    Assert(ThreadIndex < (s32)GetTotalThreadCount());

    debug_state *State = GetDebugState();

    // NOTE(Jesse): It's possible that during initialization between the time
    // that "DebugState->Initialized" is set to true and the time the
    // debug_thread_state structs are allocated that we'll call this function,
    // which is why we have to check that ThreadStates is valid.
    //
    // TODO(Jesse): Audit codebase to make sure callers of this function actually
    // check that it returns a valid pointer.
    Result = State->ThreadStates ?  State->ThreadStates + ThreadIndex : 0;
  }

  return Result;
}

void
RegisterThread(thread_local_state *Thread)
{
  // NOTE(Jesse): This cold-sets this because it's called every time the worker
  // threads loop such that hot-loading works properly
  /* SetThreadLocal_ThreadIndex(Thread->ThreadIndex); */
  ThreadLocal_ThreadIndex = Thread->ThreadIndex;

  debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  ThreadState->ThreadId = GetCurrentThreadId();
}

inline debug_thread_state*
GetThreadLocalState()
{
  debug_thread_state* Result = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  return Result;
}



/****************************                       **************************/
/****************************  Arena Introspection  **************************/
/****************************                       **************************/


void
RegisterArena(const char *Name, memory_arena *Arena, s32 ThreadId)
{
  debug_state* DebugState = GetDebugState();
  b32 Registered = False;
  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];

    const char *CurrentName = Current->Name;
    if (!CurrentName)
    {
      if (AtomicCompareExchange( (volatile void **)&Current->Name, (void*)Name, (void*)CurrentName ))
      {
        Current->Arena = Arena;
        Current->ThreadId = ThreadId;
        Registered = True;
        break;
      }
      else
      {
        continue;
      }
    }
  }

  if (!Registered)
  {
    SoftError("Registering Arena (%s); too many arenas registered!", Name);
  }

  return;
}

void
UnregisterArena(memory_arena *Arena)
{
  debug_state* DebugState = GetDebugState();
  b32 Found = False;
  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &DebugState->RegisteredMemoryArenas[Index];
    if (Current->Arena == Arena) { Found = True; Current->Tombstone = True; }
  }
}

b32
PushesShareName(memory_record *First, memory_record *Second)
{
  b32 Result = ( First->ThreadId == Second->ThreadId  &&
                 StringsMatch(First->Name, Second->Name) );
  return Result;
}

b32
PushesShareHeadArena(memory_record *First, memory_record *Second)
{
  b32 Result = (First->ArenaMemoryBlock == Second->ArenaMemoryBlock &&
                First->StructSize       == Second->StructSize       &&
                First->StructCount      == Second->StructCount      &&
                First->ThreadId      == Second->ThreadId      &&
                StringsMatch(First->Name, Second->Name) );
  return Result;
}

b32
PushesMatchExactly(memory_record *First, memory_record *Second)
{
  b32 Result = (First->ArenaAddress     == Second->ArenaAddress     &&
                First->ArenaMemoryBlock == Second->ArenaMemoryBlock &&
                First->StructSize       == Second->StructSize       &&
                First->StructCount      == Second->StructCount      &&
                First->ThreadId      == Second->ThreadId      &&
                StringsMatch(First->Name, Second->Name) );
  return Result;
}

#define META_TABLE_SIZE (1024)
inline void
ClearMemoryRecordsFor(memory_arena *Arena)
{
  TIMED_FUNCTION();

#if 1
  umm ArenaBlockHash   =      HashArenaBlock(Arena);
  umm ArenaHash        =      HashArena(Arena);
  s32 TotalThreadCount = (s32)GetTotalThreadCount();

  for ( s32 ThreadIndex = 0;
            ThreadIndex < TotalThreadCount;
          ++ThreadIndex)
  {
    for ( u32 MetaIndex = 0;
              MetaIndex < META_TABLE_SIZE;
            ++MetaIndex)
    {
      memory_record *Meta = GetThreadLocalStateFor(ThreadIndex)->MetaTable + MetaIndex;
      if (Meta->ArenaMemoryBlock == ArenaBlockHash || Meta->ArenaAddress == ArenaHash)
      {
        Clear(Meta);
      }
    }
  }
#endif

}



/*****************************                  ******************************/
/*****************************  Arena Metadata  ******************************/
/*****************************                  ******************************/



registered_memory_arena *
GetRegisteredMemoryArena(memory_arena *Arena)
{
  registered_memory_arena *Result = 0;

  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = &GetDebugState()->RegisteredMemoryArenas[Index];
    if (Current->Arena == Arena)
    {
      Assert(Current->Tombstone == False);
      Result = Current;
      break;
    }
  }

  return Result;
}

link_internal void
WriteToMetaTable(memory_record *Query, memory_record *Table, meta_comparator Comparator)
{
#if 1
  cs QueryName = CS(Query->Name);
  u32 HashValue = u32(Hash(QueryName)) % META_TABLE_SIZE;
  u32 FirstHashValue = HashValue;

  memory_record *PickMeta = Table + HashValue;
  while (PickMeta->Name)
  {
    if (Comparator(PickMeta, Query))
    {
      break;
    }

    HashValue = (HashValue+1)%META_TABLE_SIZE;
    PickMeta = Table + HashValue;
    if (HashValue == FirstHashValue)
    {
      /* Warn("Meta Table is full, discarding allocation metadata for (%s)", Query->Name); */
      return;
    }
  }

  if (PickMeta->Name)
  {
    PickMeta->PushCount += Query->PushCount;
  }
  else
  {
    *PickMeta = *Query;

    // NOTE(Jesse): We have to copy the string when we write a new record so
    // that we don't hold pointers into old DLLs when we reload.
    //
    // NOTE(Jesse): We can't copy the strings here because we have no way of
    // freeing them when the allocation turns out to be on an arena that gets
    // rolled back or vaporized and the allocation table entries get cleared.
    //
    // Instead, when we reload the DLL we copy the strings into a temp arena
    // so we don't point to an old DLL, and we can roll it back every time we
    // reload so we don't leak.
    //
    // @meta_table_allocation_name_copy
    /* PickMeta->Name = CopyZString(Query->Name, ThreadsafeDebugMemoryAllocator()); */
  }

  return;
#endif
}


// @meta_table_allocation_name_copy
link_internal void
DuplicateMetaTableNameStrings(engine_resources *Engine)
{
  debug_state *DebugState = GetDebugState();

  memory_arena *NewArena = AllocateArena();
  DEBUG_REGISTER_NAMED_ARENA(NewArena, ThreadLocal_ThreadIndex, "MetaTableNameStringsArena");

  s32 ThreadCount = s32(GetTotalThreadCount());
  RangeIterator(ThreadIndex, ThreadCount)
  {
    debug_thread_state *Thread = GetThreadLocalStateFor(ThreadIndex);
    RangeIterator( MetaIndex, META_TABLE_SIZE )
    {
      memory_record *Meta = Thread->MetaTable + MetaIndex;
      if (Meta->Name)  { Meta->Name = CopyZString(Meta->Name, NewArena); }
    }
  }

  VaporizeArena(DebugState->MetaTableNameStringsArena);
  DebugState->MetaTableNameStringsArena = NewArena;
}

void
CollateMetadata(memory_record *InputMeta, memory_record *MetaTable)
{
  WriteToMetaTable(InputMeta, MetaTable, PushesShareHeadArena);
}

void
WriteMemoryRecord(memory_record *InputMeta)
{
  debug_thread_state *Thread = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  if (Thread)
  {
    memory_record *MetaTable = Thread->MetaTable;
    WriteToMetaTable(InputMeta, MetaTable, PushesMatchExactly);
  }
}



/****************************                    *****************************/
/****************************  Memory Allocator  *****************************/
/****************************                    *****************************/



#if 0
// NOTE(Jesse): The `void *Source` is whatever allocator it came from
link_internal void
TrackAllocation(void* Source, umm StructSize, umm StructCount, const char *AllocationUUID, s32 Line, const char *File, umm Alignment, b32 MemProtect)
{
  umm PushSize = StructCount * StructSize;
  void* Result = PushStruct( Arena, PushSize, Alignment, MemProtect);

  memory_record ArenaMetadata =
  {
    .Name = AllocationUUID,
    .ArenaAddress = HashArena(Arena),
    .ArenaMemoryBlock = HashArenaBlock(Arena),
    .StructSize = StructSize,
    .StructCount = StructCount,
    .ThreadId = ThreadLocal_ThreadIndex,
    .PushCount = 1
  };
  WriteMemoryRecord(&ArenaMetadata);

#if BONSAI_INTERNAL
  Arena->Pushes++;
#endif

  if (!Result) { Error("Pushing %s on Line: %d, in file %s", AllocationUUID, Line, File); }

  return Result;
}
#endif

void*
DEBUG_Allocate(memory_arena* Arena, umm StructSize, umm StructCount, const char *AllocationUUID, s32 Line, const char *File, umm Alignment, b32 MemProtect)
{
  umm PushSize = StructCount * StructSize;
  void* Result = PushStruct( Arena, PushSize, Alignment, MemProtect);

  memory_record ArenaMetadata =
  {
    .Name = AllocationUUID,
    .ArenaAddress = HashArena(Arena),
    .ArenaMemoryBlock = HashArenaBlock(Arena),
    .StructSize = StructSize,
    .StructCount = StructCount,
    .ThreadId = ThreadLocal_ThreadIndex,
    .PushCount = 1
  };
  WriteMemoryRecord(&ArenaMetadata);

#if BONSAI_INTERNAL
  Arena->Pushes++;
#endif

  if (!Result) { Error("Pushing %s on Line: %d, in file %s", AllocationUUID, Line, File); }

  return Result;
}

memory_arena_stats
GetMemoryArenaStats(memory_arena *ArenaIn)
{
  memory_arena_stats Result = {};

#if BONSAI_INTERNAL
  AcquireFutex(&ArenaIn->DebugFutex);
#endif

  memory_arena *Arena = ArenaIn;
  while (Arena)
  {
    if (Arena->Start)
    {
      Result.Allocations++;
      Result.TotalAllocated += TotalSize(Arena);
      Result.Remaining += Remaining(Arena);

#if BONSAI_INTERNAL
      Result.Pushes += Arena->Pushes;
#endif

    }
    Arena = Arena->Prev;
  }

#if BONSAI_INTERNAL
  ReleaseFutex(&ArenaIn->DebugFutex);
#endif

  return Result;
}

#if 0
link_internal memory_arena_stats
GetTotalMemoryArenaStats()
{
  TIMED_FUNCTION();
  memory_arena_stats Result = {};

  u32 TotalThreadCount = GetTotalThreadCount();

  for ( u32 ThreadIndex = 0;
      ThreadIndex < TotalThreadCount;
      ++ThreadIndex)
  {
    auto TLS = GetThreadLocalStateFor(ThreadIndex);
    for ( u32 MetaIndex = 0;
        MetaIndex < META_TABLE_SIZE;
        ++MetaIndex)
    {
      memory_record *Record = TLS->MetaTable + MetaIndex;
      /* memory_arena_stats CurrentStats = GetMemoryArenaStats(Current->Arena); */
      /* Result.Allocations          += CurrentStats.Allocations; */
      Result.Pushes               += Record->PushCount;
      Result.TotalAllocated       += Record->StructSize*Record->StructCount;
      /* Result.Remaining            += CurrentStats.Remaining; */
    }
  }

  return Result;
}

#else

memory_arena_stats
GetTotalMemoryArenaStats()
{
  TIMED_FUNCTION();

  memory_arena_stats TotalStats = {};
  debug_state *DebugState = GetDebugState();

  for ( u32 Index = 0;
        Index < REGISTERED_MEMORY_ARENA_COUNT;
        ++Index )
  {
    registered_memory_arena *Current = DebugState->RegisteredMemoryArenas + Index;
    if (!Current->Arena) continue;

    if (Current->Tombstone == False)
    {
      memory_arena_stats CurrentStats = GetMemoryArenaStats(Current->Arena);
      TotalStats.Allocations          += CurrentStats.Allocations;
      TotalStats.Pushes               += CurrentStats.Pushes;
      TotalStats.TotalAllocated       += CurrentStats.TotalAllocated;
      TotalStats.Remaining            += CurrentStats.Remaining;
    }
  }

  return TotalStats;
}
#endif



/**************************                     ******************************/
/**************************  Utility Functions  ******************************/
/**************************                     ******************************/



link_internal u64
GetCycleCount(debug_profile_scope *Scope)
{
  u64 Result = 0;
  if (Scope->EndingCycle)
  {
    Assert(Scope->EndingCycle > Scope->StartingCycle);
    Result = Scope->EndingCycle - Scope->StartingCycle;
  }
  return Result;
}

void
InitScopeTree(debug_scope_tree *Tree)
{
  /* TIMED_FUNCTION(); */ // Cannot be timed because it has to run to initialize the scope tree system

  Clear(Tree);
  Tree->WriteScope   = &Tree->Root;

  return;
}

void
FreeScopes(debug_thread_state *ThreadState, debug_profile_scope *ScopeToFree)
{
  // Behaves poorly when timed because it adds a profile scope for every
  // profile scope present, which has the net effect of adding infinite scopes
  // over the course of the program.
  //
  // Can be timed by wrapping the call site with a TIMED_BLOCK
  //
  /* TIMED_FUNCTION(); */


  debug_profile_scope* Current = ScopeToFree;
  while (Current)
  {
    debug_profile_scope* Next = Current->Sibling;

    FreeScopes(ThreadState, Current->Child);

    Clear(Current);

    Current->Sibling = ThreadState->FirstFreeScope;
    ThreadState->FirstFreeScope = Current;

    Current = Next;
  }

  return;
}

// TODO(Jesse, id: 103, tags: cleanup): Replace this expression everywhere!!!  This might be done.
inline u32
GetNextDebugFrameIndex(u32 Current)
{
  u32 Result = (Current + 1) % DEBUG_FRAMES_TRACKED;
  return Result;
}

inline void
AdvanceThreadState(debug_thread_state *ThreadState, u32 NextFrameId)
{
  /* TIMED_FUNCTION(); */
  // Can this actually be timed if we're freeing scopes?  I think it can,
  // because we're freeing the _next_ frames scopes, however it's behaving
  // badly when turned on

  u32 NextWriteIndex = NextFrameId % DEBUG_FRAMES_TRACKED;
  debug_scope_tree *NextWriteTree = ThreadState->ScopeTrees + NextWriteIndex;

  FreeScopes(ThreadState, NextWriteTree->Root);
  InitScopeTree(NextWriteTree);

  ThreadState->MutexOps[NextWriteIndex].NextRecord = 0;

  NextWriteTree->FrameRecorded = NextFrameId;

  return;
}

inline void
WorkerThreadAdvanceDebugSystem()
{
  Assert(ThreadLocal_ThreadIndex != INVALID_THREAD_LOCAL_THREAD_INDEX);

  debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  debug_thread_state *MainThreadState = GetThreadLocalStateFor(0);

  if (ThreadState != MainThreadState)
  {
    u32 NextFrameId = MainThreadState->WriteIndex;

    // Note(Jesse): WriteIndex must not straddle a cache line!
    Assert(sizeof(MainThreadState->WriteIndex) == 4);
    Assert((umm)(&MainThreadState->WriteIndex)%64 <= 60 );

    if (NextFrameId != ThreadState->WriteIndex)
    {
      ThreadState->WriteIndex = NextFrameId;
      AdvanceThreadState(ThreadState, NextFrameId);
    }

  }
}

void
MainThreadAdvanceDebugSystem(r32 Dt)
{
  TIMED_FUNCTION();
  Assert(ThreadLocal_ThreadIndex == 0);

  Assert(ThreadLocal_ThreadIndex == 0);
  debug_thread_state *MainThreadState = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  debug_state *SharedState = GetDebugState();

  if (SharedState->DebugDoScopeProfiling)
  {
    u64 CurrentCycles = GetCycleCount();

    u32 ThisFrameWriteIndex = MainThreadState->WriteIndex % DEBUG_FRAMES_TRACKED;

    AtomicIncrement(&MainThreadState->WriteIndex);
    AdvanceThreadState(MainThreadState, MainThreadState->WriteIndex);

    /* SharedState->ReadScopeIndex = GetNextDebugFrameIndex(SharedState->ReadScopeIndex); */
    SharedState->ReadScopeIndex = ThisFrameWriteIndex;


    /* Record frame cycle counts */

    frame_stats *ThisFrame = SharedState->Frames + ThisFrameWriteIndex;
    ThisFrame->FrameMs = Dt * 1000.0f;
    ThisFrame->TotalCycles = CurrentCycles - ThisFrame->StartingCycle;


    u32 NextFrameWriteIndex = GetNextDebugFrameIndex(ThisFrameWriteIndex);
    frame_stats *NextFrame = SharedState->Frames + NextFrameWriteIndex;
    Clear(NextFrame);
    NextFrame->StartingCycle = CurrentCycles;
  }
}

min_max_avg_dt
ComputeMinMaxAvgDt()
{
  TIMED_FUNCTION();

  debug_state *SharedState = GetDebugState();

  min_max_avg_dt Dt = {};
  Dt.Min = f32_MAX;

  u32 FrameCount = 0;
  for (u32 FrameIndex = 0;
      FrameIndex < DEBUG_FRAMES_TRACKED;
      ++FrameIndex )
  {
    frame_stats *Frame = SharedState->Frames + FrameIndex;

    if (Frame->FrameMs > 0.1f) { ++FrameCount; }
    if (Frame->FrameMs > 0.1f) { Dt.Min = Min(Dt.Min, Frame->FrameMs); }
    Dt.Max = Max(Dt.Max, Frame->FrameMs);
    Dt.Avg += Frame->FrameMs;
  }

  Dt.Avg = SafeDivide0(Dt.Avg, r32(FrameCount));

  return Dt;
}

inline memory_arena *
ThreadsafeDebugMemoryAllocator_debug_profile_scope_only()
{
  debug_state *State = GetDebugState();
  memory_arena *Arena = State->ThreadStates[ThreadLocal_ThreadIndex].MemoryFor_debug_profile_scope;
  return Arena;
}

inline memory_arena *
ThreadsafeDebugMemoryAllocator()
{
  debug_state *State = GetDebugState();
  memory_arena *Arena = State->ThreadStates[ThreadLocal_ThreadIndex].Memory;
  return Arena;
}



/*************************                       *****************************/
/*************************  Profile Scope Trees  *****************************/
/*************************                       *****************************/



debug_profile_scope *
GetProfileScope()
{
  debug_profile_scope *Result = 0;
  debug_thread_state *State = GetThreadLocalState();

  if (State)
  {
    if (State->FirstFreeScope)
    {
      Result = State->FirstFreeScope;
      State->FirstFreeScope = State->FirstFreeScope->Sibling;

      Clear(Result);
    }
    else
    {
      memory_arena *Memory = ThreadsafeDebugMemoryAllocator_debug_profile_scope_only();
      Result = AllocateProtection(debug_profile_scope, Memory, 1, False);
    }
  }

  return Result;
}

inline mutex_op_record *
ReserveMutexOpRecord(mutex *Mutex, mutex_op Op, debug_state *State)
{
  if (!State->DebugDoScopeProfiling) return 0;

  mutex_op_record *Record = 0;
  debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadLocal_ThreadIndex);
  u32 WriteIndex = ThreadState->WriteIndex % DEBUG_FRAMES_TRACKED;
  mutex_op_array *MutexOps = &ThreadState->MutexOps[WriteIndex];

  if (MutexOps->NextRecord < MUTEX_OPS_PER_FRAME)
  {
    Record = MutexOps->Records + MutexOps->NextRecord++;
    Record->Cycle = GetCycleCount();
    Record->Op = Op;
    Record->Mutex = Mutex;
  }
  else
  {
    Warn("Total debug mutex operations of %u exceeded, discarding record info.", MUTEX_OPS_PER_FRAME);
  }

  return Record;
}

inline void
MutexWait(mutex *Mutex)
{
  /* mutex_op_record *Record = */ ReserveMutexOpRecord(Mutex, MutexOp_Waiting, GetDebugState());
  return;
}

inline void
MutexAquired(mutex *Mutex)
{
  /* mutex_op_record *Record = */ ReserveMutexOpRecord(Mutex, MutexOp_Aquired, GetDebugState());
  return;
}

inline void
MutexReleased(mutex *Mutex)
{
  /* mutex_op_record *Record = */ ReserveMutexOpRecord(Mutex, MutexOp_Released, GetDebugState());
  return;
}

mutex_op_record *
FindRecord(mutex_op_record *WaitRecord, mutex_op_record *FinalRecord, mutex_op SearchOp)
{
  Assert(WaitRecord->Op == MutexOp_Waiting);

  mutex_op_record *Result = 0;;
  mutex_op_record *SearchRecord = WaitRecord;

  while (SearchRecord < FinalRecord)
  {
    if (SearchRecord->Op == SearchOp &&
        SearchRecord->Mutex == WaitRecord->Mutex)
    {
      Result = SearchRecord;
      break;
    }

    ++SearchRecord;
  }

  return Result;
}



/*****************************              **********************************/
/*****************************  Call Graph  **********************************/
/*****************************              **********************************/


inline u32
GetTotalMutexOpsForReadFrame()
{
  u32 ReadIndex = GetDebugState()->ReadScopeIndex;

  u32 OpCount = 0;
  for (s32 ThreadIndex = 0;
           ThreadIndex < (s32)GetTotalThreadCount();
         ++ThreadIndex)
  {
    debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
    OpCount += ThreadState->MutexOps[ReadIndex].NextRecord;
  }

  return OpCount;
}

umm
GetAllocationSize(memory_record *Meta)
{
  umm AllocationSize = Meta->StructSize*Meta->StructCount*Meta->PushCount;
  return AllocationSize;
}

link_internal void
PushHistogramDataPoint(u64 Sample)
{
  debug_state *DebugState = GetDebugState();
  AcquireFutex(&DebugState->HistogramFutex);
  if (TotalElements(&DebugState->HistogramSamples) > 0)
  {
    if (AtElements(&DebugState->HistogramSamples) == TotalElements(&DebugState->HistogramSamples))
    {
      DebugState->HistogramSamples.At = DebugState->HistogramSamples.Start;
    }

    Push(&DebugState->HistogramSamples, Sample);
  }
  ReleaseFutex(&DebugState->HistogramFutex);
}


/********************                               **************************/
/********************  Debug System Initialization  **************************/
/********************                               **************************/



void
InitDebugMemoryAllocationSystem(debug_state *State)
{
  u32 TotalThreadCount = GetTotalThreadCount();

  // FIXME(Jesse): Can this be allocated in a more sensible way?
  umm Size = TotalThreadCount * sizeof(debug_thread_state);

  // FIXME(Jesse): This should allocate roughly enough space (maybe more than necessary)
  // for the Size parameter, however it seems to be under-allocating, which causes
  // the PushStruct to allocate again.  Bad bad bad.
  memory_arena *BoostrapArena = AllocateArena(Size);
  DEBUG_REGISTER_ARENA(BoostrapArena, ThreadLocal_ThreadIndex);
  State->ThreadStates = (debug_thread_state*)PushStruct(BoostrapArena, Size, CACHE_LINE_SIZE);

#if BONSAI_INTERNAL

  /* The WriteIndex member gets read from multiple threads, and on x86_64 reads
   * are atomic that don't span a cache-line boundary.  This ensures we never
   * hit that case.
   */
  CAssert(sizeof(debug_thread_state) == CACHE_LINE_SIZE);
  for (s32 ThreadIndex = 0;
           ThreadIndex < (s32)TotalThreadCount;
         ++ThreadIndex)
  {
    debug_thread_state* ThreadState = GetThreadLocalStateFor(ThreadIndex);
    Assert( (umm)(&ThreadState->WriteIndex) % CACHE_LINE_SIZE < (CACHE_LINE_SIZE)-sizeof(ThreadState->WriteIndex) );
  }
#endif

  umm MetaTableSize = META_TABLE_SIZE * sizeof(memory_record);
  for (s32 ThreadIndex = 0;
           ThreadIndex < (s32)TotalThreadCount;
         ++ThreadIndex)
  {
    debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
    Assert((umm)ThreadState % CACHE_LINE_SIZE == 0);

    memory_arena *DebugThreadArena = AllocateArena();
    DEBUG_REGISTER_ARENA(DebugThreadArena, ThreadIndex);

    ThreadState->Memory = DebugThreadArena;

    umm DebugProfileScopeArenaSize = Megabytes(32);

    memory_arena *DebugThreadArenaFor_debug_profile_scope = AllocateArena(DebugProfileScopeArenaSize);
    DEBUG_REGISTER_ARENA(DebugThreadArenaFor_debug_profile_scope, ThreadIndex);

    ThreadState->MemoryFor_debug_profile_scope = DebugThreadArenaFor_debug_profile_scope;

    ThreadState->MetaTable = (memory_record*)PushStruct(ThreadsafeDebugMemoryAllocator(), MetaTableSize, CACHE_LINE_SIZE);
    ThreadState->MutexOps = AllocateAligned(mutex_op_array, DebugThreadArena, DEBUG_FRAMES_TRACKED, CACHE_LINE_SIZE);
    ThreadState->ScopeTrees = AllocateAligned(debug_scope_tree, DebugThreadArena, DEBUG_FRAMES_TRACKED, CACHE_LINE_SIZE);
  }

  return;
}


link_internal void
AllocateContextSwitchBuffer(debug_context_switch_event_buffer *Result, memory_arena *Arena, u32 EventCount)
{
  Result->End = EventCount;
  Result->Events = Allocate(debug_context_switch_event, Arena, EventCount);
}

link_internal debug_context_switch_event_buffer *
AllocateContextSwitchBuffer(memory_arena *Arena, u32 EventCount)
{
  debug_context_switch_event_buffer *Result = Allocate(debug_context_switch_event_buffer, Arena, 1);
  AllocateContextSwitchBuffer(Result, Arena, EventCount);
  return Result;
}

link_internal debug_context_switch_event_buffer_stream_block *
AllocateContextSwitchBufferStreamBlock(memory_arena *Arena, u32 EventCount)
{
  debug_context_switch_event_buffer_stream_block *Result = Allocate(debug_context_switch_event_buffer_stream_block, Arena, 1);
  AllocateContextSwitchBuffer(&Result->Buffer, Arena, EventCount );
  return Result;
}

link_internal debug_context_switch_event_buffer_stream *
AllocateContextSwitchBufferStream(memory_arena *Arena, u32 EventCount)
{
  debug_context_switch_event_buffer_stream *Result = Allocate(debug_context_switch_event_buffer_stream, Arena, 1);
  Result->FirstBlock   = AllocateContextSwitchBufferStreamBlock(Arena, EventCount);
  Result->CurrentBlock = Result->FirstBlock;
  return Result;
}

link_internal void
InitDebugDataSystem(debug_state *DebugState)
{
  Assert(ThreadLocal_ThreadIndex == 0);

  InitDebugMemoryAllocationSystem(DebugState);

  debug_thread_state *MainThreadState = GetThreadLocalStateFor(0);
  MainThreadState->ThreadId = GetCurrentThreadId();

  DebugState->HistogramSamples = U64Cursor(DEBUG_HISTOGRAM_MAX_SAMPLES, ThreadsafeDebugMemoryAllocator());
    /* DebugProfileScopeCursor(DEBUG_HISTOGRAM_MAX_SAMPLES, ThreadsafeDebugMemoryAllocator()); */

  s32 TotalThreadCount = (s32)GetTotalThreadCount();
  for (s32 ThreadIndex = 0;
           ThreadIndex < TotalThreadCount;
         ++ThreadIndex)
  {
    debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
    ThreadState->ContextSwitches = AllocateContextSwitchBufferStream(ThreadsafeDebugMemoryAllocator(), MAX_CONTEXT_SWITCH_EVENTS );

    for (u32 TreeIndex = 0;
        TreeIndex < DEBUG_FRAMES_TRACKED;
        ++TreeIndex)
    {
      InitScopeTree(ThreadState->ScopeTrees + TreeIndex);
      ThreadState->WriteIndex = GetDebugState()->ReadScopeIndex + 1;
    }
  }

  return;
}
