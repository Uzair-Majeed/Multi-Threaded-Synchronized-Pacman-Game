#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <SFML/Graphics.h>
#include <SFML/Audio.h>
#include <SFML/Window.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>

#define TILE_SIZE 24
#define ROWS 32
#define COLS 29

#define MAX_QUEUE  (ROWS * COLS) //for queue size in BFS algorithm
bool dotMap[ROWS][COLS];
bool powerPellet[ROWS][COLS];
bool fruits[ROWS][COLS];

sfFont * pacmanFont;
sfFont * boldFont;
sfTexture * pacTexture[5];
sfTexture * heartTexture;
sfTexture * gridTexture;
sfTexture * pelletTexture;
sfTexture * bigpelletTexture;  
sfTexture * ghostTexture[18];
sfTexture * fruitTexture[4];
int fruitCount = 0;

sfSound * menuSound;
sfSound * eatingSound;
sfSound * pacDeathSound;
sfSound * ghostDeathSound;

// Direction offsets: UP, DOWN, LEFT, RIGHT
static const int rowOffset[4] = { -1, 1,  0, 0 };
static const int colOffset[4] = {  0, 0, -1, 1 };

sem_t keySemaphore;
sem_t exitPermitSemaphore;




char grid[ROWS][COLS] = {
    "#############################", // 0 
    "#............##..........O.##", // 1
    "#.####.#####.##.#####.####.##", // 2
    "#.####.#####.##.#####.####.##", // 3
    "#.####.#####.##.#####.####.##", // 4
    "#O.........................##", // 5
    "#.####.##.########.##.####.##", // 6
    "#.####.##.########.##.####.##", // 7
    "#......##....##....##......##", // 8
    "######.#####.##.#####.#######", // 9
    "     #.#####.##.#####.#      ", // 10
    "     #.##..........##.#      ", // 11
    "     #.##.###__###.##.#      ", // 12
    "######.##.#      #.##.#######", // 13
    "..........# 1  2 #.......... ", // 14
    "######.##.# 3  4 #.##.#######", // 15
    "     #.##.########.##.#      ", // 16
    "     #.##..........##.#      ", // 17
    "     #.##.########.##.#      ", // 18
    "######.##.########.##.#######", // 19
    "#............##..P.........##", // 20
    "#.####.#####.##.#####.####.##", // 21
    "#.####.#####.##.#####.####.##", // 22
    "#...##................##...##", // 23
    "###.##.##.########.##.##.####", // 24
    "###.##.##.########.##.##.####", // 25
    "#......##....##....##...O..##", // 26
    "#.##########.##.##########.##", // 27
    "#.##########.##.##########.##", // 28
    "#.....O....................##", // 29
    "#############################", // 30
    "#############################"  // 31
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


enum PacMouth{
    OPEN,
    CLOSED,
};


enum TargetState
{
	CHASE,
	FRIGHTENED,
	GOHOME

};


enum UIStateType{
    STATE_START,
    STATE_MAIN_MENU,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_GAME_OVER,
    STATE_HIGHSCORE,
    STATE_INSTRUCTIONS
};

enum SoundStateType{
    MAIN_MENU,
    EATING_PELLETS,
    PAC_DEATH,
    GHOST_DEATH
};


// A small struct for BFS queue
typedef struct {
    int r;
    int c;
} Node;

typedef struct{

    char name[5][50];
    int topScores[5];

}HighScores;

typedef struct {
    int r,c;
    sfVector2f pos;
    enum Direction direction;
    enum PacMouth mouth;
} Pacman;

typedef struct {
    int r,c;
    sfVector2f pos;
    enum Direction direction;
    enum GhostType color; // Optional
    enum TargetState state; // CHASE,GO HOME OR FRIGHTENED STATE
    bool hasExited;
    float frightenedTimer;
} Ghost;

typedef struct {
    int score;
    int lives;
    int pelletsRemaining;
    int gameOver;
    Pacman pacman;
    Ghost ghosts[4];
} GameState;


typedef struct {
    char currPlayer[50];
    enum UIStateType currentState;
    bool quit;
    HighScores highScore;
    enum SoundStateType sound;
} UIState;


pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t uiMutex = PTHREAD_MUTEX_INITIALIZER;

GameState gameState;
UIState uiState;

void readHighScores(){
    int fd = open("highScores.txt", O_RDONLY);

    if (fd == -1) {
        printf("Failed to open the file.\n");
        exit(1);
    }

    char buffer[500];
    ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead == -1) {
        printf("Failed to read from the file.\n");
        close(fd);
        exit(1);
    }

    buffer[bytesRead] = '\0';
    close(fd);

    int start = 0;
    int nameIndex = 0;
    int score = 0;
    bool flag = false;

    HighScores temp;

    for (int i = 0; i < bytesRead; i++) {
        char c = buffer[i];

        if (c == ',') {
            temp.name[start][nameIndex] = '\0';
            nameIndex = 0;
            flag = true;
            score = 0;
        }
        else if (c == '\n' || c == '\0') {
            temp.topScores[start] = score;
            start++;
            flag = false;

            // Stop if we filled 5 entries
            if (start >= 5) break;
        }
        else if (flag) {
            score = score * 10 + (c - '0');
        }
        else {
            temp.name[start][nameIndex++] = c;
        }
    }

    for (int i = 0; i < 5; i++) {
        int j = 0;
        while (temp.name[i][j] != '\0') {
            uiState.highScore.name[i][j] = temp.name[i][j];
            j++;
        }
        uiState.highScore.name[i][j] = '\0';
        uiState.highScore.topScores[i] = temp.topScores[i];
    }
}


void writeHighScores(){

    int fd = open("highScores.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644);

    if (fd == -1) {
        printf("Failed to open the file for writing.\n");
        exit(1);
    }

    char buffer[500];
    int pos = 0;

    for (int i = 0; i < 5; i++) {
        int len = sprintf(buffer + pos, "%s,%d\n", uiState.highScore.name[i], uiState.highScore.topScores[i]);
        pos += len;
    }

    ssize_t bytesWritten = write(fd, buffer, pos);
    if (bytesWritten == -1) {
        printf("Failed to write to the file.\n");
        close(fd);
        exit(1);
    }

    close(fd);
}

void sortHighScores(){

    HighScores* temp = &uiState.highScore;
    int sc = gameState.score;

    if(sc < temp->topScores[4]){
        return;
    }
    
    temp->topScores[4] = sc;
    strncpy(temp->name[4], uiState.currPlayer, sizeof(temp->name[4]) - 1);

    temp->name[4][sizeof(temp->name[4]) - 1] = '\0';

    // Sort the scores and names (Bubble Sort)
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 4 - i; j++) {
            if (temp->topScores[j] < temp->topScores[j + 1]) {

                int tmpScore = temp->topScores[j];
                temp->topScores[j] = temp->topScores[j + 1];
                temp->topScores[j + 1] = tmpScore;

                char tmpName[50];
                strcpy(tmpName, temp->name[j]);
                strcpy(temp->name[j], temp->name[j + 1]);
                strcpy(temp->name[j + 1], tmpName);
            }
        }
    }


}
// Render the Main Menu screen with name input
void renderStartMenu(sfRenderWindow *window) {


    static char playerName[50] = "";
    static int playerNameLength = 0;

    sfText *title = sfText_create();
    sfText_setString(title, "PAC-MAN \n\nEnter Your Name:");
    sfText_setFont(title, pacmanFont);
    sfText_setColor(title, sfYellow);
    sfText_setCharacterSize(title, 30);
    sfText_setPosition(title, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, title, NULL);

    sfText *nameText = sfText_create();
    sfText_setFont(nameText, boldFont);
    sfText_setColor(nameText, sfWhite);
    sfText_setCharacterSize(nameText, 30);
    sfText_setPosition(nameText, (sfVector2f){100.f, 250.f});
    sfText_setString(nameText, playerName);
    sfRenderWindow_drawText(window, nameText, NULL);

    sfText *confirmText = sfText_create();
    sfText_setString(confirmText, "Press SPACE to Confirm");
    sfText_setFont(confirmText, pacmanFont);
    sfText_setColor(confirmText, sfYellow);
    sfText_setCharacterSize(confirmText, 30);
    sfText_setPosition(confirmText, (sfVector2f){100.f, 450.f});
    sfRenderWindow_drawText(window, confirmText, NULL);

    sfEvent event;
    while (sfRenderWindow_pollEvent(window, &event)) {
        if (event.type == sfEvtClosed) {
            sfRenderWindow_close(window);
        } 
        else if (event.type == sfEvtTextEntered) {
            if (event.text.unicode >= 32 && event.text.unicode <= 126) {
                if (playerNameLength < sizeof(playerName) - 1) {
                    playerName[playerNameLength++] = (char)event.text.unicode;
                    playerName[playerNameLength] = '\0';
                }
            }
            else if (event.text.unicode == 8 && playerNameLength > 0) {
                playerName[--playerNameLength] = '\0';
            }
            else if (event.text.unicode == '\r' && playerNameLength > 0) {
                strcpy(uiState.currPlayer, playerName);
            }
        }
    }

    sfText_destroy(title);
    sfText_destroy(nameText);
    sfText_destroy(confirmText);
}





// Renders the Main Menu screen
void renderMainMenu(sfRenderWindow *window) {


    sfText *title = sfText_create();
    sfText_setString(title,
        "PAC-MAN \n\n\n\n"
        "Press Enter to Play\n\n\n\n"
        "Press I for Instructions\n\n\n\n"
        "Press H for High Score\n\n\n\n"
        "Press ESC to Exit");
    sfText_setFont(title, pacmanFont);
    sfText_setColor(title,sfYellow);
    sfText_setCharacterSize(title, 25);
    sfText_setPosition(title, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, title, NULL);
    sfText_destroy(title);
}

// Renders the Pause Menu screen
void renderPauseMenu(sfRenderWindow *window) {

    sfText *pauseText = sfText_create();
    sfText_setString(pauseText,
        "GAME PAUSED\n\n\n\n"
        "Press Enter to Resume\n\n\n\n"
        "Press M to Go To Main Menu\n\n\n\n"
        "Press ESC to Exit");
    sfText_setFont(pauseText, pacmanFont);
    sfText_setColor(pauseText,sfYellow);
    sfText_setCharacterSize(pauseText, 25);
    sfText_setPosition(pauseText, (sfVector2f){100.f, 150.f});
    sfRenderWindow_drawText(window, pauseText, NULL);
    sfText_destroy(pauseText);
}

// Renders the High Score screen
void renderHighScore(sfRenderWindow *window) {


    char buffer[512];

    snprintf(buffer, sizeof(buffer), "HIGH SCORES:\n\n");
    for (int i = 0; i < 5; i++) {
        char line[64];
        snprintf(line, sizeof(line), "%d. %s - %d\n\n", i + 1, uiState.highScore.name[i], uiState.highScore.topScores[i]);
        strcat(buffer, line);
    }

    strcat(buffer, "\nPress ENTER to \n\t\treturn to Menu");

    sfText *scoreText = sfText_create();
    sfText_setString(scoreText, buffer);
    sfText_setColor(scoreText, sfYellow);
    sfText_setFont(scoreText, boldFont);
    sfText_setCharacterSize(scoreText, 25);
    sfText_setPosition(scoreText, (sfVector2f){100.f, 150.0f});
    sfRenderWindow_drawText(window, scoreText, NULL);
    sfText_destroy(scoreText);
}

// Renders the Instructions screen
void renderInstructions(sfRenderWindow *window) {

    sfText *instructions = sfText_create();
    sfText_setString(instructions,
        "Instructions \n\n\n\n"
        "-Use Arrow Keys to Move \n\n\n\n"
        "-Avoid Ghosts\n\n\n\n"
        "-Eat all the pellets to win!\n\n\n\n"
        "Press Enter to\n\t\t Return to Menu\n\n\n\n");
    sfText_setFont(instructions, pacmanFont);
    sfText_setColor(instructions, sfYellow);
    sfText_setCharacterSize(instructions, 25);
    sfText_setPosition(instructions, (sfVector2f){20.f, 100.f});
    sfRenderWindow_drawText(window, instructions, NULL);
    sfText_destroy(instructions);
}

// Renders the GameOver screen
void renderGameOver(sfRenderWindow *window) {

    char buffer[512];

    snprintf(buffer, sizeof(buffer), "GAME OVER !! \n\nHIGH SCORES:\n\n");
    for (int i = 0; i < 5; i++) {
        char line[64];
        snprintf(line, sizeof(line), "%d. %s - %d\n\n", i + 1, uiState.highScore.name[i], uiState.highScore.topScores[i]);
        strcat(buffer, line);
    }


    char buffer2[128];
    snprintf(buffer2, sizeof(buffer2),"\n\n""YOUR FINAL SCORE: %d \n\n""Press ENTER to \n\t\treturn to Menu",gameState.score);
    strcat(buffer, buffer2);

    sfText *scoreText = sfText_create();
    sfText_setString(scoreText, buffer);
    sfText_setColor(scoreText, sfYellow);
    sfText_setFont(scoreText, boldFont);
    sfText_setCharacterSize(scoreText, 25);
    sfText_setPosition(scoreText, (sfVector2f){100.f, 150.0f});
    sfRenderWindow_drawText(window, scoreText, NULL);
    sfText_destroy(scoreText);

    sortHighScores();
    writeHighScores();
}




void renderGraphics(sfRenderWindow * window){



    //score
    sfText* score = sfText_create();
    sfText_setFont(score,pacmanFont);
    sfText_setString(score, "SCORE : ");

    sfText_setCharacterSize(score,40);
    sfVector2f textPosition = {20.0f, 10.0f}; 
    sfText_setPosition(score, textPosition);
    sfText_setColor(score,sfYellow);

    //======
    sfText* sc = sfText_create();
    sfText_setFont(sc,boldFont);
    char str[12];
    sprintf(str, "%d", gameState.score);
    sfText_setString(sc, str);

    sfText_setCharacterSize(sc,40);
    sfVector2f textPosition2 = {250.0f, 10.0f}; 
    sfText_setPosition(sc, textPosition2);
    sfText_setColor(sc,sfYellow);

    //print heart
    sfSprite * hsprite = sfSprite_create();
    sfVector2f hPosition = {600.0f, 10.0f}; 
    sfVector2f hscale = {3.0f,3.0f};
    sfSprite_setScale(hsprite,hscale);
    sfSprite_setPosition(hsprite, hPosition);
    sfSprite_setTexture(hsprite, heartTexture, sfTrue);

    sfSprite * hsprite1 = sfSprite_create();
    sfVector2f hPosition1 = {500.0f, 10.0f}; 
    sfVector2f hscale1 = {3.0f,3.0f};
    sfSprite_setScale(hsprite1,hscale1);
    sfSprite_setPosition(hsprite1, hPosition1);
    sfSprite_setTexture(hsprite1, heartTexture, sfTrue);
    
    sfSprite * hsprite2 = sfSprite_create();
    sfVector2f hPosition2 = {550.0f, 10.0f}; 
    sfVector2f hscale2 = {3.0f,3.0f};
    sfSprite_setScale(hsprite2,hscale2);
    sfSprite_setPosition(hsprite2, hPosition2);
    sfSprite_setTexture(hsprite2, heartTexture, sfTrue);

    //grid
    sfSprite* gridSprite = sfSprite_create();
    sfSprite_setTexture(gridSprite, gridTexture, sfTrue);
    sfVector2f scale = {3.00f, 3.02f};
    sfVector2f gridPosition = {0.0f, 96.0f};
    sfSprite_setScale(gridSprite,scale);
    sfSprite_setPosition(gridSprite, gridPosition);

    //display pacman
    sfSprite * pac = sfSprite_create();

    if(gameState.pacman.mouth == OPEN){
        if(gameState.pacman.direction == UP){
            sfSprite_setTexture(pac,pacTexture[0],sfTrue);
        }
        else if(gameState.pacman.direction == DOWN){
            sfSprite_setTexture(pac,pacTexture[1],sfTrue);
        }
        else if(gameState.pacman.direction == LEFT){
            sfSprite_setTexture(pac,pacTexture[2],sfTrue);
        }
        else if(gameState.pacman.direction == RIGHT){
            sfSprite_setTexture(pac,pacTexture[3],sfTrue);
        }
    }
    else {
        sfSprite_setTexture(pac,pacTexture[4],sfTrue);
    }

    sfSprite_setPosition(pac,gameState.pacman.pos);
    sfSprite_setScale(pac,(sfVector2f){0.85f,0.85f});

    

    //ghosts
    sfSprite* ghostSpr[4];

    for(int i=0;i<4;i++)ghostSpr[i] = sfSprite_create();
    sfTexture* ghTexture[4];

    for(int i=0; i< 4;i++){

        if(gameState.ghosts[i].state == CHASE){
            if(i == 0){
            if(gameState.ghosts[0].direction == UP)ghTexture[0] = ghostTexture[0];
            else if(gameState.ghosts[0].direction == DOWN)ghTexture[0] = ghostTexture[1];
            else if(gameState.ghosts[0].direction == LEFT)ghTexture[0] = ghostTexture[2];
            else if(gameState.ghosts[0].direction == RIGHT)ghTexture[0] = ghostTexture[3];
        
            }
            else if(i==1){
                if(gameState.ghosts[1].direction == UP)ghTexture[1] = ghostTexture[4];
                else if(gameState.ghosts[1].direction == DOWN)ghTexture[1] = ghostTexture[5];
                else if(gameState.ghosts[1].direction == LEFT)ghTexture[1] = ghostTexture[6];
                else if(gameState.ghosts[1].direction == RIGHT)ghTexture[1] = ghostTexture[7];
            
            

            }
            else if(i==2){
                if(gameState.ghosts[2].direction == UP)ghTexture[2] = ghostTexture[8];
                else if(gameState.ghosts[2].direction == DOWN)ghTexture[2] = ghostTexture[9];
                else if(gameState.ghosts[2].direction == LEFT)ghTexture[2] = ghostTexture[10];
                else if(gameState.ghosts[2].direction == RIGHT)ghTexture[2] = ghostTexture[11];
            

            }
            else{
            if(gameState.ghosts[3].direction == UP)ghTexture[3] = ghostTexture[12];
            else if(gameState.ghosts[3].direction == DOWN)ghTexture[3] = ghostTexture[13];
            else if(gameState.ghosts[3].direction == LEFT)ghTexture[3] = ghostTexture[14];
            else if(gameState.ghosts[3].direction == RIGHT)ghTexture[3] = ghostTexture[15];
            }
        
        }
        else if(gameState.ghosts[i].state == FRIGHTENED){
            ghTexture[i] = ghostTexture[16];
        }
        else{
            ghTexture[i] = ghostTexture[17];

        }
    }
    
    for(int i=0;i<4;i++){
        sfVector2f sc = {1.5f,1.5f};
        sfSprite_setScale(ghostSpr[i],sc);
        sfSprite_setTexture(ghostSpr[i],ghTexture[i],sfTrue);
        sfSprite_setPosition(ghostSpr[i],gameState.ghosts[i].pos);
    }


    //pellets
    sfSprite* pelletSpr = sfSprite_create();
    sfSprite_setTexture(pelletSpr,pelletTexture,sfTrue);

    //bigPellet
    sfSprite* bigpelletSpr = sfSprite_create();
    sfSprite_setTexture(bigpelletSpr,bigpelletTexture,sfTrue);

    //fruits
    sfSprite* appleSpr = sfSprite_create();
    sfSprite_setTexture(appleSpr,fruitTexture[0],sfTrue);

    sfSprite* cherrySpr = sfSprite_create();
    sfSprite_setTexture(cherrySpr,fruitTexture[1],sfTrue);

    sfSprite* orangeSpr = sfSprite_create();
    sfSprite_setTexture(orangeSpr,fruitTexture[2],sfTrue);

    sfSprite* strawberrySpr = sfSprite_create();
    sfSprite_setTexture(strawberrySpr,fruitTexture[3],sfTrue);

    
    sfEvent event;
    
    while (sfRenderWindow_pollEvent(window, &event))
            if (event.type == sfEvtClosed)
                sfRenderWindow_close(window);
    

        sfRenderWindow_clear(window, sfBlack);

        sfRenderWindow_drawText(window,score,NULL);
        sfRenderWindow_drawText(window,sc,NULL);

        if(gameState.lives > 2){
        sfRenderWindow_drawSprite(window, hsprite, NULL);
        }
        if(gameState.lives > 0){
        sfRenderWindow_drawSprite(window, hsprite1, NULL);
        }
        if(gameState.lives > 1){
        sfRenderWindow_drawSprite(window, hsprite2, NULL);
        }

        sfRenderWindow_drawSprite(window, gridSprite, NULL);
        sfRenderWindow_drawSprite(window, pac, NULL);

        for(int i=0;i<4;i++){

            sfRenderWindow_drawSprite(window, ghostSpr[i], NULL);
        }


        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                // Render small dots
                if (dotMap[row][col]) {
                    
                    sfVector2f temp = {col * TILE_SIZE + 10,row* TILE_SIZE + 110};
                    sfVector2f scale3 = {1.5f,1.5f};
                    sfSprite_setScale(pelletSpr,scale3);
                    sfSprite_setPosition(pelletSpr,temp);
                    sfRenderWindow_drawSprite(window,pelletSpr,NULL);
                }
    
                // Render power pellets
                if (powerPellet[row][col]) {
                    sfVector2f temp = {col * TILE_SIZE + 10,row* TILE_SIZE + 110};
                    sfVector2f scale3 = {1.5f,1.5f};
                    sfSprite_setScale(bigpelletSpr,scale3);
                    sfSprite_setPosition(bigpelletSpr,temp);
                    sfRenderWindow_drawSprite(window,bigpelletSpr,NULL);
                }
            }
        }
        //fruits 
        if(fruits[1][25]){
            
          sfVector2f temp = {25 * TILE_SIZE + 10,1 * TILE_SIZE + 100};
          sfVector2f scale3 = {1.5f,1.5f};
          sfSprite_setScale(appleSpr,scale3);
          sfSprite_setPosition(appleSpr,temp);
          sfRenderWindow_drawSprite(window,appleSpr,NULL);
        }
        else if(fruits[5][1]){
            sfVector2f temp = {1 * TILE_SIZE + 10,5 * TILE_SIZE + 100};
            sfVector2f scale3 = {1.5f,1.5f};
            sfSprite_setScale(cherrySpr,scale3);
            sfSprite_setPosition(cherrySpr,temp);
            sfRenderWindow_drawSprite(window,cherrySpr,NULL);
    
        }
        else if(fruits[26][24]){
            sfVector2f temp = {24 * TILE_SIZE + 10,26 * TILE_SIZE + 100};
            sfVector2f scale3 = {1.5f,1.5f};
            sfSprite_setScale(orangeSpr,scale3);
            sfSprite_setPosition(orangeSpr,temp);
            sfRenderWindow_drawSprite(window,orangeSpr,NULL);
        }
        else if(fruits[29][6]){
            sfVector2f temp = {6 * TILE_SIZE + 10,29 * TILE_SIZE + 100};
            sfVector2f scale3 = {1.5f,1.5f};
            sfSprite_setScale(strawberrySpr,scale3);
            sfSprite_setPosition(strawberrySpr,temp);
            sfRenderWindow_drawSprite(window,strawberrySpr,NULL);
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
    sfSprite_destroy(appleSpr);
    sfSprite_destroy(cherrySpr);
    sfSprite_destroy(orangeSpr);
    sfSprite_destroy(strawberrySpr);


}

void handleUIState(sfRenderWindow* window) {
    pthread_mutex_lock(&uiMutex);

    if (uiState.quit) {
        sfRenderWindow_close(window);
        pthread_mutex_unlock(&uiMutex);
        return;
    }

    switch (uiState.currentState) {
        case STATE_START:

        uiState.sound = MAIN_MENU;

        sfRenderWindow_clear(window, sfBlack);
        renderStartMenu(window);
        sfRenderWindow_display(window);

        break;
        case STATE_MAIN_MENU:

            uiState.sound = MAIN_MENU;
            sfRenderWindow_clear(window, sfBlack);
            renderMainMenu(window);
            sfRenderWindow_display(window);
            break;

        case STATE_PAUSED:

            uiState.sound = MAIN_MENU;
            sfRenderWindow_clear(window, sfBlack);
            renderPauseMenu(window);
            sfRenderWindow_display(window);
            break;

        case STATE_GAME_OVER:

            uiState.sound = MAIN_MENU;
            sfRenderWindow_clear(window, sfBlack);
            renderGameOver(window);
            sfRenderWindow_display(window);
            break;

        case STATE_HIGHSCORE:

            uiState.sound = MAIN_MENU;
            sfRenderWindow_clear(window, sfBlack);
            renderHighScore(window);
            sfRenderWindow_display(window);
            break;

        case STATE_INSTRUCTIONS:

            uiState.sound = MAIN_MENU;
            sfRenderWindow_clear(window, sfBlack);
            renderInstructions(window);
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

                case STATE_START:
                if (event.key.code == sfKeySpace) {
                    uiState.currentState = STATE_MAIN_MENU;
                }

                break;
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
                    } 
                    else if (event.key.code == sfKeyUp) {
                            gameState.pacman.direction = UP;
                    } 
                    else if (event.key.code == sfKeyDown) {
                            gameState.pacman.direction = DOWN;
                        
                    } 
                    else if (event.key.code == sfKeyLeft) {
                      
                            gameState.pacman.direction = LEFT;
                        
                    } 
                    else if (event.key.code == sfKeyRight) {
                      
                            gameState.pacman.direction = RIGHT;
                       
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

                        uiState.currentState= STATE_MAIN_MENU;
                    }

                    break;

                default:
                    break;
            }

            pthread_mutex_unlock(&uiMutex); 
        }
    }
}

void initPacman();
void initGhosts();

bool winCondition(){
    if(gameState.pelletsRemaining == 0){
        return true;
    }
    return false;
}


bool loseCondition(){
    if(gameState.lives <= 0){
        return true;
    }
    return false;
}

// Resets the pacman position after he loses a life
void resetPositions() {
    
    initPacman();

    // Reset ghosts
    initGhosts();

}


void checkCollisionWithGhosts() {

    for (int i = 0; i < 4; i++) {
        float distance = sqrtf(powf(gameState.ghosts[i].pos.x - gameState.pacman.pos.x, 2) + 
                             powf(gameState.ghosts[i].pos.y - gameState.pacman.pos.y, 2));
        
        if (distance < TILE_SIZE/2) {  // Collision detected
            if (gameState.ghosts[i].state == FRIGHTENED) {
                // Ghost is vulnerable - eat it
                gameState.score += 200;  // Bonus points
                gameState.ghosts[i].state = GOHOME;

            } 
            else if (gameState.ghosts[i].state == CHASE && gameState.lives > 0) {

                
                // Pac-Man gets hit
                gameState.lives -= 1;

                uiState.sound = PAC_DEATH;
                resetPositions();  
            }
        }
    }
}
void randomFruit(int x,int y){
    //1 25,5 1,26 24,29 6

    int random =  rand() % 4;

    if(random == 0 && x != 1 && y != 25){
        fruits[1][25] = true;
    }
    else if(random == 1 && x != 5 && y != 1){
        fruits[5][1] = true;

    }
    else if(random == 2 && x != 26 && y != 24){

        fruits[26][24] = true;
    }
    else if (random == 3 && x != 29 && y != 6){

        fruits[29][6] = true;
    }
}

//pacman eats simple pellet or dot
void eating() {

    // Convert Pac-Man's pixel position to grid coordinates
    int pacmanGridX = (int)(gameState.pacman.pos.x) / TILE_SIZE;
    int pacmanGridY = ((int)(gameState.pacman.pos.y - 90)) / TILE_SIZE;

    // Ensure we're within grid bounds
    if (pacmanGridX < 0 || pacmanGridX >= COLS || pacmanGridY < 0 || pacmanGridY >= ROWS) {
        return;
    }

    // Check for regular pellet
    if (dotMap[pacmanGridY][pacmanGridX]) {
        dotMap[pacmanGridY][pacmanGridX] = false;  // Remove the pellet
        gameState.score += 10;  // Add 10 points
        gameState.pelletsRemaining--;
    }
    // Check for power pellet
    else if (powerPellet[pacmanGridY][pacmanGridX]) {
        powerPellet[pacmanGridY][pacmanGridX] = false;  // Remove power pellet
        gameState.score += 50;  // Add 50 points
        gameState.pelletsRemaining--;

        

        uiState.sound = GHOST_DEATH;
        // Activate frightened mode for all ghosts
        for (int i = 0; i < 4; i++) {
            gameState.ghosts[i].state = FRIGHTENED;
            gameState.ghosts[i].frightenedTimer = 5.0f;
        }

        fruitCount++;
        if(fruitCount == 4){
            randomFruit(pacmanGridY,pacmanGridX);
        }
    }
    if(fruits[pacmanGridY][pacmanGridX]){
        gameState.score += 200; 
        fruits[pacmanGridY][pacmanGridX] = false;
    }
}


void movePacman(float pacmanSpeed, float deltaTime, enum Direction prevDir) {

    uiState.sound = EATING_PELLETS;
    float dx = 0, dy = 0;
    //teleport logic
    if (gameState.pacman.pos.y > 14 * TILE_SIZE + 90 && gameState.pacman.pos.y < 15 * TILE_SIZE + 90) {
        if (gameState.pacman.pos.x <= 0 && gameState.pacman.direction == LEFT) {
            gameState.pacman.pos.x = 28 * TILE_SIZE;
            return;
        }
        else if (gameState.pacman.pos.x >= 28 * TILE_SIZE && gameState.pacman.direction == RIGHT) {
            gameState.pacman.pos.x = 0;
            return;
        }
    }

    // Apply Movement
    switch (gameState.pacman.direction) {
        case UP:    dy = -pacmanSpeed * deltaTime; break;
        case DOWN:  dy =  pacmanSpeed * deltaTime; break;
        case LEFT:  dx = -pacmanSpeed * deltaTime; break;
        case RIGHT: dx =  pacmanSpeed * deltaTime; break;
        default: break;
    }

    // Position Update
    float newX = gameState.pacman.pos.x + dx;
    float newY = gameState.pacman.pos.y + dy;
    int gridX = (int)(newX) / TILE_SIZE;
    int gridY = ((int)(newY - 90)) / TILE_SIZE;


    if (gridY >= 0 && gridY < ROWS && gridX >= 0 && gridX < COLS && grid[gridY][gridX] != '#') {

        
        gameState.pacman.pos.x = newX;
        gameState.pacman.pos.y = newY;

        // check for pallet eating
    }

    eating();  

    checkCollisionWithGhosts();

    // Mouth Animation
    static float mouthTimer = 0.0f;
    const float mouthInterval = 0.25f;
    mouthTimer += deltaTime;
    if (mouthTimer >= mouthInterval) {
        mouthTimer = 0.0f;
        gameState.pacman.mouth = (gameState.pacman.mouth == OPEN) ? CLOSED : OPEN;
    }
}




void *gameEngineLoop(void *arg) {


    sfRenderWindow* window;

    sfVideoMode mode = {672, 864, 32};
    window = sfRenderWindow_create(mode, "PACMAN", sfResize | sfClose, NULL);


    sfClock* clock = sfClock_create();
    float pacmanSpeed = 100.0f;

    
    while (sfRenderWindow_isOpen(window)) {


        handleInput(window);

        handleUIState(window);  


        pthread_mutex_lock(&uiMutex);
            bool flag = (uiState.currentState == STATE_RUNNING);

        pthread_mutex_unlock(&uiMutex);

        if(!flag){
            continue;
        }

        sfTime elapsed = sfClock_restart(clock);
        float deltaTime = sfTime_asSeconds(elapsed);
        
        pthread_mutex_lock(&stateMutex);


        if(winCondition() || loseCondition()){

            pthread_mutex_lock(&uiMutex);
            uiState.currentState = STATE_GAME_OVER;
            pthread_mutex_unlock(&uiMutex);

            continue;
        }

        enum Direction prevDir = gameState.pacman.direction;
        movePacman(pacmanSpeed,deltaTime,prevDir);
        renderGraphics(window);

        pthread_mutex_unlock(&stateMutex);
        usleep(1000000 / 60);  // ~60 FPS

    }

    sfClock_destroy(clock);
    sfRenderWindow_destroy(window);
    return NULL;
}


void *UIFunction(void *arg) {

    //sfRenderWindow* window = (sfRenderWindow*)arg;
    //sfEvent event;
    //sfBool prevEnter = sfFalse;
    //sfBool prevP = sfFalse;

    
    while (1) {
        pthread_mutex_lock(&uiMutex);
    
        switch (uiState.sound) {
            case MAIN_MENU:
                if (sfSound_getStatus(menuSound) != sfPlaying) {
                    sfSound_play(menuSound);
                }
                break;
            case EATING_PELLETS:
                if (sfSound_getStatus(eatingSound) != sfPlaying) {
                    sfSound_play(eatingSound);
                }
                break;
            case PAC_DEATH:
                if (sfSound_getStatus(pacDeathSound) != sfPlaying) {
                    sfSound_play(pacDeathSound);
                }
                break;
            case GHOST_DEATH:
                if (sfSound_getStatus(ghostDeathSound) != sfPlaying) {
                    sfSound_play(ghostDeathSound);
                }
                break;
        }
    
        pthread_mutex_unlock(&uiMutex);
        usleep(8000);
    }
    return NULL;
}



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
        
        pthread_mutex_lock(&uiMutex);
            bool flag = (uiState.currentState == STATE_RUNNING);

        pthread_mutex_unlock(&uiMutex);

        if(!flag){
            continue;
        }
        
        if(ghost->state == CHASE){
        // --- Ghost House Exit Logic ---
        if (!ghost->hasExited) {
            static const float exitDelay[] = {5.0f, 10.0f, 15.0f, 30.0f}; // Delay for each ghost
            static float exitTimers[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            exitTimers[ghostIndex] += deltaTime;

            if (exitTimers[ghostIndex] >= exitDelay[ghostIndex]) {
                sem_wait(&keySemaphore);
                 sem_wait(&exitPermitSemaphore);
                 pthread_mutex_lock(&stateMutex);

                 ghost->hasExited = true;
                 ghost->pos = (sfVector2f){13 * TILE_SIZE, 11 * TILE_SIZE + 90};

                 pthread_mutex_unlock(&stateMutex);
                 sem_post(&exitPermitSemaphore);
                 sem_post(&keySemaphore);
             }
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
                (int)((gameState.pacman.pos.y - 90) / TILE_SIZE),
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
        
        }
        else if(ghost->state == FRIGHTENED){

            ghost->frightenedTimer -= deltaTime;
             if (ghost->frightenedTimer <= 0.0f) {
                ghost->state = CHASE;
                ghost->frightenedTimer = 0.0f;
             }


            if(!ghost->hasExited){
                continue;
            }

            int ghostGridR = (int)((ghost->pos.y - 90) / TILE_SIZE);
            int ghostGridC = (int)(ghost->pos.x / TILE_SIZE);
            
            // If the ghost is not moving in a valid direction, pick a new one
            int nextR = ghostGridR + rowOffset[ghost->direction];
            int nextC = ghostGridC + colOffset[ghost->direction];
        
            // Check if the current direction is still valid
            if (nextR < 0 || nextR >= ROWS || nextC < 0 || nextC >= COLS || grid[nextR][nextC] == '#') {
                // Pick a new random direction
                enum Direction validDirs[4];
                int validCount = 0;
                
                // Find all possible directions
                for (int dirInt = UP; dirInt <= RIGHT; dirInt++) {
                    int testR = ghostGridR + rowOffset[dirInt];
                    int testC = ghostGridC + colOffset[dirInt];
                    
                    if (testR >= 0 && testR < ROWS && testC >= 0 && testC < COLS && 
                        grid[testR][testC] != '#') {
                        validDirs[validCount++] = (enum Direction)dirInt;
                    }
                }
                
                // Choose a random valid direction if available
                if (validCount > 0) {
                    ghost->direction = validDirs[rand() % validCount];
                }
            }
            
            // Move in the chosen direction
            float dx = 0, dy = 0;
            switch (ghost->direction) {
                case UP:    dy = -speed * deltaTime; break;
                case DOWN:  dy =  speed * deltaTime; break;
                case LEFT:  dx = -speed * deltaTime; break;
                case RIGHT: dx =  speed * deltaTime; break;
            }
            
            float newX = ghost->pos.x + dx;
            float newY = ghost->pos.y + dy;
            
            // Lock the ghost position update
            pthread_mutex_lock(&stateMutex);
            ghost->pos.x = newX;
            ghost->pos.y = newY;
            pthread_mutex_unlock(&stateMutex);
            
        }
        else { //ghost->state == GOHOME
            
            Node homePos = findEntityInGrid('1' + ghostIndex);  // Get the ghost's home position
            Node ghostPos = {
                (int)((ghost->pos.y - 90) / TILE_SIZE),
                (int)(ghost->pos.x / TILE_SIZE)
            };
        
            // Calculate direction vector to home
            float dx = homePos.c * TILE_SIZE - ghost->pos.x;
            float dy = homePos.r * TILE_SIZE + 90 - ghost->pos.y;
            
            float distance = sqrtf(dx * dx + dy * dy);
            float speedFactor = speed * deltaTime;
        
            // Normalize the direction vector
            dx = (dx / distance) * speedFactor;
            dy = (dy / distance) * speedFactor;
        
            pthread_mutex_lock(&stateMutex);
            ghost->pos.x += dx;
            ghost->pos.y += dy;
        
            // Check if the ghost has reached its home
            float homeThreshold = 2.0f;  // Allow a small error margin
            if (fabs(ghost->pos.x - homePos.c * TILE_SIZE) < homeThreshold &&
                fabs(ghost->pos.y - (homePos.r * TILE_SIZE + 90)) < homeThreshold) {
                ghost->state = CHASE;
                ghost->hasExited = false;
            }
            pthread_mutex_unlock(&stateMutex);
        }
        


        usleep(1000000 / 60); // ~60 FPS
    }
    
    sfClock_destroy(clock);
    sfClock_destroy(pathClock);
    return NULL;
}

void initFonts(){


    pacmanFont = sfFont_createFromFile("content/PAC-FONT.TTF");
    boldFont = sfFont_createFromFile("content/THEBOLDFONT-FREEVERSION.ttf");

}

void initTextures(){
    
    heartTexture  = sfTexture_createFromFile("content/heartt.png",NULL);
    gridTexture = sfTexture_createFromFile("content/grid.png",NULL);
    pelletTexture = sfTexture_createFromFile("content/small_powerup.png",NULL);
    bigpelletTexture = sfTexture_createFromFile("content/power_up.png",NULL); 

    pacTexture[0] = sfTexture_createFromFile("content/pac_up.png",NULL);
    pacTexture[1] = sfTexture_createFromFile("content/pac_down.png",NULL);
    pacTexture[2] = sfTexture_createFromFile("content/pac_left.png",NULL);
    pacTexture[3] = sfTexture_createFromFile("content/pac_right.png",NULL);
    pacTexture[4] = sfTexture_createFromFile("content/pacman3.png",NULL);

    ghostTexture[0] = sfTexture_createFromFile("content/red_ghost_up.png",NULL);
    ghostTexture[1] = sfTexture_createFromFile("content/red_ghost_down.png",NULL);
    ghostTexture[2] = sfTexture_createFromFile("content/red_ghost_left.png",NULL);
    ghostTexture[3] = sfTexture_createFromFile("content/red_ghost_right.png",NULL);
    ghostTexture[4] = sfTexture_createFromFile("content/pink_ghost_up.png",NULL);
    ghostTexture[5] = sfTexture_createFromFile("content/pink_ghost_down.png",NULL);
    ghostTexture[6] = sfTexture_createFromFile("content/pink_ghost_left.png",NULL);
    ghostTexture[7] = sfTexture_createFromFile("content/pink_ghost_right.png",NULL);
    ghostTexture[8] = sfTexture_createFromFile("content/cyan_ghost_up.png",NULL);
    ghostTexture[9] = sfTexture_createFromFile("content/cyan_ghost_down.png",NULL);
    ghostTexture[10] = sfTexture_createFromFile("content/cyan_ghost_left.png",NULL);
    ghostTexture[11] = sfTexture_createFromFile("content/cyan_ghost_right.png",NULL);
    ghostTexture[12] = sfTexture_createFromFile("content/orange_ghost_up.png",NULL);
    ghostTexture[13] = sfTexture_createFromFile("content/orange_ghost_down.png",NULL);
    ghostTexture[14] = sfTexture_createFromFile("content/orange_ghost_left.png",NULL);
    ghostTexture[15] = sfTexture_createFromFile("content/orange_ghost_right.png",NULL);
    ghostTexture[16] = sfTexture_createFromFile("content/frightened_ghost.png",NULL);
    ghostTexture[17] = sfTexture_createFromFile("content/return_ghost.png",NULL);

    fruitTexture[0] = sfTexture_createFromFile("content/apple.png",NULL);
    fruitTexture[1] = sfTexture_createFromFile("content/cherry.png",NULL);
    fruitTexture[2] = sfTexture_createFromFile("content/orange.png",NULL);
    fruitTexture[3] = sfTexture_createFromFile("content/strawberry.png",NULL);
}

void initSounds(){
    sfSoundBuffer * menuBuffer = sfSoundBuffer_createFromFile("sounds/main_menu.wav");
    sfSoundBuffer * eatingBuffer = sfSoundBuffer_createFromFile("sounds/pacman_chomp.wav");
    sfSoundBuffer * pacDeathBuffer = sfSoundBuffer_createFromFile("sounds/pacman_death.wav");
    sfSoundBuffer * ghostDeathBuffer = sfSoundBuffer_createFromFile("sounds/pacman_eatghost.wav");


    menuSound = sfSound_create();
    sfSound_setBuffer(menuSound, menuBuffer);


    eatingSound = sfSound_create();
    sfSound_setBuffer(eatingSound, eatingBuffer);


    pacDeathSound = sfSound_create();
    sfSound_setBuffer(pacDeathSound, pacDeathBuffer);


    ghostDeathSound = sfSound_create();
    sfSound_setBuffer(ghostDeathSound, ghostDeathBuffer);
}

void destroy(){
    sfFont_destroy(pacmanFont);
    sfFont_destroy(boldFont);

    sfTexture_destroy(heartTexture);
    sfTexture_destroy(gridTexture);
    sfTexture_destroy(pelletTexture);
    sfTexture_destroy(bigpelletTexture);


    for(int i=0;i<5;i++)
        sfTexture_destroy(pacTexture[i]);

    for(int i=0;i<18;i++)
        sfTexture_destroy(ghostTexture[i]);

    for(int i=0;i<4;i++)
        sfTexture_destroy(fruitTexture[i]);

    sfSound_destroy(menuSound);    
    sfSound_destroy(eatingSound);    
    sfSound_destroy(pacDeathSound);    
    sfSound_destroy(ghostDeathSound);    

}

void initGhosts(){

    
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == '1') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[0].pos = temp;
                gameState.ghosts[0].direction = UP;
                gameState.ghosts[0].state = CHASE;
                gameState.ghosts[0].hasExited = false;
                gameState.ghosts[0].frightenedTimer = 0.0f;
            }
            if (grid[row][col] == '2') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[1].pos = temp;
                gameState.ghosts[1].direction = DOWN;
                gameState.ghosts[1].state = CHASE;
                gameState.ghosts[1].hasExited = false;
                gameState.ghosts[0].frightenedTimer = 0.0f;
            }
            if (grid[row][col] == '3') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[2].pos = temp;
                gameState.ghosts[2].direction = LEFT;
                gameState.ghosts[2].state = CHASE;
                gameState.ghosts[2].hasExited = false;
                gameState.ghosts[0].frightenedTimer = 0.0f;
            }
            if (grid[row][col] == '4') {
                sfVector2f temp = {col * TILE_SIZE,row * TILE_SIZE + 90};
                gameState.ghosts[3].pos = temp;
                gameState.ghosts[3].direction = RIGHT;
                gameState.ghosts[3].state = CHASE;
                gameState.ghosts[3].hasExited = false;
                gameState.ghosts[0].frightenedTimer = 0.0f;
            }
        }
    }
}


void initPacman(){


    // Set initial position of Pacman
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            if (grid[row][col] == 'P') {
                sfVector2f temp = {col * TILE_SIZE , row * TILE_SIZE + 90};
                gameState.pacman.pos = temp;
                gameState.pacman.direction = NONE;
                gameState.pacman.mouth = OPEN;
                return;
            }
        }
    }

}

//generate map
void generateDotMap() {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            dotMap[i][j] = (grid[i][j] == '.');
            powerPellet[i][j] = (grid[i][j] == 'O');
            fruits[i][j] = false;
        }
    }
}


void countPellets(){
    gameState.pelletsRemaining = 0;
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            if (dotMap[y][x] || powerPellet[y][x]) {
                gameState.pelletsRemaining++;
            }
        }
    }
}

void initializeGame(){

    sem_init(&keySemaphore,0,2);
    sem_init(&exitPermitSemaphore,0,2);

    generateDotMap();
    initFonts();
    initTextures();
    initSounds();
    initPacman();
    initGhosts();

    readHighScores();


    // Count initial pellets
    countPellets();
    
    gameState.score = 0;  // Reset score
    gameState.lives = 3;
    uiState.currentState = STATE_START;
    uiState.sound = MAIN_MENU;
    uiState.quit = false;


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

    for (int i = 0; i < 4; i++) {
        pthread_join(GhostThread[i], NULL);
    }

    destroy();

    return 0;
}