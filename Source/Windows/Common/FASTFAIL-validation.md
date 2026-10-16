# Windows fast-fail validation build

This build packages the existing INT 0x29 support in `Exception.h` without
changing emulator behavior or game files.

## Report motivating the build

TLBB's 32-bit `webclient.exe`, under FEXCore 2605 and Proton 10 ARM64EC,
repeatedly reports guest EIP 0x446ced and `Unknown interrupt: 0x29`.
The retained report contains 20,507 illegal-instruction exception events;
its middle was truncated, so this is not a full-session count.

## Existing implementation being exercised

`HandleGuestException` maps guest INT 0x29 to
`STATUS_STACK_BUFFER_OVERRUN`, sets `EXCEPTION_NONCONTINUABLE`, preserves
the faulting guest instruction address and supplies the guest ECX/RCX
fast-fail reason as its single exception parameter. It sets `FirstChance`
to FALSE. Both the WOW64 and ARM64EC frontends pass that value to their
NtRaiseException entry point, allowing second-chance termination rather
than resuming the faulting guest instruction.

## Device validation

- Confirm the installed FEX package version matches this build.
- Reproduce the launcher/browser startup with the same configuration.
- Check that the unknown-interrupt/illegal-instruction loop disappears and
  the failing helper terminates with the fast-fail status and reason.
- Treat any remaining CEF rendering failure, OgreMain access violation or
  Android input ANR as separate evidence. This change does not repair the
  underlying condition that made the helper request fast-fail termination.

No device execution has been performed as part of preparing this build.
