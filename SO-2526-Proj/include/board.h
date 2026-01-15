#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

#define MAX_MOVES    20
#define MAX_LEVELS   20
#define MAX_FILENAME 256
#define MAX_GHOSTS   25

typedef enum {
    REACHED_PORTAL =  1,
    VALID_MOVE     =  0,
    INVALID_MOVE   = -1,
    DEAD_PACMAN    = -2
} move_t;

typedef struct {
    char command;
    int turns;
    int turns_left;
} command_t;

typedef struct {
    int pos_x, pos_y;     // current position
    int alive;            // if is alive
    int points;           // how many points have been collected
    int passo;            // number of plays to wait before starting
    command_t moves[MAX_MOVES];
    int current_move;
    int n_moves;          // 0 if controlled by user, >0 if read from file
    int waiting;
} pacman_t;

typedef struct {
    int pos_x, pos_y;     // current position
    int passo;            // number of plays to wait between each move
    command_t moves[MAX_MOVES];
    int n_moves;          // number of predefined moves from level file
    int current_move;
    int waiting;
    int charged;
} ghost_t;

typedef struct {
    char content;         // 'P', 'M', 'W', ' '
    int has_dot;          // whether there is a dot in this position or not
    int has_portal;       // whether there is a portal in this position or not
} board_pos_t;

typedef struct {
    int width, height;                    // dimensions of the board
    board_pos_t* board;                   // row-major matrix
    int n_pacmans;
    pacman_t* pacmans;                    // (your code allocates 1)
    int n_ghosts;
    ghost_t* ghosts;
    char level_name[256];
    char pacman_file[256];
    char ghosts_files[MAX_GHOSTS][256];
    int tempo;                            // duration of each play
} board_t;

typedef struct {
    char full_path[512];
} LevelEntry;

typedef struct {
    int x;
    int y;
} pos_t;

typedef struct {
    board_t* board;
    ghost_t* ghost;
    int id;
} ghost_thread_args_t;

/* timing */
void sleep_ms(int milliseconds);

/* movement */
int move_pacman(board_t* board, int pacman_index, command_t* command);
int move_ghost(board_t* board, int ghost_index, command_t* command);
int move_ghost_charged(board_t* board, int ghost_index, char direction);

/* pacman lifecycle */
void kill_pacman(board_t* board, int pacman_index);

/* control / init helpers*/
bool is_manual_control(board_t* board);
pos_t get_valid_start_pos(board_t* board, int x, int y);
void load_pacman_ini(board_t* board, int points);
void load_ghost_ini(board_t* board, ghost_t* ghost);

/* file / parsing helpers*/
char* read_file_dynamic(const char* filepath);
void parse_passo(const char* line, int* passo);
void parse_pos(board_t* board, const char* line, int* pos_x, int* pos_y);
int  is_valid_move_char(char c);
void parse_move_line(command_t* moves, int* move_idx, const char* line);

void parse_dim(board_t* board, const char* line);
void parse_tempo(board_t* board, const char* line);
void parse_pac(board_t* board, const char* line);
void parse_ghosts(board_t* board, const char* line);
void parse_board(board_t* board, const char* line, int* map_row);

/* loading */
int load_pacman(board_t* board, int points, const char* dir);
int load_ghost(board_t* board, const char* dir);
int load_level(board_t* board, int accumulated_points, const char* level_path, const char* dir);
void unload_level(board_t* board);

/* debug */
void open_debug_file(char* filename);
void close_debug_file(void);
void debug(const char* format, ...);
void print_board(board_t* board);

#endif /* BOARD_H */
