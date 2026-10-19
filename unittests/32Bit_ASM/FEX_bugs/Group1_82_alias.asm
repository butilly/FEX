%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x00000000"
  },
  "Mode": "32BIT"
}
%endif

; Compare the legacy 82 aliases against 80, including defined flags and
; preservation of adjacent bytes. AF is undefined for logical operations.
mov esp, 0xe0001000
mov esi, 0xe0000000
xor edi, edi

%macro CHECK_REGISTER 3
  mov eax, %2
  db 0x80, 0xc0 + (%1 << 3), %3
  pushfd
  pop ebx
  mov edx, eax
  mov eax, %2
  db 0x82, 0xc0 + (%1 << 3), %3
  pushfd
  pop ecx
  xor eax, edx
  or edi, eax
  xor ecx, ebx
  and ecx, 0x8c5
  or edi, ecx
%endmacro

%macro CHECK_MEMORY 2
  mov dword [esi], 0x12347fa5
  %if %2
    db 0xf0
  %endif
  db 0x80, 0x46 + (%1 << 3), 1, 0x81
  pushfd
  pop ebx
  mov edx, [esi]
  mov dword [esi], 0x12347fa5
  %if %2
    db 0xf0
  %endif
  db 0x82, 0x46 + (%1 << 3), 1, 0x81
  pushfd
  pop ecx
  mov eax, [esi]
  xor eax, edx
  or edi, eax
  xor ecx, ebx
  and ecx, 0x8c5
  or edi, ecx
%endmacro

%macro CHECK_ALIAS 1
  CHECK_REGISTER %1, 0x1234a57f, 1
  CHECK_REGISTER %1, 0x1234a5ff, 1
  CHECK_REGISTER %1, 0x1234a580, 0x80
  CHECK_MEMORY %1, 0
  CHECK_MEMORY %1, 1
%endmacro

CHECK_ALIAS 0 ; ADD
CHECK_ALIAS 1 ; OR
CHECK_ALIAS 4 ; AND (dispatcher id 0x14)
CHECK_ALIAS 5 ; SUB
CHECK_ALIAS 6 ; XOR

mov eax, edi
hlt
