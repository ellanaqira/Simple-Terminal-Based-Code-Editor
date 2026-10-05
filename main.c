/*** Includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** Defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** Data ***/
struct editorConfig {
    int screenrows;
    int screencols;
    struct termios OrigTermSet;
};

struct editorConfig editor;


/*** Functions Declaration ***/
// Terminal ---
void die(const char *s);
void disableRawMode();
void enableRawMode();
char editorReadKey();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
// Output ---
void editorRefreshScreen();
void editorDrawRows();
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

// Output ---
void editorDrawRows() {
    int y;
    for(y=0; y < editor.screenrows; y++) {
        write(STDOUT_FILENO, "~", 1);

        if (y < editor.screenrows-1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);     // clear the screen
    write(STDOUT_FILENO, "\x1b[H", 3);      // move the cursor to the top-left corner

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);      // move the cursor to the top-left corner
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
