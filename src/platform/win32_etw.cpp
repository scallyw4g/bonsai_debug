
#include <evntrace.h>
#include <evntcons.h>
#include <strsafe.h>
#include <tdh.h>

#pragma pack(push, 1)
struct context_switch_event
{
  u32 NewThreadId;
  u32 OldThreadId;
  s8  NewThreadPriority;
  s8  OldThreadPriority;
  u8  PreviousCState;
  s8  SpareByte;
  s8  OldThreadWaitReason;
  s8  OldThreadWaitMode;
  s8  OldThreadState;
  s8  OldThreadWaitIdealProcessor;
  u32 NewThreadWaitTime;
  u32 Reserved;
};
#pragma pack(pop)

global_variable memory_arena *ETWArena = AllocateArena();

link_internal debug_context_switch_event_buffer_stream_block*
MaybeFreeBlock(debug_context_switch_event_buffer_stream *Stream, debug_context_switch_event_buffer_stream_block *CurrentBlock)
{
  debug_context_switch_event_buffer_stream_block *Result = 0;

  {
    //  Block is head, has next
    if (CurrentBlock == Stream->FirstBlock &&
        CurrentBlock->Next)
    {
      InvalidCodePath();
      Assert(BufferHasRoomFor(&CurrentBlock->Buffer, 1) == False);
      Assert(CurrentBlock != Stream->CurrentBlock);
      auto BlockToFree = CurrentBlock;
      Stream->FirstBlock = CurrentBlock->Next;

      BlockToFree->Next = Stream->FirstFreeBlock;
      Stream->FirstFreeBlock = BlockToFree;

      Result = Stream->FirstBlock;
    }

    /* Block is head, no next // Block is tail, no prev */
    else if (CurrentBlock == Stream->FirstBlock &&
             CurrentBlock == Stream->CurrentBlock)
    {
      CurrentBlock->Buffer.At = 0;
    }
    /* Block is tail, has prev*/
    else if ( CurrentBlock != Stream->FirstBlock &&
              CurrentBlock == Stream->CurrentBlock )
    {
      CurrentBlock->Buffer.At = 0;
    }
    else
    {
      Assert(false);
    }

  }

  return Result;
}

link_internal ULONG
ETWBufferCallback( EVENT_TRACE_LOGFILEA *Logfile )
{
  Assert(Logfile->EventsLost == 0);

  s32 TotalThreadCount = (s32)GetTotalThreadCount();
  for ( s32 ThreadIndex = 0;
            ThreadIndex < TotalThreadCount;
          ++ThreadIndex)
  {
    /* TIMED_NAMED_BLOCK("Thread Loop"); */

    debug_thread_state *ThreadState = GetThreadLocalStateFor(ThreadIndex);
    Assert(ThreadState->ThreadId);

    b32 FoundOutOfOrderEvent = True;
    while (FoundOutOfOrderEvent)
    {
      FoundOutOfOrderEvent = False;
      debug_context_switch_event_buffer_stream *ContextSwitchStream = ThreadState->ContextSwitches;
      debug_context_switch_event_buffer_stream_block *PrevBlock = 0;
      debug_context_switch_event_buffer_stream_block *CurrentBlock = ContextSwitchStream->FirstBlock;
      while (CurrentBlock)
      {
        debug_context_switch_event_buffer *ContextSwitches = &CurrentBlock->Buffer;
        debug_context_switch_event *LastCSwitchEvt = ContextSwitches->Events;

        CurrentBlock->MinCycles = LastCSwitchEvt->CycleCount;
        CurrentBlock->MaxCycles = LastCSwitchEvt->CycleCount;

        for ( u32 ContextSwitchEventIndex = 1;
                  ContextSwitchEventIndex < ContextSwitches->At;
                ++ContextSwitchEventIndex )
        {
          debug_context_switch_event *CSwitch = ContextSwitches->Events + ContextSwitchEventIndex;
          if (LastCSwitchEvt->CycleCount > CSwitch->CycleCount)
          {
            FoundOutOfOrderEvent = True;
            debug_context_switch_event Tmp = *CSwitch;
            *CSwitch = *LastCSwitchEvt;
            *LastCSwitchEvt = Tmp;
          }

          CurrentBlock->MinCycles = Min(CurrentBlock->MinCycles, CSwitch->CycleCount);
          CurrentBlock->MaxCycles = Max(CurrentBlock->MaxCycles, CSwitch->CycleCount);

          LastCSwitchEvt = CSwitch;
        }

        CurrentBlock = CurrentBlock->Next;
      }
    }

    debug_context_switch_event_buffer_stream *ContextSwitchStream = ThreadState->ContextSwitches;
    /* debug_context_switch_event_buffer_stream_block *PrevBlock = 0; */
    debug_context_switch_event_buffer_stream_block *CurrentBlock = ContextSwitchStream->FirstBlock;
    debug_state *DebugState = GetDebugState();
    while (CurrentBlock && CurrentBlock != ContextSwitchStream->CurrentBlock)
    {
      if ( RangeContainsInclusive(DebugState->MinCycles, CurrentBlock->MinCycles, DebugState->MaxCycles) ||
           RangeContainsInclusive(DebugState->MinCycles, CurrentBlock->MaxCycles, DebugState->MaxCycles)  )
      {
        break;
      }
      else
      {
        /* CurrentBlock = MaybeFreeBlock(ContextSwitchStream, CurrentBlock); */
        Assert(CurrentBlock == ContextSwitchStream->FirstBlock && CurrentBlock->Next);
        Assert(BufferHasRoomFor(&CurrentBlock->Buffer, 1) == False);
        Assert(CurrentBlock != ContextSwitchStream->CurrentBlock);

        {
          auto BlockToFree = CurrentBlock;
          ContextSwitchStream->FirstBlock = CurrentBlock->Next;

          BlockToFree->Next = ContextSwitchStream->FirstFreeBlock;
          ContextSwitchStream->FirstFreeBlock = BlockToFree;

          CurrentBlock = ContextSwitchStream->FirstBlock;
        }

      }

    }


    if ( RangeContainsInclusive(DebugState->MinCycles, ContextSwitchStream->CurrentBlock->MinCycles, DebugState->MaxCycles) ||
         RangeContainsInclusive(DebugState->MinCycles, ContextSwitchStream->CurrentBlock->MaxCycles, DebugState->MaxCycles)  )
    {
    }
    else
    {
      MaybeFreeBlock(ContextSwitchStream, ContextSwitchStream->CurrentBlock);
    }


#if 0
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
#endif
  }


  return True; // Continue processing buffers
}

global_variable u32 CSwitchEventsPerFrame = 0;


void
ClearBlock(debug_context_switch_event_buffer_stream_block *Block)
{
  Block->Buffer.At = 0;
  Block->Next = 0;
}

link_internal void
PushContextSwitch(debug_context_switch_event_buffer_stream *Stream, debug_context_switch_event *Evt)
{
  Assert(Evt->Type);

  debug_context_switch_event_buffer_stream_block *CurrentBlock = Stream->CurrentBlock;
  if (BufferHasRoomFor(&CurrentBlock->Buffer, 1))
  {
    CurrentBlock->Buffer.Events[CurrentBlock->Buffer.At++] = *Evt;
  }
  else
  {
    debug_context_switch_event_buffer_stream_block *NextBlock = Stream->FirstFreeBlock;
    if (NextBlock)
    {
      Stream->FirstFreeBlock = NextBlock->Next;
      ClearBlock(NextBlock);
    }
    else
    {
      NextBlock = AllocateContextSwitchBufferStreamBlock(ETWArena, MAX_CONTEXT_SWITCH_EVENTS);
    }

    Stream->CurrentBlock->Next = NextBlock;
    Stream->CurrentBlock = NextBlock;

    PushContextSwitch(Stream, Evt);
  }
}


link_internal void
ETWEventCallback(EVENT_RECORD *Event)
{
  u32 ProcessorNumber = Event->BufferContext.ProcessorNumber;
  switch(Event->EventHeader.EventDescriptor.Opcode)
  {
/*   u8 ProcessorNumber = Event->BufferContext.ProcessorNumber; */
/*   u32 ThreadID = Header.ThreadId; */
/*   s64 CycleTime = Header.TimeStamp.QuadPart; */

    // CSwitch event
    // https://learn.microsoft.com/en-us/windows/win32/etw/cswitch
    case 36:
    {
#if 1
      Assert(Event->UserDataLength == sizeof(context_switch_event));

#if 0
      // According to the docs, the events aren't packed at any particular
      // alignment, so we have to memcpy into an aligned struct in case they're
      // at a weird offset
      //
      // NOTE(Jesse): I didn't actually run into a problem by just casting, so
      // I'm going to do that.
      //
      context_switch_event *SystemEvent = Allocate(context_switch_event, ETWArena, 1);
      MemCopy((u8*)Event->UserData, (u8*)SystemEvent, sizeof(context_switch_event));
#else
      context_switch_event *SystemEvent = (context_switch_event*)Event->UserData;
#endif

      // Skip events that are just state changes
      if (SystemEvent->NewThreadId == SystemEvent->OldThreadId) { return; }

      ++CSwitchEventsPerFrame;

      debug_state *DebugState = GetDebugState();

      u32 TotalThreads = GetTotalThreadCount();
      for (u32 ThreadIndex = 0; ThreadIndex < TotalThreads; ++ThreadIndex)
      {
        debug_thread_state *TS = DebugState->ThreadStates + ThreadIndex;
        Assert(TS->ThreadId);

        u64 CycleCount = (u64)Event->EventHeader.TimeStamp.QuadPart;

        debug_context_switch_event CSwitch = {
          .ProcessorNumber = ProcessorNumber,
          .CycleCount = CycleCount,
          /* .SystemEvent = SystemEvent, */
        };

        if (TS->ThreadId == SystemEvent->NewThreadId)
        {
          CSwitch.Type = ContextSwitch_On;
        }

        if (TS->ThreadId == SystemEvent->OldThreadId)
        {
          CSwitch.Type = ContextSwitch_Off;
        }

        if (CSwitch.Type)
        {
          Assert(CSwitch.CycleCount);

          debug_context_switch_event *LastCSwitchEvt = GetLatest(TS->ContextSwitches);
          /* if (LastCSwitchEvt && BufferHasRoomFor(TS->ContextSwitches, 1)) */
          /* { */
          /*   /1* Assert(LastCSwitchEvt->Type != CSwitch.Type); *1/ */
          /*   /1* if (LastCSwitchEvt->Type == CSwitch.Type) *1/ */
          /*   /1* { *1/ */
          /*   /1*   Assert(LastCSwitchEvt->CycleCount > CSwitch.CycleCount); *1/ */
          /*   /1* } *1/ */
          /* } */

          {
            /* Assert(LastCSwitchEvt->Type != CSwitch.Type); */
            /* SaturatingSub(TS->ContextSwitches->At); */
          }
          /* else */
          {
            /* DebugLine("Pushing CSwitch from thread (%u)", GetCurrentThreadId()); */
            /* if (LastCSwitchEvt) { Assert(LastCSwitchEvt->ProcessorNumber == CSwitch.ProcessorNumber); } */
            /* if (LastCSwitchEvt) { Assert(LastCSwitchEvt->CycleCount < CSwitch.CycleCount); } */
            /* if (LastCSwitchEvt) { Assert(LastCSwitchEvt->Type != CSwitch.Type); } */
            PushContextSwitch(TS->ContextSwitches, &CSwitch);
          }
        }
      }
#endif

    } break;

    default: {} break;
  }
}

link_internal b32
CloseAnyExistingTrace(EVENT_TRACE_PROPERTIES *EventTracingProps)
{
  b32 Result = False;

  auto ControlTraceResult = ControlTrace(0, KERNEL_LOGGER_NAME, EventTracingProps, EVENT_TRACE_CONTROL_STOP);
  if (ControlTraceResult == ERROR_SUCCESS || ControlTraceResult == ERROR_WMI_INSTANCE_NOT_FOUND)
  {
    Result = True;
  }
  return Result;
}

link_internal void
SetupPropsForContextSwitchEventTracing(EVENT_TRACE_PROPERTIES *EventTracingProps, u32 BufferSize)
{
  ZeroMemory(EventTracingProps, BufferSize);

  EventTracingProps->Wnode.BufferSize = BufferSize;
  EventTracingProps->Wnode.Guid = SystemTraceControlGuid;  // Magic value
  EventTracingProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID; // Magic value
  EventTracingProps->Wnode.ClientContext = 3;              // 3 == send us timestamps with __rdtsc; 1 == QueryPerformanceCounter(); 2 == System Time.

  EventTracingProps->BufferSize = 64; // Size (in KB) of the buffer ETW allocates for events.  4 (4kb) is minimum, 16384 (16mb) is maximum

  // I observed these getting set to 64 after calling StartTrace
  EventTracingProps->MinimumBuffers = 1;
  EventTracingProps->MaximumBuffers = 1;

  // ETW usees a default of 1 second if we don't tell it what timing to use
  /* EventTracingProps->FlushTimer = 1; */

  EventTracingProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE; // Provide events in "real time" .. aka. every timeout, which defalts to 1s
  EventTracingProps->EnableFlags = EVENT_TRACE_FLAG_CSWITCH;   // Subscribe to context switch events

  // We have to copy the kernel logger name string into a buffer we allocated after the struct.  lolwut
  EventTracingProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
  StringCbCopy(((char*)EventTracingProps + EventTracingProps->LoggerNameOffset), sizeof(KERNEL_LOGGER_NAME), KERNEL_LOGGER_NAME);
}

link_internal DWORD WINAPI
Win32TracingThread(void *ignored)
{
  Global_EventTracingStatus = EventTracingStatus_Starting;

  u32 BufferSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME);
  EVENT_TRACE_PROPERTIES *EventTracingProps = (EVENT_TRACE_PROPERTIES*)malloc(BufferSize);

  SetupPropsForContextSwitchEventTracing(EventTracingProps, BufferSize);

  // We have to first shut down anyone using this tracer because there's a
  // system-wide limit of _one_ consumer of it.  If we opened a trace and
  // crashed, it's still open, and our call to StartTrace will fail.
  Info("Stopping existing Kernel Trace Session if it exists");
  if (CloseAnyExistingTrace(EventTracingProps))
  {
    // NOTE(Jesse): ControlTrace() modifies the EVENT_TRACE_PROPERTIES struct,
    // so we have to zero it and fill it out again when calling StartTrace
    SetupPropsForContextSwitchEventTracing(EventTracingProps, BufferSize);

    Info("Starting Trace");
    TRACEHANDLE TraceHandle = {};
    auto StartTraceResult = StartTraceA(&TraceHandle, KERNEL_LOGGER_NAME, EventTracingProps);

    if ( StartTraceResult == ERROR_SUCCESS )
    {
      EVENT_TRACE_LOGFILE Logfile = {};

      Logfile.LoggerName = (char*)KERNEL_LOGGER_NAME;
      Logfile.ProcessTraceMode = (PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP);
      Logfile.EventRecordCallback = ETWEventCallback;
      Logfile.BufferCallback = ETWBufferCallback;

      Info("OpenTrace");
      TRACEHANDLE OpenTraceHandle = OpenTrace(&Logfile);
      if (OpenTraceHandle != INVALID_PROCESSTRACE_HANDLE)
      {
        Global_EventTracingStatus = EventTracingStatus_Running;
        Info("Started Context Switch tracing");
        Ensure( ProcessTrace(&OpenTraceHandle, 1, 0, 0) == ERROR_SUCCESS );
      }
      else
      {
        u64 SpecificErrorCode = GetLastError();
        SoftError("Opening Trace : error number (%u)", SpecificErrorCode);
      }
    }
    else
    {
      switch(StartTraceResult)
      {
        case ERROR_ACCESS_DENIED:
        {
          SoftError("Insufficient privileges to start ETW context switch tracing.");
        } break;

        default:
        {
          SoftError("Starting Trace : error number (%u)", StartTraceResult);
        } break;
      }
    }
  }
  else
  {
    SoftError("Closing Existing tracing sessions failed.");
  }

  Global_EventTracingStatus = EventTracingStatus_Error;

  Info("Exiting ProcessTrace thread");
  return 0;
}

void
Platform_EnableContextSwitchTracing()
{
  Info("Creating tracing thread");
  HANDLE ThreadHandle = CreateThread(0, 0, Win32TracingThread, 0, 0, 0);
}

