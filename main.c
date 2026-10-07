/*** Includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** Defines ***/
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0};

/*** Data ***/
// Editor Configuration ---
struct editorConfig {
    int screenrows;
    int screencols;
    struct termios OrigTermSet;
};
struct editorConfig editor;
// Append Buffer ---
struct appendBuffer {
    char *buf;
    int len;
};


/*** Functions Declaration ***/
// Terminal ---
void die(const char *s);
void disableRawMode();
void enableRawMode();
char editorReadKey();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
// Append Buffer ---
void abAppend(struct appendBuffer *buffer, const char *s, int length);
void abFree(struct appendBuffer *buffer);
// Output ---
void editorDrawRows(struct appendBuffer *buffer);
void editorRefreshScreen();
// Input ---
int editorProcessKeypress();
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


/*** Functions Definition ***/
// Terminal ---
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.OrigTermSet) == -1) die("tcsetattr");

    write(STDOUT_FILENO, "\x1b[?1049l", 8); // restore cursor position and disable alternative buffer
    printf("row = %d\r\n", editor.screenrows);
    printf("col = %d\r\n", editor.screencols);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &editor.OrigTermSet) == -1) die("tcgetattr");

    struct termios ModifTermSet = editor.OrigTermSet;
    ModifTermSet.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    ModifTermSet.c_oflag &= ~(OPOST);
    ModifTermSet.c_cflag |= (CS8);
    ModifTermSet.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    ModifTermSet.c_cc[VMIN] = 0;
    ModifTermSet.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ModifTermSet) == -1) die("tcsetattr");

    write(STDOUT_FILENO, "\x1b[?1049h", 8); // save cursor position and enable alternative buffer

    atexit(disableRawMode); // call disableRawMode when the program terminates normally
}

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buff[32];
    unsigned int i = 0;
    // n = Requests and reports the general status,
    // with parameter 6 to report cursor's active position,
    // and then send the cursor position to stdin.
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buff)-1) {
        // store the cursor position to buff
        if(read(STDIN_FILENO, &buff[i], 1) != 1) break;
        if(buff[i] == 'R') break;
        i++;
    }
    buff[i] = '\0';

    if (buff[0] != '\x1b' || buff[1] != '[') return -1;
    if (sscanf(&buff[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize winsz;

    // ioctl() isn’t guaranteed to be able to request the window size on all systems,
    // so we are going to provide a fallback method of getting the window size.
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz) == -1 || winsz.ws_col == 0) {
        // move the cursor right and downward until hit the margin
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B" , 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = winsz.ws_col;
        *rows = winsz.ws_row;
        return 0;
    }
}

// Append Buffer ---
void abAppend(struct appendBuffer *buffer, const char *s, int length) {
    // resize the memory of buf from struct buffer and store the reaturn at new_buff
    char *new_buff = realloc(buffer->buf, buffer->len + length);

    if (new_buff == NULL) return;   // exit function if new_buff == NULL
    memcpy(&new_buff[buffer->len], s, length);  // copy the s to new_buff at index len
    buffer->buf = new_buff; // store the content of new_buff to buf
    buffer->len += length;  // add previous len with lenght and store it at len

}

void abFree(struct appendBuffer *buffer) {
    free(buffer->buf);
}

// Output ---
void editorDrawRows(struct appendBuffer *buffer) {
    int y;
    for(y=0; y < editor.screenrows; y++) {
        // string placement based on rows
        if (y == editor.screenrows / 3) {
        // Sector and Version
            char sectorVer[50];
            int sectorVerLen = snprintf(sectorVer, sizeof(sectorVer), "Sector -- version %s", KILO_VERSION);

            // string placement based on columns
            if (sectorVerLen > editor.screencols) sectorVerLen = editor.screencols;
            int padding1 = (editor.screencols - sectorVerLen) / 2;
            if (padding1) {
                abAppend(buffer, "~", 1);
                padding1--;
            }
            while (padding1 != 0) {
                abAppend(buffer, " ", 1);
                padding1 --;
            }
            abAppend(buffer, sectorVer, sectorVerLen);

        // Made by Ellan Aqira
            abAppend(buffer, "\r\n", 2);
            char madeBy[50];
            int madeByLen = snprintf(madeBy, sizeof(madeBy), "Made by Ellan Aqira");

            // string placement based on columns
            if (madeByLen > editor.screencols) madeByLen = editor.screencols;
            int padding2 = (editor.screencols - madeByLen) / 2;
            if (padding2) {
                abAppend(buffer, "~", 1);
                padding2--;
            }
            while (padding2 != 0) {
                abAppend(buffer, " ", 1);
                padding2 --;
            }
            abAppend(buffer, madeBy, madeByLen);

        }
        else {
            abAppend(buffer, "~", 1);
        }
        abAppend(buffer, "\x1b[K", 3);  // erase character from the active position to the end of line
        if (y < editor.screenrows-1) {
            abAppend(buffer, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct appendBuffer add_buffer = ABUF_INIT; // initialize add_buffer.buf to NULL and add_buffer.len to 0

    abAppend(&add_buffer, "\x1b[?25l", 6);  // makes the cursor invisible      
    abAppend(&add_buffer, "\x1b[H", 3);     // move the cursor to the top-left corner

    editorDrawRows(&add_buffer);

    abAppend(&add_buffer, "\x1b[H", 3);     // move the cursor to the top-left corner
    abAppend(&add_buffer, "\x1b[?25h", 6);  // makes the cursor visible

    write(STDOUT_FILENO, add_buffer.buf, add_buffer.len);   // write all stored character at buf to the screen
    abFree(&add_buffer);
}

// Input ---
int editorProcessKeypress() {
    char c = editorReadKey();
    switch(c) {
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
    return 0;
}

// Init ---
void initEditor() {
    // pass the value of screen rows and cols to editor struct;
    if (getWindowSize(&editor.screenrows, &editor.screencols) == -1) die("getWindowSize");
}
