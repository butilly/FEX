# TLBB WOW64 exception diagnostic, round 2

The previous validation package removed the retained unknown-INT-0x29 and
illegal-instruction messages. The follow-up still repeats host PC
0x7fdffd0350, guest EIP 0x446ced, host code 0xc0000005. That host exception
can be the deliberate null load in the dispatcher guest SIGSEGV trampoline;
it does not establish what HandleGuestException returned.

Only the process named webclient.exe gets additional diagnostics. Each
thread logs its first four events and subsequent power-of-two counts:

- `WOW64 exception diagnostic`: pre-conversion generated-fault state,
  signal, trap, error code, ECX and host access-violation parameters.
- `WOW64 exception dispatch`: converted status, flags, first-chance state,
  guest address, reason, and native entry-context PC/SP/flags.
- `WOW64 exception returned`: NtRaiseException return status, if it returns.

The two existing reconstruction messages use the same sampling gate in this
process. Other processes and exception handling semantics are unchanged.
No guest instructions are skipped, no exceptions are swallowed, and no
game files are modified.

For a generated GP interrupt 0x29, expect status 0xc0000409, flags 1,
first_chance=0 and ECX as info0. A repeat counter that continues increasing
after that distinguishes delivery/resumption from missing conversion;
generated=false or a different trap instead redirects the investigation.
Capture the first diagnostic event as well as later samples. The package
still requires FEX logging enabled to expose these debug-level messages.

The next report must compare the exception counter and helper lifetime,
then independently compare Game.exe CPU and median FPS (previously 89%
and 0.8 FPS). No performance improvement is promised by this instrument.
The attached Android ANR predates the measured run and blocks in
ClientSocket.write while sending pointer events; it is not evidence that
this measured run ended in an ANR, nor a fault this package repairs.

Validation: source diff review and diff whitespace check; CI builds the
Windows frontends. Device behavior must be verified with the next report.
