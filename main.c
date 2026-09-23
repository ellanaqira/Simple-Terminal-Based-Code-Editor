
/*** Includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>


/*** Defines ***/
#define HOO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};


/*** Data ***/
typedef struct erow {       // erow = editor row
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx, cy;
    int row_offset;
    int screen_rows;
    int screen_cols;

    int numrows;
    erow *row;

    struct termios orig_termios;
};
struct editorConfig E;

// append buffer ---
struct abuf {
    char *b;
    int len;
};


/*** Functions Declaration ***/
// Terminal ---
void die(const char *s);
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
// row operation ---
void editorAppendRow(char *s, size_t len);
// file i/o
void editorOpen(char *filename);
// append buffer ---
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);
// Output ---
void editorScroll();
void editorDrawRows();
void editorRefreshScreen();
// Input ---
void editorProcessKeypress();
void editorMoveCursor(int key);
// Init ---
void initEditor();


/*** Main ***/
int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}


/*** Function Definition ***/
// Terminal ---
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {     // fetch current terminal settings (attributes) and copies them into orig_termios struct
        die("tcgetattr");
    }
    atexit(disableRawMode);                                 // execute disableRawMode if program ends normally

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {         // apply changes to the terminal
        die("tcsetattr");
    }
}

void disableRawMode() {
    printf("Exit Raw-Mode\r\n");
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }

    if (c == '\x1b') {
        char seq[3];

        // read two more bytes into seq buffer
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }

            else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }

        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }

    else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        ++i;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

// row operation ---
void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
}

// file i/o
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        die("fopen");
    }

    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &lineCapacity, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }

        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

// Append Buffer ---
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        return;
    }

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

// Output ---
void editorScroll() {
    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.screen_rows) {
        E.row_offset = E.cy - E.screen_rows + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y=0; y < E.screen_rows; ++y) {
        int filerow = y + E.row_offset;
        if (filerow >= E.numrows) {    

            // Hoo and Version
            if (E.numrows == 0 && y == E.screen_rows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Sector - version %s ", HOO_VERSION);

                if (welcomelen > E.screen_cols) {           // if terminal is too tiny to fit welcome message,
                    welcomelen = E.screen_cols;             // truncate the length of the welcomelen
                }

                int padding_1st_l = (E.screen_cols - welcomelen) / 2;
                if (padding_1st_l) {
                    abAppend(ab, "~", 1);
                    padding_1st_l--;
                }

                while (padding_1st_l--) {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, welcome, welcomelen);
            }

            // Created by Ellan Aqira
            else if (E.numrows == 0 && y == (E.screen_rows / 3) + 1) {
                char creator[50];
                int creatorlen = snprintf(creator, sizeof(creator), "by  Ellan Aqira.");

                if (creatorlen > E.screen_cols) {
                    creatorlen = E.screen_cols;
                }

                int padding_2st_l = (E.screen_cols - creatorlen) / 2;
                if (padding_2st_l) {
                    abAppend(ab, "~", 1);
                    --padding_2st_l;
                }

                while (--padding_2st_l) {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, creator, creatorlen);
            }

            // Hoo is open source!
            else if (E.numrows == 0 && y == (E.screen_rows / 3) + 2) {
                char str[50];
                int strlen = snprintf(str, sizeof(str), "Sector is open source!");

                if (strlen > E.screen_cols) {
                    strlen = E.screen_cols;
                }

                int padding_3st_l = (E.screen_cols - strlen) / 2;
                if (padding_3st_l) {
                    abAppend(ab, "~", 1);
                    --padding_3st_l;
                }

                while (--padding_3st_l) {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, str, strlen);
            }

            // type ctrl + q to exit
            else if (E.numrows == 0 && y == (E.screen_rows / 3) + 3) {
                char howtoexit[50];
                int howtoexitlen = snprintf(howtoexit, sizeof(howtoexit), "type  : 'ctrl + q' to exit");

                if (howtoexitlen > E.screen_cols) {
                    howtoexitlen = E.screen_cols;
                }

                int padding_4st_l = (E.screen_cols - howtoexitlen) / 2;
                if (padding_4st_l) {
                    abAppend(ab, "~", 1);
                    --padding_4st_l;
                }

                while (--padding_4st_l) {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, howtoexit, howtoexitlen);
            }


            else {
                abAppend(ab, "~", 1);
            }
        }

        else {
            int len = E.row[filerow].size;
            if (len > E.screen_cols) {
                len = E.screen_cols;
            }
            abAppend(ab, E.row[filerow].chars, len);
        }

        abAppend(ab, "\x1b[K", 3);          // [K = erases part of the current line

        if (y < E.screen_rows-1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);          // [?25l = hide the cursor
    abAppend(&ab, "\x1b[H", 3);             // writing 3 bytes escape sequence to set the cursor at 1st row and 1st column

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);          // ?25h = show the cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// Input ---
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            }
            break;
        case ARROW_RIGHT:
            if (E.cx != E.screen_cols - 1) {
                E.cx++;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            E.cx = E.screen_cols - 1;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screen_rows;
                while (times--) {
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

// Init ---
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.row_offset = 0;
    E.numrows = 0;
    E.row = NULL;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
}