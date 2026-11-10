/*
 * Kernel entry. Limine has already put the CPU in 64-bit mode, mapped the
 * kernel at 0xffffffff80000000, mapped all physical memory at the HHDM
 * offset, given us a 64 KiB stack, and filled in the requests below.
 */
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include "cpu.h"
#include "kernel.h"
#include "lib/string.h"
#include "serial.h"
#include "test.h"

/*
 * Requests: Limine finds them between the start and end markers, which the
 * linker script keeps together in one writable segment.
 */
#define REQUEST [[gnu::used, gnu::section(".limine_requests")]] static volatile

[[gnu::used, gnu::section(".limine_requests_start")]]
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;

REQUEST uint64_t base_revision[] = LIMINE_BASE_REVISION(6);

REQUEST struct limine_bootloader_info_request info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0, .response = nullptr,
};
REQUEST struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID, .revision = 0, .response = nullptr,
};
REQUEST struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0, .response = nullptr,
};
REQUEST struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0, .response = nullptr,
};
REQUEST struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID, .revision = 0, .response = nullptr, .flags = 0,
};

[[gnu::used, gnu::section(".limine_requests_end")]]
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

static const char *memmap_type_name(uint64_t type)
{
    static const char *const names[LIMINE_MEMMAP_RESERVED_MAPPED + 1] = {
        [LIMINE_MEMMAP_USABLE] = "usable",
        [LIMINE_MEMMAP_RESERVED] = "reserved",
        [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = "ACPI reclaimable",
        [LIMINE_MEMMAP_ACPI_NVS] = "ACPI NVS",
        [LIMINE_MEMMAP_BAD_MEMORY] = "bad",
        [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = "bootloader reclaimable",
        [LIMINE_MEMMAP_EXECUTABLE_AND_MODULES] = "kernel and modules",
        [LIMINE_MEMMAP_FRAMEBUFFER] = "framebuffer",
        [LIMINE_MEMMAP_RESERVED_MAPPED] = "reserved (mapped)",
    };
    constexpr size_t count = (sizeof names) / (sizeof names[0]);

    return (type < count) ? names[type] : "unknown";
}

static void print_memmap(const struct limine_memmap_response *mm)
{
    uint64_t usable = 0;

    kprintf("xm3: memory map (%lu entries):\n", mm->entry_count);
    for (uint64_t i = 0; i < mm->entry_count; i++) {
        const struct limine_memmap_entry *e = mm->entries[i];
        kprintf("  %016lx-%016lx  %s\n", e->base, e->base + e->length,
                memmap_type_name(e->type));
        if (e->type == (uint64_t)LIMINE_MEMMAP_USABLE) {
            usable += e->length;
        }
    }
    kprintf("xm3: %lu MiB usable\n", usable >> 20);
}

bool cmdline_has(const char *cmdline, const char *word)
{
    size_t n = strlen(word);

    const char *p = cmdline;

    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        size_t len = 0;
        while ((p[len] != ' ') && (p[len] != '\0')) {
            len++;
        }
        if ((len == n) && (len > 0U) && (strncmp(p, word, n) == 0)) {
            return true;
        }
        p = &p[len];
    }
    return false;
}

[[noreturn]] void kmain(void);

void kmain(void)
{
    serial_init();
    kprintf("xm3: hello from CPU 0\n");

    /* LIMINE_BASE_REVISION_SUPPORTED: Limine zeroes the third word if it
     * honoured the requested revision. */
    if (base_revision[2] != 0U) {
        panic("bootloader does not support Limine base revision 6");
    }
    if ((memmap_request.response == nullptr) || (hhdm_request.response == nullptr)) {
        panic("bootloader did not provide a memory map or HHDM");
    }

    const struct limine_bootloader_info_response *info = info_request.response;
    if (info != nullptr) {
        kprintf("xm3: booted by %s %s\n", info->name, info->version);
    }
    const char *cmdline = "";
    if ((cmdline_request.response != nullptr) && (cmdline_request.response->cmdline != nullptr)) {
        cmdline = cmdline_request.response->cmdline;
    }
    kprintf("xm3: command line \"%s\"\n", cmdline);
    kprintf("xm3: physical memory mapped at %p (HHDM)\n",
            (void *)(uintptr_t)hhdm_request.response->offset);
    print_memmap(memmap_request.response);
    if (mp_request.response != nullptr) {
        kprintf("xm3: %lu CPUs (only CPU 0 is running)\n", mp_request.response->cpu_count);
    }

    if (cmdline_has(cmdline, "test")) {
        test_run(cmdline);
    }
    kprintf("xm3: boot complete, halting\n");
    cpu_halt_forever();
}
