
/*** Includes ***/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** Defines ***/
#define HOO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}


/*** Data ***/
struct editorConfig {
    int cx, cy;
    int screen_rows;
    int screen_cols;
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
char editorReadKey();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
// append buffer ---
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);
// Output ---
void editorDrawRows();
void editorRefreshScreen();
// Input ---
void editorProcessKeypress();
void editorMoveCursor(char key);
// Init ---
void initEditor();


/*** Main ***/
int main() {
    enableRawMode();
    initEditor();

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

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    return c;
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
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y=0; y < E.screen_rows; ++y) {
        // Hoo and Version
        if (y == E.screen_rows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Hoo -- version %s", HOO_VERSION);

            if (welcomelen > E.screen_cols) {           // if terminal is too tiny to fit welcome message,
                welcomelen = E.screen_cols;             // truncate the length of the welcomelen
            }

            int padding_1st_l = (E.screen_cols - welcomelen) / 2;
            if (padding_1st_l) {
                abAppend(ab, "~", 1);
                --padding_1st_l;
            }

            while (--padding_1st_l) {
                abAppend(ab, " ", 1);
            }
            abAppend(ab, welcome, welcomelen);
        }

        // Created by Ellan Aqira
        else if (y == (E.screen_rows / 3) + 1) {
            char creator[50];
            int creatorlen = snprintf(creator, sizeof(creator), "by Ellan Aqira.");

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
        else if (y == (E.screen_rows / 3) + 2) {
            char str[50];
            int strlen = snprintf(str, sizeof(str), "Hoo is open source!");

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
        else if (y == (E.screen_rows / 3) + 3) {
            char howtoexit[50];
            int howtoexitlen = snprintf(howtoexit, sizeof(howtoexit), "type  :'ctrl + q' to exit");

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

        abAppend(ab, "\x1b[K", 3);          // [K = erases part of the current line

        if (y < E.screen_rows-1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
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
void editorMoveCursor(char key) {
    switch (key) {
        case 'a':
            E.cx--;
            break;
        case 'd':
            E.cx++;
            break;
        case 'w':
            E.cy--;
            break;
        case 's':
            E.cy++;
            break;
    }
}

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        case 'w':
        case 'a':
        case 's':
        case 'd':
            editorMoveCursor(c);
            break;
    }
}

// Init ---
void initEditor() {
    E.cx = 0;
    E.cy = 0;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) {
        die("getWindowSize");
    }
}