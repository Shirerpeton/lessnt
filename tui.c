#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>
#include "tui.h"

void init_buffer(buffer *buf, size_t rows, size_t cols);
void free_buffer(buffer *buf, size_t rows);
void init_str_buffer(size_t capacity, struct str_buffer *str_buf);
void free_str_buffer(struct str_buffer *str_buf);
void render_buffer_to_str(buffer *buf, struct str_buffer *str_buf, size_t rows, size_t cols);

const struct color DEFAULT_FG_COLOR = { .r = 255, .g = 255, .b = 255 };
const struct color DEFAULT_BG_COLOR = { .r = 10, .g = 10, .b = 10 };
const bool NO_DEFAULT_BG_COLOR = true; 

struct tui *init_tui(size_t rows, size_t cols) {
    struct tui *tui = malloc(sizeof(struct tui));
    tui->rows = rows;
    tui->cols = cols;
    init_buffer(&tui->buf, rows, cols);
    size_t str_buf_size = rows * cols * sizeof(wchar_t) * 20 + 1;
    init_str_buffer(str_buf_size, &tui->str_buf);
    return tui;
}

void free_tui(struct tui *tui) {
    free_buffer(&tui->buf, tui->rows);
    free_str_buffer(&tui->str_buf);
    free(tui);
}

void refresh(struct tui *tui) {
    render_buffer_to_str(&tui->buf, &tui->str_buf, tui->rows, tui->cols);
    fputws(tui->str_buf.data, stdout);
    fflush(stdout);
}

int print_tui(struct tui *tui, struct print_options print_opt, wchar_t *str) {
    int len = wcslen(str);
    if(print_opt.x + len > tui->cols ||
        print_opt.y > tui->rows) {
        return 1;
    }
    for(int i = 0; i < len; i++) {
        struct cell *curr_cell = &tui->buf[print_opt.y][print_opt.x + i];
        curr_cell->character = str[i];
        if(print_opt.fg_color != NULL) {
            curr_cell->fg_color.r = print_opt.fg_color->r;
            curr_cell->fg_color.g = print_opt.fg_color->g;
            curr_cell->fg_color.b = print_opt.fg_color->b;
        }
        if(print_opt.bg_color != NULL) {
            curr_cell->bg_color.r = print_opt.bg_color->r;
            curr_cell->bg_color.g = print_opt.bg_color->g;
            curr_cell->bg_color.b = print_opt.bg_color->b;
        }
    }
    return 0;
}

void clear(struct tui *tui) {
    for(int i = 0; i < tui->rows; i++) {
        for(int j = 0; j < tui->cols; j++) {
            struct cell *curr_cell = &tui->buf[i][j];
            curr_cell->character = L' ';
            curr_cell->fg_color = DEFAULT_FG_COLOR;
            curr_cell->bg_color = DEFAULT_BG_COLOR;
        }
    }
}

void init_buffer(buffer *buf, size_t rows, size_t cols) {
    *buf = malloc(rows * sizeof(struct cell *));
    for(size_t i = 0; i < rows; i++) {
        (*buf)[i] = malloc(cols * sizeof(struct cell));
        for(size_t j = 0; j < cols; j++) {
            (*buf)[i][j].character = L' ';
            (*buf)[i][j].fg_color = DEFAULT_FG_COLOR;
            (*buf)[i][j].bg_color = DEFAULT_BG_COLOR;
        }
    }
}

void free_buffer(buffer *buf, size_t rows) {
    for(size_t i = 0; i < rows; i++) {
        free((*buf)[i]);
    }
    free(*buf);
}

void init_str_buffer(size_t capacity, struct str_buffer *str_buf) {
    str_buf->capacity = capacity;
    str_buf->length = 0;
    str_buf->data = malloc(capacity);
}

void free_str_buffer(struct str_buffer *str_buf) {
    free(str_buf->data);
}

bool eq_colors(struct color *first, struct color *second) {
    return first->r == second->r &&
        first->g == second->g &&
        first->b == second->b;
}

void append_to_str(const wchar_t *src, struct str_buffer *str_buf) {
    wcscpy(str_buf->data + str_buf->length, src);
    str_buf->length += wcslen(src);
}

void append_char_to_str(const wchar_t src, struct str_buffer *str_buf) {
    str_buf->data[str_buf->length] = src;
    str_buf->length++;
}

void append_color_to_str(const wchar_t *format, struct str_buffer *str_buf, struct color color) {
    size_t written = swprintf(str_buf->data + str_buf->length,
            str_buf->capacity - str_buf->length,
            format,
            color.r,
            color.g,
            color.b);
    if(written < 0) {
    } else {
        str_buf->length += written;
    }
}

void render_buffer_to_str(buffer *buf, struct str_buffer *str_buf, size_t rows, size_t cols) {
    str_buf->length = 0;
    append_to_str(L"\e[0m\e[2J\e[H", str_buf); //reset mode, erase screen, return cursor to home position
    append_color_to_str(L"\e[38;2;%d;%d;%dm", str_buf, DEFAULT_FG_COLOR);
    if(!NO_DEFAULT_BG_COLOR) {
        append_color_to_str(L"\e[48;2;%d;%d;%dm", str_buf, DEFAULT_BG_COLOR);
    } 
    struct color prev_fg_color = DEFAULT_FG_COLOR;
    struct color prev_bg_color = DEFAULT_BG_COLOR;
    for(size_t i = 0; i < rows; i++) {
        for(size_t j = 0; j < cols; j++) {
            struct cell *cur_cell = &(*buf)[i][j];
            if(!eq_colors(&cur_cell->fg_color, &prev_fg_color)) {
                append_color_to_str(L"\e[38;2;%d;%d;%dm", str_buf, cur_cell->fg_color);
            }
            if(!eq_colors(&cur_cell->bg_color, &prev_bg_color)) {
                if(NO_DEFAULT_BG_COLOR && eq_colors(&cur_cell->bg_color, (struct color *)&DEFAULT_BG_COLOR)) {
                    append_to_str(L"\e[49m", str_buf);
                } else {
                    append_color_to_str(L"\e[48;2;%d;%d;%dm", str_buf, cur_cell->bg_color);
                }
            }
            append_char_to_str(cur_cell->character, str_buf);
            prev_fg_color = cur_cell->fg_color;
            prev_bg_color = cur_cell->bg_color;
        }
        append_char_to_str(L'\n', str_buf);
    }
    append_to_str(L"\e[0m", str_buf); //reset mode
}
