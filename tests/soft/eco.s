.global _start
.section .text
_start:
again:
  xor %rax,%rax
  xor %rdi,%rdi
  lea buff(%rip),%rsi
  mov $128,%rdx
  syscall
  cmp $0,%rax
  jle end
  mov %rax,%rdx
  mov $1,%rax
  mov $1,%rdi
  lea buff(%rip),%rsi
  syscall
  jmp again
end:
  mov $60,%rax
  xor %rdi,%rdi
  syscall
.section .bss
.lcomm buff,128
