#include "paging.h"
#include "page.h"
#include "string.h"

static uint32_t *page_directory = 0;

/* Tạo page table rỗng (dùng sau khi enable paging) */

void paging_init(void) {
    page_directory = (uint32_t *)pmem_alloc_page();
    memset(page_directory, 0, PAGE_SIZE);

    /* Identity map 4MB đầu (kernel space) */
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t addr = i * 0x100000;
        /* Dùng page 4MB cho đơn giản */
        page_directory[i] = addr | PAGE_PRESENT | PAGE_WRITE | PAGE_4MB;
    }

    /* Map heap pages */
    uint32_t heap_start_page = KERNEL_HEAP_START / (4 * 1024 * 1024);
    for (uint32_t i = 0; i < KERNEL_HEAP_SIZE / (4 * 1024 * 1024); i++) {
        uint32_t idx = heap_start_page + i;
        if (idx < 1024) {
            page_directory[idx] = (KERNEL_HEAP_START + i * 0x100000) | PAGE_PRESENT | PAGE_WRITE | PAGE_4MB;
        }
    }
}

void paging_map_page(void *virt, void *phys, uint32_t flags) {
    uint32_t pd_idx = PD_IDX(virt);
    uint32_t pt_idx = PT_IDX(virt);

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        /* No PDE at all - create new page table */
        uint32_t *pt = (uint32_t *)pmem_alloc_page();
        memset(pt, 0, PAGE_SIZE);
        page_directory[pd_idx] = ((uint32_t)pt) | PAGE_PRESENT | PAGE_WRITE;
    } else if (page_directory[pd_idx] & PAGE_4MB) {
        /* PDE is a 4MB page - need to split into 4KB page table */
        uint32_t pde_flags = page_directory[pd_idx] & 0xFFF;
        uint32_t large_phys = page_directory[pd_idx] & 0xFFC00000;
        uint32_t *pt = (uint32_t *)pmem_alloc_page();
        if (!pt) return;
        /* Fill new page table with 1024 entries mapping the 4MB region */
        for (uint32_t i = 0; i < 1024; i++) {
            pt[i] = (large_phys + i * PAGE_SIZE) | (pde_flags & ~PAGE_4MB);
        }
        /* Replace PDE to point to page table (clear 4MB/PS flag, merge caller's access bits) */
        page_directory[pd_idx] = ((uint32_t)pt) | (pde_flags & ~PAGE_4MB) | (flags & (PAGE_USER | PAGE_WRITE));
        /* Flush TLB for this 4MB region */
        asm volatile("invlpg (%0)" : : "r"(virt));
    }

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_MASK);
    pt[pt_idx] = ((uint32_t)phys & PAGE_MASK) | flags | PAGE_PRESENT;

    asm volatile("invlpg (%0)" : : "r"(virt));
}

void paging_map_region(void *virt, void *phys, uint32_t size, uint32_t flags) {
    uint32_t v = (uint32_t)virt;
    uint32_t p = (uint32_t)phys;
    while (size > 0) {
        paging_map_page((void *)v, (void *)p, flags);
        v += PAGE_SIZE;
        p += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

void paging_unmap_page(void *virt) {
    uint32_t pd_idx = PD_IDX(virt);
    uint32_t pt_idx = PT_IDX(virt);

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_MASK);
    pt[pt_idx] = 0;

    asm volatile("invlpg (%0)" : : "r"(virt));
}

void *paging_get_phys(void *virt) {
    uint32_t pd_idx = PD_IDX(virt);
    uint32_t pt_idx = PT_IDX(virt);

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return 0;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_MASK);
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    return (void *)((pt[pt_idx] & PAGE_MASK) | ((uint32_t)virt & 0xFFF));
}

void paging_enable(void) {
    uint32_t cr0;
    asm volatile("movl %%cr3, %%eax; movl %0, %%cr3" : : "r"(page_directory) : "eax");
    asm volatile("movl %%cr0, %0; orl $0x80000000, %0; movl %0, %%cr0" : "=r"(cr0) : : "memory");
}