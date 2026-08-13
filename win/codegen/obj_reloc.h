/* obj_reloc.h - 平台无关的重定位类型语义。
 *
 * encoder.c 在 IrReloc.type 里填这两个值。coff_writer 直接用它们
 * (值与 IMAGE_REL_AMD64_* 一致,所以 COFF 输出零改动);elf_writer
 * 把它们映射到对应的 R_X86_64_* 常量。
 *
 * 保持值为 4/1 是为了和现有 COFF 代码兼容 —— IrReloc.type 是 uint16_t,
 * encoder 已按这两个值填充,改动值会破坏 Win64 路径。 */
#ifndef MIRA_OBJ_RELOC_H
#define MIRA_OBJ_RELOC_H

/* RIP-relative 32 位重定位:call/jmp/mov [rip+disp32]。
 * COFF: IMAGE_REL_AMD64_REL32 (=4)
 * ELF:  R_X86_64_PLT32 (=4, 公式 S+A-P) */
#define MIRA_RELOC_RIP32  4

/* 64 位绝对地址重定位:数据段里的指针。
 * COFF: IMAGE_REL_AMD64_ADDR64 (=1)
 * ELF:  R_X86_64_64 (=1, 公式 S+A) */
#define MIRA_RELOC_ABS64  1

#endif /* MIRA_OBJ_RELOC_H */
