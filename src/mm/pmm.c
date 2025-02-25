#include <boot/stivale2.h>
#include <mm/pmm.h>
#include <printf.h>
#include <stddef.h>
#include <string.h>

// Bit functions. Provided by AtieP
#define ALIGN_UP(__number) (((__number) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define ALIGN_DOWN(__number) ((__number) & ~(PAGE_SIZE - 1))

#define BIT_SET(__bit) (bitmap[(__bit) / 8] |= (1 << ((__bit) % 8)))
#define BIT_CLEAR(__bit) (bitmap[(__bit) / 8] &= ~(1 << ((__bit) % 8)))
#define BIT_TEST(__bit) ((bitmap[(__bit) / 8] >> ((__bit) % 8)) & 1)

// Size of bitmap
size_t bitmap_size;
// Highest page
static uintptr_t highest_page = 0;
// The bitmap itself
uint8_t *bitmap;

// Free page at adress
void free_page(void *adr) {
  uint32_t index = (uint32_t)(uintptr_t)adr / PAGE_SIZE;
  BIT_CLEAR(index);
}

// Reserve page at adress
void reserve_page(void *adr) {
  uint32_t index = (uint32_t)(uintptr_t)adr / PAGE_SIZE;
  BIT_SET(index);
}

// Free x amount of pages at adress
void free_pages(void *adr, uint32_t page_count) {
  for (uint32_t i = 0; i < page_count; i++) {
    free_page((void *)(adr + (i * PAGE_SIZE)));
  }
}

// Reserve x amount of pages at adress
void reserve_pages(void *adr, uint32_t page_count) {
  for (uint32_t i = 0; i < page_count; i++) {
    reserve_page((void *)(adr + (i * PAGE_SIZE)));
  }
}

// Allocate x amount of pages
void *pmalloc(uint32_t pages) {
  for (uint32_t i = 0; i < bitmap_size * 8; i++) {
    for (uint32_t j = 0; j < pages; j++) {
      if (BIT_TEST(i))
        break;
      else if (!BIT_TEST(i) && j == pages - 1) {
        reserve_pages((void *)(uintptr_t)(i * PAGE_SIZE), pages);
        return (void *)(uintptr_t)(i * PAGE_SIZE);
      }
    }
  }
  return NULL;
}

// Allocate x amount of pages. Filled with 0's
void *pcalloc(uint64_t pages) {
  uint64_t *p = (uint64_t *)pmalloc(pages);

  for (uint64_t i = 0; i < (pages * PAGE_SIZE); i++)
    p[i] = 0;
  return (void *)p;
}

// Init physical memory management
int init_pmm(struct stivale2_mmap_entry_t *memory_map, size_t memory_entries) {
  uintptr_t top;

  for (int i = 0; (size_t)i < memory_entries; i++) {
    struct stivale2_mmap_entry_t entry = memory_map[i];

    if (entry.type != STIVALE2_MMAP_USABLE &&
        entry.type != STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE &&
        entry.type != STIVALE2_MMAP_KERNEL_AND_MODULES)
      continue;

    top = entry.base + entry.length;

    if (top > highest_page)
      highest_page = top;
  }

  bitmap_size = ALIGN_DOWN(highest_page) / PAGE_SIZE / 8;

  for (int i = 0; (size_t)i < memory_entries; i++) {
    struct stivale2_mmap_entry_t entry = memory_map[i];

    if (entry.type != STIVALE2_MMAP_USABLE)
      continue;

    if (entry.length >= bitmap_size) {
      bitmap = (uint8_t *)entry.base;
      entry.base += bitmap_size;
      entry.length -= bitmap_size;
      break;
    }
  }

  for (int i = 0; (size_t)i < memory_entries; i++) {
    if (memory_map[i].type != STIVALE2_MMAP_USABLE)
      reserve_pages((void *)memory_map[i].base,
                    memory_map[i].length / PAGE_SIZE);
    else
      free_pages((void *)memory_map[i].base, memory_map[i].length / PAGE_SIZE);
  }

  return 0;
}
