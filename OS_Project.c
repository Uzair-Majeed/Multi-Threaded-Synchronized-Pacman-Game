#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <SFML/Graphics.h>
#include <stdbool.h>
#include <time.h>



#define TILE_SIZE 24
#define ROWS 31
#define COLS 28

char grid[ROWS][COLS + 1] = {
    "############################", // 0
    "#............##..........O.#", // 1
    "#.####.#####.##.#####.####.#", // 2
    "#.####.#####.##.#####.####.#", // 3
    "#.####.#####.##.#####.####.#", // 4
    "#O.........................#", // 5
    "#.####.##.########.##.####.#", // 6
    "#.####.##.########.##.####.#", // 7
    "#......##....##....##......#", // 8
    "######.#####.##.#####.######", // 9
    "     #.#####.##.#####.#     ", // 10
    "     #.##...1......##.#     ", // 11
    "     #.##.###__###.##.#     ", // 12
    "######.##.#      #.##.######", // 13
    "..........#    2 #..........", // 14
    "######.##.# 3  4 #.##.######", // 15
    "     #.##.########.##.#     ", // 16
    "     #.##..........##.#     ", // 17
    "     #.##.########.##.#     ", // 18
    "######.##.########.##.######", // 19
    "#............##..P.........#", // 20
    "#.####.#####.##.#####.####.#", // 21
    "#.####.#####.##.#####.####.#", // 22
    "#...##................##...#", // 23
    "###.##.##.########.##.##.###", // 24
    "###.##.##.########.##.##.###", // 25
    "#......##....##....##...O..#", // 26
    "#.##########.##.##########.#", // 27
    "#.##########.##.##########.#", // 28
    "#.....O....................#", // 29
    "############################"  // 30
};


enum Direction{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE

};

enum GhostType
{
	RED,
	PINK,
	CYAN,
	ORANGE,
};

enum TileType {
    TILE_WALL,
    TILE_EMPTY,
    TILE_PELLET,
    TILE_POWER,
    TILE_GATE
};

enum PacMouth{
    OPEN,
    HALF_OPEN,
    CLOSED,
};


enum TargetState
{
	CHASE,
	CORNER,
	FRIGHTENED,
	GOHOME,		// inital state to pathfind to the square above the door
	HOMEBASE,	// indefinite state going up and down in home
	LEAVEHOME,
	ENTERHOME,
};
enum State
{
	MENU,
	GAMESTART,
	MAINLOOP,
	GAMEWIN,
	GAMELOSE,
	GAMEOVER,
};

typedef struct {
    sfVector2f pos;
    enum Direction direction;
    int lives;
    enum PacMouth mouth;
} Pacman;

typedef struct {
    sfVector2f pos;
    enum Direction direction;
    enum GhostType color; // Optional
    enum TargetState state; // or other states
} Ghost;

typedef struct {
    int score;
    int lives;
    int pelletsRemaining;
    int gameOver;
    Pacman pacman;
    Ghost ghosts[4];
} GameState;


enum UIStateType{
    STATE_MAIN_MENU,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_HIGHSCORE,
    STATE_INSTRUCTIONS
};

typedef struct {
    enum UIStateType currentState;
    bool quit;
    int score;
    int lives;
} UIState;

UIState uiState;

pthread_mutex_t uiMutex = PTHREAD_MUTEX_INITIALIZER;

GameState gameState;

pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;


// Renders the Main Menu screen
void renderMainMenu(sfRenderWindow *window, sfFont *font) {
    sfText *title = sfText_create();
    sfText_setString(title,
        "PAC-MAN \n\n\n\n"
        "Press Enter to Play\n\n\n\n"
        "Press I for Instructions\n\n\n\n"
        "Press H for High Score\n\n\n\n"
        "Press ESC to Exit");
    sfText_setFont(title, font);
    sfText_setColor(title,sfYellow);
    sfText_setCharacterSize(title, 25);
    sfText_setPosition(title, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, title, NULL);
    sfText_destroy(title);
}

// Renders the Pause Menu screen
void renderPauseMenu(sfRenderWindow *window, sfFont *font) {
    sfText *pauseText = sfText_create();
    sfText_setString(pauseText,
        "GAME PAUSED\n\n\n\n"
        "Press Enter to Resume\n\n\n\n"
        "Press M to Go To Main Menu\n\n\n\n"
        "Press ESC to Exit");
    sfText_setFont(pauseText, font);
    sfText_setColor(pauseText,sfYellow);
    sfText_setCharacterSize(pauseText, 25);
    sfText_setPosition(pauseText, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, pauseText, NULL);
    sfText_destroy(pauseText);
}

// Renders the High Score screen
void renderHighScore(sfRenderWindow *window, sfFont *font, int highScore) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
        "HIGH SCORE: %d\\n\n\n\n"
        "Press ENTER to return to Menu", highScore);

    sfText *scoreText = sfText_create();
    sfText_setString(scoreText, buffer);
    sfText_setColor(scoreText,sfYellow);
    sfText_setFont(scoreText, font);
    sfText_setCharacterSize(scoreText, 25);
    sfText_setPosition(scoreText, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, scoreText, NULL);
    sfText_destroy(scoreText);
}

// Renders the Instructions screen
void renderInstructions(sfRenderWindow *window, sfFont *font) {
    sfText *instructions = sfText_create();
    sfText_setString(instructions,
        "Instructions\n\n\n"
        "-Use Arrow Keys to Move\n\n"
        "-Avoid Ghosts\n\n"
        "-Eat all the pellets to win!\n\n\n"
        "Press Enter to Return to Menu\n\n"
        "Press ESC to Exit");
    sfText_setFont(instructions, font);
    sfText_setColor(instructions, sfYellow);
    sfText_setCharacterSize(instructions, 25);
    sfText_setPosition(instructions, (sfVector2f){50.f, 100.f});
    sfRenderWindow_drawText(window, instructions, NULL);
    sfText_destroy(instructions);
}




void renderGraphics(sfRenderWindow * window){


    sfFont* font = sfFont_createFromFile("PAC-FONT.TTF");

    //score
    sfText* score = sfText_create();
    sfText_setFont(score,font);
    sfText_setString(score, "SCORE");  
    sfText_setCharacterSize(score,40);
    sfVector2f textPosition = {20.0f, 10.0f}; 
    sfText_setPosition(score, textPosition);
    sfText_setColor(score,sfYellow);

    //print heart
    sfTexture * htexture = sfTexture_createFromFile("heartt.png",NULL);
    sfSprite * hsprite = sfSprite_create();
    sfVector2f hPosition = {600.0f, 10.0f}; 
    sfVector2f hscale = {3.0f,3.0f};
    sfSprite_setScale(hsprite,hscale);
    sfSprite_setPosition(hsprite, hPosition);
    sfSprite_setTexture(hsprite, htexture, sfTrue);

    //grid
    sfTexture* texture = sfTexture_createFromFile("grid.png", NULL);
    sfSprite* gridSprite = sfSprite_create();
    sfSprite_setTexture(gridSprite, texture, sfTrue);

    sfVector2f scale = {2.98f, 3.05f};
    sfVector2f gridPosition = {0.0f, 100.0f};
    sfSprite_setScale(gridSprite,scale);
    sfSprite_setPosition(gridSprite, gridPosition);

    //display pacman
    sfSprite * pac = sfSprite_create();
    sfTexture * pacTexture;
    if(gameState.pacman.mouth == OPEN)pacTexture = sfTexture_createFromFile("pacman1.png",NULL);
    else if(gameState.pacman.mouth == HALF_OPEN)pacTexture = sfTexture_createFromFile("pacman2.png",NULL);
    else pacTexture = sfTexture_createFromFile("pacman3.png",NULL);
    
    sfSprite_setTexture(pac,pacTexture,sfTrue);
    sfSprite_setPosition(pac,gameState.pacman.pos);
    if(gameState.pacman.direction == UP)sfSprite_setRotation(pac,90.0f);
    else if(gameState.pacman.direction == DOWN)sfSprite_setRotation(pac,-90.0f);
    else if(gameState.pacman.direction == LEFT)sfSprite_setRotation(pac,180.0f);
    else if(gameState.pacman.direction == RIGHT)sfSprite_setRotation(pac,0.0f);
    

    //ghosts
    sfSprite* ghostSpr[4];

    for(int i=0;i<4;i++)ghostSpr[i] = sfSprite_create();
    sfTexture* ghTexture[4];

    if(gameState.ghosts[0].direction == UP)ghTexture[0] = sfTexture_createFromFile("red_ghost_up.png",NULL);
    else if(gameState.ghosts[0].direction == DOWN)ghTexture[0] = sfTexture_createFromFile("red_ghost_down.png",NULL);
    else if(gameState.ghosts[0].direction == LEFT)ghTexture[0] = sfTexture_createFromFile("red_ghost_left.png",NULL);
    else if(gameState.ghosts[0].direction == RIGHT)ghTexture[0] = sfTexture_createFromFile("red_ghost_right.png",NULL);

    if(gameState.ghosts[1].direction == UP)ghTexture[1] = sfTexture_createFromFile("pink_ghost_up.png",NULL);
    else if(gameState.ghosts[1].direction == DOWN)ghTexture[1] = sfTexture_createFromFile("pink_ghost_down.png",NULL);
    else if(gameState.ghosts[1].direction == LEFT)ghTexture[1] = sfTexture_createFromFile("pink_ghost_left.png",NULL);
    else if(gameState.ghosts[1].direction == RIGHT)ghTexture[1] = sfTexture_createFromFile("pink_ghost_right.png",NULL);


    if(gameState.ghosts[2].direction == UP)ghTexture[2] = sfTexture_createFromFile("cyan_ghost_up.png",NULL);
    else if(gameState.ghosts[2].direction == DOWN)ghTexture[2] = sfTexture_createFromFile("cyan_ghost_down.png",NULL);
    else if(gameState.ghosts[2].direction == LEFT)ghTexture[2] = sfTexture_createFromFile("cyan_ghost_left.png",NULL);
    else if(gameState.ghosts[2].direction == RIGHT)ghTexture[2] = sfTexture_createFromFile("cyan_ghost_right.png",NULL);


    if(gameState.ghosts[3].direction == UP)ghTexture[3] = sfTexture_createFromFile("orange_ghost_up.png",NULL);
    else if(gameState.ghosts[3].direction == DOWN)ghTexture[3] = sfTexture_createFromFile("orange_ghost_down.png",NULL);
    else if(gameState.ghosts[3].direction == LEFT)ghTexture[3] = sfTexture_createFromFile("orange_ghost_left.png",NULL);
    else if(gameState.ghosts[3].direction == RIGHT)ghTexture[3] = sfTexture_createFromFile("orange_ghost_right.png",NULL);


    
    for(int i=0;i<4;i++){
        sfVector2f sc = {1.5f,1.5f};
        sfSprite_setScale(ghostSpr[i],sc);
        sfSprite_setTexture(ghostSpr[i],ghTexture[i],sfTrue);
        sfSprite_setPosition(ghostSpr[i],gameState.ghosts[i].pos);
    }


    //pellets
    sfSprite* pelletSpr = sfSprite_create();
    sfTexture* pelletTxt = sfTexture_createFromFile("small_powerup.png",NULL);
    sfSprite_setTexture(pelletSpr,pelletTxt,sfTrue);

    //bigPellet
    sfSprite* bigpelletSpr = sfSprite_create();
    sfTexture* bigpelletTxt = sfTexture_createFromFile("power_up.png",NULL);
    sfSprite_setTexture(bigpelletSpr,bigpelletTxt,sfTrue);

    
        sfEvent event;
        while (sfRenderWindow_pollEvent(window, &event))
            if (event.type == sfEvtClosed)
                sfRenderWindow_close(window);

        sfRenderWindow_clear(window, sfBlack);
        sfRenderWindow_drawText(window,score,NULL);
        sfRenderWindow_drawSprite(window, hsprite, NULL);
        sfRenderWindow_drawSprite(window, gridSprite, NULL);
        sfRenderWindow_drawSprite(window, pac, NULL);

        for(int i=0;i<4;i++){

            sfRenderWindow_drawSprite(window, ghostSpr[i], NULL);
        }

        for(int i=0;i<ROWS;i++){
            for(int j=0;j<COLS;j++){
                if(grid[i][j] == '.'){
    
                    sfVector2f temp = {j * TILE_SIZE + 10,i* TILE_SIZE + 120};
                    sfVector2f scale3 = {1.5f,1.5f};
                    sfSprite_setScale(pelletSpr,scale3);
                    sfSprite_setPosition(pelletSpr,temp);
                    sfRenderWindow_drawSprite(window,pelletSpr,NULL);
    
                }
                if(grid[i][j] == 'O'){
                    sfVector2f temp = {j * TILE_SIZE + 10,i* TILE_SIZE + 110};
                    sfVector2f scale3 = {1.5f,1.5f};
                    sfSprite_setScale(bigpelletSpr,scale3);
                    sfSprite_setPosition(bigpelletSpr,temp);
                    sfRenderWindow_drawSprite(window,bigpelletSpr,NULL);
                }
    
            }
    
        }

        //displayPellet(pelletSpr,window);
        sfRenderWindow_display(window);
    

    for(int i=0;i<4;i++){
        sfSprite_destroy(ghostSpr[i]);
    }
    sfSprite_destroy(pelletSpr);
    sfSprite_destroy(bigpelletSpr);
    sfSprite_destroy(hsprite);
    sfSprite_destroy(pac);
    sfSprite_destroy(gridSprite);
    sfText_destroy(score);
    sfFont_destroy(font);
    sfTexture_destroy(texture);


}
void handleUIState(sfRenderWindow* window, sfFont* font) {
    pthread_mutex_lock(&uiMutex);

    if (uiState.quit) {
        sfRenderWindow_close(window);
        pthread_mutex_unlock(&uiMutex);
        return;
    }

    switch (uiState.currentState) {
        case STATE_MAIN_MENU:
            sfRenderWindow_clear(window, sfBlack);
            renderMainMenu(window, font);
            sfRenderWindow_display(window);
            break;

        case STATE_PAUSED:
            sfRenderWindow_clear(window, sfBlack);
            renderPauseMenu(window, font);
            sfRenderWindow_display(window);
            break;

        case STATE_GAME_OVER:
            sfRenderWindow_clear(window, sfBlack);
            renderHighScore(window, font,uiState.score); 
            sfRenderWindow_display(window);
            break;

        case STATE_HIGHSCORE:
            sfRenderWindow_clear(window, sfBlack);
            renderHighScore(window, font, uiState.score);
            sfRenderWindow_display(window);
            break;

        case STATE_INSTRUCTIONS:
            sfRenderWindow_clear(window, sfBlack);
            renderInstructions(window, font);
            sfRenderWindow_display(window);
            break;

        default:
            break;
    }

    pthread_mutex_unlock(&uiMutex);
}

void handleInput(sfRenderWindow *window) {
    sfEvent event;
    
    while (sfRenderWindow_pollEvent(window, &event)) {
        if (event.type == sfEvtKeyPressed) {
            pthread_mutex_lock(&uiMutex); 

            switch (uiState.currentState) {
                case STATE_MAIN_MENU:
                    if (event.key.code == sfKeyEnter) {
                        uiState.currentState = STATE_RUNNING;
                    } else if (event.key.code == sfKeyEscape) {
                        uiState.quit = true;
                    } else if (event.key.code == sfKeyH) {
                        uiState.currentState = STATE_HIGHSCORE;
                    } else if (event.key.code == sfKeyI) {
                        uiState.currentState = STATE_INSTRUCTIONS;
                    }
                    break;

                case STATE_RUNNING:
                    if (event.key.code == sfKeyEscape) {
                        uiState.currentState = STATE_PAUSED;
                    } else if (event.key.code == sfKeyUp) {
                        if (gameState.pacman.direction != DOWN) {
                            gameState.pacman.direction = UP;
                        }
                    } else if (event.key.code == sfKeyDown) {
                        if (gameState.pacman.direction != UP) {
                            gameState.pacman.direction = DOWN;
                        }
                    } else if (event.key.code == sfKeyLeft) {
                        if (gameState.pacman.direction != RIGHT) {
                            gameState.pacman.direction = LEFT;
                        }
                    } else if (event.key.code == sfKeyRight) {
                        if (gameState.pacman.direction != LEFT) {
                            gameState.pacman.direction = RIGHT;
                        }
                    }
                    break;

                case STATE_PAUSED:
                    if (event.key.code == sfKeyEnter) {
                        uiState.currentState = STATE_RUNNING;
                    }else if (event.key.code == sfKeyM) {
                        uiState.currentState = STATE_MAIN_MENU;
                    } else if (event.key.code == sfKeyEscape) {
                        uiState.quit = true;
                    }
                    break;

                case STATE_GAME_OVER:
                    if (event.key.code == sfKeyEnter) {
                        uiState.currentState = STATE_MAIN_MENU;
                    } else if (event.key.code == sfKeyEscape) {
                        uiState.quit = true;
                    }
                    break;

                case STATE_HIGHSCORE:
                    if (event.key.code == sfKeyEnter) {
                    uiState.currentState = STATE_MAIN_MENU;
                    }
                    break;
                case STATE_INSTRUCTIONS:
                    if (event.key.code == sfKeyEnter) {
                        uiState.currentState = STATE_MAIN_MENU;
                    }
                    break;

                default:
                    break;
            }

            pthread_mutex_unlock(&uiMutex); 
        }
    }
}



void *gameEngineLoop(void *arg) {


    sfRenderWindow* window;

    sfVideoMode mode = {672, 864, 32};
    window = sfRenderWindow_create(mode, "PACMAN", sfResize | sfClose, NULL);


    sfFont* font = sfFont_createFromFile("PAC-FONT.TTF");
    sfClock* clock = sfClock_create();
    float pacmanSpeed = 100.0f;

    // Set initial position of Pacman
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == 'P') {
                sfVector2f temp = {col * TILE_SIZE, row * TILE_SIZE + 100};
                gameState.pacman.pos = temp;
                gameState.pacman.direction = NONE;
                break;
            }
        }
    }

    while (sfRenderWindow_isOpen(window)) {
        handleInput(window);
        handleUIState(window, font);  

        pthread_mutex_lock(&uiMutex);
            bool flag = (uiState.currentState == STATE_RUNNING);
        pthread_mutex_unlock(&uiMutex);

        if(!flag){
            continue;
        }


        sfTime elapsed = sfClock_restart(clock);
        float deltaTime = sfTime_asSeconds(elapsed);

        pthread_mutex_lock(&stateMutex);

        float dx = 0, dy = 0;
        switch (gameState.pacman.direction) {
            case UP:    dy = -pacmanSpeed * deltaTime; break;
            case DOWN:  dy =  pacmanSpeed * deltaTime; break;
            case LEFT:  dx = -pacmanSpeed * deltaTime; break;
            case RIGHT: dx =  pacmanSpeed * deltaTime; break;
            default: break;
        }

        static float mouthTimer = 0.0f;
        const float mouthInterval = 0.25f;
        mouthTimer += deltaTime;

        if (mouthTimer >= mouthInterval) {
            mouthTimer = 0.0f;
            gameState.pacman.mouth = (gameState.pacman.mouth + 1) % 3;
        }

        float newX = gameState.pacman.pos.x + dx;
        float newY = gameState.pacman.pos.y + dy;
        int gridX = (int)(newX) / TILE_SIZE;
        int gridY = ((int)(newY - 100)) / TILE_SIZE;

        if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
            gameState.pacman.pos.x = newX;
            gameState.pacman.pos.y = newY;
        }

        renderGraphics(window);
        pthread_mutex_unlock(&stateMutex);

        usleep(1000000 / 60);  // ~60 FPS
    }

    sfClock_destroy(clock);
    sfFont_destroy(font);
    sfRenderWindow_destroy(window);
    return NULL;
}


void *UIFunction(void *arg) {

    //sfRenderWindow* window = (sfRenderWindow*)arg;
    //sfEvent event;
    //sfBool prevEnter = sfFalse;
    //sfBool prevP = sfFalse;

    

    return NULL;
}


void* ghostLogic(void* arg) {
    int ghostIndex = *(int*)arg;
    Ghost* ghost = &gameState.ghosts[ghostIndex];
    float speed = 75.0f; // pixels per second
    sfClock* clock = sfClock_create();

    while (1) {
        sfTime elapsed = sfClock_restart(clock);
        float deltaTime = sfTime_asSeconds(elapsed);

        pthread_mutex_lock(&stateMutex);

        float dx = 0, dy = 0;
        switch (ghost->direction) {
            case UP: dy = -speed * deltaTime; break;
            case DOWN: dy = speed * deltaTime; break;
            case LEFT: dx = -speed * deltaTime; break;
            case RIGHT: dx = speed * deltaTime; break;
            default: break;
        }

        float newX = ghost->pos.x + dx;
        float newY = ghost->pos.y + dy;
        int gridX = (int)(newX) / TILE_SIZE;
        int gridY = ((int)(newY - 100)) / TILE_SIZE;

        if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
            ghost->pos.x = newX;
            ghost->pos.y = newY;
        } else {
            // Pick a random new direction
            ghost->direction = rand() % 4;
        }

        pthread_mutex_unlock(&stateMutex);

        usleep(1000000 / 60); // 60 FPS
    }

    sfClock_destroy(clock);
    return NULL;
}

void* ghostThread(void* arg) {
    int id = *(int*)arg;
    Ghost* ghost = &gameState.ghosts[id];
    sfClock* clock = sfClock_create();
    float speed = 80.0f; // pixels per second

    while (1) {
        sfTime elapsed = sfClock_restart(clock);
        float dt = sfTime_asSeconds(elapsed);

        pthread_mutex_lock(&stateMutex);

        // Calculate next direction if needed (random for now)
        if (rand() % 100 < 20) { // change direction sometimes
            enum Direction directions[4] = {UP, DOWN, LEFT, RIGHT};
            ghost->direction = directions[rand() % 4];
        }

        // Proposed movement
        float dx = 0, dy = 0;
        switch (ghost->direction) {
            case UP:    dy = -speed * dt; break;
            case DOWN:  dy =  speed * dt; break;
            case LEFT:  dx = -speed * dt; break;
            case RIGHT: dx =  speed * dt; break;
            default: break;
        }

        float newX = ghost->pos.x + dx;
        float newY = ghost->pos.y + dy;
        int gridX = (int)(newX) / TILE_SIZE;
        int gridY = ((int)(newY - 100)) / TILE_SIZE;

        // Stay inside bounds and avoid walls
        if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
            ghost->pos.x = newX;
            ghost->pos.y = newY;
        } else {
            // Hit a wall — choose a new direction
            enum Direction directions[4] = {UP, DOWN, LEFT, RIGHT};
            ghost->direction = directions[rand() % 4];
        }

        pthread_mutex_unlock(&stateMutex);

        usleep(1000000 / 60); 
    }

    sfClock_destroy(clock);
    return NULL;
}



void initializeGame(){


    //sample ghost
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == '1') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 100};
                gameState.ghosts[0].pos = temp;
                gameState.ghosts[0].direction = UP;
            }
            if (grid[row][col] == '2') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 100};
                gameState.ghosts[1].pos = temp;
                gameState.ghosts[1].direction = DOWN;
            }
            if (grid[row][col] == '3') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 100};
                gameState.ghosts[2].pos = temp;
                gameState.ghosts[2].direction = LEFT;
            }
            if (grid[row][col] == '4') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 100};
                gameState.ghosts[3].pos = temp;
                gameState.ghosts[3].direction = RIGHT;
            }
        }
    }

    gameState.pacman.mouth = OPEN;
    uiState.currentState = STATE_MAIN_MENU;

}

int main() {
    pthread_t GameEngineThread;
    pthread_t GhostThread[4];
    pthread_t UIThread;


    initializeGame();

    pthread_create(&GameEngineThread, NULL, gameEngineLoop,NULL);

    
    pthread_create(&UIThread, NULL, UIFunction,NULL);
    srand(time(NULL)); // Seed RNG

    int ghostIndices[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; i++) {
        pthread_create(&GhostThread[i], NULL, ghostLogic, &ghostIndices[i]);
    }

    pthread_join(GameEngineThread, NULL);

    pthread_join(UIThread,NULL);

    for (int i = 0; i < 4; ++i) {
        pthread_cancel(GhostThread[i]); // optionally clean up
    }
    for (int i = 0; i < 4; i++) {
        pthread_join(GhostThread[i], NULL);
    }

    return 0;
}