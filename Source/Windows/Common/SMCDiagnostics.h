// SPDX-License-Identifier: MIT
#pragma once

#include <FEXCore/fextl/fmt.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <windows.h>

namespace FEX::Windows::SMCDiagnostics {
inline uint64_t Ticks() {
  LARGE_INTEGER Counter {};
  QueryPerformanceCounter(&Counter);
  return Counter.QuadPart;
}

struct Event {
  uint64_t Start {};
  // ThreadCreationMutex wait, interval lookup, code mutex wait, shared code
  // invalidation, thread cache/return-stack invalidation, page unprotection.
  std::array<uint64_t, 6> Stages {};
};

// The ARM64EC caller holds ThreadCreationMutex for both sampling and reporting.
// This intentionally bypasses SILENTLOG: component cards need no logging env
// override, and enabling the existing per-fault debug stream perturbs this test.
class Summary {
public:
  void Record(const Event& Sample, uint64_t End) {
    ++Count;
    ++TotalCount;
    TotalTicks += End - Sample.Start;
    MaxTicks = std::max(MaxTicks, End - Sample.Start);
    for (size_t i = 0; i < Sum.size(); ++i) {
      Sum[i] += Sample.Stages[i];
      Max[i] = std::max(Max[i], Sample.Stages[i]);
    }

    if (LastReport && End - LastReport < Frequency) {
      return;
    }
    const auto Ms = [this](uint64_t Value) { return static_cast<double>(Value) * 1000.0 / Frequency; };
    if (Output) {
      const auto Line = fextl::fmt::format(
        "FEX-SMC-TIMING pid={} qpc_ms={:.3f} window_ms={:.3f} count={} total_count={} total_ms={:.3f} max_ms={:.3f} "
        "thread_wait_ms={:.3f}/{:.3f} lookup_ms={:.3f}/{:.3f} code_wait_ms={:.3f}/{:.3f} "
        "buffers_ms={:.3f}/{:.3f} caches_ms={:.3f}/{:.3f} protect_ms={:.3f}/{:.3f} (sum/max)\n",
        GetCurrentProcessId(), Ms(End), LastReport ? Ms(End - LastReport) : 0.0, Count, TotalCount, Ms(TotalTicks), Ms(MaxTicks),
        Ms(Sum[0]), Ms(Max[0]), Ms(Sum[1]), Ms(Max[1]), Ms(Sum[2]), Ms(Max[2]), Ms(Sum[3]), Ms(Max[3]),
        Ms(Sum[4]), Ms(Max[4]), Ms(Sum[5]), Ms(Max[5]));
      Output(Line.c_str());
    }
    LastReport = End;
    Count = TotalTicks = MaxTicks = 0;
    Sum.fill(0);
    Max.fill(0);
  }

private:
  uint64_t Frequency = [] {
    LARGE_INTEGER Value {};
    QueryPerformanceFrequency(&Value);
    return static_cast<uint64_t>(Value.QuadPart ? Value.QuadPart : 1);
  }();
  void (*Output)(const char*) = reinterpret_cast<void (*)(const char*)>(
    GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "__wine_dbg_output"));
  uint64_t LastReport {}, Count {}, TotalCount {}, TotalTicks {}, MaxTicks {};
  std::array<uint64_t, 6> Sum {}, Max {};
};
} // namespace FEX::Windows::SMCDiagnostics
