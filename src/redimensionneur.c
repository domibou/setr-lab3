/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de redimensionnement d'images
 ******************************************************************************/

// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"


int main(int argc, char* argv[]){
    // On desactive le buffering pour les printf(), pour qu'il soit possible de les voir depuis votre ordinateur
	setbuf(stdout, NULL);
    
    // Initialise le profilage
    char signatureProfilage[128] = {0};
    char* nomProgramme = (argv[0][0] == '.') ? argv[0]+2 : argv[0];
    snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme, (unsigned int)getpid());
    InfosProfilage profInfos;
    initProfilage(&profInfos, signatureProfilage);
    
    // Premier evenement de profilage : l'initialisation du programme
    evenementProfilage(&profInfos, ETAT_INITIALISATION);
    
    
    // Écrivez le code permettant de redimensionner une image (en utilisant les fonctions précodées
    // dans utils.c, celles commençant par "resize"). Votre code doit lire une image depuis une zone 
    // mémoire partagée et envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

    char *entree, *sortie;
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;
    unsigned int runtime, deadline, period;
    unsigned int hauteurSortie, largeurSortie;
    int resizeOption;

    if(argc < 2){
        printf("Nombre d'arguments insuffisant\n");
        return -1;
    }

    if (0) {
    // if(strcmp(argv[1], "--debug") == 0){
    //     // Mode debug, vous pouvez changer ces valeurs pour ce qui convient dans vos tests
    //     printf("Mode debug selectionne pour le convertisseur niveau de gris\n");
    //     entree = (char*)"/mem1";
    //     sortie = (char*)"/mem2";
    }
    else{
        int c;
        int deadlineParamIndex = 0;
        char* splitString;

        opterr = 0;
        int filterType;

        while ((c = getopt (argc, argv, "s:d:w:h:r:")) != -1){
            switch (c)
                {
                case 's':
                    if(strcmp(optarg, "NORT") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                    }
                    else if(strcmp(optarg, "RR") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_RR;
                    }
                    else if(strcmp(optarg, "FIFO") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_FIFO;
                    }
                    else if(strcmp(optarg, "DEADLINE") == 0){
                        modeOrdonnanceur = ORDONNANCEMENT_DEADLINE;
                    }
                    else{
                        modeOrdonnanceur = ORDONNANCEMENT_NORT;
                        printf("Mode d'ordonnancement %s non valide, defaut sur NORT\n", optarg);
                    }
                    break;
                case 'd':
                    splitString = strtok(optarg, ",");
                    while (splitString != NULL)
                    {
                        if(deadlineParamIndex == 0){
                            // Runtime
                            runtime = atoi(splitString);
                        }
                        else if(deadlineParamIndex == 1){
                            deadline = atoi(splitString);
                        }
                        else{
                            period = atoi(splitString);
                            break;
                        }
                        deadlineParamIndex++;
                        splitString = strtok(NULL, ",");
                    }
                    break;
                case 'w':
                    largeurSortie = atoi(optarg);
                    if (largeurSortie <= 0) {
                        printf("largeur de sortie invalide: %d\n", largeurSortie);
                        return -1;
                    }
                    break;
                case 'h':
                    hauteurSortie = atoi(optarg);
                    if (hauteurSortie <= 0) {
                        printf("hauteur de sortie invalide: %d\n", hauteurSortie);
                        return -1;
                    }
                    break;
                case 'r':
                    resizeOption = atoi(optarg);
                    if (resizeOption != 0 || resizeOption != 1) {
                        printf("option de redimensionnement invalide: %d\n", resizeOption);
                        return -1;
                    }
                default:
                    continue;
                }
        }

        // Ce qui suit est la description des zones memoires d'entree et de sortie
        if(argc - optind < 2){
            printf("Arguments manquants (fichier_entree flux_sortie)\n");
            return -1;
        }
        entree = argv[optind];
        sortie = argv[optind+1];
    }
    
    struct memPartage* memoireLecture = (struct memPartage*)tempsreel_malloc(sizeof(struct memPartage));
    initMemoirePartageeLecteur(entree, memoireLecture);
    
    struct memPartageHeader* headerInfos = (struct memPartageHeader*)tempsreel_malloc(sizeof(struct memPartageHeader));

    pthread_mutex_lock(&(memoireLecture->header->mutex));
    int hauteur = memoireLecture->header->hauteur;
    int largeur = memoireLecture->header->largeur;
    int canaux = memoireLecture->header->canaux;

    headerInfos->fps = memoireLecture->header->fps;
    headerInfos->frameReader = memoireLecture->header->frameReader;
    headerInfos->frameWriter = memoireLecture->header->frameWriter;

    headerInfos->canaux = canaux;
    headerInfos->hauteur = hauteurSortie;
    headerInfos->largeur = largeurSortie;
    
    headerInfos->mutex = memoireLecture->header->mutex;
    pthread_mutex_unlock(&(memoireLecture->header->mutex));

    unsigned int taille = sizeof(struct memPartageHeader) + (hauteur* largeur * canaux);
    struct memPartage* memoireEcriture = (struct memPartage*)tempsreel_malloc(sizeof(struct memPartage));
    initMemoirePartageeEcrivain(sortie, memoireEcriture, taille, headerInfos);

    unsigned char* image = (unsigned char*)tempsreel_malloc(memoireLecture->tailleDonnees);
    unsigned char* imageRedimensionnee = (unsigned char*)tempsreel_malloc(memoireEcriture->tailleDonnees);

    pthread_mutex_lock(&(memoireLecture->header->mutex));
    ResizeGrid rgNearestNeighbor = resizeNearestNeighborInit(hauteurSortie, largeurSortie, memoireLecture->header->hauteur, memoireLecture->header->largeur);
    ResizeGrid rgBilinear = resizeBilinearInit(hauteurSortie, largeurSortie, memoireLecture->header->hauteur, memoireLecture->header->largeur);
    pthread_mutex_unlock(&(memoireLecture->header->mutex));

    
    while (1) {
        pthread_mutex_lock(&(memoireLecture->header->mutex));
        memoireLecture->header->frameReader++;
        memcpy(image, memoireLecture->data, memoireLecture->tailleDonnees);
        memoireLecture->copieCompteur = memoireLecture->header->frameWriter;
        if (resizeOption == 0) {
            resizeNearestNeighbor(image, memoireLecture->header->hauteur, memoireLecture->header->largeur, imageRedimensionnee, hauteurSortie, largeurSortie, rgNearestNeighbor, canaux);
        }
        else {
            resizeBilinear(image, memoireLecture->header->hauteur, memoireLecture->header->largeur, imageRedimensionnee, hauteurSortie, largeurSortie, rgBilinear, canaux);
        }
        pthread_mutex_unlock(&(memoireLecture->header->mutex));
        attenteLecteur(memoireLecture);

        memcpy(memoireEcriture->data, imageRedimensionnee, memoireEcriture->tailleDonnees);
        memoireEcriture->copieCompteur = memoireEcriture->header->frameReader;
        pthread_mutex_unlock(&(memoireEcriture->header->mutex));
        attenteEcrivain(memoireEcriture);
        pthread_mutex_lock(&(memoireEcriture->header->mutex));
        memoireEcriture->header->frameWriter++;

    }
    
    tempsreel_free(image);
    tempsreel_free(imageRedimensionnee);
    shm_unlink(sortie);
    close(memoireEcriture->fd);
    close(memoireLecture->fd);

    return 0;
    }
