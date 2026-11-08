/*
 * vehicule.h
 * Interface entre la simulation du vehicule (main.c, en C)
 * et l'ecran TouchGFX (Screen1View.cpp, en C++).
 *
 * A placer dans : C:\TouchGFXProjects\MyApplication\TouchGFX\gui\include\gui\vehicule.h
 */
#ifndef VEHICULE_H
#define VEHICULE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Etat du vehicule, tel qu'il est affiche a l'ecran */
typedef struct
{
    int vitesse;      /* km/h                                  */
    int regime;       /* tr/min                                */
    int temperature;  /* degres C                              */
    int tension_dV;   /* dixiemes de volt (126 = 12.6 V)       */
    int phares;       /* 0 = eteints, 1 = allumes (trame 0x500) */
} EtatVehicule;

/* Appelee par l'ecran quand le curseur d'accelerateur bouge (0 a 130 km/h) */
void Vehicule_SetConsigne(int vitesse_kmh);

/* Appelee par l'ecran pour recuperer une copie de l'etat actuel */
void Vehicule_LireEtat(EtatVehicule *e);

/* Boucle de la tache FreeRTOS "vehiculeTask" (ne se termine jamais) */
void Vehicule_Tache(void);

#ifdef __cplusplus
}
#endif

#endif /* VEHICULE_H */
