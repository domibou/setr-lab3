/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de filtrage des images
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
	setbuf(stdout, NULL);
    
    char signatureProfilage[128] = {0};
    char* nomProgramme = (argv[0][0] == '.') ? argv[0]+2 : argv[0];
    snprintf(signatureProfilage, 128, "profilage-%s-%u.txt", nomProgramme, (unsigned int)getpid());
    InfosProfilage profInfos;
    initProfilage(&profInfos, signatureProfilage);
    
    evenementProfilage(&profInfos, ETAT_INITIALISATION);
    
    // Écrivez le code permettant de filtrer une image (en utilisant les fonctions précodées
    // dans utils.c). Votre code doit lire une image depuis une zone mémoire partagée et
    // envoyer le résultat sur une autre zone mémoire partagée.
    // N'oubliez pas de respecter la syntaxe de la ligne de commande présentée dans l'énoncé.

    char *entree, *sortie;
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;
    unsigned int runtime, deadline, period;
    int filterType;

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

        while ((c = getopt (argc, argv, "s:d:f:")) != -1){
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
                case 'f':
                    if(strcmp(optarg, "0") == 0) {
                        filterType = 0;
                    }
                    else if (strcmp(optarg, "1") == 0) {
                        filterType = 1;
                    }
                    else {
                        printf("valeur invalide pour l'argument -f");
                        return -1;
                    }
                    break;
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
    memcpy(headerInfos, memoireLecture, sizeof(struct memPartageHeader));
    unsigned int taille = sizeof(struct memPartageHeader) + (hauteur * largeur * canaux);
    pthread_mutex_unlock(&(memoireLecture->header->mutex));

    struct memPartage* memoireEcriture = (struct memPartage*)tempsreel_malloc(sizeof(struct memPartage));
    initMemoirePartageeEcrivain(sortie, memoireEcriture, taille, headerInfos);

    unsigned char* image = (unsigned char*)tempsreel_malloc(memoireLecture->tailleDonnees);
    unsigned char* imageFiltree = (unsigned char*)tempsreel_malloc(memoireEcriture->tailleDonnees);

    int kernel_size = 3;
    float sigma = 5;
    
    while (1) {
        pthread_mutex_lock(&(memoireLecture->header->mutex));
        memoireLecture->header->frameReader++;
        memcpy(image, memoireLecture->data, memoireLecture->tailleDonnees);
        memoireLecture->copieCompteur = memoireLecture->header->frameWriter;
        if (filterType == 0) {
            lowpassFilter(hauteur, largeur, image, imageFiltree, kernel_size, sigma, canaux);
        }
        else {
            highpassFilter(hauteur, largeur, image, imageFiltree, kernel_size, sigma, canaux);
        }
        pthread_mutex_unlock(&(memoireLecture->header->mutex));
        attenteLecteur(memoireLecture);

        memcpy(memoireEcriture->data, imageFiltree, memoireEcriture->tailleDonnees);
        memoireEcriture->copieCompteur = memoireEcriture->header->frameReader;
        pthread_mutex_unlock(&(memoireEcriture->header->mutex));
        attenteEcrivain(memoireEcriture);
        pthread_mutex_lock(&(memoireEcriture->header->mutex));
        memoireEcriture->header->frameWriter++;

    }
    
    tempsreel_free(image);
    tempsreel_free(imageFiltree);
    shm_unlink(entree);
    shm_unlink(sortie);
    close(memoireEcriture->fd);
    close(memoireLecture->fd);

    return 0;
}
