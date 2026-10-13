.global _start
.section .text
_start:
  mov $1,%rax
  mov $1,%rdi
  lea message(%rip),%rsi
  mov $length,%rdx
  syscall
  mov $60,%rax
  xor %rdi,%rdi
  syscall
.section .rodata
message: .ascii "Hola desde un ELF Linux real, sin Hyper-V ni QEMU!\n"
.set length, . - message
