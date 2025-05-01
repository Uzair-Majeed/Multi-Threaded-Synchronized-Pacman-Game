#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <SFML/Graphics.h>
#include <stdbool.h>


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
} Ghost;

typedef struct {
    int score;
    int lives;
    int pelletsRemaining;
    int gameOver;
    Pacman pacman;
    Ghost ghosts[4];
} GameState;


void renderGraphics();

// Global game state (for now — consider passing it by pointer instead)
GameState gameState;


// Optional: mutex for shared state (if multiple threads interact)
pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;

void *gameEngineLoop(void *arg) {


    sfRenderWindow* window;
    sfVideoMode mode = {672, 864, 32};


    window = sfRenderWindow_create(mode, "PACMAN", sfResize | sfClose, NULL);

    sfClock* clock = sfClock_create();
    float pacmanSpeed = 100.0f; // pixels per second

    // Find initial Pacman position from grid
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == 'P') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 100};
                gameState.pacman.pos = temp;
                gameState.pacman.direction = NONE;
                break;
            }
        }
    }

    while (sfRenderWindow_isOpen(window))
    {
        sfTime elapsed = sfClock_restart(clock);
        float deltaTime = sfTime_asSeconds(elapsed);

        // Lock shared state if needed
        pthread_mutex_lock(&stateMutex);

        // Example: move Pacman
        float dx = 0, dy = 0;
        switch (gameState.pacman.direction) {
            case UP:    dy = -pacmanSpeed * deltaTime; break;
            case DOWN:  dy =  pacmanSpeed * deltaTime; break;
            case LEFT:  dx = -pacmanSpeed * deltaTime; break;
            case RIGHT: dx =  pacmanSpeed * deltaTime; break;
            default: break;
        }
        //pacman mouth movement
        static float mouthTimer = 0.0f;
        const float mouthInterval = 0.25f; // seconds
    
        mouthTimer += deltaTime;
        if (mouthTimer >= mouthInterval) {
            mouthTimer = 0.0f;
    
            if (gameState.pacman.mouth == OPEN)
                gameState.pacman.mouth = HALF_OPEN;
            else if (gameState.pacman.mouth == HALF_OPEN)
                gameState.pacman.mouth = CLOSED;
            else
                gameState.pacman.mouth = OPEN;
        }


        // Check for wall collisions before applying movement
        float newX = gameState.pacman.pos.x + dx;
        float newY = gameState.pacman.pos.y + dy;

        int gridX = (int)(newX) / TILE_SIZE;
        int gridY = ((int)(newY - 100)) / TILE_SIZE; // adjust for offset

        if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {
            gameState.pacman.pos.x = newX;
            gameState.pacman.pos.y = newY;
        }

        renderGraphics(window);
        pthread_mutex_unlock(&stateMutex);

        // Cap to ~60 FPS
        usleep(1000000 / 60);
    }

    sfRenderWindow_destroy(window);
    sfClock_destroy(clock);
    return NULL;
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

}

int main() {


    pthread_t GameEngineThread;
    pthread_t UIThread;
    pthread_t GhostThread[4];

    initializeGame();
    pthread_create(&GameEngineThread,NULL,gameEngineLoop, NULL);

    pthread_join(GameEngineThread,NULL);


    return 0;
}
