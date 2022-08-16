#include "defs.h"
#include "string.h"
#include "types.h"
#include "printk.h"
#include "mm.h"
// #include <stdio.h>

extern char _stext[];
extern char _etext[];
extern char _srodata[];
extern char _erodata[];

// arch/riscv/kernel/vm.c
/* early_pgtbl: ⽤于 setup_vm 进⾏ 1GB 的 映射。 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);

unsigned long early_pgtbl[2048] __attribute__((__aligned__(PGSIZE)));

unsigned long pud[2048] __attribute__((__aligned__(PGSIZE)));

unsigned long pmd[2048] __attribute__((__aligned__(PGSIZE)));

void setup_vm(void)
{
    /**
	 *   直接映射:   0x80000000     0xffffffe000000000 各自1GB
	 * 57      47 46       36 35      25 24       14  13           0
	 * 00000000000 _ 00000000000 _ 00001000000 _ 00000000000 _ 00000000000000
	 *                  (0)            (64)          (0)           (0)
	 * 11111111111 _ 11111111110 _ 00000000000 _ 00000000000 _ 00000000000000
	 *    (2047)        (2046)         (0)           (0) 
	 */
    /*early_pgtbl[0] = (((uint64)page1 >> PGSHIFT) << 10) + 1;
	for(int i=256;i<384;i++){
		page1[i] = (   (i*512 + (0x80000000 >> PGSHIFT))    << 10) + 15;
	}

	early_pgtbl[480] = (((uint64)page2 >> PGSHIFT) << 10) + 1;
	for(int i=0;i<128;i++){
		page2[i] = (   (i*512 + (0x80000000 >> PGSHIFT))    << 10) + 15;
	}*/
    early_pgtbl[2047] = ((unsigned long)pud >> PGSHIFT << 10) | 0x1;
    pud[2046] = ((unsigned long)pmd >> PGSHIFT << 10) | 0x1;
    for(int i=0;i<32;i++){
		pmd[i] = (   (i*2048 + (0x80000000 >> PGSHIFT))    << 10) | 0xf;
	}

}
/* swapper_pg_dir: kernel pagetable 根⽬录， 在 setup_vm_final 进⾏映射。*/
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(PGSIZE)));
void setup_vm_final(void)
{
    memset(swapper_pg_dir, 0x0, PGSIZE); //清除swapper_pg_dir
    // No OpenSBI mapping required
    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir, (uint64)_stext, ((uint64)_stext - PA2VA_OFFSET), ((uint64)_etext - (uint64)_stext), 11);
    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64)_srodata, ((uint64)_srodata - PA2VA_OFFSET), ((uint64)_erodata - (uint64)_srodata), 3);

    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, PGROUNDUP((uint64)_erodata), (PGROUNDUP((uint64)_erodata) - PA2VA_OFFSET), (PHY_END + PA2VA_OFFSET - PGROUNDUP((uint64)_erodata)), 7);

    // set satp with swapper_pg_dir
    uint64 swp_tlb_pa = (uint64)swapper_pg_dir - PA2VA_OFFSET; //将虚拟地址转换为物理地址
    uint64 set_satp = (swp_tlb_pa >> 12) | ((uint64)8 << 60);  //设置satp
    csr_write(satp, set_satp);
    // YOUR CODE HERE
    //  flush TLB

    asm volatile("sfence.vma zero, zero");
    return;
}

/* 创建多级⻚表映射关系 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    // use 3'bxwr to represent perm

    /*
    pgtbl 为根⻚表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的⼤⼩
    perm 为映射的读写权限，可设置不同section所在⻚的属性，完成对不同section的保护
    创建多级⻚表的时候可以使⽤ kalloc() 来获取⼀⻚作为⻚表⽬录
    可以使⽤ V bit 来判断⻚表项是否存在
    */

    uint64 perm64 = (uint64)perm; //先做类型转换

    uint64 sva = PGROUNDDOWN(va);       //向下取整（事实上起始地址都是4KB对齐的）
    uint64 eva = PGROUNDUP(va + sz);    //向上取整

    uint64 spa = PGROUNDDOWN(pa);       //同理
    uint64 epa = PGROUNDUP(pa + sz);

    uint64 tmpa;
    uint64 cva = sva;
    uint64 cpa = spa;

    uint64 VPN0, VPN1, VPN2; //对应虚拟地址中的虚拟页号
    uint64 PPN0, PPN1, PPN2; //对应物理地址中的物理页号

    uint64 *p;

    static int cnt = 0;

    while (cpa < epa)
    {                                                          //未映射完，继续映射
        VPN0 = cva << 17 >> 17 >> (11 + 11 + PGSHIFT);           //提取9位一级页表索引
        VPN1 = cva << (17 + 11) >> (17 + 11) >> (11 + PGSHIFT);   //提取9位二级页表索引
        VPN2 = cva << (17 + 11 + 11) >> (17 + 11 + 11) >> PGSHIFT; //提取9位三级页表索引

        if (!(pgtbl[VPN0] << 63 >> 63))
        {                                                         //若当前页无效
            tmpa = kalloc();                                      //申请一块16KB的内存
            cnt++;                                                //用于统计申请的总页数，无其他意义
            pgtbl[VPN0] = (tmpa - PA2VA_OFFSET) >> PGSHIFT << 10; //写入其物理页号
            pgtbl[VPN0] |= 1;                                     // Valid位置1
        }

        p = (uint64 *)((pgtbl[VPN0] >> 10 << PGSHIFT) + PA2VA_OFFSET); //继续搜索三级页表

        if (!(p[VPN1] << 63 >> 63))
        {                    //若无效，申请一块新的页
            tmpa = kalloc(); //申请页
            cnt++;
            p[VPN1] = (tmpa - PA2VA_OFFSET) >> PGSHIFT << 10;
            p[VPN1] |= 1; // Valid置位
        }

        p = (uint64 *)((p[VPN1] >> 10 << PGSHIFT) + PA2VA_OFFSET);

        p[VPN2] = cpa >> PGSHIFT << 10; //三级页出写入最终物理地址的物理页号
        p[VPN2] |= perm64;              //改写权限

        cpa += PGSIZE; //继续映射下一块内存
        cva += PGSIZE;
    }
}

