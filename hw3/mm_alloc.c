/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef struct heap {
    size_t size;
    int free;
    struct heap* next;
    struct heap* prev;
} heap_t;

heap_t* heap_ptr = NULL;

/*
* Returns a pointer to heap_t from a pointer to data.
*/
heap_t* data_to_heap(void* data_ptr){
    return (heap_t*)((size_t)data_ptr - sizeof(heap_t));
}

/*
* Returns a pointer to data from a pointer to heap_t.
*/
void* heap_to_data(heap_t* h_ptr) {
    return (void*) ((size_t)h_ptr + sizeof(heap_t));
}

/*
* Fragments the block if necessary.
*/
void fragment(heap_t* struct_ptr, size_t request_size) {
    size_t original_size;
    heap_t* new_elem;
    
    if (struct_ptr == NULL) {
        return;
    }

    if (struct_ptr->next == NULL) {
        original_size = sbrk(0) - (size_t) struct_ptr - sizeof(heap_t);
    } else {
        original_size = (size_t) struct_ptr->next - (size_t) struct_ptr - sizeof(heap_t);
    }

    if (original_size - request_size > sizeof(heap_t)) {
        new_elem = (heap_t*) ((size_t) struct_ptr + sizeof(heap_t) + request_size);

        new_elem->prev = struct_ptr;
        new_elem->free = 1;
        
        if (struct_ptr->next != NULL) {
            new_elem->next = struct_ptr->next;
            new_elem->size = (size_t) new_elem->next - (size_t) new_elem - sizeof(heap_t);

            new_elem->next->prev = new_elem;
        } else {
          new_elem->next = NULL;
          new_elem->size = sbrk(0) - (size_t)new_elem - sizeof(heap_t);
        }
        struct_ptr->next = new_elem;
    }
}

/*
* Finds the first fit and returns pointer to the struct heap_t.
*/
heap_t* find_first_fit(size_t request_size) {
    heap_t* iter;
    size_t heap_t_size = sizeof(heap_t);

    for (iter = heap_ptr; iter != NULL; iter = iter->next) {
        if (iter->free && iter->size >= heap_t_size + request_size) {
            fragment(iter, request_size);
            return iter;
        }
    }
    return NULL;
}

/*
* Returns the last element in the list of structs.
*/ heap_t* get_last_elem() {
    heap_t* iter = heap_ptr;

    while (iter != NULL) {
        if (iter->next == NULL) {
            return iter;
        }
        iter = iter->next;
    }

    return NULL;
}

/*
* Adds the entry ENTRY to the Heap Data Structure Linked-list.
*/
void push_back_lst(heap_t* entry) {
    heap_t* last_entry;

    if (heap_ptr == NULL) {
        heap_ptr = entry;
        heap_ptr->prev = NULL;
        heap_ptr->next = NULL;
    } else {
        last_entry = get_last_elem();

        entry->prev = last_entry;
        last_entry->next = entry;
    }
}


void* mm_malloc(size_t size) {
  heap_t* found, * meta_ptr;
  void* data_ptr;

  if (size == 0) {
      return NULL;
  }
  
  found = find_first_fit(size);

  if (found != NULL) {
      found->free = 0;
      found->size = size;
      return heap_to_data(found);
  } else {
      meta_ptr = sbrk(sizeof(heap_t));
      data_ptr = sbrk(size);

      if (meta_ptr == (void*) -1 || data_ptr == (void*) -1) {
          return NULL;
      }
      meta_ptr->free = 0;
      meta_ptr->size = size;
      push_back_lst(meta_ptr);

      memset(data_ptr, 0, size);

      return data_ptr;
  }
}

void* mm_realloc(void* ptr, size_t size) {
    void* new_block = NULL;
    size_t cur_size;

    if (ptr != NULL) {
        if (size == 0) {
            mm_free(ptr);
            return NULL;
        } else {
            cur_size = data_to_heap(ptr)->size;

            if (size > cur_size) {
              new_block = mm_malloc(size);
              if (new_block == NULL) {
                return NULL;
              }

              memcpy(new_block, ptr, data_to_heap(ptr)->size);
              mm_free(ptr);

              return new_block;
            } else if (size == cur_size) {
                return ptr;
            } else {
                fragment(data_to_heap(ptr), size);
                return ptr;
            }
        }
    } else {
        return mm_malloc(size);
    }
}

/* Joins free blocks if necessary. */
void coalesce(heap_t* meta_ptr) {
    heap_t* prev_neighbor, * next_neighbor;

    if (meta_ptr->next != NULL && meta_ptr->next->free) {
        next_neighbor = meta_ptr->next->next;
        meta_ptr->next = next_neighbor;

        if (next_neighbor != NULL) {
            next_neighbor->prev = meta_ptr;
            meta_ptr->size = (size_t) next_neighbor - (size_t) heap_to_data(meta_ptr);
        } else {
            meta_ptr->size = (size_t) sbrk(0) - (size_t) heap_to_data(meta_ptr);
        }
    }
    if (meta_ptr->prev != NULL && meta_ptr->prev->free) {
        prev_neighbor = meta_ptr->prev;
        prev_neighbor->next = meta_ptr->next;

        if (prev_neighbor->next != NULL) {
            prev_neighbor->next->prev = prev_neighbor;
            prev_neighbor->size = (size_t) prev_neighbor->next - (size_t) heap_to_data(prev_neighbor);
        } else {
            prev_neighbor->size = (size_t) sbrk(0) - (size_t) heap_to_data(prev_neighbor);
        }
    }
}

void mm_free(void* ptr) {
    heap_t* meta_ptr;
    
    if (ptr == NULL) {
        return;
    }

    meta_ptr = data_to_heap(ptr);
    meta_ptr->free = 1;

    coalesce(meta_ptr);
}
