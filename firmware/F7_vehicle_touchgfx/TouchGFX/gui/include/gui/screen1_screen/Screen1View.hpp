#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    /* Appelee par TouchGFX quand le curseur bouge (interaction du Designer) */
    virtual void accelerateurChange(int value);

    /* Appelee par TouchGFX a chaque image (environ 60 fois par seconde) */
    virtual void handleTickEvent();

protected:
    void setPhares(bool allumes);

    int compteurTicks;

    /* Dernieres valeurs affichees : on ne redessine que ce qui change */
    int derniereVitesse;
    int dernierRegime;
    int derniereTemp;
    int derniereTension;
    int derniersPhares;
};

#endif // SCREEN1VIEW_HPP
