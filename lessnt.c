#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/vfs.h>
#include "tui.h"

const size_t ROWS = 30;
const size_t COLS = 120;
const size_t LINE_NUMBER_PREFIX_SIZE = 6;
const size_t FILE_COLS = COLS - LINE_NUMBER_PREFIX_SIZE;

struct line {
    wchar_t *data;
    uint line_number;
};

struct chunk {
    struct line *lines;
    uint size;
};

struct file_context {
    FILE *file;
    struct chunk *chunks;
    uint cur_chunk;
    uint cur_position;
    uint total_lines;
    uint last_line_read;
    bool at_end;
};

int count_file_lines(FILE *file);
int print_file(struct tui *tui, struct file_context *file_context);
int init_file_context(FILE *file, uint file_lines, struct file_context *file_context);
void free_file_context(struct file_context *file_context);
//int scroll_file_context(struct file_context *file_context);

int main(int argc, char **argv) {
    srand(time(NULL));

    if(argc < 2) {
        puts("pass file name");
        return 1;
    }
    FILE *file = fopen(argv[1], "r");
    if(!file) {
        puts("file doesn't exist or you don't have permissions to read it");
        return 1;
    } 
    int file_lines = count_file_lines(file); 
    if(file_lines < 0) {
        puts("error counting file lines");
        return 1;
    }
    struct file_context file_context;
    setlocale(LC_CTYPE, "");
    init_file_context(file, (uint)file_lines, &file_context);
    fputws(L"\e[?1049h", stdout); //enable alternate buffer
    fputws(L"\e[?25l", stdout); //set cursor invisible
    fflush(stdout);

    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    struct tui *tui = init_tui(ROWS, COLS);

    char seq[3];
    struct color curr_fg_color = { .r = 255, .g = 255, .b = 0 };
    struct print_options print_opts = { .x = 10, .y = 10, .fg_color = &curr_fg_color, .bg_color = NULL };
    while(true) {
        clear(tui);
        print_file(tui, &file_context);
        refresh(tui);
        if(read(STDIN_FILENO, &seq[0], 1) > 0) {
            if(seq[0] == '\e') {
                if(read(STDIN_FILENO, &seq[1], 1) > 0 &&
                        read(STDIN_FILENO, &seq[2], 1) > 0) {
                    if(seq[1] == '[') {
                        switch(seq[2]) {
                            case 'A': //up arrow
                                break;
                            case 'B': //down arrow
                                //scroll_file_context(&file_context);
                                break;
                            case 'C': //right arrow
                                break;
                            case 'D': //left arrow
                                break;
                        }
                    }
                } else {
                    break;
                }
            } else if(seq[0] == 'q') {
                break;
            } else {
                switch(seq[0]) {
                    case 'j':
                        //scroll_file_context(&file_context);
                        break;
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    fputws(L"\e[?1049l", stdout); //disable alternate buffer
    fputws(L"\e[?25h", stdout); //set cursor invisible

    free_tui(tui);
    free_file_context(&file_context);

    return 0;
}

int count_file_lines(FILE *file) {
    wchar_t *buf = malloc(4096 * sizeof(wchar_t));
    int lines = 0;
    while(1) {
        size_t read_size = fread(buf, sizeof(wchar_t), 4096, file);
        if(ferror(file)) {
            return -1;
        }
        for(int i = 0; i < read_size; i++) {
            if(buf[i] == L'\n') {
                lines++;
            }
        }
        if(feof(file)) {
            break;
        }
    }
    free(buf);
    rewind(file);
    return lines;
}

int print_line(
        struct tui *tui,
        struct print_options *line_print_opts,
        struct print_options *print_opts,
        int *prev_line_number,
        wchar_t *line_num_buf,
        struct line *line) {
        if(*prev_line_number != line->line_number) {
            swprintf(line_num_buf, LINE_NUMBER_PREFIX_SIZE, L"%3d |", line->line_number);
            *prev_line_number = line->line_number;
        } else {
            wcpcpy(line_num_buf, L"    |");
        }
        int result = print_tui(tui, *line_print_opts, line_num_buf);
        if(result != 0) {
            return result;
        }
        result = print_tui(tui, *print_opts, line->data);
        if(result != 0) {
            return result;
        }
        line_print_opts->y++;
        print_opts->y++;

        return 0;
}

int print_file(struct tui *tui, struct file_context *file_context) {
    int result = 0;
    wchar_t line_nums[LINE_NUMBER_PREFIX_SIZE];
    struct color line_color = { .r = 200, .g = 50, .b = 80 };
    struct print_options line_print_opts = { .x = 0, .y = 0, .fg_color = &line_color, .bg_color = NULL };
    struct print_options print_opts = { .x = LINE_NUMBER_PREFIX_SIZE, .y = 0, .fg_color = NULL, .bg_color = NULL };
    int prev_line_number = -1;
    struct chunk *cur_chunk = &file_context->chunks[file_context->cur_chunk];
    for(uint i = file_context->cur_position; i < ROWS; i++) {
        struct line *cur_line = &cur_chunk->lines[i];
        result = print_line(tui, &line_print_opts, &print_opts, &prev_line_number, line_nums, cur_line);
    }
    struct chunk *next_chunk = &file_context->chunks[file_context->cur_chunk + 1];
    uint max_line = next_chunk->size < file_context->cur_position ? next_chunk->size : file_context->cur_position;
    for(uint i = 0; i < max_line; i++) {
        struct line *cur_line = &next_chunk->lines[i];
        result = print_line(tui, &line_print_opts, &print_opts, &prev_line_number, line_nums, cur_line);
    }
    return result;
}

int fill_chunk(struct file_context *file_context, uint chunk_idx) {
    struct chunk *chunk = &file_context->chunks[chunk_idx];
    chunk->size = 0;
    chunk->lines = malloc(ROWS * sizeof(struct line));
    for(uint i = 0; i < ROWS; i++) {
        chunk->lines[i].data = malloc(FILE_COLS * sizeof(wchar_t));
    }
    for(uint i = 0; i < ROWS; i++) {
        struct line *cur_line = &chunk->lines[i];
        fgetws(cur_line->data, FILE_COLS, file_context->file);
        cur_line->line_number = file_context->last_line_read;
        uint cur_buf_length = wcslen(cur_line->data);
        for(uint j = 0; j < cur_buf_length; j++) {
            if(cur_line->data[j] == L'\n') {
                file_context->last_line_read++;
                cur_line->data[j] = L' ';
            }
            if(cur_line->data[j] == L'\r') {
                cur_line->data[j] = L' ';
            }
        }
        if(ferror(file_context->file)) {
            return -1; 
        }
        if(feof(file_context->file)) {
            file_context->at_end = true;
            break;
        }
        chunk->size++;
    }
    return 0;
}

int init_file_context(FILE *file, uint file_lines, struct file_context *file_context) {
    file_context->file = file;
    file_context->at_end = false;
    file_context->total_lines = file_lines;
    file_context->cur_chunk = 0;
    file_context->cur_position = 0;
    uint total_chunks = ceil((double)file_lines / ROWS);
    file_context->chunks = malloc(total_chunks * sizeof(struct chunk));
    file_context->last_line_read = 0;
    int status = fill_chunk(file_context, 0); 
    if(status != 0) {
        return status;
    }
    return 0;
}

//int scroll_file_context(struct file_context *file_context) {
//    if(file_context->at_end) {
//        return 0;
//    }
//    struct line *cur_line = &file_context->lines[file_context->position];
//
//    fgetws(cur_line->data, FILE_COLS, file_context->file);
//    cur_line->line_number = file_context->last_line_read;
//    uint cur_buf_length = wcslen(cur_line->data);
//    for(uint j = 0; j < cur_buf_length; j++) {
//        if(cur_line->data[j] == L'\n') {
//            file_context->last_line_read++;
//            cur_line->data[j] = L' ';
//        }
//        if(cur_line->data[j] == L'\r') {
//            cur_line->data[j] = L' ';
//        }
//    }
//    if(ferror(file_context->file)) {
//        return -1; 
//    }
//    if(feof(file_context->file)) {
//        file_context->at_end = true;
//    }
//    if(file_context->position < ROWS - 1) {
//        file_context->position++;
//    } else {
//        file_context->position = 0;
//    }
//    return 0;
//}

void free_file_context(struct file_context *file_context) {
    fclose(file_context->file);
    for(uint i = 0; i < ceil((double)file_context->total_lines / ROWS); i++) {
        for(uint j = 0; j < ROWS; j++) {
            free(file_context->chunks[i].lines[j].data);
        }
        free(file_context->chunks[i].lines);
    }
    free(file_context->chunks);
}

