/******************************************************************************
 * Laboratoire 3
 * GIF-3004 Systèmes embarqués temps réel
 * Hiver 2025
 * Marc-André Gardner
 * 
 * Fichier implémentant le programme de décodage des fichiers ULV
 ******************************************************************************/


// Gestion des ressources et permissions
#include <sys/resource.h>

// Nécessaire pour pouvoir utiliser sched_setattr et le mode DEADLINE
#include <sched.h>
#include "schedsupp.h"

#include "allocateurMemoire.h"
#include "commMemoirePartagee.h"
#include "utils.h"

#include "jpgd.h"

// Définition de diverses structures pouvant vous être utiles pour la lecture d'un fichier ULV
#define HEADER_SIZE 4
const char header[] = "SETR";

struct videoInfos{
        uint32_t largeur;
        uint32_t hauteur;
        uint32_t canaux;
        uint32_t fps;
};

/******************************************************************************
* FORMAT DU FICHIER VIDEO
* Offset     Taille     Type      Description
* 0          4          char      Header (toujours "SETR" en ASCII)

* 4          4          uint32    Largeur des images du vidéo
* 8          4          uint32    Hauteur des images du vidéo
* 12         4          uint32    Nombre de canaux dans les images
* 16         4          uint32    Nombre d'images par seconde (FPS)


* 20         4          uint32    Taille (en octets) de la première image -> N
* 24         N          char      Contenu de la première image (row-first)

* 24+N       4          uint32    Taille (en octets) de la seconde image -> N2
* 24+N+4     N2         char      Contenu de la seconde image

* 24+N+N2    4          uint32    Taille (en octets) de la troisième image -> N2
* ...                             Toutes les images composant la vidéo, à la suite
*            4          uint32    0 (indique la fin du fichier)
******************************************************************************/

void closeProfilage(InfosProfilage *dataprof) {
    free(dataprof->data);
    fclose(dataprof->fd);
}

void print_memPartageHeader(struct memPartageHeader* mem_header) {
    if (!mem_header) {
        printf("Invalid header pointer\n");
        return;
    }

    printf("=== memPartageHeader ===\n");
    printf("frameWriter: %u\n", mem_header->frameWriter);
    printf("frameReader: %u\n", mem_header->frameReader);
    printf("hauteur: %u\n", mem_header->hauteur);
    printf("largeur: %u\n", mem_header->largeur);
    printf("canaux: %u\n", mem_header->canaux);
    printf("fps: %u\n", mem_header->fps);
}

void printMemPartageContent(struct memPartage* zone) {
    printf("///////////////\n");
    printf("fd: %d\n", zone->fd);
    printf("tailleDonnees: %u\n", zone->tailleDonnees);
    printf("copieCompteur: %u\n", zone->copieCompteur);

    if (zone->header != NULL) {
        print_memPartageHeader(zone->header);
    } else {
        printf("Header is NULL.\n");
    }

    if (zone->data != NULL) {
        printf("\nData (First 10 bytes): ");
        for (size_t i = 0; i < 10 && i < zone->tailleDonnees; ++i) {
            printf("%02x ", zone->data[i]);
        }
        printf("\n");
    } else {
        printf("Data is NULL.\n");
    }
    printf("///////////////\n");
}

void *read_file_mmap(const char *filename, size_t *size) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open failed\n");
        return NULL;
    }

    struct stat sb; // get file size
    if (fstat(fd, &sb) == -1) {
        perror("fstat failed");
        close(fd);
        return NULL;
    }

    *size = sb.st_size;

    // map full file into memory
    void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    close(fd);  // File closed after mmap

    if (data == MAP_FAILED) {
        perror("mmap failed");
        printf("\n");
        return NULL;
    }

    return data;
}

int fill_file_info(char* mmap_file, struct videoInfos* info) {
    if (strncmp(mmap_file, header, HEADER_SIZE) != 0) {
        printf("Invalid file format\n");
        return -1;
    }

    memcpy(info, mmap_file + HEADER_SIZE, sizeof(struct videoInfos));

    return 0;
}

void loop(char* mmap_file) {
    uint32_t *ptr_img_data = (uint32_t *)(mmap_file + HEADER_SIZE + sizeof(struct videoInfos));

    while (1) {
        uint32_t frame_size = *ptr_img_data;
        if (frame_size == 0) {
            ptr_img_data = (uint32_t *)(mmap_file + HEADER_SIZE + sizeof(struct videoInfos));
        }

        // Move pointer to the image data
        ptr_img_data++;

        // Read Image data
        char *image_data = (char *)ptr_img_data;

        // Next size location
        ptr_img_data = (uint32_t *)(image_data + frame_size);
    }
}

void initMemPartageHeader(
    struct memPartageHeader* headerInfos,
    struct videoInfos* videoInfos
) {
    pthread_mutex_init(&headerInfos->mutex, NULL);

    headerInfos->frameWriter = 0;
    headerInfos->frameReader = 0;

    headerInfos->hauteur = videoInfos->hauteur;
    headerInfos->largeur = videoInfos->largeur;
    headerInfos->canaux = videoInfos->canaux;
    headerInfos->fps = videoInfos->fps;
}

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

    // Écrivez le code de décodage et d'envoi sur la zone mémoire partagée ici!
    // N'oubliez pas que vous pouvez utiliser jpgd::decompress_jpeg_image_from_memory()
    // pour décoder une image JPEG contenue dans un buffer!
    // N'oubliez pas également que ce décodeur doit lire les fichiers ULV EN BOUCLE

    char *sortie; // Zones memoires de sortie
    const char* file_name;
    int modeOrdonnanceur = ORDONNANCEMENT_NORT;     // NORT est la valeur par defaut
    unsigned int runtime = 0, deadline = 0, period = 0;

    if(strcmp(argv[1], "--debug") == 0){
        printf("Mode debug selectionne pour le decodeur\n");
        sortie = (char*)"/mem1";
        file_name = "/home/pi/projects/laboratoire3/240p/02_Sintel.ulv";
    } else {
        int c;
        int deadlineParamIndex = 0;
        char* splitString;

        opterr = 0; // disable error messages for getopt

        while ((c = getopt (argc, argv, "s:d:")) != -1){
            switch (c)
                {
                case 's': // mode d'ordonnancement
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
                case 'd': // DEADLINE, parameters
                    splitString = strtok(optarg, ",");
                    while (splitString != NULL)
                    {
                        if(deadlineParamIndex == 0){
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
                default:
                    continue;
                }
        }

        if(argc - optind < 2){
            printf("Arguments manquants (fichier_entree flux_sortie)\n");
            return -1;
        }

        file_name = argv[optind];
        sortie = argv[optind+1];

    }

    printf("Sortie: %s, Mode: %d, Runtime: %u, Deadline: %u, Period: %u\n", sortie, modeOrdonnanceur, runtime, deadline, period);

    size_t size_of_file = 0;
    char *data = (char *)read_file_mmap(file_name, &size_of_file);
    if (data == NULL) {
        printf("error while reading file");
        return -3;
    }

    struct videoInfos video_info = {0, 0, 0, 0};
    if (fill_file_info(data, &video_info) < 0) {
        munmap(data, size_of_file);
        return -2;
    }
    printf("Video Info: %dx%d, Channels: %u, FPS: %u\n", video_info.largeur, video_info.hauteur, video_info.canaux, video_info.fps);

    size_t taille = sizeof(struct memPartageHeader) + video_info.canaux * video_info.largeur * video_info.hauteur;
    struct memPartage *shared_memory = (struct memPartage *)tempsreel_malloc(sizeof(struct memPartage));
    struct memPartageHeader *shared_memory_header = (struct memPartageHeader *)tempsreel_malloc(sizeof(struct memPartageHeader));

    initMemPartageHeader(shared_memory_header, &video_info);
    initMemoirePartageeEcrivain(sortie, shared_memory, taille, shared_memory_header);

    // make shared_memory_header point to the shared memory directly
    tempsreel_free(shared_memory_header);
    shared_memory_header = shared_memory->header;

    // print_memPartageHeader(shared_memory_header);
    // printMemPartageContent(shared_memory);

    pthread_mutex_lock(&shared_memory->header->mutex);
    shared_memory_header->frameWriter++;

    uint32_t *ptr_img_data = (uint32_t *)(data + HEADER_SIZE + sizeof(struct videoInfos));
    int frame_count = 0;
    uint32_t frame_size = *ptr_img_data;

    //// FIRST LOOP

    ptr_img_data++;  // Move pointer to the image data
    printf("Frame %d: Size = %u bytes\n", ++frame_count, frame_size);

    // Image data
    unsigned char *image_data = (unsigned char *)ptr_img_data;

    int width = video_info.largeur, height = video_info.hauteur, actual_comps = video_info.canaux;
    unsigned char* input = jpgd::decompress_jpeg_image_from_memory(image_data, frame_size, &width, &height, &actual_comps, video_info.canaux);

        memcpy(shared_memory->data, input, shared_memory->tailleDonnees);

        shared_memory->copieCompteur = shared_memory_header->frameReader;

    pthread_mutex_unlock(&shared_memory->header->mutex);

    tempsreel_free(input);
    //// FIRST LOOP END

    // Next size location
    ptr_img_data = (uint32_t *)(image_data + frame_size);

    while (1)
    {

        frame_size = *ptr_img_data;
        if (frame_size == 0) {
            printf("god damn it\n");
            ptr_img_data = (uint32_t *)(data + HEADER_SIZE + sizeof(struct videoInfos));
        }

        ptr_img_data++;  // Move pointer to the image data
        // printf("Frame %d: Size = %u bytes\n", ++frame_count, frame_size);
    
        // Image data
        image_data = (unsigned char *)ptr_img_data;
    
        width = video_info.largeur;
        height = video_info.hauteur;
        actual_comps = video_info.canaux;
        // printf("Video Info: %dx%d, Channels: %u\n", width, height, actual_comps);
        input = jpgd::decompress_jpeg_image_from_memory(image_data, frame_size, &width, &height, &actual_comps, video_info.canaux);

        // printf("%u vs %i", shared_memory->tailleDonnees, width * height * actual_comps);
        // enregistreImage(input, video_info.hauteur, video_info.largeur, video_info.canaux, "whatever.ppm");

        while (shared_memory_header->frameReader == shared_memory->copieCompteur)
        {
            sched_yield();
        }
        pthread_mutex_lock(&shared_memory->header->mutex);
            shared_memory_header->frameWriter++;

            memcpy(shared_memory->data, input, shared_memory->tailleDonnees);

            shared_memory->copieCompteur = shared_memory_header->frameReader;

        pthread_mutex_unlock(&shared_memory->header->mutex);

        tempsreel_free(input);
        // Next size location
        ptr_img_data = (uint32_t *)(image_data + frame_size);
    }

    tempsreel_free(shared_memory_header);
    closeMemoirePartageeEcrivain(sortie, shared_memory, taille, shared_memory_header);

    return 0;
}
