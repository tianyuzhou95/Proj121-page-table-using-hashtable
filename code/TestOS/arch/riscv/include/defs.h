#ifndef _DEFS_H
#define _DEFS_H


#define PHY_START 0x0000000080000000
#define PHY_SIZE  128 * 1024 * 1024 // 128MB， QEMU 默认内存大小
#define PHY_END   (PHY_START + PHY_SIZE)

#define PGSIZE 0x4000 // 16KB
#define PGSHIFT 14
#define PGROUNDUP(addr) ((addr + PGSIZE - 1) & (~(PGSIZE - 1)))
#define PGROUNDDOWN(addr) (addr & (~(PGSIZE - 1)))


#define OPENSBI_SIZE (0x200000)

#define VM_START (0xffffffe000000000)
#define VM_END   (0xffffffff00000000)
#define VM_SIZE  (VM_END - VM_START)

#define PA2VA_OFFSET (VM_START - PHY_START)


#define csr_read(csr)                       \
({                                          \
    register unsigned long __v;                    \
    asm volatile ("csrr" "%0", "#csr"       \
                    : "=r"  (__v)           \
                    :                       \
                    : "memory");            \
}) //使用内联汇编配合csrr伪指令将对应csr寄存器的内容读到__v中

#define csr_write(csr, val)                         \
({                                                  \
    unsigned long __v = (unsigned long)(val);                     \
    asm volatile ("csrw " #csr ", %0"               \
                    : : "r" (__v)                   \
                    : "memory");                    \
})

#endif
