#include <windows.h>

typedef int b32;
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif

#define ALIGNMENT 16

// Pack struct so it can be aligned correctly
#pragma pack(push, 1)
typedef struct Block {
  size_t size;
  b32 free;
  struct Block *next;

  // Padding to ensure alignment
  char align[ALIGNMENT -
             ((sizeof(size_t) + sizeof(b32) + sizeof(void *)) % ALIGNMENT)];
} Block;
#pragma pack(pop)

static Block *global_free_list = NULL;

size_t align_forward(size_t ptr, size_t alignment) {
  size_t modulo = ptr % alignment;

  if (modulo) {
    ptr += alignment - modulo;
  }

  return ptr;
}

void allocate_init(size_t size) {
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
}

void allocate_end(void) {
  if (global_free_list) {
    VirtualFree(global_free_list, 0, MEM_RELEASE);
    global_free_list = NULL;
  }
}

void *mini_malloc(size_t size) {
  size_t aligned_size = align_forward(size, ALIGNMENT);

  Block *block_aux = global_free_list;
  for (; block_aux; block_aux = block_aux->next) {
    if (block_aux->free && block_aux->size >= aligned_size) {
      // Split condition
      if (block_aux->size >= aligned_size + sizeof(Block) + ALIGNMENT) {
        size_t old_block_size = block_aux->size;

        block_aux->free = FALSE;
        block_aux->size = aligned_size;

        char *block_header_unaligned_address =
            (char *)(block_aux + 1) + aligned_size;
        char *block_header_aligned_address = (char *)align_forward(
            (size_t)block_header_unaligned_address, ALIGNMENT);
        Block *new_block = (Block *)block_header_aligned_address;

        size_t padding =
            block_header_aligned_address - block_header_unaligned_address;

        new_block->next = block_aux->next;
        new_block->free = TRUE;
        new_block->size =
            old_block_size - aligned_size - sizeof(Block) - padding;

        block_aux->next = new_block;

        return (void *)(block_aux + 1);
      } else {
        block_aux->free = FALSE;
        return (void *)(block_aux + 1);
      }
    }
  }

  return NULL;
}

void mini_free(void *ptr) {
  if (!ptr) {
    return;
  }

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
    current->size = current->size + block->size;
    current->next = block->next;
  }
}

#ifdef RUN_STANDALONE
int main(int argc, char **argv) {
  int allocation_size = 4 * 1024 * 1024;
  allocate_init(allocation_size);

  void *p1 = mini_malloc(100);
  void *p2 = mini_malloc(100);

  mini_free(p2);
  mini_free(p1);

  allocate_end();

  return 0;
}
#endif
