%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0",
    "RBX": "0x11220000",
    "RCX": "0x0",
    "RDX": "0x0",
    "RSI": "0x12340000",
    "RDI": "0x55660000",
    "RBP": "0x0"
  },
  "Mode": "32BIT"
}
%endif

mov esp, 0xe0000100
mov eax, 0xe0000000
mov dword [eax], 0x11223344
mov dword [eax + 4], 0x55667788
mov ecx, -1
mov esi, 0x1234ffff
cmp eax, eax
pushfd
pop ebp

db 0x0f, 0x00, 0x00       ; sldt [eax]: exact failing encoding
db 0x66, 0x0f, 0x00, 0x40, 0x04 ; sldt word [eax + 4]
db 0x0f, 0x00, 0xc1       ; sldt ecx: zero-extend to 32 bits
db 0x66, 0x0f, 0x00, 0xc6 ; sldt si: preserve upper 16 bits

pushfd
pop edx
xor ebp, edx
and ebp, 0x8d5             ; arithmetic flags must not change

mov ebx, [eax]
mov edi, [eax + 4]
movzx edx, word [eax + 4]
movzx eax, word [eax]
xor eax, ecx
xor edx, ecx
movzx ecx, si
xor esi, ecx
and ebx, 0xffff0000        ; memory stores must not touch either sentinel
and edi, 0xffff0000
and ecx, 0xffff0000
hlt
