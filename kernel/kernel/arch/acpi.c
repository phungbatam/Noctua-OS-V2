#include "acpi.h"
#include "string.h"
#include "ports.h"
#include "screen.h"

typedef struct {
    char sig[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
} __attribute__((packed)) rsdp_t;

typedef struct {
    char sig[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) sdt_header_t;



typedef struct {
    sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt_addr;
    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_len;
    uint8_t gpe1_len;
    uint8_t gpe1_base;
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t iapc_boot_arch;
    uint8_t reserved2;
    uint32_t flags;
} __attribute__((packed)) fadt_t;

static rsdp_t *rsdp = 0;
static fadt_t *fadt = 0;
static int acpi_initialized = 0;

static uint8_t acpi_checksum(void *table, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum += ((uint8_t *)table)[i];
    return sum == 0;
}

static rsdp_t *acpi_find_rsdp(void) {
    /* Scan BIOS area 0xE0000 - 0xFFFFF for RSDP */
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
            rsdp_t *r = (rsdp_t *)addr;
            if (acpi_checksum(r, 20))
                return r;
        }
    }
    /* Scan EBDA (0x400-0x41F contains segment of EBDA) */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Warray-bounds"
    uint16_t ebda_seg = *(uint16_t *)0x40E;
    #pragma GCC diagnostic pop
    uint32_t ebda = ebda_seg << 4;
    if (ebda > 0x80000 && ebda < 0xA0000) {
        for (uint32_t addr = ebda; addr < ebda + 1024; addr += 16) {
            if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
                rsdp_t *r = (rsdp_t *)addr;
                if (acpi_checksum(r, 20))
                    return r;
            }
        }
    }
    return 0;
}

static int acpi_find_fadt(rsdp_t *rs) {
    if (!rs || !rs->rsdt_addr) return -1;
    uint32_t *rsdt_base = (uint32_t *)(uint32_t)rs->rsdt_addr;
    uint32_t rsdt_len = *(uint32_t *)((uint32_t)rs->rsdt_addr + 4);
    uint32_t count = (rsdt_len - sizeof(sdt_header_t)) / 4;
    uint32_t *entries = (uint32_t *)(rsdt_base + 5);
    for (uint32_t i = 0; i < count; i++) {
        sdt_header_t *hdr = (sdt_header_t *)(uint32_t)entries[i];
        if (memcmp(hdr->sig, "FACP", 4) == 0) {
            fadt = (fadt_t *)hdr;
            return 0;
        }
    }
    return -1;
}

int acpi_init(void) {
    rsdp = acpi_find_rsdp();
    if (!rsdp) return -1;
    if (acpi_find_fadt(rsdp) < 0) return -1;
    acpi_initialized = 1;
    return 0;
}

int acpi_available(void) {
    return acpi_initialized && fadt;
}

int acpi_poweroff(void) {
    if (!acpi_available()) return -1;

    /* Get SLP_TYP for S5 (soft off) from DSDT */
    /* Default: SLP_TYPa = 7, SLP_TYPb = 7 for S5 */
    uint16_t slp_typa = 7;
    uint16_t slp_typb = 7;

    /* PM1a_CNT: write SLP_TYP | SCI_EN */
    uint16_t pm1a_cnt = fadt->pm1a_cnt_blk;
    if (pm1a_cnt) {
        outw(pm1a_cnt, slp_typa << 10 | 0x2000);
    }

    /* PM1b_CNT if present */
    uint16_t pm1b_cnt = fadt->pm1b_cnt_blk;
    if (pm1b_cnt) {
        outw(pm1b_cnt, slp_typb << 10 | 0x2000);
    }

    return 0;
}

int acpi_reboot(void) {
    if (!acpi_available()) return -1;

    /* Try using reset register from FADT (offset 116: 12-byte GEN_ADDR) */
    uint8_t *fadt_bytes = (uint8_t *)fadt;
    uint8_t reset_reg = fadt_bytes[118]; /* Address space ID at offset 116+2 */
    uint32_t reset_addr = *(uint32_t *)&fadt_bytes[120]; /* Address at offset 120 */
    uint8_t reset_val = fadt_bytes[128]; /* Reset value at offset 128 */

    if (reset_reg == 1 && reset_addr) { /* 1 = system I/O */
        outb((uint16_t)reset_addr, reset_val);
        return 0;
    }
    return -1;
}
