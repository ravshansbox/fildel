#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>

#ifndef VERSION
#define VERSION "1.3.0"
#endif

#define INITIAL_CAPACITY 1024
#define MAX_LINE_LEN 8192

typedef struct {
    char *text;
    size_t len;
    int selected;
} Line;

typedef struct {
    Line *lines;
    size_t count;
    size_t capacity;
} Buffer;

typedef struct {
    int *indices;
    size_t count;
    size_t capacity;
} FilterResult;

static Buffer buffer = {0};
static FilterResult filtered = {0};
static char search_buf[256] = {0};
static size_t search_len = 0;
static int cursor_pos = 0;
static int scroll_offset = 0;
static int needs_filter = 1;
static char *filename = NULL;
static int modified = 0;

static void buffer_init(Buffer *b) {
    b->capacity = INITIAL_CAPACITY;
    b->lines = calloc(b->capacity, sizeof(Line));
    b->count = 0;
}

static void buffer_free(Buffer *b) {
    for (size_t i = 0; i < b->count; i++) {
        free(b->lines[i].text);
    }
    free(b->lines);
    b->lines = NULL;
    b->count = 0;
    b->capacity = 0;
}

static void buffer_add_line(Buffer *b, const char *text, size_t len) {
    if (b->count >= b->capacity) {
        b->capacity *= 2;
        b->lines = realloc(b->lines, b->capacity * sizeof(Line));
    }
    b->lines[b->count].text = strndup(text, len);
    b->lines[b->count].len = len;
    b->lines[b->count].selected = 0;
    b->count++;
}

static void filter_init(FilterResult *f) {
    f->capacity = INITIAL_CAPACITY;
    f->indices = calloc(f->capacity, sizeof(int));
    f->count = 0;
}

static void filter_free(FilterResult *f) {
    free(f->indices);
    f->indices = NULL;
    f->count = 0;
    f->capacity = 0;
}

static void filter_add(FilterResult *f, int idx) {
    if (f->count >= f->capacity) {
        f->capacity *= 2;
        f->indices = realloc(f->indices, f->capacity * sizeof(int));
    }
    f->indices[f->count++] = idx;
}

static int read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "fildel: cannot open '%s'\n", filename);
        return 0;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) > 0) {
        if (linelen > 0 && line[linelen-1] == '\n') {
            linelen--;
        }
        buffer_add_line(&buffer, line, linelen);
    }

    free(line);
    fclose(fp);
    return 1;
}

static int save_file(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    for (size_t i = 0; i < buffer.count; i++) {
        fprintf(fp, "%s\n", buffer.lines[i].text);
    }

    fclose(fp);
    modified = 0;
    return 1;
}

static void apply_filter(void) {
    filtered.count = 0;
    
    for (size_t i = 0; i < buffer.count; i++) {
        if (search_len == 0) {
            filter_add(&filtered, (int)i);
        } else {
            if (strcasestr(buffer.lines[i].text, search_buf)) {
                filter_add(&filtered, (int)i);
            }
        }
    }
    
    if (cursor_pos >= (int)filtered.count) {
        cursor_pos = (int)filtered.count - 1;
    }
    if (cursor_pos < 0) {
        cursor_pos = 0;
    }
    needs_filter = 0;
}

static int has_selected(void) {
    for (size_t i = 0; i < buffer.count; i++) {
        if (buffer.lines[i].selected) return 1;
    }
    return 0;
}

static void delete_selected(void) {
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < buffer.count; read_idx++) {
        if (buffer.lines[read_idx].selected) {
            free(buffer.lines[read_idx].text);
        } else {
            if (write_idx != read_idx) {
                buffer.lines[write_idx] = buffer.lines[read_idx];
            }
            write_idx++;
        }
    }
    buffer.count = write_idx;
    modified = 1;
    needs_filter = 1;
}

static void delete_line(int filtered_idx) {
    if (filtered_idx < 0 || filtered_idx >= (int)filtered.count) return;
    
    int buf_idx = filtered.indices[filtered_idx];
    
    free(buffer.lines[buf_idx].text);
    
    memmove(&buffer.lines[buf_idx], &buffer.lines[buf_idx + 1],
            (buffer.count - buf_idx - 1) * sizeof(Line));
    buffer.count--;
    
    modified = 1;
    needs_filter = 1;
}

static void draw_ui(void) {
    clear();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    attron(A_BOLD);
    mvprintw(0, 0, "Filter: %s%s", search_buf, modified ? " [modified]" : "");
    attroff(A_BOLD);
    
    int display_count = max_y - 2;
    
    if (cursor_pos < scroll_offset) {
        scroll_offset = cursor_pos;
    }
    if (cursor_pos >= scroll_offset + display_count) {
        scroll_offset = cursor_pos - display_count + 1;
    }
    
    int total_lines = (int)filtered.count;
    if (scroll_offset > total_lines - display_count) {
        scroll_offset = total_lines - display_count;
    }
    if (scroll_offset < 0) {
        scroll_offset = 0;
    }
    
    for (int i = 0; i < display_count && (scroll_offset + i) < (int)filtered.count; i++) {
        int filtered_idx = scroll_offset + i;
        int buf_idx = filtered.indices[filtered_idx];
        Line *line = &buffer.lines[buf_idx];
        
        int y = i + 1;
        
        if (filtered_idx == cursor_pos) {
            attron(A_REVERSE);
        }
        
        char checkbox[4];
        snprintf(checkbox, sizeof(checkbox), "[%c]", line->selected ? 'x' : ' ');
        
        mvprintw(y, 0, "%s ", checkbox);
        
        int avail_width = max_x - 5;
        if (avail_width < 0) avail_width = 0;
        
        char display[MAX_LINE_LEN];
        strncpy(display, line->text, avail_width);
        display[avail_width] = '\0';
        
        mvprintw(y, 4, "%s", display);
        
        if (filtered_idx == cursor_pos) {
            attroff(A_REVERSE);
        }
    }
    
    attron(A_DIM);
    mvprintw(max_y - 1, 0, "%zu/%zu lines | arrows:navigate SPACE:select d:delete r:reverse w:save q:quit", 
             filtered.count, buffer.count);
    attroff(A_DIM);
    
    refresh();
}

static int compare_lines(const void *a, const void *b) {
    const Line *la = *(const Line **)a;
    const Line *lb = *(const Line **)b;
    return strcmp(lb->text, la->text);
}

static int compare_lines_forward(const void *a, const void *b) {
    const Line *la = *(const Line **)a;
    const Line *lb = *(const Line **)b;
    return strcmp(la->text, lb->text);
}

static void sort_lines(int reverse) {
    if (buffer.count < 2) return;

    Line **temp = malloc(buffer.count * sizeof(Line *));
    for (size_t i = 0; i < buffer.count; i++) {
        temp[i] = &buffer.lines[i];
    }

    qsort(temp, buffer.count, sizeof(Line *), reverse ? compare_lines : compare_lines_forward);

    Line *new_lines = calloc(buffer.capacity, sizeof(Line));
    for (size_t i = 0; i < buffer.count; i++) {
        new_lines[i] = *temp[i];
    }

    free(buffer.lines);
    buffer.lines = new_lines;
    free(temp);

    modified = 1;
    needs_filter = 1;
}

static int confirm_quit(void) {
    if (!modified) return 1;
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    attron(A_BOLD | A_REVERSE);
    mvprintw(max_y - 1, 0, " Save changes before quitting? (y/n/c) ");
    attroff(A_BOLD | A_REVERSE);
    clrtoeol();
    refresh();
    
    int ch;
    while ((ch = getch())) {
        if (ch == 'y' || ch == 'Y') {
            if (save_file(filename)) {
                return 1;
            } else {
                attron(A_BOLD | A_REVERSE);
                mvprintw(max_y - 1, 0, " Error saving file! Press any key ");
                attroff(A_BOLD | A_REVERSE);
                clrtoeol();
                refresh();
                getch();
                return 0;
            }
        } else if (ch == 'n' || ch == 'N') {
            return 1;
        } else if (ch == 'c' || ch == 'C' || ch == 27) {
            return 0;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("fildel %s\n", VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <filename>\n", argv[0]);
        printf("\n");
        printf("Interactive file line editor with filtering.\n");
        printf("\n");
        printf("Keys:\n");
        printf("  arrows/j/k    Navigate\n");
        printf("  SPACE         Select line\n");
        printf("  d             Delete selected or current line\n");
        printf("  r             Toggle sort order (reverse/forward)\n");
        printf("  w             Save file\n");
        printf("  q             Quit\n");
        return 0;
    }

    filename = argv[1];
    
    buffer_init(&buffer);
    filter_init(&filtered);
    
    if (!read_file(filename)) {
        buffer_free(&buffer);
        filter_free(&filtered);
        return 1;
    }
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    sort_lines(1);
    apply_filter();
    
    int running = 1;
    while (running) {
        if (needs_filter) {
            apply_filter();
        }
        draw_ui();
        
        int ch = getch();
        
        switch (ch) {
            case 'q':
            case 'Q':
                if (confirm_quit()) {
                    running = 0;
                }
                break;
                
            case 'w':
            case 'W':
                if (save_file(filename)) {
                    modified = 1;
                }
                break;
                
            case KEY_UP:
            case 'k':
                if (cursor_pos > 0) {
                    cursor_pos--;
                }
                break;
                
            case KEY_DOWN:
            case 'j':
                if (cursor_pos < (int)filtered.count - 1) {
                    cursor_pos++;
                }
                break;
                
            case KEY_PPAGE:
                cursor_pos -= 10;
                if (cursor_pos < 0) cursor_pos = 0;
                break;
                
            case KEY_NPAGE:
                cursor_pos += 10;
                if (cursor_pos >= (int)filtered.count) {
                    cursor_pos = (int)filtered.count - 1;
                }
                break;
                
            case KEY_HOME:
                cursor_pos = 0;
                break;
                
            case KEY_END:
                cursor_pos = (int)filtered.count - 1;
                break;
                
            case ' ':
                if (cursor_pos >= 0 && cursor_pos < (int)filtered.count) {
                    int buf_idx = filtered.indices[cursor_pos];
                    buffer.lines[buf_idx].selected = !buffer.lines[buf_idx].selected;
                }
                break;
                
            case 'd':
            case 'D':
                if (has_selected()) {
                    delete_selected();
                } else {
                    delete_line(cursor_pos);
                }
                break;
                
            case 'r':
            case 'R':
                sort_lines(0);
                break;
                
            case KEY_BACKSPACE:
            case 127:
            case '\b':
                if (search_len > 0) {
                    search_buf[--search_len] = '\0';
                    needs_filter = 1;
                }
                break;
                
            case KEY_DC:
                search_buf[0] = '\0';
                search_len = 0;
                needs_filter = 1;
                break;
                
            case 27:
                search_buf[0] = '\0';
                search_len = 0;
                needs_filter = 1;
                break;
                
            default:
                if (isprint(ch) && search_len < sizeof(search_buf) - 1) {
                    search_buf[search_len++] = (char)ch;
                    search_buf[search_len] = '\0';
                    needs_filter = 1;
                }
                break;
        }
    }
    
    endwin();
    
    buffer_free(&buffer);
    filter_free(&filtered);
    
    return 0;
}
