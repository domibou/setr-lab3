/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de l'allocateur mémoire temps réel
 ******************************************************************************/

#include "allocateurMemoire.h"

// TODO: Implementez ici votre allocateur memoire utilisant l'interface decrite dans allocateurMemoire.h
// Prépare les buffers nécessaires pour une allocation correspondante aux tailles
// d'images passées en paramètre. Retourne 0 en cas de succès, et -1 si un
// problème (par exemple manque de mémoire) est survenu.
// int prepareMemoire(size_t tailleImageEntree, size_t tailleImageSortie) {
//     return -1;
// }

// Ces deux fonctions doivent pouvoir s'utiliser exactement comme malloc() et free()
// (dans la limite de la mémoire disponible, bien sûr)
void* tempsreel_malloc(size_t taille) {
    return malloc(taille);
}

void tempsreel_free(void* ptr) {
    free(ptr);
}
