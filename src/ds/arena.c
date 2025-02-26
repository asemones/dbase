#include "arena.h"
arena *malloc_arena(size_t size) {
    arena *a = (arena *)malloc(sizeof(arena));
    if (!a) return NULL;

    a->data = malloc(size);
    if (!a->data) {
        free(a);
        return NULL;
    }

    a->size = size;
    a->capacity = 0;
    a->next = NULL;
    return a;
}

arena *calloc_arena(size_t size) {
    arena *a = (arena *)malloc(sizeof(arena));
    if (!a) return NULL;

    a->data = calloc(1, size);
    if (!a->data) {
        free(a);
        return NULL;
    }

    a->size = size;
    a->capacity = 0;
    a->next = NULL;
    return a;
}
void reset_arena(void *va) {
    arena * a = (arena*)va;
    memset(a->data, 0, a->size);
    a->capacity = 0;
}

void *arena_alloc(arena *a, size_t size) {
    if (a->capacity + size > a->size) {
        return NULL;
    }
    void *temp = &((char *)a->data)[a->capacity];
    a->capacity += size;
    return temp;
}

void free_arena(void *a) {
    arena *aren = (arena*)a;
    if (aren == NULL) return;
    free(aren->data);
    free(aren);
}
