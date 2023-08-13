#if 0
#include <cassert>
#include <iostream>
#include <vector>
#include <chrono>
#include <optional>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <random>

#pragma comment(lib, "Tdh.lib")
#pragma comment(lib, "ntdll")

void check(ULONG errorCode) {

    if (errorCode != ERROR_SUCCESS) {
        char buf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr,
                       errorCode,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buf,
                       sizeof(buf) / sizeof(char),
                       nullptr);

        OutputDebugStringA(buf);

        throw std::runtime_error(buf);
    }
}

BOOL SetPrivilege(unsigned int priv) {
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid.LowPart = priv;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Enable the privilege or disable all privileges.
    HANDLE hToken;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken);

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        static_cast<PTOKEN_PRIVILEGES>(nullptr),
        static_cast<PDWORD>(nullptr))) {
        check(GetLastError());
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        return FALSE;
    }

    return TRUE;
}

struct pmc_counter {
    std::wstring Name;
    uint64_t interval;
    uint32_t native_source;

    bool operator==(const pmc_counter& rhs) {
        return native_source == rhs.native_source;
    }
};

struct pmc_counter_hash {
    size_t operator()(const pmc_counter& counter) {
        return std::hash<uint32_t>{}(counter.native_source);
    }
};

std::vector<pmc_counter> query_available_pmc() {

    std::vector<pmc_counter> output;

    ULONG bufferLength;
    TraceQueryInformation(0, TraceProfileSourceListInfo, nullptr, 0, &bufferLength);
    std::vector<BYTE> buffer(bufferLength);
    check(TraceQueryInformation(0,
                                TraceProfileSourceListInfo,
                                buffer.data(),
                                static_cast<ULONG>(buffer.size()),
                                &bufferLength));

    auto profileSource = reinterpret_cast<PROFILE_SOURCE_INFO*>(buffer.data());
    TRACE_PROFILE_INTERVAL interval;

    for (;;) {

        pmc_counter counter;
        counter.Name = profileSource->Description;
        counter.native_source = profileSource->Source;

        if (profileSource->NextEntryOffset == 0) {
            break;
        }

        interval.Source = profileSource->Source;
        ULONG intervalLength;
        check(TraceQueryInformation(0,
                                    TraceSampledProfileIntervalInfo,
                                    &interval,
                                    sizeof(interval),
                                    &intervalLength));

        counter.interval = interval.Interval;

        output.push_back(counter);

        profileSource = reinterpret_cast<PROFILE_SOURCE_INFO*>(
            reinterpret_cast<BYTE*>(profileSource) + profileSource->NextEntryOffset);
    }

    return output;
}

#define PROCESS_TRACE_MODE_REAL_TIME 0x00000100
#define PROCESS_TRACE_MODE_RAW_TIMESTAMP 0x00001000
#define PROCESS_TRACE_MODE_EVENT_RECORD 0x10000000

//https://github.com/microsoft/krabsetw/blob/67a2b6ffcb4bffe3855d628cda205616d05eaaa1/krabs/krabs/perfinfo_groupmask.hpp

#define PERF_PMC_PROFILE 0x20000400

#define PERF_MASK_INDEX (0xe0000000)
#define PERF_MASK_GROUP (~PERF_MASK_INDEX)
#define PERF_NUM_MASKS 8

typedef ULONG PERFINFO_MASK;

typedef struct _PERFINFO_GROUPMASK {
    PERFINFO_MASK Masks[PERF_NUM_MASKS];
} PERFINFO_GROUPMASK, *PPERFINFO_GROUPMASK;

typedef enum _EVENT_TRACE_INFORMATION_CLASS {
    EventTraceKernelVersionInformation,
    EventTraceGroupMaskInformation,
    EventTracePerformanceInformation,
    EventTraceTimeProfileInformation,
    EventTraceSessionSecurityInformation,
    EventTraceSpinlockInformation,
    EventTraceStackTracingInformation,
    EventTraceExecutiveResourceInformation,
    EventTraceHeapTracingInformation,
    EventTraceHeapSummaryTracingInformation,
    EventTracePoolTagFilterInformation,
    EventTracePebsTracingInformation,
    EventTraceProfileConfigInformation,
    EventTraceProfileSourceListInformation,
    EventTraceProfileEventListInformation,
    EventTraceProfileCounterListInformation,
    EventTraceStackCachingInformation,
    EventTraceObjectTypeFilterInformation,
    MaxEventTraceInfoClass
} EVENT_TRACE_INFORMATION_CLASS;

typedef struct _EVENT_TRACE_GROUPMASK_INFORMATION {
    EVENT_TRACE_INFORMATION_CLASS EventTraceInformationClass;
    TRACEHANDLE TraceHandle;
    PERFINFO_GROUPMASK EventTraceGroupMasks;
} EVENT_TRACE_GROUPMASK_INFORMATION, *PEVENT_TRACE_GROUPMASK_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {} SYSTEM_INFORMATION_CLASS;

extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _Out_writes_bytes_to_opt_(SystemInformationLength, *ReturnLength) PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength
);

extern "C" NTSTATUS NTAPI NtSetSystemInformation(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _In_reads_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
    _In_ ULONG SystemInformationLength
);

constexpr auto SystemPerformanceTraceInformation{static_cast<SYSTEM_INFORMATION_CLASS>(0x1f)};

#define PERF_GET_MASK_INDEX(GM) (((GM) & PERF_MASK_INDEX) >> 29)
#define PERF_GET_MASK_GROUP(GM) ((GM) & PERF_MASK_GROUP)
#define PERFINFO_OR_GROUP_WITH_GROUPMASK(Group, pGroupMask) \
    (pGroupMask)->Masks[PERF_GET_MASK_INDEX(Group)] |= PERF_GET_MASK_GROUP(Group)

namespace {

    template <class T>
    T read_event_property(PEVENT_RECORD pEvent, const wchar_t* propertyName) {

        PROPERTY_DATA_DESCRIPTOR descriptor;
        descriptor.PropertyName = reinterpret_cast<ULONGLONG>(propertyName);
        descriptor.ArrayIndex = 0;

        ULONG valueSize;
        check(TdhGetPropertySize(pEvent, 0, nullptr, 1, &descriptor, &valueSize));

        std::vector<std::byte> valueBuffer(valueSize);
        check(TdhGetProperty(pEvent, 0, nullptr, 1, &descriptor, valueSize,
                             reinterpret_cast<PBYTE>(valueBuffer.data())));

        if (valueSize == sizeof(T))
        {
          return *reinterpret_cast<T*>(valueBuffer.data());
        }
        else
          return 0;
    }

#if 1
    void dump_event_info(PEVENT_RECORD pEvent) {
        ULONG bufferSize = 0;
        auto result = TdhGetEventInformation(pEvent, 0, nullptr, nullptr, &bufferSize);

        assert(result == ERROR_INSUFFICIENT_BUFFER);

        std::vector<std::byte> buffer(bufferSize);
        auto eventInfo = reinterpret_cast<PTRACE_EVENT_INFO>(buffer.data());

        check(TdhGetEventInformation(pEvent, 0, nullptr, eventInfo, &bufferSize));

        auto opCode = pEvent->EventHeader.EventDescriptor.Opcode;
        auto opCodeName = reinterpret_cast<const wchar_t*>(&buffer[eventInfo->OpcodeNameOffset]);
        auto taskName = reinterpret_cast<const wchar_t*>(&buffer[eventInfo->TaskNameOffset]);
        auto providerName = reinterpret_cast<const wchar_t*>(&buffer[eventInfo->ProviderNameOffset]);

        std::wcout << "Task: " << taskName << std::endl;
        std::wcout << "OpCode: " << opCodeName << " (" << opCode << ")" << std::endl;
        std::wcout << "Provider: " << providerName << std::endl;

        std::wcout << "Properties: ";

        for (unsigned i = 0; i < eventInfo->TopLevelPropertyCount; ++i) {
            EVENT_PROPERTY_INFO* propertyInfo = &eventInfo->EventPropertyInfoArray[i];

            auto propertyName = reinterpret_cast<const wchar_t*>(&buffer[propertyInfo->NameOffset]);

            std::wcout << propertyName << ",";
        }

        std::wcout << std::endl;
    }
#endif

}

class pmc_kernel_session
{
    TRACEHANDLE _handle{INVALID_PROCESSTRACE_HANDLE};
    TRACEHANDLE _session{INVALID_PROCESSTRACE_HANDLE};
    std::vector<std::byte> _propBuffer;
    std::thread _sessionThread;
    std::unordered_map<pmc_counter, uint64_t, pmc_counter_hash> _sessionCounter;
    bool _isRunning{false};
    DWORD _threadId { 0 };

    //https://docs.microsoft.com/en-us/windows/win32/etw/perfinfo
    static constexpr GUID PerfInfoGuid = {
        0xce1dbfb4, 0x137e, 0x4da6, {0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc}
    };

    static VOID WINAPI StaticRecordEventCallback(PEVENT_RECORD pEvent) {

        auto instance = static_cast<pmc_kernel_session*>(pEvent->UserContext);

        if (!IsEqualGUID(pEvent->EventHeader.ProviderId, PerfInfoGuid)) {
            return;
        }

        switch (pEvent->EventHeader.EventDescriptor.Opcode) {
        case 73: //CollectionStart PERFINFO_LOG_TYPE_PFMAPPED_SECTION_CREATE

            {
                if (pEvent->EventHeader.ThreadId != instance->_threadId) {
                    return;
                }

                auto profileSource = read_event_property<uint32_t>(pEvent, L"Source");

                if (profileSource == 0) {
                    return;
                }

                auto newInterval = read_event_property<uint32_t>(pEvent, L"NewInterval");

                std::cout << "Source: " << profileSource << std::endl;
                std::cout << "NewInterval: " << newInterval << std::endl;

                auto it = instance->_sessionCounter.find(pmc_counter{{}, {}, profileSource});
                if (it != instance->_sessionCounter.end())
                {
                  auto& pmc = const_cast<pmc_counter&>(it->first);
                  pmc.interval = newInterval;
                }

                break;
            }


        case 46: // 0x2e SampledProfile
        {
          auto tid = read_event_property<uint32_t>(pEvent, L"ThreadId");
          auto ip = read_event_property<uint64_t>(pEvent, L"InstructionPointer");
          auto count = read_event_property<uint16_t>(pEvent, L"Count");
          /* auto profileSource = read_event_property<uint32_t>(pEvent, L"Source"); */
          /* dump_event_info(pEvent); */
          break;
        }

        case 0x37: // SampledProfileCache
        {
          Assert(False);
          break;
        }

        case 47: // 0x2f PmcCounterProf
            {
                auto tid = read_event_property<uint32_t>(pEvent, L"ThreadId");

                /* if (tid != instance->_threadId) { */
                /*     return; */
                /* } */

                auto profileSource = read_event_property<uint16_t>(pEvent, L"ProfileSource");

                /* std::wcout << "Source " << profileSource << std::endl; */

                auto it = instance->_sessionCounter.find(pmc_counter{{}, {}, profileSource});
                if (it != instance->_sessionCounter.end())
                {
                  /* std::wcout << it->first.Name << "(" << profileSource << ")(" << it->first.interval << ")(" << tid << ")" << std::endl; */
                  it->second += it->first.interval;
                }

                break;
            }
        }
    }

    static BOOL WINAPI StaticBufferEventCallback(PEVENT_TRACE_LOGFILE buf) {
        return TRUE;
    }

    void start_kernel_trace()
    {

        auto bufferSize = sizeof(EVENT_TRACE_PROPERTIES_V2) + sizeof(KERNEL_LOGGER_NAME);

        _propBuffer.clear();
        _propBuffer.resize(bufferSize);

        //TODO: Filtering has no effect?
        //auto targetPid = GetCurrentProcessId();

        //EVENT_FILTER_DESCRIPTOR filterDesc{};
        //filterDesc.Type = EVENT_FILTER_TYPE_PID;
        //filterDesc.Size = sizeof(targetPid);
        //filterDesc.Ptr = reinterpret_cast<ULONGLONG>(&targetPid);

        auto properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(_propBuffer.data());
        properties->EnableFlags = EVENT_TRACE_FLAG_PROFILE;
        properties->Wnode.Guid = SystemTraceControlGuid;
        properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID | WNODE_FLAG_VERSIONED_PROPERTIES;
        properties->Wnode.BufferSize = static_cast<ULONG>(bufferSize);
        properties->Wnode.ClientContext = 1; //100ns precision
        properties->LogFileMode = EVENT_TRACE_INDEPENDENT_SESSION_MODE | EVENT_TRACE_REAL_TIME_MODE;
        properties->FlushTimer = 1;
        properties->LogFileNameOffset = 0;
        properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

        //properties->VersionNumber = 2;
        //properties->FilterDescCount = 1;
        //properties->FilterDesc = &filterDesc;

        ControlTrace(0, KERNEL_LOGGER_NAME, properties, EVENT_TRACE_CONTROL_STOP);

        check(StartTrace(&_handle, KERNEL_LOGGER_NAME, properties));

        //Group masks

        EVENT_TRACE_GROUPMASK_INFORMATION gmi{};
        gmi.EventTraceInformationClass = EventTraceGroupMaskInformation;
        gmi.TraceHandle = _handle;

        check((ULONG)NtQuerySystemInformation(SystemPerformanceTraceInformation, &gmi, sizeof(gmi), nullptr));

        PERFINFO_MASK groupMask = PERF_PMC_PROFILE;
        PERFINFO_OR_GROUP_WITH_GROUPMASK(groupMask, &gmi.EventTraceGroupMasks);

        check((ULONG)NtSetSystemInformation(SystemPerformanceTraceInformation, &gmi, sizeof(gmi)));
    }

    void launch_trace()
    {

        EVENT_TRACE_LOGFILE logfile{};
        logfile.LoggerName = const_cast<char *>(KERNEL_LOGGER_NAME);
        logfile.EventRecordCallback = reinterpret_cast<PEVENT_RECORD_CALLBACK>(StaticRecordEventCallback);
        logfile.BufferCallback = reinterpret_cast<PEVENT_TRACE_BUFFER_CALLBACK>(StaticBufferEventCallback);
        logfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_REAL_TIME;
        logfile.Context = this;

        _session = OpenTraceA(&logfile);

        if (_session == INVALID_PROCESSTRACE_HANDLE) {
            check(GetLastError());
        }

        _sessionThread = std::thread([this]() {
            check(ProcessTrace(&_session, 1, nullptr, nullptr));
        });

        /* Sleep(100); */
    }

    void enable_pmc(const std::initializer_list<pmc_counter>& counter) {

        std::vector<int> sources;

        sources.reserve(counter.size());

        for (const auto& pmc_counter : counter) {
            TRACE_PROFILE_INTERVAL interval;
            interval.Source = pmc_counter.native_source;
            interval.Interval = static_cast<ULONG>(pmc_counter.interval);

            /* check(TraceSetInformation(0, */
            /*                           TraceSampledProfileIntervalInfo, */
            /*                           &interval, */
            /*                           sizeof(TRACE_PROFILE_INTERVAL))); */

            sources.push_back((int)pmc_counter.native_source);

            _sessionCounter[pmc_counter] = 0;
        }

        auto result = TraceSetInformation(0,
                                          TraceProfileSourceConfigInfo,
                                          sources.data(),
                                          static_cast<ULONG>(sources.size() * sizeof(int)));

        if (result != ERROR_WMI_ALREADY_ENABLED) {
            check(result);
        }
    }

public:

    void start(const std::initializer_list<pmc_counter>& counter)
    {
        Assert(_isRunning == false);

        _threadId = GetCurrentThreadId();

        SetPrivilege(11); //SE_SYSTEM_PROFILE_PRIVILEGE
        enable_pmc(counter);

        if (_handle == INVALID_PROCESSTRACE_HANDLE) {
            start_kernel_trace();
        }

        launch_trace();
        _isRunning = true;
    }

    void stop() {

        Assert(_isRunning == true);

        auto properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(_propBuffer.data());
        check(ControlTrace(_handle, NULL, properties, EVENT_TRACE_CONTROL_STOP));

        auto result = CloseTrace(_session);

        if (result != ERROR_CTX_CLOSE_PENDING) {
            check(result);
        }

        _sessionThread.join();
        _isRunning = false;
        _session = INVALID_PROCESSTRACE_HANDLE;
    }

    auto results() const {
        assert(!_isRunning);
        return _sessionCounter;
    }

    ~pmc_kernel_session() {
      Assert(_isRunning == false);
    }
};

void AMD_CheckSupportedCounters()
{
#if 0
  // AMD -- https://www.amd.com/system/files/TechDocs/24593.pdf Page 453
  u32 cpuInfo[4];
  u32 function_id = 0x80000001;
  __cpuid((int*)cpuInfo, (int)function_id);

  u32 LWPBit = (1 << 15);
  u32 REG_ECX = 2;
  /* u32 REG_EDX = 3; */
  b32 LWPSupported = cpuInfo[REG_ECX] & LWPBit;

  DebugLine("LWP Supported (%u)", LWPSupported);

#elif 0
  // AMD -- https://www.amd.com/system/files/TechDocs/24593.pdf Page 454
  u32 cpuInfo[4];
  u32 function_id = 0x8000001C;
  __cpuid((int*)cpuInfo, (int)function_id);

  u32 REG_EAX = 0;
  u32 REG_EBX = 1;
  u32 REG_ECX = 2;
  u32 REG_EDX = 3;
  u32 LWPBit = (1 << 0);
  b32 LWPSupported = cpuInfo[REG_EAX] & LWPBit;

  DebugLine("LWP Supported (%u)", LWPSupported);
#else
  // AMD -- https://www.amd.com/system/files/TechDocs/24593.pdf Page 422
  u32 cpuInfo[4];
  u32 function_id = 0x80000001;
  __cpuid((int*)cpuInfo, (int)function_id);

  u32 REG_EAX = 0;
  u32 REG_EBX = 1;
  u32 REG_ECX = 2;
  u32 REG_EDX = 3;


  u32 L3CountersBit = 1 << 28;
  b32 L3Counters = cpuInfo[REG_ECX] & L3CountersBit;

  DebugLine("L3 Counters Supported (%u)", L3Counters);
#endif
}
#endif

int RunPMCStuff() {
#if 0

    for (const auto& pmc : query_available_pmc()) {
        std::wcout << pmc.Name << "(" << pmc.native_source << ")" << std::endl;
    }

    //https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ke/profobj/kprofile_source.htm
    /* pmc_counter pmc0{  L"BranchMispredictions", 1, 0x0B }; */
    /* pmc_counter pmc1{  L"BranchInstruction   ", 1, 0x06 }; */

    pmc_counter pmc0{  L"DCMiss", 1, 50 };
    pmc_counter pmc1{  L"DCAccess", 1, 49 };

    /* pmc_counter pmc0{  L"DCacheMisses", 1, 8 }; */
    /* pmc_counter pmc1{  L"ICacheMisses", 1, 9 }; */

    /* pmc_counter pmc0{  L"ICMiss ", 1, 95 }; */
    /* pmc_counter pmc1{  L"ICFetch", 1, 94 }; */

    /* pmc_counter pmc0{  L"DCacheMisses", 1, 8 }; */
    /* pmc_counter pmc1{  L"DCacheAccesses", 1, 21 }; */

/*     pmc_counter pmc0{  L"ICacheMisses", 1, 9 }; */
/*     pmc_counter pmc1{  L"ICacheIssues", 1, 20 }; */

    pmc_kernel_session session;

    session.start({});
    /* session.start({pmc0}); */
    /* session.start({pmc0, pmc1}); */

    /* memory_arena *Arena = AllocateArena(); */
    /* heap_allocator Heap = InitHeap(Gigabytes(8)); */
    /* LoadVoxModel(Arena, &Heap, "models/chr_rain.vox"); */

    session.stop();

    std::cout << "++++++RESULTS+++++++" << std::endl;

    auto results = session.results();

    auto t0 = results[pmc0];
    auto t1 = results[pmc1];

    /* DebugLine("Total %s : %llu", pmc0.Name, t0); */
    /* DebugLine("Total %s : %llu", pmc1.Name, t1); */

    std::wcout << "Total " << pmc0.Name << "(" << t0 << ")" << std::endl;
    std::wcout << "Total " << pmc1.Name << "(" << t1 << ")" << std::endl;

    std::cout << "Ratio: " << SafeDivide0((double)t0, (double)t1) << std::endl;

#endif
    return 0;
}
