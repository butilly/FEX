%ifdef CONFIG
{
  "RegData": {
    "RAX": "0x0",
    "RBX": "0x0",
    "RCX": "0x1122334455660000",
    "RDX": "0x1122334455660000",
    "RDI": "0x1122334455660000",
    "R8": "0x0",
    "R9": "0x0",
    "R10": "0x0",
    "R11": "0x0"
  }
}
%endif

mov rsp, 0xe0000100
mov rsi, 0xe0000000
mov rdx, 0x1122334455667788
mov [rsi], rdx
mov [rsi + 8], rdx
mov rax, -1
mov rbx, -1
mov rcx, 0x112233445566ffff
mov r8, -1
mov r9, -1
cmp rsi, rsi
pushfq
pop r10

db 0x48, 0x0f, 0x00, 0xc0       ; sldt rax
db 0x0f, 0x00, 0xc3             ; sldt ebx: clear upper 32 bits
db 0x66, 0x0f, 0x00, 0xc1       ; sldt cx: preserve upper 48 bits
db 0x48, 0x0f, 0x00, 0x06       ; REX.W memory still writes only 16 bits
db 0x66, 0x0f, 0x00, 0x46, 0x08 ; 66 memory also writes only 16 bits
db 0xf3, 0x49, 0x0f, 0x00, 0xc0 ; rep sldt r8
db 0xf2, 0x49, 0x0f, 0x00, 0xc1 ; repne sldt r9

pushfq
pop r11
xor r10, r11
and r10, 0x8d5

mov rdx, [rsi]
mov rdi, [rsi + 8]
movzx r11d, word [rsi]
xor rax, r11
xor rbx, r11
xor rcx, r11
xor rdx, r11
xor rdi, r11
xor r8, r11
xor r9, r11
and r11, -65536
hlt
