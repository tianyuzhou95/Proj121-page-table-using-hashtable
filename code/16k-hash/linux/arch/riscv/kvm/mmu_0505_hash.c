// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *     Anup Patel <anup.patel@wdc.com>
 */

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hugetlb.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/kvm_host.h>
#include <linux/sched/signal.h>
#include <asm/csr.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/sbi.h>

#ifdef CONFIG_64BIT
static unsigned long stage2_mode = (HGATP_MODE_SV39X4 << HGATP_MODE_SHIFT);
static unsigned long stage2_pgd_levels = 3;
#define stage2_index_bits 11
#else
static unsigned long stage2_mode = (HGATP_MODE_SV32X4 << HGATP_MODE_SHIFT);
static unsigned long stage2_pgd_levels = 2;
#define stage2_index_bits 10
#endif

#define stage2_pgd_xbits 2
#define stage2_pgd_size (1UL << (HGATP_PAGE_SHIFT + stage2_pgd_xbits))
#define stage2_gpa_bits                                                        \
	(HGATP_PAGE_SHIFT + (stage2_pgd_levels * stage2_index_bits) +          \
	 stage2_pgd_xbits)
#define stage2_gpa_size ((gpa_t)(1ULL << stage2_gpa_bits))

#define stage2_pte_leaf(__ptep)                                                \
	(pte_val(*(__ptep)) & (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC))

void my_set_pte(pte_t * ptep, unsigned long value){
    //将ptep的前7个bytes置为value的值
    unsigned char* charValue = (unsigned char*)&value;
    unsigned char* charPte = (unsigned char*)ptep;
    int i=0;
    for(i=0;i<7;i++){
        charPte[i] = charValue[i];
    }
}

unsigned long my_pte_val(pte_t pte){
    //取出pte前7个bytes的值
    unsigned long  tmp = 0;
    unsigned char* charPte = (unsigned char*)&pte;
    unsigned char* charValue = (unsigned char*)&tmp;
    int i=0;
    for(i=0;i<7;i++){
        charValue[i] = charPte[i];
    }
    return tmp;
}

static inline unsigned long stage2_pte_page_vaddr(pte_t pte)
{
	return (unsigned long)pfn_to_virt(my_pte_val(pte) >> _PAGE_PFN_SHIFT);
}

static bool stage2_get_leaf_entry(struct kvm *kvm, gpa_t addr, pte_t **ptepp,
				  u32 *ptep_level)
{
	//addr ---hash---> 找到对应的位置
	//获取PGD的起始地址
    unsigned char *tmpPgd = (unsigned char *)(kvm->arch.pgd);
    //获取哈希的idx和组号
    unsigned long blockNum = (unsigned long)addr >> 17;
    unsigned long hashIdx = blockNum & 0xff; //取后8位作为idx
    unsigned long blockOff = (unsigned long)addr << 47 >> 61;
    //获取该行的tag，需要进行比对是否一致
    unsigned char *curLine = tmpPgd + (hashIdx * 64);

    //前8位(再去掉最开始两个)为tag位
    unsigned long tag, valid;
    unsigned long i = 0, j = 0;

    //startTable
    unsigned char *startTable;

	while (1) {
        tag = *((unsigned long *)curLine);
        startTable = curLine + blockOff * 7 + 8;
        valid = tag >> 56;
        if (valid) {
            if ((tag & 0x0000ffffffffffff) == blockNum) {
                //找到正确的位置
                *ptep_level = 1;
				*ptepp = (pte_t *)startTable;
				break;
            } else {
                //跳到下一行
                curLine += 64;
            }
        } else {
            return false;
        }
    }
	return true;
}

static void stage2_remote_tlb_flush(struct kvm *kvm, u32 level, gpa_t addr)
{
	unsigned long size = PAGE_SIZE;
	struct kvm_vmid *vmid = &kvm->arch.vmid;

	addr &= ~(size - 1);

	/*
	 * TODO: Instead of cpu_online_mask, we should only target CPUs
	 * where the Guest/VM is running.
	 */
	preempt_disable();
	sbi_remote_hfence_gvma_vmid(cpu_online_mask, addr, size,
				    READ_ONCE(vmid->vmid));
	preempt_enable();
}

static int stage2_set_pte(struct kvm *kvm, u32 level,
			  struct kvm_mmu_memory_cache *pcache, gpa_t addr,
			  const pte_t *new_pte)
{
	kvm_info("new_pte = %lx\n",new_pte->pte);
	//获取PGD的起始地址
	unsigned char *tmpPgd = (unsigned char *)(kvm->arch.pgd);
	kvm_info("tmpPgd = %lx\n",tmpPgd);
	//获取哈希的idx和组号
	unsigned long blockNum = (unsigned long)addr >> 17;
	unsigned long hashIdx = blockNum & 0xff; //取后8位作为idx
	unsigned long blockOff = (unsigned long)addr << 47 >> 61;
	//获取该行的tag，需要进行比对是否一致
	unsigned char *curLine = tmpPgd + (hashIdx * 64);
	kvm_info("hashIdx = %lx\n",hashIdx);
	kvm_info("curLine = %lx\n",curLine);

	//前8位(再去掉最开始两个)为tag位
	unsigned long tag, valid;
	unsigned long i = 0, j = 0;

	//startTable
	unsigned char *startTable;
	//PTE开始的地方
	unsigned char *startPTE = (unsigned char *)&(new_pte->pte);
	// kvm_info("startPTE = %lx\n",startPTE);
	while (1) {
		tag = *((unsigned long *)curLine);
		startTable = curLine + blockOff * 7 + 8;
		kvm_info("startTable = %lx\n",startTable);
		valid = tag >> 56;
		// kvm_info("valid = %lx\n",valid);
		// kvm_info("tag = %lx\n",tag & 0x0000ffffffffffff);
		if (valid) {
			if ((tag & 0x0000ffffffffffff) == blockNum) {
				//写PTE
				// memcpy(startTable, startPTE, 7);
				for (j = 0; j < 7; j++) {
					startTable[j] = startPTE[j];
				}
				break;
			} else {
				//跳到下一行
				curLine += 64;
			}
		} else {
			//写下PTE
			// memcpy(startTable, startPTE, 7);
			for (j = 0; j < 7; j++) {
				startTable[j] = startPTE[j];
			}
			// kvm_info("PTE = %lx\n",my_pte_val((pte_t)*startTable));
			
			//写下TAG
			*((unsigned long *)curLine) =
				0x0100000000000000 | blockNum;
			kvm_info("TAG = %lx\n",*((unsigned long *)curLine));
			break;
		}
	}
	// stage2_remote_tlb_flush(kvm, 1, addr);
	return 0;
}

static int stage2_map_page(struct kvm *kvm, struct kvm_mmu_memory_cache *pcache,
			   gpa_t gpa, phys_addr_t hpa, unsigned long page_size,
			   bool page_rdonly, bool page_exec)
{
	int ret;
	u32 level = 0;
	pte_t new_pte;
	pgprot_t prot;

	// kvm_info("enter into stage2_map_page\n");

	// if (ret)
	// 	return ret;

	/*
	 * A RISC-V implementation can choose to either:
	 * 1) Update 'A' and 'D' PTE bits in hardware
	 * 2) Generate page fault when 'A' and/or 'D' bits are not set
	 *    PTE so that software can update these bits.
	 *
	 * We support both options mentioned above. To achieve this, we
	 * always set 'A' and 'D' PTE bits at time of creating stage2
	 * mapping. To support KVM dirty page logging with both options
	 * mentioned above, we will write-protect stage2 PTEs to track
	 * dirty pages.
	 */

	if (page_exec) {
		if (page_rdonly)
			prot = PAGE_READ_EXEC;
		else
			prot = PAGE_WRITE_EXEC;
	} else {
		if (page_rdonly)
			prot = PAGE_READ;
		else
			prot = PAGE_WRITE;
	}
	// kvm_info("hpa = %lx\n",hpa);
	new_pte = pfn_pte(PFN_DOWN(hpa), prot);
	new_pte = pte_mkdirty(new_pte);
	// kvm_info("new_pte = %lx\n",new_pte.pte);

	return stage2_set_pte(kvm, level, pcache, gpa, &new_pte);
}

enum stage2_op {
	STAGE2_OP_NOP = 0, /* Nothing */
	STAGE2_OP_CLEAR, /* Clear/Unmap */
	STAGE2_OP_WP, /* Write-protect */
};

// 将 pte 置为 0
static void stage2_op_pte(struct kvm *kvm, gpa_t addr, pte_t *ptep,
			  u32 ptep_level, enum stage2_op op)
{
	int i, ret;
	pte_t *next_ptep;
	u32 next_ptep_level;
	unsigned long next_page_size, page_size;

	page_size = PAGE_SIZE;

	BUG_ON(addr & (page_size - 1));

	//判断这个pte为0
	if (!my_pte_val(*ptep))
		return;

	if (op == STAGE2_OP_CLEAR)
		my_set_pte(ptep, 0);//定义函数:
	else if (op == STAGE2_OP_WP)
		my_set_pte(ptep, my_pte_val(*ptep) & ~_PAGE_WRITE);
	// stage2_remote_tlb_flush(kvm, 1, addr);
}

static void stage2_unmap_range(struct kvm *kvm, gpa_t start, gpa_t size,
			       bool may_block)
{
	int ret;
	pte_t *ptep;
	u32 ptep_level;
	bool found_leaf;
	unsigned long page_size;
	gpa_t addr = start, end = start + size;

	while (addr < end) {
		found_leaf =
			stage2_get_leaf_entry(kvm, addr, &ptep, &ptep_level);
		page_size = PAGE_SIZE;

		if (!found_leaf)
			goto next;

		if (!(addr & (page_size - 1)) && ((end - addr) >= page_size))
			stage2_op_pte(kvm, addr, ptep, ptep_level,
				      STAGE2_OP_CLEAR);

	next:
		addr += page_size;

		/*
		 * If the range is too large, release the kvm->mmu_lock
		 * to prevent starvation and lockup detector warnings.
		 */
		if (may_block && addr < end)
			cond_resched_lock(&kvm->mmu_lock);
	}
}

static void stage2_wp_range(struct kvm *kvm, gpa_t start, gpa_t end)
{
	int ret;
	pte_t *ptep;
	u32 ptep_level;
	bool found_leaf;
	gpa_t addr = start;
	unsigned long page_size;

	while (addr < end) {
		found_leaf =
			stage2_get_leaf_entry(kvm, addr, &ptep, &ptep_level);
		page_size = PAGE_SIZE;

		if (!found_leaf)
			goto next;

		if (!(addr & (page_size - 1)) && ((end - addr) >= page_size))
			stage2_op_pte(kvm, addr, ptep, ptep_level,
				      STAGE2_OP_WP);

	next:
		addr += page_size;
	}
}

static void stage2_wp_memory_region(struct kvm *kvm, int slot)
{
	struct kvm_memslots *slots = kvm_memslots(kvm);
	struct kvm_memory_slot *memslot = id_to_memslot(slots, slot);
	phys_addr_t start = memslot->base_gfn << PAGE_SHIFT;
	phys_addr_t end = (memslot->base_gfn + memslot->npages) << PAGE_SHIFT;

	spin_lock(&kvm->mmu_lock);
	stage2_wp_range(kvm, start, end);
	spin_unlock(&kvm->mmu_lock);
	kvm_flush_remote_tlbs(kvm);
}

static int stage2_ioremap(struct kvm *kvm, gpa_t gpa, phys_addr_t hpa,
			  unsigned long size, bool writable)
{
	pte_t pte;
	int ret = 0;
	unsigned long pfn;
	phys_addr_t addr, end;
	struct kvm_mmu_memory_cache pcache;

	memset(&pcache, 0, sizeof(pcache));
	pcache.gfp_zero = __GFP_ZERO;

	end = (gpa + size + PAGE_SIZE - 1) & PAGE_MASK;
	pfn = __phys_to_pfn(hpa);

	for (addr = gpa; addr < end; addr += PAGE_SIZE) {
		pte = pfn_pte(pfn, PAGE_KERNEL);

		if (!writable)
			pte = pte_wrprotect(pte);

		ret = kvm_mmu_topup_memory_cache(&pcache, stage2_pgd_levels);
		if (ret)
			goto out;

		spin_lock(&kvm->mmu_lock);
		ret = stage2_set_pte(kvm, 0, &pcache, addr, &pte);
		spin_unlock(&kvm->mmu_lock);
		if (ret)
			goto out;

		pfn++;
	}

out:
	kvm_mmu_free_memory_cache(&pcache);
	return ret;
}

void kvm_arch_mmu_enable_log_dirty_pt_masked(struct kvm *kvm,
					     struct kvm_memory_slot *slot,
					     gfn_t gfn_offset,
					     unsigned long mask)
{
	phys_addr_t base_gfn = slot->base_gfn + gfn_offset;
	phys_addr_t start = (base_gfn + __ffs(mask)) << PAGE_SHIFT;
	phys_addr_t end = (base_gfn + __fls(mask) + 1) << PAGE_SHIFT;

	stage2_wp_range(kvm, start, end);
}

void kvm_arch_sync_dirty_log(struct kvm *kvm, struct kvm_memory_slot *memslot)
{
}

void kvm_arch_flush_remote_tlbs_memslot(struct kvm *kvm,
					const struct kvm_memory_slot *memslot)
{
	kvm_flush_remote_tlbs(kvm);
}

void kvm_arch_free_memslot(struct kvm *kvm, struct kvm_memory_slot *free)
{
}

void kvm_arch_memslots_updated(struct kvm *kvm, u64 gen)
{
}

void kvm_arch_flush_shadow_all(struct kvm *kvm)
{
	kvm_riscv_stage2_free_pgd(kvm);
}

void kvm_arch_flush_shadow_memslot(struct kvm *kvm,
				   struct kvm_memory_slot *slot)
{
	gpa_t gpa = slot->base_gfn << PAGE_SHIFT;
	phys_addr_t size = slot->npages << PAGE_SHIFT;

	spin_lock(&kvm->mmu_lock);
	stage2_unmap_range(kvm, gpa, size, false);
	spin_unlock(&kvm->mmu_lock);
}

void kvm_arch_commit_memory_region(struct kvm *kvm, struct kvm_memory_slot *old,
				   const struct kvm_memory_slot *new,
				   enum kvm_mr_change change)
{
	/*
	 * At this point memslot has been committed and there is an
	 * allocated dirty_bitmap[], dirty pages will be tracked while
	 * the memory slot is write protected.
	 */
	if (change != KVM_MR_DELETE && new->flags & KVM_MEM_LOG_DIRTY_PAGES)
		stage2_wp_memory_region(kvm, new->id);
}

int kvm_arch_prepare_memory_region(struct kvm *kvm,
				   const struct kvm_memory_slot *old,
				   struct kvm_memory_slot *new,
				   enum kvm_mr_change change)
{
	hva_t hva, reg_end, size;
	gpa_t base_gpa;
	bool writable;
	int ret = 0;

	if (change != KVM_MR_CREATE && change != KVM_MR_MOVE &&
	    change != KVM_MR_FLAGS_ONLY)
		return 0;

	/*
	 * Prevent userspace from creating a memory region outside of the GPA
	 * space addressable by the KVM guest GPA space.
	 */
	if ((new->base_gfn + new->npages) >= (stage2_gpa_size >> PAGE_SHIFT))
		return -EFAULT;

	hva = new->userspace_addr;
	size = new->npages << PAGE_SHIFT;
	reg_end = hva + size;
	base_gpa = new->base_gfn << PAGE_SHIFT;
	writable = !(new->flags &KVM_MEM_READONLY);

	mmap_read_lock(current->mm);

	/*
	 * A memory region could potentially cover multiple VMAs, and
	 * any holes between them, so iterate over all of them to find
	 * out if we can map any of them right now.
	 *
	 *     +--------------------------------------------+
	 * +---------------+----------------+   +----------------+
	 * |   : VMA 1     |      VMA 2     |   |    VMA 3  :    |
	 * +---------------+----------------+   +----------------+
	 *     |               memory region                |
	 *     +--------------------------------------------+
	 */
	do {
		struct vm_area_struct *vma = find_vma(current->mm, hva);
		hva_t vm_start, vm_end;

		if (!vma || vma->vm_start >= reg_end)
			break;

		/*
		 * Mapping a read-only VMA is only allowed if the
		 * memory region is configured as read-only.
		 */
		if (writable && !(vma->vm_flags & VM_WRITE)) {
			ret = -EPERM;
			break;
		}

		/* Take the intersection of this VMA with the memory region */
		vm_start = max(hva, vma->vm_start);
		vm_end = min(reg_end, vma->vm_end);

		if (vma->vm_flags & VM_PFNMAP) {
			gpa_t gpa = base_gpa + (vm_start - hva);
			phys_addr_t pa;

			pa = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;
			pa += vm_start - vma->vm_start;

			/* IO region dirty page logging not allowed */
			if (new->flags & KVM_MEM_LOG_DIRTY_PAGES) {
				ret = -EINVAL;
				goto out;
			}

			ret = stage2_ioremap(kvm, gpa, pa, vm_end - vm_start,
					     writable);
			if (ret)
				break;
		}
		hva = vm_end;
	} while (hva < reg_end);

	if (change == KVM_MR_FLAGS_ONLY)
		goto out;

	spin_lock(&kvm->mmu_lock);
	if (ret)
		stage2_unmap_range(kvm, base_gpa, size, false);
	spin_unlock(&kvm->mmu_lock);

out:
	mmap_read_unlock(current->mm);
	return ret;
}

bool kvm_unmap_gfn_range(struct kvm *kvm, struct kvm_gfn_range *range)
{
	if (!kvm->arch.pgd)
		return false;

	stage2_unmap_range(kvm, range->start << PAGE_SHIFT,
			   (range->end - range->start) << PAGE_SHIFT,
			   range->may_block);
	return false;
}

bool kvm_set_spte_gfn(struct kvm *kvm, struct kvm_gfn_range *range)
{
	int ret;
	kvm_pfn_t pfn = pte_pfn(range->pte);

	if (!kvm->arch.pgd)
		return false;

	WARN_ON(range->end - range->start != 1);

	ret = stage2_map_page(kvm, NULL, range->start << PAGE_SHIFT,
			      __pfn_to_phys(pfn), PAGE_SIZE, true, true);
	if (ret) {
		kvm_debug("Failed to map stage2 page (error %d)\n", ret);
		return true;
	}

	return false;
}

bool kvm_age_gfn(struct kvm *kvm, struct kvm_gfn_range *range)
{
	pte_t *ptep;
	u32 ptep_level = 0;
	u64 size = (range->end - range->start) << PAGE_SHIFT;

	if (!kvm->arch.pgd)
		return false;

	WARN_ON(size != PAGE_SIZE && size != PMD_SIZE && size != PGDIR_SIZE);

	if (!stage2_get_leaf_entry(kvm, range->start << PAGE_SHIFT, &ptep,
				   &ptep_level))
		return false;

	return ptep_test_and_clear_young(NULL, 0, ptep);
}

bool kvm_test_age_gfn(struct kvm *kvm, struct kvm_gfn_range *range)
{
	pte_t *ptep;
	u32 ptep_level = 0;
	u64 size = (range->end - range->start) << PAGE_SHIFT;

	if (!kvm->arch.pgd)
		return false;

	WARN_ON(size != PAGE_SIZE && size != PMD_SIZE && size != PGDIR_SIZE);

	if (!stage2_get_leaf_entry(kvm, range->start << PAGE_SHIFT, &ptep,
				   &ptep_level))
		return false;

	return pte_young(*ptep);
}

int kvm_riscv_stage2_map(struct kvm_vcpu *vcpu, struct kvm_memory_slot *memslot,
			 gpa_t gpa, unsigned long hva, bool is_write)
{
	int ret;
	kvm_pfn_t hfn;
	bool writeable;
	short vma_pageshift;
	gfn_t gfn = gpa >> PAGE_SHIFT;
	struct vm_area_struct *vma;
	struct kvm *kvm = vcpu->kvm;
	struct kvm_mmu_memory_cache *pcache = &vcpu->arch.mmu_page_cache;
	bool logging = (memslot->dirty_bitmap &&
			!(memslot->flags & KVM_MEM_READONLY)) ?
			       true :
			       false;
	unsigned long vma_pagesize, mmu_seq;

	mmap_read_lock(current->mm);

	vma = find_vma_intersection(current->mm, hva, hva + 1);
	if (unlikely(!vma)) {
		kvm_err("Failed to find VMA for hva 0x%lx\n", hva);
		mmap_read_unlock(current->mm);
		return -EFAULT;
	}

	if (is_vm_hugetlb_page(vma))
		vma_pageshift = huge_page_shift(hstate_vma(vma));
	else
		vma_pageshift = PAGE_SHIFT;
	vma_pagesize = 1ULL << vma_pageshift;
	if (logging || (vma->vm_flags & VM_PFNMAP))
		vma_pagesize = PAGE_SIZE;

	if (vma_pagesize == PMD_SIZE || vma_pagesize == PGDIR_SIZE)
		gfn = (gpa & huge_page_mask(hstate_vma(vma))) >> PAGE_SHIFT;

	mmap_read_unlock(current->mm);

	if (vma_pagesize != PGDIR_SIZE && vma_pagesize != PMD_SIZE &&
	    vma_pagesize != PAGE_SIZE) {
		kvm_err("Invalid VMA page size 0x%lx\n", vma_pagesize);
		return -EFAULT;
	}

	// kvm_info("vma_pagesize = %lx\n",vma_pagesize);

	/* We need minimum second+third level pages */
	ret = kvm_mmu_topup_memory_cache(pcache, stage2_pgd_levels);
	if (ret) {
		kvm_err("Failed to topup stage2 cache\n");
		return ret;
	}

	mmu_seq = kvm->mmu_notifier_seq;

	hfn = gfn_to_pfn_prot(kvm, gfn, is_write, &writeable);
	if (hfn == KVM_PFN_ERR_HWPOISON) {
		send_sig_mceerr(BUS_MCEERR_AR, (void __user *)hva,
				vma_pageshift, current);
		return 0;
	}
	if (is_error_noslot_pfn(hfn))
		return -EFAULT;

	/*
	 * If logging is active then we allow writable pages only
	 * for write faults.
	 */
	if (logging && !is_write)
		writeable = false;

	kvm_info("\t\t\t logging = %d , is_write = %d , writeable = %d\n",logging,is_write,writeable);
	// writeable = true;	//强行给W权限
	spin_lock(&kvm->mmu_lock);

	if (mmu_notifier_retry(kvm, mmu_seq))
		goto out_unlock;

	if (writeable) {
		kvm_set_pfn_dirty(hfn);
		mark_page_dirty(kvm, gfn);
		ret = stage2_map_page(kvm, pcache, gpa, hfn << PAGE_SHIFT,
				      vma_pagesize, false, true);
	} else {
		ret = stage2_map_page(kvm, pcache, gpa, hfn << PAGE_SHIFT,
				      vma_pagesize, true, true);
	}

	if (ret)
		kvm_err("Failed to map in stage2\n");

out_unlock:
	spin_unlock(&kvm->mmu_lock);
	kvm_set_pfn_accessed(hfn);
	kvm_release_pfn_clean(hfn);
	return ret;
}

int kvm_riscv_stage2_alloc_pgd(struct kvm *kvm)
{
	struct page *pgd_page;

	if (kvm->arch.pgd != NULL) {
		kvm_err("kvm_arch already initialized?\n");
		return -EINVAL;
	}

	pgd_page = alloc_pages(GFP_KERNEL | __GFP_ZERO,
			       get_order(stage2_pgd_size));
	if (!pgd_page)
		return -ENOMEM;
	kvm->arch.pgd = page_to_virt(pgd_page);
	kvm->arch.pgd_phys = page_to_phys(pgd_page);

	return 0;
}

void kvm_riscv_stage2_free_pgd(struct kvm *kvm)
{
	void *pgd = NULL;

	spin_lock(&kvm->mmu_lock);
	if (kvm->arch.pgd) {
		stage2_unmap_range(kvm, 0UL, HGATP_PAGE_SHIFT, false);
		pgd = READ_ONCE(kvm->arch.pgd);
		kvm->arch.pgd = NULL;
		kvm->arch.pgd_phys = 0;
	}
	spin_unlock(&kvm->mmu_lock);

	if (pgd)
		free_pages((unsigned long)pgd, get_order(HGATP_PAGE_SHIFT));
}

void kvm_riscv_stage2_update_hgatp(struct kvm_vcpu *vcpu)
{
	unsigned long hgatp = stage2_mode;
	struct kvm_arch *k = &vcpu->kvm->arch;

	hgatp |=
		(READ_ONCE(k->vmid.vmid) << HGATP_VMID_SHIFT) & HGATP_VMID_MASK;
	hgatp |= (k->pgd_phys >> PAGE_SHIFT) & HGATP_PPN;

	csr_write(CSR_HGATP, hgatp);

	if (!kvm_riscv_stage2_vmid_bits())
		__kvm_riscv_hfence_gvma_all();
}

void kvm_riscv_stage2_mode_detect(void)
{
#ifdef CONFIG_64BIT
	/* Try Sv48x4 stage2 mode */
	csr_write(CSR_HGATP, HGATP_MODE_SV48X4 << HGATP_MODE_SHIFT);
	if ((csr_read(CSR_HGATP) >> HGATP_MODE_SHIFT) == HGATP_MODE_SV48X4) {
		stage2_mode = (HGATP_MODE_SV48X4 << HGATP_MODE_SHIFT);
		stage2_pgd_levels = 4;
	}
	csr_write(CSR_HGATP, 0);

	__kvm_riscv_hfence_gvma_all();
#endif
}

unsigned long kvm_riscv_stage2_mode(void)
{
	return stage2_mode >> HGATP_MODE_SHIFT;
}

int kvm_riscv_stage2_gpa_bits(void)
{
	return stage2_gpa_bits;
}
