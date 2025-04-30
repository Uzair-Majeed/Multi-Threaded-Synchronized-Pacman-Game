#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <SFML/Graphics.h>


typedef struct{
    float x;
    float y;
}Pacman;

#define ROWS 31
#define COLS 28
#define initX 500
#define initY 500
char grid[ROWS][COLS];


enum Movement{
    UP,
    DOWN,
    LEFT,
    RIGHT,

};

void * renderGraphics(void* arg){

    sfRenderWindow* window;
    sfVideoMode mode = {672, 864, 32};

    window = sfRenderWindow_create(mode, "PACMAN", sfResize | sfClose, NULL);

    sfFont* font = sfFont_createFromFile("PAC-FONT.TTF");

    sfText* text = sfText_create();
    sfText_setFont(text,font);
    sfText_setString(text, "SCORE");  
    sfText_setCharacterSize(text,40);
    sfVector2f textPosition = {20.0f, 10.0f}; 
    sfText_setPosition(text, textPosition);
    sfText_setColor(text,sfYellow);

    sfTexture * htexture = sfTexture_createFromFile("heartt.png",NULL);
    sfSprite * hsprite = sfSprite_create();
    sfVector2f hPosition = {600.0f, 10.0f}; 
    sfVector2f hscale = {3.0f,3.0f};
    sfSprite_setScale(hsprite,hscale);
    sfSprite_setPosition(hsprite, hPosition);
    sfSprite_setTexture(hsprite, htexture, sfTrue);

    sfTexture* texture = sfTexture_createFromFile("grid.png", NULL);
    sfSprite* sprite = sfSprite_create();
    sfSprite_setTexture(sprite, texture, sfTrue);

    sfVector2f scale = {2.98f, 3.05f};
    sfVector2f spritePosition = {0.0f, 100.0f};
    sfSprite_setScale(sprite,scale);
    sfSprite_setPosition(sprite, spritePosition);


    while (sfRenderWindow_isOpen(window))
    {
        sfEvent event;
        while (sfRenderWindow_pollEvent(window, &event))
            if (event.type == sfEvtClosed)
                sfRenderWindow_close(window);

        sfRenderWindow_clear(window, sfBlack);
        sfRenderWindow_drawText(window,text,NULL);
        sfRenderWindow_drawSprite(window, hsprite, NULL);
        sfRenderWindow_drawSprite(window, sprite, NULL);
        sfRenderWindow_display(window);
    }

    sfSprite_destroy(hsprite);
    sfSprite_destroy(sprite);
    sfText_destroy(text);
    sfFont_destroy(font);
    sfTexture_destroy(texture);
    sfRenderWindow_destroy(window);

    return NULL;

}


int main() {


    pthread_t GameEngineThread;
    pthread_t UIThread;
    pthread_t GhostThread[4];

    pthread_create(&GameEngineThread,NULL,renderGraphics, NULL);

    pthread_join(GameEngineThread,NULL);


    return 0;
}
