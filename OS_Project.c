#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <SFML/Graphics.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <limits.h>
//void* GhostThreadFunction(int i);

#define TILE_SIZE 24
#define ROWS 32
#define COLS 29

#define MAX_QUEUE  (ROWS * COLS) //for queue size in BFS algorithm

// A small struct for BFS queue
typedef struct {
    int r;
    int c;
} Node;
int ghost1x = 12;
int ghost2x = 16;
int ghost3x = 12;
int ghost4x = 16;
int ghost1y = 14;
int ghost2y = 14;
int ghost3y = 15;
int ghost4y = 15;
char grid[ROWS][COLS] = {
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
    "     #.##..........##.#     ", // 11
    "     #.##.###__###.##.#     ", // 12
    "######.##.#      #.##.######", // 13
    "..........# 1  2 #..........", // 14
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
    bool hasExited;
    bool hasKey;
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


void renderScore(){

}

void renderHeart(){

}

void renderGrid(){

}

void renderPacman(){

}

void renderGhosts(){

}

void renderPellets(){
    
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
    switch (gameState.pacman.direction) {
        case UP:
            sfSprite_setRotation(pac, 270.0f); // up is 270°
            break;
        case DOWN:
            sfSprite_setRotation(pac, 90.0f);  // down is 90°
            break;
        case LEFT:
            sfSprite_setRotation(pac, 180.0f); // left is 180°
            break;
        case RIGHT:
            sfSprite_setRotation(pac, 0.0f);   // right is 0°
            break;
        default:
            // Optionally handle invalid direction
            break;
    }
    

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
                    sfVector2f temp = {j * TILE_SIZE + 10,i* TILE_SIZE + 90};
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

void initPacman(){


    // Set initial position of Pacman
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == 'P') {
                sfVector2f temp = {col * TILE_SIZE , row * TILE_SIZE + 120};
                gameState.pacman.pos = temp;
                gameState.pacman.direction = NONE;
                break;
            }
        }
    }

}

void movePacman(float pacmanSpeed,float deltaTime){

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
        switch(gameState.pacman.mouth) {
            case OPEN:      gameState.pacman.mouth = HALF_OPEN; break;
            case HALF_OPEN: gameState.pacman.mouth = CLOSED;   break;
            case CLOSED:    gameState.pacman.mouth = OPEN;     break;
        }

    }

    float newX = gameState.pacman.pos.x + dx;
    float newY = gameState.pacman.pos.y + dy;
    int gridX = (int)(newX) / TILE_SIZE;
    int gridY = ((int)(newY - 120)) / TILE_SIZE;

    if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
        gameState.pacman.pos.x = newX;
        gameState.pacman.pos.y = newY;
    }
}


void *gameEngineLoop(void *arg) {


    sfRenderWindow* window;

    sfVideoMode mode = {672, 864, 32};
    window = sfRenderWindow_create(mode, "PACMAN", sfResize | sfClose, NULL);


    sfFont* font = sfFont_createFromFile("PAC-FONT.TTF");
    sfClock* clock = sfClock_create();
    float pacmanSpeed = 100.0f;

    

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

        movePacman(pacmanSpeed,deltaTime);

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


/*
void* GhostThread_func(void* arg) {
    int ghostIndex = *(int*)arg;
    Ghost* ghost = &gameState.ghosts[ghostIndex];
    float speed = 75.0f; // pixels per second
    sfClock* clock = sfClock_create();

    while (1) {
        float deltaTime = sfTime_asSeconds(sfClock_restart(clock));

        // Step 1: Ensure ghost exits house only once
        if (!ghost->hasExited) {
            printf("Ghost %d: waiting for key...\n", ghostIndex);
            sem_wait(&keySemaphore);
            printf("Ghost %d: acquired key.\n", ghostIndex);
        
            printf("Ghost %d: waiting for exit permit...\n", ghostIndex);
            sem_wait(&exitPermitSemaphore);
            printf("Ghost %d: acquired exit permit.\n", ghostIndex);
        
            pthread_mutex_lock(&resourceMutex);
            ghost->hasExited = 1;
            ghost->pos.y -= TILE_SIZE * 2;
            
            printf("Ghost %d: exited ghost house at (%.1f, %.1f).\n", ghostIndex, ghost->pos.x, ghost->pos.y);
            sem_post(&keySemaphore);          // release key
            sem_post(&exitPermitSemaphore); 
            pthread_mutex_unlock(&resourceMutex);
        }
        

        if (ghost->hasExited) {
            float dx = 0, dy = 0;
            switch (ghost->direction) {
                case UP: dy = -speed * deltaTime; break;
                case DOWN: dy = speed * deltaTime; break;
                case LEFT: dx = -speed * deltaTime; break;
                case RIGHT: dx = speed * deltaTime; break;
            }
        
            float newX = ghost->pos.x + dx;
            float newY = ghost->pos.y + dy;
        
            int gridX = (int)(newX) / TILE_SIZE;
            int gridY = ((int)(newY - 100)) / TILE_SIZE;
        
            if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
                ghost->pos.x = newX;
                ghost->pos.y = newY;
            } else {
                ghost->direction = rand() % 4;
            }
        }

        usleep(1000000 / 60); // ~60 FPS
    }
    printf("Ghost %d position: (%.2f, %.2f)\n", ghostIndex, ghost->pos.x, ghost->pos.y);

    sfClock_destroy(clock);
    return NULL;
}
*/
// Direction offsets: UP, DOWN, LEFT, RIGHT
static const int rowOffset[4] = { -1, 1,  0, 0 };
static const int colOffset[4] = {  0, 0, -1, 1 };

sem_t keySemaphore;
sem_t exitPermitSemaphore;
sem_t mutex2;
pthread_mutex_t resourceMutex = PTHREAD_MUTEX_INITIALIZER;
//variables for ghost deaths
bool die[4]={false,false,false,false};
bool ghostkey[4]={false,false,false,false};
sfVector2f pos1,pos2,pos3,pos4;
// Helper to find entity (like ghost '1', '2'... or 'P' for Pac-Man) in the grid

Node findEntityInGrid(char target) {
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == target) {
                return (Node){row, col};
            }
        }
    }
    return (Node){-1, -1}; // Entity not found
}

//ghost death function
// void ghostdeath(int i){

//     if(gameState.ghosts[i].pos.x==gameState.pacman.pos.x && gameState.ghosts[i].pos.y==gameState.pacman.pos.y){
//        if(i==0){
//         gameState.ghosts[i].pos=pos1;
//          die[i]=true;  
//         }
//         if(i==1){
//             gameState.ghosts[i].pos=pos2; 
//          die[i]=true;        
//         }
//        else if(i==2){
//         gameState.ghosts[i].pos=pos3;
//          die[i]=true;   
//        }
//        else if(i==3){
//         gameState.ghosts[i].pos=pos4;
//          die[i]=true;   
//        }  
//     }
// }

// //getting out of cage functions
// void cagefree(int i){
//     sem_wait(&mutex2); 
//     sem_wait(&keySemaphore);
//     sem_wait(&exitPermitSemaphore);
//     printf("waiting for resources : %d",i);
//     ghostkey[i] = true; 
//     printf("Got resources : %d",i);
//     sem_post(&mutex2); 
    
    
//     if (die[i]) {
//         sem_wait(&mutex2); 
//         ghostkey[i] = false; 
//         die[i]=false;
//         sem_post(&mutex2); 
//     }     
// }
// bool start=true;

// //main thread functions
// void* GhostThread4(void* arg) {
//     int ghostIndex = *(int*)arg;
//     Ghost* ghost = &gameState.ghosts[ghostIndex];
//     sfClock* clock = sfClock_create();
//     float speed = 75.0f;
//         while (true) { 
//         if(start){}
//             if (die[3]) {
//               ghostkey[3] = false; 
//             }
//             if(ghostkey[3]==false){
//                cagefree(3);
//                if(ghostkey[3]==true){
//                 ghost->pos = (sfVector2f){13 * TILE_SIZE,11 * TILE_SIZE + 90};
//                }
//                }
//             else{
//                 GhostThreadFunction(3);
//                 ghostdeath(0);
//             }  
    
//         }
//         pthread_exit(NULL); 
//         return NULL;    
// }    

void* GhostThreadFunction(void* arg) {
    int ghostIndex = *(int*)arg;
    Ghost* ghost = &gameState.ghosts[ghostIndex];
    sfClock* clock = sfClock_create();
    float speed = 75.0f;
    
    // For path recalculation throttling
    sfClock* pathClock = sfClock_create();
    float pathUpdateInterval = 0.5f; // Update path twice per second
    
    // Each ghost gets slightly different speed for variety
    speed += (ghostIndex * 5); 

    while (1) {
        float deltaTime = sfTime_asSeconds(sfClock_restart(clock));
        
        // --- Ghost House Exit Logic ---
        if (!ghost->hasExited) {
            pthread_mutex_lock(&stateMutex);
            ghost->hasExited = true;
            ghost->pos = (sfVector2f){13 * TILE_SIZE, 11 * TILE_SIZE + 90};
            pthread_mutex_unlock(&stateMutex);
            usleep(ghostIndex * 500000); // Stagger ghost exits
            continue;
        }

        // --- Pathfinding Update (Throttled) ---
        bool shouldUpdatePath = sfTime_asSeconds(sfClock_getElapsedTime(pathClock)) > pathUpdateInterval;
        
        if (shouldUpdatePath) {
            sfClock_restart(pathClock);
            
            pthread_mutex_lock(&stateMutex);
            
            // Get current positions in grid coordinates
            Node ghostPos = {
                (int)((ghost->pos.y - 90) / TILE_SIZE),
                (int)(ghost->pos.x / TILE_SIZE)
            };
            
            Node pacmanPos = {
                (int)((gameState.pacman.pos.y - 120) / TILE_SIZE),
                (int)(gameState.pacman.pos.x / TILE_SIZE)
            };
            
            // Ghost-specific targeting (classic Pac-Man behavior)
            switch (ghostIndex) {
                case 0: // Red - Direct chase
                    break;
                case 1: // Pink - Targets 4 tiles ahead of Pac-Man's direction
                    switch (gameState.pacman.direction) {
                        case UP:    pacmanPos.r -= 4; break;
                        case DOWN:  pacmanPos.r += 4; break;
                        case LEFT:  pacmanPos.c -= 4; break;
                        case RIGHT: pacmanPos.c += 4; break;
                    }
                    break;
                case 2: // Cyan - Hybrid targeting
                    {
                        Node redGhostPos = {
                            (int)((gameState.ghosts[0].pos.y - 90) / TILE_SIZE),
                            (int)(gameState.ghosts[0].pos.x / TILE_SIZE)
                        };
                        // Vector from Red ghost to 2 tiles ahead of Pac-Man
                        pacmanPos.r = 2 * pacmanPos.r - redGhostPos.r;
                        pacmanPos.c = 2 * pacmanPos.c - redGhostPos.c;
                    }
                    break;
                case 3: // Orange - Chase until close, then scatter
                    {
                        float dr = ghostPos.r - pacmanPos.r;
                        float dc = ghostPos.c - pacmanPos.c;
                        float distance = sqrtf(dr*dr + dc*dc);  // More efficient than powf()
                        if (distance < 8) { // If close to Pac-Man
                            pacmanPos.r = 0;  // Target corner
                            pacmanPos.c = 0;
                        }
                    }
                    break;
            }
            
            // Boundary checks
            pacmanPos.r = (pacmanPos.r < 0) ? 0 : (pacmanPos.r >= ROWS) ? ROWS-1 : pacmanPos.r;
            pacmanPos.c = (pacmanPos.c < 0) ? 0 : (pacmanPos.c >= COLS) ? COLS-1 : pacmanPos.c;

            // BFS initialization
            int dist[ROWS][COLS];
            Node prev[ROWS][COLS];
            for (int r = 0; r < ROWS; r++) {
                for (int c = 0; c < COLS; c++) {
                    dist[r][c] = INT_MAX;
                }
            }
            
            // BFS queue
            Node queue[MAX_QUEUE];
            int front = 0, rear = 0;
            
            dist[ghostPos.r][ghostPos.c] = 0;
            queue[rear++] = ghostPos;
            
            // --- BFS Execution ---
            while (front < rear) {
                Node current = queue[front++];
                
                // Early exit if we reach Pac-Man
                if (current.r == pacmanPos.r && current.c == pacmanPos.c) break;
                
                // Explore neighbors
                for (int i = 0; i < 4; i++) {
                    int newR = current.r + rowOffset[i];
                    int newC = current.c + colOffset[i];
                    
                    // Skip invalid or wall tiles
                    if (newR < 0 || newR >= ROWS || newC < 0 || newC >= COLS || 
                        grid[newR][newC] == '#') {
                        continue;
                    }
                    
                    // Update if found shorter path
                    if (dist[newR][newC] > dist[current.r][current.c] + 1) {
                        dist[newR][newC] = dist[current.r][current.c] + 1;
                        prev[newR][newC] = current;
                        queue[rear++] = (Node){newR, newC};
                    }
                }
            }
            
            // --- Determine Next Direction ---
            if (dist[pacmanPos.r][pacmanPos.c] < INT_MAX) {
                // Backtrack to find first move
                Node step = pacmanPos;
                while (prev[step.r][step.c].r != ghostPos.r || 
                       prev[step.r][step.c].c != ghostPos.c) {
                    step = prev[step.r][step.c];
                }
                
                // Set direction based on movement needed
                if (step.r < ghostPos.r) ghost->direction = UP;
                else if (step.r > ghostPos.r) ghost->direction = DOWN;
                else if (step.c < ghostPos.c) ghost->direction = LEFT;
                else if (step.c > ghostPos.c) ghost->direction = RIGHT;
            }
            
            pthread_mutex_unlock(&stateMutex);
        }
        
        // --- Movement Execution ---
        float dx = 0, dy = 0;
        switch (ghost->direction) {
            case UP:    dy = -speed * deltaTime; break;
            case DOWN:  dy =  speed * deltaTime; break;
            case LEFT:  dx = -speed * deltaTime; break;
            case RIGHT: dx =  speed * deltaTime; break;
        }
        
        float newX = ghost->pos.x + dx;
        float newY = ghost->pos.y + dy;
        
        // Convert new position to grid coordinates for collision check
        int newR = (int)((newY - 90) / TILE_SIZE);
        int newC = (int)(newX / TILE_SIZE);
        
        // Check if new position is valid
        if (newR >= 0 && newR < ROWS && newC >= 0 && newC < COLS && 
            grid[newR][newC] != '#') {
            pthread_mutex_lock(&stateMutex);
            ghost->pos.x = newX;
            ghost->pos.y = newY;
            pthread_mutex_unlock(&stateMutex);
        } else {
            // Hit a wall - try to find a new valid direction
            enum Direction validDirs[4];
            int validCount = 0;
            
            // Check all possible directions
            for (int dirInt = UP; dirInt <= RIGHT; dirInt++) {
                enum Direction dir = (enum Direction)dirInt;
                int testR = (int)((ghost->pos.y - 90) / TILE_SIZE) + rowOffset[dir];
                int testC = (int)(ghost->pos.x / TILE_SIZE + colOffset[dir]);
                
                if (testR >= 0 && testR < ROWS && testC >= 0 && testC < COLS && 
                    grid[testR][testC] != '#') {
                    validDirs[validCount++] = dir;
                }
            }
            
            // Choose random valid direction if stuck
            if (validCount > 0) {
                ghost->direction = validDirs[rand() % validCount];
            }
        }
        
        usleep(1000000 / 60); // ~60 FPS
    }
    
    sfClock_destroy(clock);
    sfClock_destroy(pathClock);
    return NULL;
}


void initializeGame(){

    sem_init(&keySemaphore,0,2);
    sem_init(&mutex2, 0, 1);
    sem_init(&exitPermitSemaphore,0,2);

    pthread_mutex_init(&resourceMutex,NULL);


    //sample ghost
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == '1') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[0].pos = temp;
                pos1=temp;
                gameState.ghosts[0].direction = UP;
                gameState.ghosts[0].hasExited = false;
            }
            if (grid[row][col] == '2') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[1].pos = temp;
                pos2=temp;
                gameState.ghosts[1].direction = DOWN;
                gameState.ghosts[1].hasExited = false;
            }
            if (grid[row][col] == '3') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[2].pos = temp;
                pos3=temp;
                gameState.ghosts[2].direction = LEFT;
                gameState.ghosts[2].hasExited = false;
            }
            if (grid[row][col] == '4') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[3].pos = temp;
                pos4=temp;
                gameState.ghosts[3].direction = RIGHT;
                gameState.ghosts[3].hasExited = false;
            }
        }
    }

    gameState.pacman.mouth = OPEN;
    uiState.currentState = STATE_MAIN_MENU;

    initPacman();

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

     //pthread_create(&GhostThread[3], NULL, GhostThread4, &ghostIndices[3]);
for (int i=0;i<4;i++){
    pthread_create(&GhostThread[i], NULL,GhostThreadFunction, &ghostIndices[i]);

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