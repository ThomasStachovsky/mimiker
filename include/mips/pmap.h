#ifndef _MIPS_PMAP_H_
#define _MIPS_PMAP_H_

#ifdef _MACHDEP

/* MIPS pmap implements standard two-level hierarchical page table
 * stored in physical addresses. Indices are 10-bits wide. */
#define PTE_INDEX_MASK 0x003ff000
#define PTE_INDEX_SHIFT 12
#define PTE_SIZE 4
#define PDE_INDEX_MASK 0xffc00000
#define PDE_INDEX_SHIFT 22

#define PTE_INDEX(x) (((x)&PTE_INDEX_MASK) >> PTE_INDEX_SHIFT)
#define PDE_INDEX(x) (((x)&PDE_INDEX_MASK) >> PDE_INDEX_SHIFT)

#define PD_ENTRIES 1024 /* page directory entries */
#define PD_SIZE (PD_ENTRIES * PTE_SIZE)
#define PT_ENTRIES 1024 /* page table entries */
#define PT_SIZE (PT_ENTRIES * PTE_SIZE)

/* Base address of all user and kernel page table entries
 * must begin at 4MiB boundary since we want to use only one PDE to map it. */
#define PT_BASE 0xff800000

/* Base addresses of active user and kernel page directory tables.
 * UPD_BASE must begin at 8KiB boundary. */
#define UPD_BASE 0xffffe000
#define KPD_BASE 0xfffff000

#ifndef __ASSEMBLER__
void *pmap_kseg2_to_kseg0(void *va);
#endif /* __ASSEMBLER__ */

#endif /* !_MACHDEP */

#ifndef __ASSEMBLER__
#include <sys/types.h>

typedef uint8_t asid_t;
typedef uint32_t pte_t;
typedef uint32_t pde_t;
#endif /* __ASSEMBLER__ */

#endif /* !_MIPS_PMAP_H_ */
