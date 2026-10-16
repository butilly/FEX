# TLBB WOW64 fast-fail compatibility workaround, round 3

The round-2 package converts webclient.exe's generated SIGSEGV / GP fault
(error 0x14a, INT 0x29) correctly: guest EIP 0x446ced, status 0xc0000409,
EXCEPTION_NONCONTINUABLE, second chance, reason 7 (fatal application exit).
Nevertheless the dispatch counter reaches 67,108,864 with the same entry
context and no NtRaiseException return log. The helper remains at roughly
100% CPU. This establishes repeated re-entry, not a missing translation;
the exact Wine-side reason for restoring execution remains unverified.

For the Wine WOW64 webclient.exe profile only, terminate a translated
second-chance fast-fail directly with its original NTSTATUS when no debugger
is attached. This preserves the requested terminal outcome; it does not
skip INT 0x29, resume a failed process, or suppress a recoverable exception.
Debugger-attached runs, other executables, native Windows and ARM64EC keep
their existing delivery paths. Log guest address, status and original
reason before termination. An unexpected termination return is logged and
falls back to the existing exception path.

Validation: CI must compile both Windows frontends. On the device, expect
one fast-fail termination line followed by process exit with 0xc0000409,
not an increasing dispatch counter or sustained helper CPU saturation.
Check the parent application's remaining behavior independently. The CEF
GPU subprocess already exits after a Wine vkAllocateMemory assertion, and
Game.exe later raises a separate access violation: neither is repaired by
this workaround. No game files are changed and no device test is claimed.
