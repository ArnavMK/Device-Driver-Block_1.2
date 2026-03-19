// Simple terminal Snake game controlled by the custom Xbox controller driver.
//
// Assumptions about the kernel module:
// - It registers a character device called "gamepadDriver"
//   so the node is available as /dev/gamepadDriver (adjust DEVICE_PATH if different).
// - Each successful read(fd, &byte, 1) returns a single "button id" byte
//   matching the GAMEPAD_BTN_* values defined in gamepad.h.
//
// Build (on Linux, in this folder):
//   gcc -pthread snake_game.c -o snake_game
//
// Run (may need sudo depending on device permissions):
//   ./snake_game
//
// Quit the game by pressing Ctrl+C in the terminal.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "gamepad_shared.h"

// Path to the character device created by the driver.
// Change this if your node name is different.
#define DEVICE_PATH "/dev/" DEVICE_NAME

// Game board size
#define BOARD_WIDTH  20
#define BOARD_HEIGHT 20

// Game tick in microseconds (snake speed)
#define TICK_USEC 150000

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    int x;
    int y;
} Point;

// Maximum snake length (fits inside board)
#define MAX_SNAKE_LEN (BOARD_WIDTH * BOARD_HEIGHT)

static Point snake[MAX_SNAKE_LEN];
static int   snake_len = 3;
static Direction current_dir = DIR_RIGHT;
static int food_x = 10;
static int food_y = 10;

static volatile sig_atomic_t running = 1;
static pthread_mutex_t dir_mutex = PTHREAD_MUTEX_INITIALIZER;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

// Clear the terminal and move cursor to top-left
static void clear_screen(void)
{
    printf("\033[2J\033[H");
}

static void place_initial_snake(void)
{
    snake_len = 3;
    snake[0].x = BOARD_WIDTH / 2 + 1;
    snake[0].y = BOARD_HEIGHT / 2;
    snake[1].x = BOARD_WIDTH / 2;
    snake[1].y = BOARD_HEIGHT / 2;
    snake[2].x = BOARD_WIDTH / 2 - 1;
    snake[2].y = BOARD_HEIGHT / 2;
    current_dir = DIR_RIGHT;
}

static void spawn_food(void)
{
    // Very simple "random" placement using rand(); avoids placing on snake body.
    while (1) {
        int ok = 1;
        food_x = rand() % BOARD_WIDTH;
        food_y = rand() % BOARD_HEIGHT;
        for (int i = 0; i < snake_len; ++i) {
            if (snake[i].x == food_x && snake[i].y == food_y) {
                ok = 0;
                break;
            }
        }
        if (ok) break;
    }
}

static void draw_board(void)
{
    char board[BOARD_HEIGHT][BOARD_WIDTH];
    memset(board, ' ', sizeof(board));

    // Draw food
    if (food_x >= 0 && food_x < BOARD_WIDTH &&
        food_y >= 0 && food_y < BOARD_HEIGHT) {
        board[food_y][food_x] = '*';
    }

    // Draw snake
    for (int i = 0; i < snake_len; ++i) {
        int x = snake[i].x;
        int y = snake[i].y;
        if (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT) {
            board[y][x] = (i == 0) ? 'O' : 'o';
        }
    }

    clear_screen();
    printf("Snake (Xbox driver controlled)\n");
    printf("Use D-Pad to steer. Ctrl+C to quit.\n\n");

    // Top border
    for (int x = 0; x < BOARD_WIDTH + 2; ++x) printf("#");
    printf("\n");

    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        printf("#");
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            printf("%c", board[y][x]);
        }
        printf("#\n");
    }

    // Bottom border
    for (int x = 0; x < BOARD_WIDTH + 2; ++x) printf("#");
    printf("\n");
    fflush(stdout);
}

static int step_snake(void)
{
    // Compute new head position
    Point head = snake[0];

    pthread_mutex_lock(&dir_mutex);
    Direction dir = current_dir;
    pthread_mutex_unlock(&dir_mutex);

    switch (dir) {
        case DIR_UP:    head.y -= 1; break;
        case DIR_DOWN:  head.y += 1; break;
        case DIR_LEFT:  head.x -= 1; break;
        case DIR_RIGHT: head.x += 1; break;
    }

    // Check wall collision
    if (head.x < 0 || head.x >= BOARD_WIDTH ||
        head.y < 0 || head.y >= BOARD_HEIGHT) {
        return -1; // Game over
    }

    // Check self collision
    for (int i = 0; i < snake_len; ++i) {
        if (snake[i].x == head.x && snake[i].y == head.y) {
            return -1; // Game over
        }
    }

    // Move body
    for (int i = snake_len - 1; i > 0; --i) {
        snake[i] = snake[i - 1];
    }
    snake[0] = head;

    // Check food
    if (head.x == food_x && head.y == food_y) {
        if (snake_len < MAX_SNAKE_LEN) {
            snake[snake_len] = snake[snake_len - 1];
            snake_len++;
        }
        spawn_food();
    }

    return 0;
}

// Change direction based on a GAMEPAD button code.
static void update_direction_from_button(unsigned char code)
{
    pthread_mutex_lock(&dir_mutex);

    if (code == GAMEPAD_BTN_DPAD_UP && current_dir != DIR_DOWN) {
        current_dir = DIR_UP;
    } else if (code == GAMEPAD_BTN_DPAD_DOWN && current_dir != DIR_UP) {
        current_dir = DIR_DOWN;
    } else if (code == GAMEPAD_BTN_DPAD_LEFT && current_dir != DIR_RIGHT) {
        current_dir = DIR_LEFT;
    } else if (code == GAMEPAD_BTN_DPAD_RIGHT && current_dir != DIR_LEFT) {
        current_dir = DIR_RIGHT;
    }

    pthread_mutex_unlock(&dir_mutex);
}

// Thread that blocks on reads from the controller driver and updates direction.
static void *controller_thread_func(void *arg)
{
    (void)arg;

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", DEVICE_PATH, strerror(errno));
        running = 0;
        return NULL;
    }

    printf("Opened controller device %s\n", DEVICE_PATH);

    while (running) {
        unsigned char code;
        ssize_t n = read(fd, &code, 1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "Error reading from %s: %s\n", DEVICE_PATH, strerror(errno));
            break;
        }
        if (n == 0) {
            // No data, try again
            continue;
        }

        update_direction_from_button(code);
    }

    close(fd);
    return NULL;
}

int main(void)
{
    signal(SIGINT, handle_sigint);

    srand((unsigned int)getpid());
    place_initial_snake();
    spawn_food();

    pthread_t controller_thread;
    if (pthread_create(&controller_thread, NULL, controller_thread_func, NULL) != 0) {
        fprintf(stderr, "Failed to create controller thread\n");
        return 1;
    }

    while (running) {
        if (step_snake() < 0) {
            clear_screen();
            printf("Game Over!\n");
            printf("Snake length: %d\n", snake_len);
            printf("Press Ctrl+C to exit.\n");
            break;
        }
        draw_board();
        usleep(TICK_USEC);
    }

    running = 0;
    pthread_join(controller_thread, NULL);

    return 0;
}

