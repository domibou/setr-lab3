#!/bin/bash

# Ce script assume :
#   - Qu'il est exécuté dans le même répertoire que les fichiers exécutables
#   - Que les vidéos sont situées dans des dossiers *p (par exemple 160p) dans le même répertoire

sudo rm /dev/shm/mem*           # Supprime les identifiants des zones mémoire partagées des exécutions précédentes
echo "[Script] 07_uneVideoGris"
echo "[Script] Lancement decodeur"
sudo ./decodeur 240p/02_Sintel.ulv /mem1 &

echo "[Script] En attente de création de /mem1"
while [ ! -f /dev/shm/mem1 ]
do
    sleep 0.05
done
echo "[Script] /mem1 créé, lancement convertisseur niveau de gris"
sudo ./convertisseur /mem1 /mem3 &

echo "[Script] En attente de création de /mem3"
while [ ! -f /dev/shm/mem3 ]
do
    sleep 0.05
done
echo "[Script] /mem3 créé, lancement compositeur"
sudo ./compositeur /mem3 &

wait;
