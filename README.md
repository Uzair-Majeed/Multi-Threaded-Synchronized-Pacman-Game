# Multi Threaded Pac-Man Game in C (SFML + PThreads)

A classic Pac-Man clone implemented in C using the SFML graphics library and Pthreads for ghost AI and game logic concurrency.

## 🕹️ Features

- Fully playable Pac-Man game
- Multithreaded ghost movement (each ghost runs in its own thread)
- BFS pathfinding-inspired chase behavior for ghosts
- Power pellets and frightened ghost state
- Game UI with:
  - Start Screen 
  - Main menu
  - Pause menu
  - High score screen (top 5)
  - Instructions screen
- High score saved to file
- Collision detection
- Smooth animations (Pac-Man mouth, ghost movement)
- Pellets eating and Score calculation
- Modular rendering and input handling

---

## 🛠️ Requirements

- GCC or Clang
- SFML 2.5+ (Simple and Fast Multimedia Library)
- Pthreads library supported system (Linux)

---

## 📁 Project Structure
- OS_Project.c # Main source file
- Content folder # having all sprites and textures pngs + fonts
- Sounds folder # containing the wav files being used in project.
- highScores.txt # Persistent high score storage
- README.md # Project documentation
- Object or Application file

---

## 🧵 Threads Overview

- **Main Thread**: Handles window events, rendering, and Pac-Man logic
- **Ghost Threads (4)**: Each ghost updates its path and moves independently
- **UI Thread**: (Reserved for additional UI logic, e.g., animations,sounds,different UI interfaces)

---

## 🧩 Controls
- Enter – Start/Continue
- Arrow Keys – Move Pac-Man
- P – Pause
- I – Instructions
- H – High Scores
- M - Main Menu
- ESC – Exit

---

## 💾 High Scores
Stored in highScores.txt

Maintains top 5 scores in descending order

Automatically updated after each game over

---

## 🔊 Audio
All sound effects are located in the Sounds/ folder and are triggered during gameplay (e.g., pellet eating, ghost collision, game over).

---

## 📌 Notes
Ghost behavior mimics the original Pac-Man game, with varied targeting logic.

BFS pathfinding is used for chasing Pac-Man; frightened and return states use simplified logic.

Game runs at ~60 FPS using usleep() and SFML clocks.

Font files must be present in the Content/ folder or paths must be adjusted.

---

## 🧪 Building & Running

### 🔧 Compile and Run

```bash
gcc OS_Project.c -o yourfile -lcsfml-graphics -lcsfml-window -lcsfml-system -lcsfml-audio -lpthread -lm
./yourfile
```

##🔧Collaborators
### UZAIR MAJEED 
### HASHIR NABEEL
### RIZWAN SAEED


