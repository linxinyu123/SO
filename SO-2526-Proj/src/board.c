#include "board.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include <string.h> 
#include <fcntl.h>   
#include <stdbool.h>

FILE * debugfile;

// Helper private function to find and kill pacman at specific position
static int find_and_kill_pacman(board_t* board, int new_x, int new_y) {
    for (int p = 0; p < board->n_pacmans; p++) {
        pacman_t* pac = &board->pacmans[p];
        if (pac->pos_x == new_x && pac->pos_y == new_y && pac->alive) {
            pac->alive = 0;
            kill_pacman(board, p);
            return DEAD_PACMAN;
        }
    }
    return VALID_MOVE;
}

// Helper private function for getting board position index
static inline int get_board_index(board_t* board, int x, int y) {
    return y * board->width + x;
}

// Helper private function for checking valid position
static inline int is_valid_position(board_t* board, int x, int y) {
    return (x >= 0 && x < board->width) && (y >= 0 && y < board->height); // Inside of the board boundaries
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int move_pacman(board_t* board, int pacman_index, command_t* command) {
    if (pacman_index < 0 || !board->pacmans[pacman_index].alive) {
        return DEAD_PACMAN; // Invalid or dead pacman
    }

    pacman_t* pac = &board->pacmans[pacman_index];
    int new_x = pac->pos_x;
    int new_y = pac->pos_y;

    // check passo
    if (pac->waiting > 0) {
        pac->waiting -= 1;
        return VALID_MOVE;        
    }
    pac->waiting = pac->passo;

    char direction = command->command;

    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'T': // Wait
            if (command->turns_left == 1) {
                pac->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        case 'G':
            break;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    pac->current_move+=1;

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, pac->pos_x, pac->pos_y);
    char target_content = board->board[new_index].content;

    if (board->board[new_index].has_portal) {
        board->board[old_index].content = ' ';
        board->board[new_index].content = 'P';
        return REACHED_PORTAL;
    }

    // Check for walls
    if (target_content == 'W') {
        return INVALID_MOVE;
    }

    // Check for ghosts
    if (target_content == 'M') {
        kill_pacman(board, pacman_index);
        return DEAD_PACMAN;
    }

    // Collect points
    if (board->board[new_index].has_dot) {
        pac->points++;
        board->board[new_index].has_dot = 0;
    }

    board->board[old_index].content = ' ';
    pac->pos_x = new_x;
    pac->pos_y = new_y;
    board->board[new_index].content = 'P';

    return VALID_MOVE;
}

// Helper private function for charged ghost movement in one direction
static int move_ghost_charged_direction(board_t* board, ghost_t* ghost, char direction, int* new_x, int* new_y) {
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    *new_x = x;
    *new_y = y;
    
    switch (direction) {
        case 'W': // Up
            if (y == 0) return INVALID_MOVE;
            *new_y = 0; // In case there is no colision
            for (int i = y - 1; i >= 0; i--) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i + 1; // stop before colision
                    return VALID_MOVE;
                }
                else if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'S': // Down
            if (y == board->height - 1) return INVALID_MOVE;
            *new_y = board->height - 1; // In case there is no colision
            for (int i = y + 1; i < board->height; i++) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i - 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'A': // Left
            if (x == 0) return INVALID_MOVE;
            *new_x = 0; // In case there is no colision
            for (int j = x - 1; j >= 0; j--) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j + 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'D': // Right
            if (x == board->width - 1) return INVALID_MOVE;
            *new_x = board->width - 1; // In case there is no colision
            for (int j = x + 1; j < board->width; j++) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j - 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;
        default:
            debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
            return INVALID_MOVE;
    }
    return VALID_MOVE;
}   

int move_ghost_charged(board_t* board, int ghost_index, char direction) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    int new_x = x;
    int new_y = y;

    ghost->charged = 0; //uncharge
    int result = move_ghost_charged_direction(board, ghost, direction, &new_x, &new_y);
    if (result == INVALID_MOVE) {
        debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
        return INVALID_MOVE;
    }

    // Get board indices
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    int new_index = get_board_index(board, new_x, new_y);

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one
    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;
    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
}

int move_ghost(board_t* board, int ghost_index, command_t* command) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int new_x = ghost->pos_x;
    int new_y = ghost->pos_y;

    // check passo
    if (ghost->waiting > 0) {
        ghost->waiting -= 1;
        return VALID_MOVE;
    }
    ghost->waiting = ghost->passo;

    char direction = command->command;
    
    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'C': // Charge
            ghost->current_move += 1;
            ghost->charged = 1;
            return VALID_MOVE;
        case 'T': // Wait
            if (command->turns_left == 1) {
                ghost->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    ghost->current_move++;
    if (ghost->charged)
        return move_ghost_charged(board, ghost_index, direction);

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    // Check board position
    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    char target_content = board->board[new_index].content;

    // Check for walls and ghosts
    if (target_content == 'W' || target_content == 'M') {
        return INVALID_MOVE;
    }

    int result = VALID_MOVE;
    // Check for pacman
    if (target_content == 'P') {
        result = find_and_kill_pacman(board, new_x, new_y);
    }

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one

    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;

    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
}

void kill_pacman(board_t* board, int pacman_index) {
    debug("Killing %d pacman\n\n", pacman_index);
    pacman_t* pac = &board->pacmans[pacman_index];
    int index = pac->pos_y * board->width + pac->pos_x;

    // Remove pacman from the board
    board->board[index].content = ' ';

    // Mark pacman as dead
    pac->alive = 0;
}

bool is_manual_control (board_t* board) {
    if (board->pacman_file[0] == '\0') {
        return true;
    }
    return false;
}

pos_t get_valid_start_pos(board_t *board, int x, int y) {
    pos_t result;
    
    int idx = get_board_index(board, x, y);
    char content = board->board[idx].content;

    if (content != 'W' && content != 'M' && content != 'P') {
        result.x = x;
        result.y = y;
        return result;
    }

    for (int y = 1; y < board->height - 1; y++) {
        for (int x = 1; x < board->width - 1; x++) {
            int curr_idx = get_board_index(board, x, y);
            char c = board->board[curr_idx].content;
            
            if (c != 'W' && c != 'M' && c != 'P') {
                result.x = x;
                result.y = y;
                return result;
            }
        }
    }

    result.x = 1;
    result.y = 1;
    return result;
}

void load_pacman_ini(board_t* board, int points) {
    pos_t pos_t = get_valid_start_pos(board, 1, 1);
    board->pacmans[0].pos_x = pos_t.x;
    board->pacmans[0].pos_y = pos_t.y;
    board->pacmans[0].alive = 1;
    board->pacmans[0].points = points;
    board->pacmans[0].passo = 0;
    board->pacmans[0].n_moves = 0;
    board->pacmans[0].current_move = 0;
    board->pacmans[0].waiting = 0;
}

char* read_file_dynamic(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: Could not open file %s\n", filepath);
        return NULL;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error getting file size");
        close(fd);
        return NULL;
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("Error resetting file pointer");
        close(fd);
        return NULL;
    }

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        close(fd);
        return NULL;
    }

    size_t total_read = 0;
    ssize_t n;
    
    while (total_read < (size_t)file_size) {
        n = read(fd, buffer + total_read, file_size - total_read);
        
        if (n == -1) {
            perror("Error reading file");
            free(buffer); 
            close(fd);
            return NULL;
        }
        
        if (n == 0) {
            break;
        }
        
        total_read += n;
    }

    buffer[total_read] = '\0';
    close(fd);
    
    return buffer;
}

void parse_passo(const char *line, int *passo) {
    sscanf(line, "PASSO %d", passo);
}

void parse_pos(board_t *board, const char *line, int *pos_x, int *pos_y) {
    int req_x, req_y;

    if (sscanf(line, "POS %d %d", &req_y, &req_x) == 2) {
        if (req_x < board->width && req_y < board->height) {
            pos_t valid = get_valid_start_pos(board, req_x, req_y);
            *pos_x = valid.x;
            *pos_y = valid.y;
        }
    }
}

int is_valid_move_char(char c) {
    return (c == 'W' || c == 'A' || c == 'S' || c == 'D' || c == 'R' || c == 'C' || c == 'Q' || c == 'G');
}

void parse_move_line(command_t *moves, int *move_idx, const char *line) {
    if (*move_idx >= MAX_MOVES) return; 

    command_t *c = &moves[*move_idx];
    int duration = 0;

    if (line[0] == 'T') {
        if (sscanf(line, "T %d", &duration) == 1) {
            c->command = 'T';
            c->turns = duration;
            c->turns_left = duration;
            (*move_idx)++;
        }
    } 
    else if (is_valid_move_char(line[0])) {
        c->command = line[0];
        c->turns = 1;
        c->turns_left = 1;
        (*move_idx)++;
    }
}

int load_pacman(board_t* board, int points, const char* dir) {
    pacman_t* pac = &board->pacmans[0];
    if (is_manual_control(board)) {
        load_pacman_ini(board, points);
        board->board[get_board_index(board, pac->pos_x, pac->pos_y)].content = 'P';
    return 0;
    }

    else {  //READFILE
        load_pacman_ini(board, points);
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, board->pacman_file); //GET PATH

        char* buffer = read_file_dynamic(fullpath);
        if (buffer == NULL) {
            board->board[get_board_index(board, pac->pos_x, pac->pos_y)].content = 'P';
            return 0;
        } else {
            char *line = strtok(buffer, "\n");
            int move_idx = 0;
            pac->moves[0].command = 'R'; //Initialize behavior to avoid not reading behavior instructions
            pac->moves[0].turns = 1;
            while (line != NULL) {
                if (line[0] == '#' || line[0] == '\0' || line[0] == '\r' || line[0] == 'C') {
                    line = strtok(NULL, "\n");
                    continue;
                }
                //PASSO
                if (strncmp(line, "PASSO", 5) == 0) {
                    parse_passo(line, &pac->passo);
                }
                //POS
                else if (strncmp(line, "POS", 3) == 0) {
                    parse_pos(board, line, &pac->pos_x, &pac->pos_y);
                }
                //MOVE
                else {
                    parse_move_line(pac->moves, &move_idx, line);
                }
                line = strtok(NULL, "\n");
            }

            if(move_idx == 0) { move_idx = 1;}
            pac->n_moves = move_idx;
            board->board[get_board_index(board, pac->pos_x, pac->pos_y)].content = 'P';
            free(buffer);
        }
    }

    return 0;
}

void load_ghost_ini(board_t* board, ghost_t* ghost) {   
    pos_t pos_t = get_valid_start_pos(board, 1, 1);
    ghost->pos_x = pos_t.x;
    ghost->pos_y = pos_t.y;
    ghost->n_moves = 1;
    ghost->current_move = 0;
    ghost->waiting = 0;
    ghost->charged = 0;
    ghost->passo = 1;
    ghost->moves[0].command = 'R'; //Initialize behavior to avoid not reading behavior instructions
    ghost->moves[0].turns = 1;

}
int load_ghost(board_t* board, const char* dir) {
    for (int i = 0; i < board->n_ghosts; i++) {
        ghost_t* ghost = &board->ghosts[i];
        load_ghost_ini(board, ghost);

        // GER PATH
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, board->ghosts_files[i]);

        // READ FILE
        char* buffer = read_file_dynamic(fullpath);
        if (buffer == NULL) {;
            return 0;
        }

        // PARSE
        char *line = strtok(buffer, "\n");
        int move_idx = 0;
        while (line != NULL) {
            if (line[0] == '#' || line[0] == '\0' || line[0] == '\r' 
                || line[0] == 'G' || line[0] == 'Q') {
                line = strtok(NULL, "\n");
                continue;
            }
            // PASSO
            if (strncmp(line, "PASSO", 5) == 0) {
                parse_passo(line, &ghost->passo);
            }
            // POS
            else if (strncmp(line, "POS", 3) == 0) {
                parse_pos(board, line, &ghost->pos_x, &ghost->pos_y); 
            }
            // MOVE
            else {
                parse_move_line(ghost->moves, &move_idx, line); 
            }
            line = strtok(NULL, "\n");
        }

        if(move_idx == 0) { move_idx = 1;}
        ghost->n_moves = move_idx;
        board->board[get_board_index(board, ghost->pos_x, ghost->pos_y)].content = 'M';
        free(buffer);
    }
    return 0;
}

void parse_dim(board_t *board, const char *line) {
    int h, w;
    if (sscanf(line, "DIM %d %d", &h, &w) == 2) {
        board->width = w;
        board->height = h;
        board->board = calloc(board->width * board->height, sizeof(board_pos_t));
        board->ghosts = calloc(MAX_GHOSTS, sizeof(ghost_t));
        board->pacmans = calloc(1, sizeof(pacman_t));
        board->n_pacmans = 1;
    }
}

void parse_tempo(board_t *board,const char *line) {
    int t;
    if (sscanf(line, "TEMPO %d", &t) == 1) {
        board->tempo = t;
    }
}

void parse_pac(board_t *board, const char *line) {
    char p_file[MAX_FILENAME];
    if (sscanf(line, "PAC %s", p_file) == 1) {
        strcpy(board->pacman_file, p_file);
    }
    board->n_pacmans++;
}

void parse_ghosts(board_t *board, const char *line) {
    const char *ptr = line + 3;
    char m_file[MAX_FILENAME];
    int offset;
    while (sscanf(ptr, "%s%n", m_file, &offset) == 1) {
        if (board->n_ghosts < MAX_GHOSTS) {
            strcpy(board->ghosts_files[board->n_ghosts], m_file);
            board->n_ghosts++;
        }
        ptr += offset;
    }
}

void parse_board(board_t *board, const char* line, int* map_row ) {
     if (board->board && (*map_row) < board->height) {
        for (int col = 0; col < board->width; col++) {
            int idx = (*map_row) * board->width + col;
            char c = line[col];
            if (c == 'X') {
                board->board[idx].content = 'W';
            } else if (c == '@') {
                board->board[idx].content = ' ';
                board->board[idx].has_portal = 1;
            } else {
                board->board[idx].content = ' ';
                board->board[idx].has_dot = 1;
            }
        }
        (*map_row)++;
    }
}

int load_level(board_t *board, int points, const char *level_path, const char *dir) {

    char *buffer = read_file_dynamic(level_path);
    if (buffer == NULL) {
        return -1;
    }
    board->n_ghosts = 0;
    board->n_pacmans = 0;
    board->pacman_file[0] = '\0';
    int map_row = 0;
    
    char *line = strtok(buffer, "\n");

    while (line != NULL) {
        if (line[0] == '#') {
            line = strtok(NULL, "\n");
            continue;
        }

        if (strncmp(line, "DIM", 3) == 0) {
            parse_dim(board, line);
        } else if (strncmp(line, "TEMPO", 5) == 0) {
            parse_tempo(board, line);
        } else if (strncmp(line, "PAC", 3) == 0) {
            parse_pac(board, line);
        } else if (strncmp(line, "MON", 3) == 0) {
            parse_ghosts(board, line);      
        } else {
            parse_board(board, line, &map_row);
        }

        line = strtok(NULL, "\n");
    }

    snprintf(board->level_name, sizeof(board->level_name), "Level %s", level_path);

    free(buffer);
    
    load_ghost(board, dir);
    load_pacman(board, points, dir);

    return 0;
}

void unload_level(board_t * board) {
    free(board->board);
    free(board->pacmans);
    free(board->ghosts);
}

void open_debug_file(char *filename) {
    debugfile = fopen(filename, "w");
}

void close_debug_file() {
    fclose(debugfile);
}

void debug(const char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(debugfile, format, args);
    va_end(args);

    fflush(debugfile);
}

void print_board(board_t *board) {
    if (!board || !board->board) {
        debug("[%d] Board is empty or not initialized.\n", getpid());
        return;
    }

    // Large buffer to accumulate the whole output
    char buffer[8192];
    size_t offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "=== [%d] LEVEL INFO ===\n"
                       "Dimensions: %d x %d\n"
                       "Tempo: %d\n"    
                       "Pacman file: %s\n",
                       getpid(), board->height, board->width, board->tempo, board->pacman_file);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "Monster files (%d):\n", board->n_ghosts);

    for (int i = 0; i < board->n_ghosts; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "  - %s\n", board->ghosts_files[i]);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n=== BOARD ===\n");

    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int idx = y * board->width + x;
            if (offset < sizeof(buffer) - 2) {
                buffer[offset++] = board->board[idx].content;
            }
        }
        if (offset < sizeof(buffer) - 2) {
            buffer[offset++] = '\n';
        }
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "==================\n");

    buffer[offset] = '\0';

    debug("%s", buffer);
}
