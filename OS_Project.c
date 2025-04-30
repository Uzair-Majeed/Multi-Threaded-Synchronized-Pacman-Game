#include <SFML/Graphics.h>

int main() {
    sfRenderWindow* window;
    sfVideoMode mode = {200, 200, 32};
    sfCircleShape* shape;

    window = sfRenderWindow_create(mode, "CSFML works!", sfResize | sfClose, NULL);
    shape = sfCircleShape_create();
    sfCircleShape_setRadius(shape, 100.f);
    
    sfCircleShape_setFillColor(shape, sfGreen);

    while (sfRenderWindow_isOpen(window)) {
        sfEvent event;
        while (sfRenderWindow_pollEvent(window, &event)) {
            if (event.type == sfEvtClosed)
                sfRenderWindow_close(window);
        }

        sfRenderWindow_clear(window, sfBlack);
        sfRenderWindow_drawCircleShape(window, shape, NULL);
        sfRenderWindow_display(window);
    }

    sfCircleShape_destroy(shape);
    sfRenderWindow_destroy(window);
    return 0;
}
