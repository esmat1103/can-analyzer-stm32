#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <gui/vehicule.h>

#ifdef SIMULATOR
/* -------------------------------------------------------------------
 * Simulateur PC : il n'y a ni CAN ni FreeRTOS, on simule le vehicule ici
 * pour pouvoir tester l'ecran avec le bouton "Run Simulator".
 * Sur la carte, ce sont les fonctions de main.c qui sont utilisees.
 * ------------------------------------------------------------------- */
static int   simConsigne = 0;
static float simVitesse  = 0.0f;

void Vehicule_SetConsigne(int vitesse_kmh)
{
    simConsigne = vitesse_kmh;
}

void Vehicule_LireEtat(EtatVehicule* e)
{
    if (simVitesse < simConsigne)
    {
        simVitesse += 1.5f;
        if (simVitesse > simConsigne) simVitesse = (float)simConsigne;
    }
    else if (simVitesse > simConsigne)
    {
        simVitesse -= 2.0f;
        if (simVitesse < simConsigne) simVitesse = (float)simConsigne;
    }
    e->vitesse     = (int)(simVitesse + 0.5f);
    e->regime      = 800 + (int)(simVitesse * 25.0f);
    e->temperature = 85;
    e->tension_dV  = 141;
    e->phares      = 0;
}
#endif

Screen1View::Screen1View() :
    compteurTicks(0),
    derniereVitesse(-1),
    dernierRegime(-1),
    derniereTemp(-1),
    derniereTension(-1),
    derniersPhares(-1)
{
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    Vehicule_SetConsigne(0);
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

/* Le curseur donne la vitesse souhaitee ; la tache vehicule fait le reste */
void Screen1View::accelerateurChange(int value)
{
    Vehicule_SetConsigne(value);
}

/* Toutes les 6 images (environ 100 ms), on lit l'etat et on met a jour l'ecran */
void Screen1View::handleTickEvent()
{
    compteurTicks++;
    if (compteurTicks < 6)
    {
        return;
    }
    compteurTicks = 0;

    EtatVehicule e;
    Vehicule_LireEtat(&e);

    if (e.vitesse != derniereVitesse)
    {
        Unicode::snprintf(vitesseTextBuffer, VITESSETEXT_SIZE, "%d", e.vitesse);
        vitesseText.invalidate();
        derniereVitesse = e.vitesse;
    }

    if (e.regime != dernierRegime)
    {
        Unicode::snprintf(regimeTextBuffer, REGIMETEXT_SIZE, "%d", e.regime);
        regimeText.invalidate();
        dernierRegime = e.regime;
    }

    if (e.temperature != derniereTemp)
    {
        Unicode::snprintf(tempTextBuffer, TEMPTEXT_SIZE, "%d", e.temperature);
        tempText.invalidate();
        derniereTemp = e.temperature;
    }

    if (e.tension_dV != derniereTension)
    {
        /* 141 dixiemes de volt -> "14.1" */
        Unicode::snprintf(batterieTextBuffer, BATTERIETEXT_SIZE, "%d.%d",
                          e.tension_dV / 10, e.tension_dV % 10);
        batterieText.invalidate();
        derniereTension = e.tension_dV;
    }

    if (e.phares != derniersPhares)
    {
        setPhares(e.phares != 0);
        derniersPhares = e.phares;
    }
}

/* Voyant des phares : orange quand allumes, gris quand eteints */
void Screen1View::setPhares(bool allumes)
{
    colortype couleur = allumes
                        ? touchgfx::Color::getColorFromRGB(245, 158, 11)    /* F59E0B */
                        : touchgfx::Color::getColorFromRGB(203, 213, 225);  /* CBD5E1 */

    voyantPhares.setColor(couleur);
    voyantPhares.setBorderColor(couleur);
    voyantPhares.invalidate();
}
