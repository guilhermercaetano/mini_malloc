#include <assert.h>
#include <windows.h>
#include <stdint.h>

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

Block *global_free_list = NULL;

#ifdef __cplusplus
extern "C" {
#endif
Block * allocate_init(size_t size);
void allocate_end(void);
void *mini_malloc(size_t size);
void mini_free(void * ptr);

#ifdef __cplusplus
}
#endif
