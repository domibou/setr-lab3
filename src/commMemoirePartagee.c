/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant les fonctions de communication inter-processus
 ******************************************************************************/

#include "commMemoirePartagee.h"

// TODO: implementez ici les fonctions decrites dans commMemoirePartagee.h
int initMemoirePartageeLecteur(const char* identifiant, struct memPartage *zone) {
    // on initialise la zone du LECTEUR, on ne la pas recu du processus ecrivain
    int descripteur = -1;
    while (descripteur < 0) {
        descripteur = shm_open(identifiant, O_RDWR, 0666);
        usleep(DELAI_INIT_READER_USEC);
    }
    
    struct stat fileStat = {0};
    int taille_shm = 0;
    while (taille_shm < 1) {
        fstat(descripteur, &fileStat);
        taille_shm = fileStat.st_size;
        usleep(DELAI_INIT_READER_USEC);
    
    }

    void* ptr = mmap(NULL, taille_shm, PROT_READ | PROT_WRITE, MAP_SHARED, descripteur, 0);

    // REFERENCE de l'entete de L'ECRIVAIN
    struct memPartageHeader* entete =  (struct memPartageHeader*) ptr;

    pthread_mutex_lock(&(entete->mutex));
    while (entete->frameWriter == 0) {
        pthread_mutex_unlock(&(entete->mutex));
        usleep(DELAI_INIT_READER_USEC);
        pthread_mutex_lock(&(entete->mutex));
    }
    pthread_mutex_unlock(&(entete->mutex));

    //unsigned char* data = (unsigned char*)ptr + sizeof(struct memPartageHeader);
    unsigned char* data = (unsigned char *)ptr + sizeof(struct memPartageHeader);

    zone->fd = descripteur;
    zone->header = entete;
    zone->tailleDonnees = taille_shm - sizeof(struct memPartageHeader);
    zone->data = data;

    return 1;
}

// Appelé au début du programme pour l'initialisation de la zone mémoire (cas de l'écrivain)
int initMemoirePartageeEcrivain(const char* identifiant, struct memPartage *zone, size_t taille, struct memPartageHeader* headerInfos) {
    int descripteur = shm_open(identifiant, O_RDWR | O_CREAT, 0666);
    taille = ftruncate(descripteur, taille);   

    void* ptr = mmap(NULL, taille, PROT_READ | PROT_WRITE, MAP_SHARED, descripteur, 0);
    memset(ptr, 0, taille);
    memcpy(ptr, headerInfos, sizeof(struct memPartageHeader));

    struct memPartageHeader* entete = (struct memPartageHeader*) ptr;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setprotocol(&attr, _POSIX_THREAD_PRIO_PROTECT);
    pthread_mutex_init(&(entete->mutex), &attr);
    pthread_mutex_lock(&(entete->mutex));
    
    entete->hauteur = headerInfos->hauteur;
    entete->largeur = headerInfos->largeur;
    entete->canaux = headerInfos->canaux;
    entete->fps = headerInfos->fps;
    entete->frameReader = 0;
    entete->frameWriter = 1;

    unsigned char* data = (unsigned char*)ptr + sizeof(struct memPartageHeader);

    zone->fd = descripteur;
    zone->header = entete;
    zone->tailleDonnees = taille - sizeof(struct memPartageHeader);
    zone->data = data;

    return 1;
}

int attenteLecteur(struct memPartage *zone) {
    while (zone->header->frameReader == 0) {
        usleep(DELAI_WAIT_USEC);
    }
    return 0;
}
int attenteLecteurAsync(struct memPartage *zone) {
    
}

int attenteEcrivain(struct memPartage *zone) {
    while (zone->header->frameReader == zone->copieCompteur) {
        usleep(DELAI_WAIT_USEC);
    }
    return 0;
}