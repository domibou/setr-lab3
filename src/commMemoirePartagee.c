/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"
#include <unistd.h>
#include <sys/types.h>

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain)
int initMemoirePartageeEcrivain(const char* identifiant, struct memPartage *zone, size_t taille, struct memPartageHeader* headerInfos) {
    int descripteur = shm_open(identifiant, O_RDWR | O_CREAT, 0666);
    ftruncate(descripteur, taille);

    unsigned char *shared_mem_ptr = (unsigned char *)mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, descripteur, 0);

    struct memPartageHeader* headerInfosTemp = (struct memPartageHeader*)shared_mem_ptr;

    memcpy(headerInfosTemp, headerInfos, sizeof(struct memPartageHeader));

    zone->fd = descripteur;
    zone->header = headerInfosTemp;
    zone->tailleDonnees = taille - sizeof(struct memPartageHeader);
    zone->data = shared_mem_ptr + sizeof(struct memPartageHeader);
    zone->copieCompteur = 0;
    return descripteur;
}

void closeMemoirePartageeEcrivain(const char* identifiant, struct memPartage *zone, size_t taille, struct memPartageHeader* headerInfos) {

    munmap((void*)zone->data, taille);

    pthread_mutex_destroy(&headerInfos->mutex);
    shm_unlink(identifiant);
}