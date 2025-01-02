#ifndef TERM_H
#define TERM_H

#include "raylib.h"
#include <semaphore.h>
#include <pthread.h>

#define TERM_INPUT_BUF_SIZE 256
#define TERM_CHAR_SIZE (conf.font_size * 0.6)

typedef struct {
    pthread_mutex_t lock;
    Rectangle size;
    int char_w, char_h;
    int cursor_pos;
    Vector2 char_size;
    char (*buffer)[5];

    sem_t input_sem;
    char input_buf[TERM_INPUT_BUF_SIZE];
    int buf_start;
    int buf_end;
} Terminal;

extern Terminal term;

void term_init(void);
void term_input_put_char(char ch);
char term_input_get_char(void);
void term_scroll_down(void);
int term_print_str(const char* str);
int term_print_int(int value);
int term_print_double(double value);
void term_clear(void);
void term_resize(void);
void term_free(void);
void term_restart(void);

#endif // TERM_H
