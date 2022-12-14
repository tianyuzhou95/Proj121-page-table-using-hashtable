/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_64_H
#define _ASM_RISCV_PGTABLE_64_H

#include <linux/const.h>

extern bool pgtable_l4_enabled;

#define PGDIR_SHIFT_L3  36
#define PGDIR_SHIFT_L4  47
#define PGDIR_SIZE_L3   (_AC(1, UL) << PGDIR_SHIFT_L3)

#define PGDIR_SHIFT     (pgtable_l4_enabled ? PGDIR_SHIFT_L4 : PGDIR_SHIFT_L3)
/* Size of region mapped by a page global directory */
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

/* pud is folded into pgd in case of 3-level page table */
#define PUD_SHIFT      36
#define PUD_SIZE       (_AC(1, UL) << PUD_SHIFT)
#define PUD_MASK       (~(PUD_SIZE - 1))

#define PMD_SHIFT       25
/* Size of region mapped by a page middle directory */
#define PMD_SIZE        (_AC(1, UL) << PMD_SHIFT)
#define PMD_MASK        (~(PMD_SIZE - 1))

/* Page Upper Directory entry */
typedef struct {
	unsigned long pud;
} pud_t;

#define pud_val(x)      ((x).pud)
#define __pud(x)        ((pud_t) { (x) })
#define PTRS_PER_PUD    (PAGE_SIZE / sizeof(pud_t))

/* Page Middle Directory entry */
typedef struct {
	unsigned long pmd;
} pmd_t;

#define pmd_val(x)      ((x).pmd)
#define __pmd(x)        ((pmd_t) { (x) })

#define PTRS_PER_PMD    (PAGE_SIZE / sizeof(pmd_t))

static inline int pud_present(pud_t pud)
{
	return (pud_val(pud) & _PAGE_PRESENT);
}

static inline int pud_none(pud_t pud)
{
	return (pud_val(pud) == 0);
}

static inline int pud_bad(pud_t pud)
{
	return !pud_present(pud);
}

#define pud_leaf	pud_leaf
static inline int pud_leaf(pud_t pud)
{
	return pud_present(pud) && (pud_val(pud) & _PAGE_LEAF);
}

static inline void set_pud(pud_t *pudp, pud_t pud)
{
	*pudp = pud;
}

static inline void pud_clear(pud_t *pudp)
{
	set_pud(pudp, __pud(0));
}

static inline pud_t pfn_pud(unsigned long pfn, pgprot_t prot)
{
	return __pud((pfn << _PAGE_PFN_SHIFT) | pgprot_val(prot));
}

static inline unsigned long _pud_pfn(pud_t pud)
{
	return pud_val(pud) >> _PAGE_PFN_SHIFT;
}

static inline pmd_t *pud_pgtable(pud_t pud)
{
	return (pmd_t *)pfn_to_virt(pud_val(pud) >> _PAGE_PFN_SHIFT);
}

static inline struct page *pud_page(pud_t pud)
{
	return pfn_to_page(pud_val(pud) >> _PAGE_PFN_SHIFT);
}

#define mm_pud_folded  mm_pud_folded
static inline bool mm_pud_folded(struct mm_struct *mm)
{
	if (pgtable_l4_enabled)
		return false;

	return true;
}

#define pmd_index(addr) (((addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))

static inline pmd_t pfn_pmd(unsigned long pfn, pgprot_t prot)
{
	return __pmd((pfn << _PAGE_PFN_SHIFT) | pgprot_val(prot));
}

static inline unsigned long _pmd_pfn(pmd_t pmd)
{
	return pmd_val(pmd) >> _PAGE_PFN_SHIFT;
}

#define mk_pmd(page, prot)    pfn_pmd(page_to_pfn(page), prot)

#define pmd_ERROR(e) \
	pr_err("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))

#define pud_ERROR(e)   \
	pr_err("%s:%d: bad pud %016lx.\n", __FILE__, __LINE__, pud_val(e))

static inline void set_p4d(p4d_t *p4dp, p4d_t p4d)
{
	if (pgtable_l4_enabled)
		*p4dp = p4d;
	else
		set_pud((pud_t *)p4dp, (pud_t){ p4d_val(p4d) });
}

static inline int p4d_none(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (p4d_val(p4d) == 0);

	return 0;
}

static inline int p4d_present(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (p4d_val(p4d) & _PAGE_PRESENT);

	return 1;
}

static inline int p4d_bad(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return !p4d_present(p4d);

	return 0;
}

static inline void p4d_clear(p4d_t *p4d)
{
	if (pgtable_l4_enabled)
		set_p4d(p4d, __p4d(0));
}

static inline pud_t *p4d_pgtable(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (pud_t *)pfn_to_virt(p4d_val(p4d) >> _PAGE_PFN_SHIFT);

	return (pud_t *)pud_pgtable((pud_t) { p4d_val(p4d) });
}

static inline struct page *p4d_page(p4d_t p4d)
{
	return pfn_to_page(p4d_val(p4d) >> _PAGE_PFN_SHIFT);
}

#define pud_index(addr) (((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))

#define pud_offset pud_offset
static inline pud_t *pud_offset(p4d_t *p4d, unsigned long address)
{
	if (pgtable_l4_enabled)
		return p4d_pgtable(*p4d) + pud_index(address);

	return (pud_t *)p4d;
}

#endif /* _ASM_RISCV_PGTABLE_64_H */
