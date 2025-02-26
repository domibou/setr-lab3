/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de l'allocateur mémoire temps réel
 ******************************************************************************/

#include "allocateurMemoire.h"

static unsigned char* memory_pool = NULL;
static size_t pool_size = 0;
static size_t pool_offset = 0;

int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie) {
    size_t max_size = (tailleImageEntree > tailleImageSortie) ? tailleImageEntree : tailleImageSortie;

    if (memory_pool == NULL) {
        // If memory pool is not allocated, allocate it with the calculated size
        pool_size = max_size * ALLOC_N_BIG; // Total pool size (16 times the largest image size)
        memory_pool = (unsigned char*)malloc(pool_size);
        if (memory_pool == NULL) {
            printf("Memory pool allocation failed.\n");
            return -1;  // Return error if memory allocation fails
        }
    }

    return 0; // Return success
}

void* tempsreel_malloc(size_t taille) {
    if (pool_offset + taille <= pool_size) {
        void* ptr = memory_pool + pool_offset;
        pool_offset += taille;  // Move the offset forward after allocation
        return ptr;
    } else {
        printf("Out of memory in pool!\n");
        return NULL;  // Return NULL if there's not enough memory
    }
    // return malloc(taille);
}

void tempsreel_free(void* ptr) {
    free(ptr);
}
