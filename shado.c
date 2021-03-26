/* -- Includes -- {{{ */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "rope.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define page_size sysconf(_SC_PAGESIZE)
/* }}} */
/* Data {{{ */
struct abuf {
    char *b;
    int len;
};
#define ABUF_INIT { NULL, 0 }

typedef struct erow {
    int idx;
    int size;
    char *render;
} erow;

struct Cursor {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
};

struct GlobalState {
    struct Cursor curs;
    int screenrows;
    int screencols;
    int numrows;
    int dirty;
    char *filename;

    struct termios orig_termios;
    
    rope *rope_head;
    erow *row;

    int mode; /* 0: normal, 1: insert, 2: visual, 3: visual_line, 4: visual_blk, 5: sreplace, 6: mrerplace, 10: misc */

    int print_flag; /* Makes sure not to print escape code keys */
};

/* Modes */
#define NORMAL 0
#define INSERT 1
#define VISUAL 2
#define REPLACE 3
#define MISC 10

#define SHADO_VERSION "0.0.1"
#define DEBUG 1
#define TAB_STOP 4
#define LEN(v) (int)(sizeof(v) / sizeof(*v))
#define CTRL_KEY(k) ((k) & 0x1f)

enum char_type {
    CHAR_AZ09 = 0,
    CHAR_SYM,
    CHAR_WHITESPACE,
    CHAR_NL,
};
/* }}} */
/* Code {{{ */
struct GlobalState E;
/*  Term {{{ */
void kill (const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disable_raw () { // Restores on exit to terminal's orig attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        kill("tcsetattr");
}

void enable_raw () {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) kill("tcgetarr");
    atexit(disable_raw); // at exit, do ...

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) kill("tcsetattr");
}

void set_cursor_type () {
    if(E.mode == 1) write(STDOUT_FILENO, "\x1b[6 q", 5);
    else if(E.mode == 10) write(STDOUT_FILENO, "\x1b[4 q", 5);
    else write(STDOUT_FILENO, "\x1b[2 q", 5);
}

int get_curs_pos (int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while  (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int get_win_size (int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_curs_pos(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void quit () {
    E.mode = NORMAL;
    set_cursor_type();
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    disable_raw();
    _rope_print(E.rope_head);
    rope_free(E.rope_head);
    printf("\nE.row[i].render: %s\n", E.row[0].render);
    printf("E.row[i].size: %d\n", E.row[0].size);
    printf("E.screenrows: %d\nE.screencols: %d\n", E.screenrows, E.screencols);
    free(E.filename);
    exit(0);
}
/* }}} */
void free_row (erow *row) {
    free(row->render);
}

void cache_row (int at, char *s, size_t len) {
    
}

void insert_row (int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    rope_append(E.rope_head, (const uint8_t*)s);
    if (at > E.screenrows-1) return;

    int j, tabs = 0;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at+1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;

    E.row[at].idx = at;
    E.row[at].size = len;
    for (j=0; j < E.row[at].size; j++) if (s[j] == '\t') tabs++;
    E.row[at].render = malloc(E.row[at].size + (tabs * (TAB_STOP-1)) + 1);

    int idx = 0;
    for (j=0; j < E.row[at].size; j++) {
        if (s[j] == '\t')
            while (idx % TAB_STOP != 0)
                E.row[at].render[idx++] = ' ';
    }
    E.row[at].render[idx] = '\0';
    memcpy(E.row[at].render, s, len);
    E.row[at].render[len] = '\0';
    
    E.numrows++;
    E.dirty++;
}

void open_file (char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) kill("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n'
                    || line[linelen - 1] == '\r')) linelen--;
        insert_row(E.numrows, line, linelen);
        if (E.numrows > E.screencols-1)
            cache_row(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}
/*  Entry {{{ */
void init () {
    E.curs.cx = 0;
    E.curs.cy = 0;
    E.curs.rx = 0;
    E.curs.rowoff = 0;
    E.curs.coloff = 0;
    E.numrows = 0;
    E.filename = NULL;
    E.dirty = 0;

    E.row = NULL;
    E.rope_head = rope_new();

    E.mode = NORMAL;
    E.print_flag = 1;

    if (get_win_size(&E.screenrows, &E.screencols) == -1) kill("getWindowSize");
    E.screenrows -= 2;
}

int main (int argc, char *argv[]) {
    enable_raw();
    init();
    if (argc >= 2) open_file(argv[1]);

    /* while (1) { */
    /*     refresh_screen(); */
    /*     process_keypress(); */
    /* } */
    quit();
    return 0;
}
/*  }}} */
/* }}} */
