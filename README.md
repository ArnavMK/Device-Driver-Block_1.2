Character Device Driver connecting a controller via USB to a snake game.
A classic Snake implementation driven by a custom Linux Character Device Driver. 
This project bridges the gap between low-level kernel programming and user-space application logic, utilizing a hardware controller via USB.

Prerequisites
Linux Kernel Headers (for module compilation)

GCC Compiler

A compatible USB Controller

1. Build and Install the Driver
The driver acts as the bridge between your hardware and the game.

Bash
# Navigate to the driver directory
cd path/to/driver

# Compile the kernel module
make

# Install the module into the kernel
sudo insmod controllerDriver.ko
2. Compile the Game
The game uses POSIX threads to manage the game loop and input monitoring simultaneously.

Bash
gcc -pthread snake_game.c -o snake_game
3. Run the Game
Plug in your controller and execute:

Bash
./snake_game

How to Play
The game translates raw hardware interrupts from your controller into directional movement.

D-Pad: Navigate the snake.

Button A: Restart the game after a "Game Over."

Button B: Exit the game and return to the terminal.

The Goal: Eat apples to grow the snake. The longer you get, the higher your score. Avoid colliding with the walls or your own tail!

Technical Overview
The Character Device Driver
The controllerDriver.ko module creates a device node in /dev/. When the controller's D-Pad is pressed, the driver:

Intercepts the hardware signal.

Updates a state buffer in kernel space.

Provides that data to snake_game.c via the standard read() system call.

Multi-threaded Game Logic
The game uses pthread to ensure that input detection never interrupts the rendering of the snake's movement. This prevents "input lag" and ensures a smooth 60FPS-style experience.
