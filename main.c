#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Define a memory pool structure
struct mem_pool {
  void *memory;              // Pointer to the start of the memory pool
  size_t block_size;         // Size of each memory block
  size_t num_blocks;         // Total number of blocks in the pool
  size_t free_blocks;        // Number of currently free blocks
  ssize_t *allocation_sizes; // Array storing allocation metadata:
                             //   0  => free block
                             //  +N  => head of an allocation using N blocks
                             //  -1  => continuation block
};

// Create a memory pool using user-provided memory
struct mem_pool *pool_init(void *memory, size_t block_size,
                             size_t num_blocks) {
  if (!memory || block_size == 0 || num_blocks == 0) {
    return NULL; // Invalid input
  }

  // Allocate the pool control structure itself
  struct mem_pool *pool = malloc(sizeof(struct mem_pool));
  if (!pool) {
    return NULL; // Allocation failed
  }

  pool->memory = memory;
  pool->block_size = block_size;
  pool->num_blocks = num_blocks;
  pool->free_blocks = num_blocks;

  // Allocate and initialize the array that tracks allocations
  pool->allocation_sizes = malloc(num_blocks * sizeof(ssize_t));
  if (!pool->allocation_sizes) {
    free(pool);
    return NULL; // Allocation failed
  }

  for (size_t i = 0; i < num_blocks; i++) {
    pool->allocation_sizes[i] = 0;
  }

  return pool;
}

// Allocate memory of the requested size from the pool
void *pool_alloc(struct mem_pool *pool, size_t size) {
  if (!pool || size == 0) {
    return NULL; // Invalid request
  }
  // Quick capacity check: if total free blocks * block_size < size, fail early
  if (size > pool->block_size * pool->free_blocks) {
    return NULL; // Not enough total free space
  }

  // Number of blocks needed, rounding up
  size_t num_blocks_needed = (size + pool->block_size - 1) / pool->block_size;

  // Search for a contiguous range of free blocks
  for (size_t i = 0; i + num_blocks_needed <= pool->num_blocks; i++) {
    // Check if block i is free
    if (pool->allocation_sizes[i] != 0) {
      continue; // Not free (either head of allocation or continuation)
    }
    // Check subsequent blocks
    bool can_allocate = true;
    for (size_t j = 0; j < num_blocks_needed; j++) {
      if (pool->allocation_sizes[i + j] != 0) {
        can_allocate = false;
        break;
      }
    }

    // If we can allocate here, mark the blocks
    if (can_allocate) {
      // Mark the head with the positive count
      pool->allocation_sizes[i] = (ssize_t)num_blocks_needed;
      // Mark continuation blocks as -1
      for (size_t k = 1; k < num_blocks_needed; k++) {
        pool->allocation_sizes[i + k] = -1;
      }

      // Decrement the free block count
      pool->free_blocks -= num_blocks_needed;

      // Return the pointer to the allocated memory
      return (char *)pool->memory + i * pool->block_size;
    }
  }

  // No suitable contiguous range found
  return NULL;
}

// Free a block back to the memory pool
void pool_free(struct mem_pool *pool, void *ptr) {
  if (!pool || !ptr) {
    return; // Invalid input
  }

  // Calculate which block index `ptr` corresponds to
  size_t offset = (char *)ptr - (char *)pool->memory;
  if (offset % pool->block_size != 0) {
    return; // Pointer is not aligned to a block boundary
  }

  size_t block_index = offset / pool->block_size;
  if (block_index >= pool->num_blocks) {
    return; // Out-of-bounds
  }

  // Check if this block is the head of an allocation
  ssize_t num_blocks_alloc = pool->allocation_sizes[block_index];
  if (num_blocks_alloc <= 0) {
    // alloc_size == 0 => already free
    // alloc_size < 0  => a continuation block
    return;
  }

  // Mark blocks as free
  if (block_index + num_blocks_alloc > pool->num_blocks) {
    return; // Out-of-bounds, indicates corrupted info or invalid free
  }

  for (size_t i = 0; i < (size_t)num_blocks_alloc; i++) {
    // Only valid if it was previously allocated. We'll just set them to 0.
    pool->allocation_sizes[block_index + i] = 0;
  }

  // Increase free count
  pool->free_blocks += num_blocks_alloc;
}

// Destroy the memory pool and free its resources
void pool_destroy(struct mem_pool *pool) {
  if (!pool) {
    return;
  }

  // The user-provided memory (pool->memory) is not freed here
  // because we do not own it. We only free the tracking array
  // and the pool structure itself.
  free(pool->allocation_sizes);
  free(pool);
}
// Example usage
int main(void) {
  // User-provided memory allocation (static)
	unsigned char memory[320];
  struct mem_pool *pool = pool_init(memory, sizeof(unsigned char), 320);
  if (!pool) {
    printf("Failed to create memory pool.\n");
    return -1;
  }

  void *block1 = pool_alloc(pool, 64); // Allocate 64 bytes
  printf("Allocated block1: %p\n", block1);

  pool_free(pool, block1); // Free the 64 bytes

  pool_destroy(pool);

  // User-provided memory allocation (dynamic)
  unsigned char *memory2 =
      malloc(sizeof(unsigned char) * 320); // Dynamically allocate memory for the pool
  struct mem_pool *pool2 = pool_init(memory2, sizeof(unsigned char) , 320);
  if (!pool2) {
    printf("Failed to create second memory pool.\n");
    free(memory2);
    return -1;
  }

  void *block2 = pool_alloc(pool2, 96); // Allocate 96 bytes
  printf("Allocated block2: %p\n", block2);

  pool_free(pool2, block2); // Free the 96 bytes

  pool_destroy(pool2);
  free(memory2);

  return 0;
}
