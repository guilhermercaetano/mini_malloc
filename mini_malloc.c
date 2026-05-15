#include <assert.h>
#include <windows.h>

// NOTE: inline compatibility with C89
#if __STDC_VERSION__ < 199901L 
#  if defined(_MSC_VER)
#    define inline __inline
#  else
#    define inline
#  endif
#endif

typedef int b32;
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))
#define BLOCK_SIZE ALIGN(sizeof(Block))

static CRITICAL_SECTION lock;

typedef struct Block {
  size_t size;
  b32 free;
  struct Block *next;
} Block;

typedef struct AllocatorStats {
  uint32_t total_free_memory;
  Block *largest_free_block;
  float external_fragmentation;
} AllocatorStats;

static Block *global_free_list = NULL;

AllocatorStats allocator_stats;

#define KILO(size) size * 1024
#define MEGA(size) KILO(size) * 1024

typedef enum {
  BUCKET_16B   = 16, 
  BUCKET_32B   = 32, 
  BUCKET_48B   = 48, 
  BUCKET_64B   = 64, 
  BUCKET_128B  = 128, 
  BUCKET_256B  = 256, 
  BUCKET_512B  = 512,
  BUCKET_1KB   = KILO(1), 
  BUCKET_2KB   = KILO(2), 
  BUCKET_4KB   = KILO(4), 
  BUCKET_8KB   = KILO(8), 
  BUCKET_16KB  = KILO(16), 
  BUCKET_32KB  = KILO(32), 
  BUCKET_64KB  = KILO(64),
  BUCKET_128KB = KILO(128), 
  BUCKET_256KB = KILO(256), 
  BUCKET_512KB = KILO(512), 
  BUCKET_1MB   = MEGA(1), 
  BUCKET_2MB   = MEGA(2), 
  BUCKET_4MB   = MEGA(4)
} Bucket_Sizes;

// NOTE: Since we are allocating always selecting a bucket size 
size_t align_forward(size_t ptr, size_t alignment) {
  size_t modulo = ptr % alignment;

  if (modulo) {
    ptr += alignment - modulo;
  }

  return ptr;
}

inline int find_bucket_to_fit(size_t size) {
  if (size <= BUCKET_16B) return BUCKET_16B;
  if (size <= BUCKET_32B) return BUCKET_32B;
  if (size <= BUCKET_64B) return BUCKET_64B;
  if (size <= BUCKET_128B) return BUCKET_128B;
  if (size <= BUCKET_256B) return BUCKET_256B;
  if (size <= BUCKET_512B) return BUCKET_512B;
  if (size <= BUCKET_1KB) return BUCKET_1KB;
  if (size <= BUCKET_2KB) return BUCKET_2KB;
  if (size <= BUCKET_4KB) return BUCKET_4KB;
  if (size <= BUCKET_8KB) return BUCKET_8KB;
  if (size <= BUCKET_16KB) return BUCKET_16KB;
  if (size <= BUCKET_32KB) return BUCKET_32KB;
  if (size <= BUCKET_64KB) return BUCKET_64KB;
  if (size <= BUCKET_128KB) return BUCKET_128KB;
  if (size <= BUCKET_256KB) return BUCKET_256KB;
  if (size <= BUCKET_512KB) return BUCKET_512KB;
  if (size <= BUCKET_1MB) return BUCKET_1MB;
  if (size <= BUCKET_2MB) return BUCKET_4MB;
  if (size <= BUCKET_4MB) return BUCKET_4MB;
  return -1;
}

void allocate_init(size_t size) {
  InitializeCriticalSection(&lock);

  allocator_stats.total_free_memory = 0;
  allocator_stats.largest_free_block = NULL;

  void *memory_block =
      VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  if (!memory_block) {
    global_free_list = NULL;
    return;
  }

  global_free_list = (Block *)memory_block;
  global_free_list->size = size - sizeof(Block);
  global_free_list->free = TRUE;
  global_free_list->next = NULL;

  allocator_stats.largest_free_block = global_free_list;
  allocator_stats.total_free_memory = global_free_list->size;
  allocator_stats.external_fragmentation = 0;
}

void allocate_end(void) {
  if (global_free_list) {
    VirtualFree(global_free_list, 0, MEM_RELEASE);
    global_free_list = NULL;

    allocator_stats.largest_free_block = NULL;
    allocator_stats.total_free_memory = 0;
    allocator_stats.external_fragmentation = 0;
  }

  DeleteCriticalSection(&lock);
}

void *mini_malloc(size_t size) {
  int size_bucket_fit = find_bucket_to_fit(size);

  if (size_bucket_fit == -1) return 0;

  EnterCriticalSection(&lock);

  size_t aligned_size = size_bucket_fit;

  Block *block_aux = global_free_list;
  for (; block_aux; block_aux = block_aux->next) {
    if (block_aux->free && block_aux->size >= aligned_size) {
      // Split condition
      if (block_aux->size >= aligned_size + sizeof(Block) + ALIGNMENT) {
        size_t old_block_size = block_aux->size;

        block_aux->free = FALSE;
        block_aux->size = aligned_size;

        Block *new_block =
            (Block *)((char *)(block_aux + 1) + BLOCK_SIZE + aligned_size);

        new_block->next = block_aux->next;
        new_block->free = TRUE;

        // Size of block will be always aligned, so this .
        new_block->size = old_block_size - aligned_size - sizeof(Block);

        block_aux->next = new_block;

        // NOTE: if largest free block is now occupied, that means the largest free
        // block was splitted. So it must be updated to reflect the new
        // largest block
        if (block_aux == allocator_stats.largest_free_block) {
          allocator_stats.largest_free_block = new_block;
          allocator_stats.total_free_memory -= (old_block_size - new_block->size);
          allocator_stats.external_fragmentation = 1 - (new_block->size) / allocatot_stats.total_free_memory;
        }

        LeaveCriticalSection(&lock);
        return (void *)(block_aux + 1);
      } else {
        block_aux->free = FALSE;

        LeaveCriticalSection(&lock);
        return (void *)(block_aux + 1);
      }
    }
  }

  LeaveCriticalSection(&lock);
  return NULL;
}

void mini_free(void *ptr) {
  if (!ptr) {
    return;
  }

  EnterCriticalSection(&lock);

  Block *block = (Block *)ptr - 1;
  block->free = TRUE;

  // Merge forward
  if (block->next && block->next->free) {
    block->size = block->size + sizeof(Block) + block->next->size;
    block->next = block->next->next;
  }

  Block *current = global_free_list;

  while (current && current->next != block) {
    current = current->next;
  }

  // Merge backward
  if (current && current->free) {
    current->size = current->size + block->size + sizeof(Block);
    current->next = block->next;
  }

  LeaveCriticalSection(&lock);
}

// New allocations should first fit
void test_two_allocation_reuse_memory() {
  int allocation_size = 4 * 1024 * 1024;
  allocate_init(allocation_size);

  printf("Allocate 100 bytes\n");
  void *p1 = mini_malloc(100);

  printf("Allocate 100 bytes\n");
  void *p2 = mini_malloc(100);

  mini_free(p1);

  void *p3 = mini_malloc(100);

  assert(p1 == p3);

  mini_free(p2);
  mini_free(p3);

  allocate_end();
}

#if RUN_STANDALONE
int main(int argc, char **argv) {
  test_two_allocation_reuse_memory();
  return 0;
}
#endif
