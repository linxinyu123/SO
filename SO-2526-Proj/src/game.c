#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h> 
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define REQUEST_SAVE 5
#define PAC_SLEEP_TIME 100 //ms
#define GHOST_SLEEP_TIME 100 //ms


int has_saved = 0;
int game_result = 0;
pthread_t p_thread;        
pthread_t g_threads[MAX_GHOSTS];
pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str) return 0;
    return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}

LevelEntry* get_levels(const char* dir_path, int* out_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        printf("Error: can't open folder. %s\n", dir_path);
        *out_count = 0;
        return NULL;
    }

    struct dirent *entry;
    LevelEntry *list = NULL;
    int count = 0;
    int capacity = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (ends_with(entry->d_name, ".lvl")) {
            
            if (count >= capacity) {
                capacity = (capacity == 0) ? 4 : capacity * 2;
                LevelEntry *temp = realloc(list, capacity * sizeof(LevelEntry));
                if (!temp) { 
                    free(list); 
                    closedir(dir); 
                    return NULL; 
                }
                list = temp;
            }

            sprintf(list[count].full_path, "%s/%s", dir_path, entry->d_name);
            
            count++;
        }
    }
    closedir(dir);

    *out_count = count;
    return list;
}

void handle_pacman_death() {
    if (has_saved == 1) {
        pthread_mutex_unlock(&board_mutex); 
        exit(LOAD_BACKUP); 
    } else {
        game_result = QUIT_GAME;
    }
}

void* ghost_thread_func(void* arg) {
    ghost_thread_args_t* args = (ghost_thread_args_t*) arg;

    board_t* game_board = args->board;
    ghost_t* ghost = args->ghost;
    int id = args->id;

    free(args);

    while (true) { 
        if (ghost->n_moves <= 0) {
             sleep_ms(GHOST_SLEEP_TIME);     
             continue;
        }

        pthread_mutex_lock(&board_mutex);
        int current_status = game_result;
        pthread_mutex_unlock(&board_mutex);

        if (current_status != CONTINUE_PLAY) {
            sleep(1); 
            continue;
        }

        command_t* c = &ghost->moves[ghost->current_move % ghost->n_moves];
    
        pthread_mutex_lock(&board_mutex);

        move_ghost(game_board, id, c);

        if (!game_board->pacmans[0].alive) {
            handle_pacman_death();
        }

        pthread_mutex_unlock(&board_mutex);

        sleep_ms(GHOST_SLEEP_TIME);
    }
    return NULL;
}

void* pacman_thread_func(void* arg) {
    board_t* game_board = (board_t*) arg;
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    command_t c;

    while (true) {
        if (pacman->n_moves == 0) {
            c.command = get_input();

            if(c.command == '\0')
                continue;

            c.turns = 1;
            play = &c;
        }
        else {
            play = &pacman->moves[pacman->current_move % pacman->n_moves];
            sleep_ms(PAC_SLEEP_TIME);
        }

        if (play->command == 'Q') {
            pthread_mutex_lock(&board_mutex);
            game_result = QUIT_GAME;
            pthread_mutex_unlock(&board_mutex);
            break;
        }

        if (play->command == 'G'  ) {
            if (has_saved == 0) {
                pthread_mutex_lock(&board_mutex);
                game_result = REQUEST_SAVE;
                pthread_mutex_unlock(&board_mutex);
                sleep_ms(game_board->tempo);
                continue;
            }
        }

        pthread_mutex_lock(&board_mutex);

        int result = move_pacman(game_board, 0, play);

        if (result == REACHED_PORTAL) {
            game_result = NEXT_LEVEL;
        }

        if (result == DEAD_PACMAN) {
            handle_pacman_death();
        }
        
        if (!game_board->pacmans[0].alive) {
            handle_pacman_death();
        }

        pthread_mutex_unlock(&board_mutex);

        sleep_ms(PAC_SLEEP_TIME);
    }
    return NULL;
}

void start_threads(board_t* board) {

    if (pthread_create(&p_thread, NULL, pacman_thread_func, board) != 0) {
        perror("Failed to create Pacman thread");
        exit(1);
    }

    for (int i = 0; i < board->n_ghosts; i++) {
        if (i >= MAX_GHOSTS) break;

        ghost_thread_args_t *args = malloc(sizeof(ghost_thread_args_t));
        if (args == NULL) {
            perror("Malloc failed");
            exit(1);
        }

        args->board = board;  
        args->ghost = &board->ghosts[i]; 
        args->id = i;


        if (pthread_create(&g_threads[i], NULL, ghost_thread_func, args) != 0) {
            perror("Failed to create Ghost thread");
            free(args);
        }
    }
}


void stop_threads(board_t* board) {
    pthread_cancel(p_thread);
    pthread_join(p_thread, NULL);

    for (int i = 0; i < board->n_ghosts; i++) {
        if (i >= MAX_GHOSTS) break;
        
        pthread_cancel(g_threads[i]);
        pthread_join(g_threads[i], NULL);
    }
}

void save_point(board_t* board) {
    stop_threads(board);
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
    } 
    else if (pid == 0) {
        has_saved = 1;
        game_result = CONTINUE_PLAY;
        start_threads(board);
    } 
    else {
        int status;
        wait(&status); 
        if (WIFEXITED(status) && WEXITSTATUS(status) == LOAD_BACKUP) {
            has_saved = 0; 
            game_result = CONTINUE_PLAY;
            start_threads(board);
        } 
        else {
            exit(0);
        }
    }
}


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        // TODO receive inputs
        return 1;
    }
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();

    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board;

    int total_levels = 0;
    int current_lvl;
    LevelEntry *level_list = get_levels(argv[1], &total_levels);

    if (total_levels == 0 || level_list == NULL) {
        printf("Failed\n");
        return 1;
    }
    
    while (!end_game && current_lvl < total_levels) {
        pthread_mutex_init(&board_mutex, NULL);
        game_result = CONTINUE_PLAY;
        load_level(&game_board, accumulated_points, level_list[current_lvl].full_path, argv[1]);
        start_threads(&game_board);

        pthread_mutex_lock(&board_mutex);
        draw_board(&game_board, DRAW_MENU);
        pthread_mutex_unlock(&board_mutex);

        refresh_screen();

        while(true) {
            pthread_mutex_lock(&board_mutex);
            draw_board(&game_board, DRAW_MENU);
            int result = game_result;
            pthread_mutex_unlock(&board_mutex);

            if(result == REQUEST_SAVE) {
                save_point(&game_board);
                continue;
            }  

            if(result == NEXT_LEVEL) {
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(game_board.tempo);
                current_lvl++;
                break;
            }

            if(result == QUIT_GAME) {
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                end_game = true;
                break;
            } 
    
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;
            sleep_ms(50);      
        }
        stop_threads(&game_board);
        print_board(&game_board);
        unload_level(&game_board);
    }    
    free(level_list);

    terminal_cleanup();

    close_debug_file();

    return 0;
}
