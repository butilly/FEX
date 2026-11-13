// SPDX-License-Identifier: MIT
#pragma once

// Temporary, executable-scoped diagnostic. Use Wine's cooperative suspension
// and x64 context conversion; never inspect a running FEX thread's StateFrame.
namespace DotAgeDiagnostic {
using NextThreadFn = NTSTATUS(WINAPI*)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, HANDLE*);
using ReadMemoryFn = NTSTATUS(WINAPI*)(HANDLE, const void*, void*, SIZE_T, SIZE_T*);
using ResumeFn = NTSTATUS(WINAPI*)(HANDLE, ULONG*);
using OutputFn = void (*)(const char*);
static NextThreadFn NextThread;
static ReadMemoryFn ReadMemory;
static ResumeFn Resume;
static OutputFn Output;

template<typename... Args>
void Log(fmt::format_string<Args...> Format, Args&&... Arguments) {
  auto Message = fextl::fmt::format(Format, std::forward<Args>(Arguments)...);
  Message = fextl::fmt::format("[dotage-hang] {}\n", Message);
  Output(Message.c_str());
}

// Mapping queries and formatting must run AFTER resume: a target may own the
// heap, loader, or an emulator lock. The only stack reads use NtReadVirtualMemory.
void Describe(unsigned Round, DWORD TID, const char* Kind, uint64_t Address, unsigned Slot) {
  MEMORY_BASIC_INFORMATION Info {};
  if (!Address || NtQueryVirtualMemory(NtCurrentProcess(), reinterpret_cast<void*>(Address), MemoryBasicInformation,
                                       &Info, sizeof(Info), nullptr)) {
    return;
  }
  const ULONG Execute = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  if (Info.State != MEM_COMMIT || !(Info.Protect & Execute) || (Info.Protect & PAGE_GUARD)) {
    return;
  }
  const auto Path = FEX::Windows::GetSectionFilePath(Address);
  Log("round={} tid={:x} {}[{}]={:x} allocation={:x}+{:x} type={:x} module={}", Round, TID, Kind, Slot, Address,
      reinterpret_cast<uintptr_t>(Info.AllocationBase), Address - reinterpret_cast<uintptr_t>(Info.AllocationBase), Info.Type,
      Path.empty() ? std::string_view {"<private/unmapped-name>"} : FEX::Windows::BaseName(Path));
}

void Sample(unsigned Round, HANDLE Thread, DWORD TID) {
  CONTEXT Context {};
  Context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  uint64_t Stack[256] {};
  SIZE_T StackBytes {};
  struct Frame { uint64_t Next; uint64_t Return; } Frames[8] {};
  unsigned FrameCount {};

  const NTSTATUS Suspended = NtSuspendThread(Thread, nullptr);
  if (Suspended) {
    Log("round={} tid={:x} suspend-status={:x}", Round, TID, static_cast<uint32_t>(Suspended));
    return;
  }
  // NtGetContextThread waits for the cooperative JIT suspend point. No frontend
  // mutex is held, and the target is resumed on EVERY path after a successful suspend.
  const NTSTATUS GotContext = NtGetContextThread(Thread, &Context);
  NTSTATUS ReadStatus = STATUS_UNSUCCESSFUL;
  if (!GotContext) {
    ReadStatus = ReadMemory(NtCurrentProcess(), reinterpret_cast<void*>(Context.Rsp), Stack, sizeof(Stack), &StackBytes);
    uint64_t FramePointer = Context.Rbp;
    for (; FrameCount < 8; ++FrameCount) {
      // Do not chase arbitrary RBP values when frame pointers are omitted.
      if (FramePointer < Context.Rsp || FramePointer - Context.Rsp > 1024 * 1024 || (FramePointer & 7)) {
        break;
      }
      SIZE_T Read {};
      if (ReadMemory(NtCurrentProcess(), reinterpret_cast<void*>(FramePointer), &Frames[FrameCount], sizeof(Frame), &Read) ||
          Read != sizeof(Frame)) {
        break;
      }
      const uint64_t Next = Frames[FrameCount].Next;
      if (Next <= FramePointer) {
        ++FrameCount;
        break;
      }
      FramePointer = Next;
    }
  }
  NTSTATUS Resumed = Resume(Thread, nullptr);
  if (Resumed) {
    // No formatting or allocation while a target could still be suspended.
    // A terminated thread needs no resume; otherwise retry the failed syscall.
    Resumed = Resume(Thread, nullptr);
    if (Resumed) {
      return;
    }
  }

  struct { LARGE_INTEGER Created, Exited, Kernel, User; } Times {};
  const NTSTATUS TimeStatus = NtQueryInformationThread(Thread, static_cast<THREADINFOCLASS>(1) /* ThreadTimes */, &Times, sizeof(Times), nullptr);
  Log("round={} pid={:x} tid={:x} context-status={:x} resume-status={:x} rip={:x} rsp={:x} rbp={:x} "
      "rcx={:x} rdx={:x} r8={:x} r9={:x} stack-status={:x} bytes={} time-status={:x} kernel100ns={} user100ns={}",
      Round, GetCurrentProcessId(), TID, static_cast<uint32_t>(GotContext), static_cast<uint32_t>(Resumed), Context.Rip, Context.Rsp,
      Context.Rbp, Context.Rcx, Context.Rdx, Context.R8, Context.R9, static_cast<uint32_t>(ReadStatus), StackBytes,
      static_cast<uint32_t>(TimeStatus), Times.Kernel.QuadPart, Times.User.QuadPart);
  if (GotContext || Resumed) {
    return;
  }
  Describe(Round, TID, "pc", Context.Rip, 0);
  for (unsigned i = 0; i < FrameCount; ++i) {
    Describe(Round, TID, "frame-pointer-candidate", Frames[i].Return, i);
  }
  // These are executable stack words, NOT an unwound backtrace. Ignore duplicates
  // and cap output; a repeated candidate across rounds is useful even for Mono JIT.
  uint64_t Seen[8] {};
  unsigned Count {};
  for (unsigned i = 0; i < StackBytes / sizeof(uint64_t) && Count < 8; ++i) {
    const uint64_t Address = Stack[i];
    bool Duplicate = Address == Context.Rip;
    for (unsigned j = 0; j < Count; ++j) {
      Duplicate |= Seen[j] == Address;
    }
    if (!Address || Duplicate) {
      continue;
    }
    MEMORY_BASIC_INFORMATION Info {};
    if (!NtQueryVirtualMemory(NtCurrentProcess(), reinterpret_cast<void*>(Address), MemoryBasicInformation, &Info, sizeof(Info), nullptr) &&
        Info.State == MEM_COMMIT && !(Info.Protect & PAGE_GUARD) &&
        (Info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
      Seen[Count++] = Address;
      Describe(Round, TID, "stack-candidate", Address, i);
    }
  }
}

DWORD WINAPI Run(void*) {
  Log("sampler-entry pid={:x} tid={:x} exe=dotAge.exe arch=arm64ec rounds=3 delay=60s interval=15s", GetCurrentProcessId(),
      GetCurrentThreadId());
  for (unsigned Round = 0; Round < 3; ++Round) {
    LARGE_INTEGER Delay;
    Delay.QuadPart = -(Round ? 15LL : 60LL) * 10000000LL;
    NtDelayExecution(FALSE, &Delay);
    HANDLE Previous {};
    unsigned Visited {};
    for (; Visited < 128; ++Visited) {
      HANDLE Thread {};
      const NTSTATUS Status = NextThread(NtCurrentProcess(), Previous, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME,
                                        0, 0, &Thread);
      if (Previous) {
        NtClose(Previous);
      }
      Previous = Thread;
      if (Status) {
        Log("round={} enumeration-end={:x} visited={}", Round, static_cast<uint32_t>(Status), Visited);
        break;
      }
      THREAD_BASIC_INFORMATION Info {};
      if (!NtQueryInformationThread(Thread, ThreadBasicInformation, &Info, sizeof(Info), nullptr)) {
        const DWORD TID = static_cast<DWORD>(reinterpret_cast<uintptr_t>(Info.ClientId.UniqueThread));
        if (TID != GetCurrentThreadId()) {
          Sample(Round, Thread, TID);
        }
      }
    }
    if (Previous) {
      NtClose(Previous);
    }
    Log("round={} complete visited={} capped={}", Round, Visited, Visited == 128);
  }
  Log("sampler-complete pid={:x}", GetCurrentProcessId());
  return 0;
}

void Start() {
  if (AppConfigName != "dotAge.exe") {
    return;
  }
  const auto Ntdll = GetModuleHandleW(L"ntdll.dll");
  Output = reinterpret_cast<OutputFn>(GetProcAddress(Ntdll, "__wine_dbg_output"));
  NextThread = reinterpret_cast<NextThreadFn>(GetProcAddress(Ntdll, "NtGetNextThread"));
  ReadMemory = reinterpret_cast<ReadMemoryFn>(GetProcAddress(Ntdll, "NtReadVirtualMemory"));
  Resume = reinterpret_cast<ResumeFn>(GetProcAddress(Ntdll, "NtResumeThread"));
  if (!Output) {
    return;
  }
  if (!NextThread || !ReadMemory || !Resume) {
    Log("sampler-unavailable: required NT entry point missing");
    return;
  }
  HANDLE Worker {};
  // Ordinary ARM64EC thread: allow loader and FEX thread initialization. Do not
  // wait under ProcessInit/loader lock or use WOW64 native-thread bypass flags.
  const NTSTATUS Status = NtCreateThreadEx(&Worker, THREAD_ALL_ACCESS, nullptr, NtCurrentProcess(),
                                         reinterpret_cast<PRTL_THREAD_START_ROUTINE>(&Run), nullptr, 0, 0, 0, 0, nullptr);
  Log("sampler-scheduled pid={:x} status={:x}", GetCurrentProcessId(), static_cast<uint32_t>(Status));
  if (!Status) {
    NtClose(Worker);
  }
}
} // namespace DotAgeDiagnostic
