
/*** Includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** Defines ***/
#define SCTR_VERSION "0.0.1"
#define SCTR "Sector -- version"
#define MADE_BY "Made by Ellan Aqira"
#define SCTR_IS_OPEN_SOURCE "Sector is open source"
#define HOW_TO_EXIT "type  :ctrl + q     to exit"

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0};

enum editorKey {
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    DELETE_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** Data ***/
// Editor Row
typedef struct editorRow {
    int size;
    char *characters;
} editorRow;
// Editor Configuration ---
struct editorConfig {
    int cursorX, cursorY;
    int rowOffset;
    int colOffset;
    int screenrows;
    int screencols;
    int numrows;
    editorRow *row;  // 'Editor.row' is an array of 'editorRow' 
    struct termios OrigTermSet;
};
struct editorConfig Editor;
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
int editorReadKey();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
// Row Operations ---
void editorAppendRow(char *s, size_t len);
// File I/O ---
void editorOpen(char *filename);
// Append Buffer ---
void abAppend(struct appendBuffer *buffer, const char *s, int length);
void abFree(struct appendBuffer *buffer);
// Output ---
void editorAddMessage(struct appendBuffer *buffer, char *message, int messageLen);
void editorScroll();
void editorDrawRows(struct appendBuffer *buffer);
void editorRefreshScreen();
// Input ---
void editorMoveCursor(int key);
int editorProcessKeypress();
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


/*** Functions Definition ***/
// Terminal ---
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.OrigTermSet) == -1) die("tcsetattr");    // apply the original settings of terminal

    write(STDOUT_FILENO, "\x1b[?1049l", 8); // restore cursor position and disable alternative buffer
    printf("row = %d\r\n", Editor.screenrows);
    printf("col = %d\r\n", Editor.screencols);
}

void enableRawMode() {
    // get the current settings of terminal and store it at Editor.OrigTerSet struct
    if (tcgetattr(STDIN_FILENO, &Editor.OrigTermSet) == -1) die("tcgetattr");

    // copy the original terminal settings to ModifTermSet struct
    struct termios ModifTermSet = Editor.OrigTermSet;
    // change the original settings of terminal through ModifTermSet struct to enable raw mode
    ModifTermSet.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    ModifTermSet.c_oflag &= ~(OPOST);
    ModifTermSet.c_cflag |= (CS8);
    ModifTermSet.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    ModifTermSet.c_cc[VMIN] = 0;
    ModifTermSet.c_cc[VTIME] = 1;
 
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ModifTermSet) == -1) die("tcsetattr");  // apply the changes to terminal

    write(STDOUT_FILENO, "\x1b[?1049h", 8); // save cursor position and enable alternative buffer

    atexit(disableRawMode); // call disableRawMode when the program terminates normally
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        // pressing arrow key producing "ESC [ (A/B/C/D)"
        // ESC ("\x1b") is already stored at c
        // "[" and "A"/"B"/"C"/"D" remain and are waiting to be read
        char escapeSeq[3];

        // try to read "[" and store it at escapeSeq[0]
        if (read(STDIN_FILENO, &escapeSeq[0], 1) != 1) return '\x1b';
        // try to read "A"/"B"/"C"/"D" and store it at escapeSeq[1]
        if (read(STDIN_FILENO, &escapeSeq[1], 1) != 1) return '\x1b';

        if (escapeSeq[0] == '[') {
            if (escapeSeq[1] >= '0' && escapeSeq[1] <= '9') {
                if (read(STDIN_FILENO, &escapeSeq[2], 1) != 1) return '\x1b';
                if (escapeSeq[2] == '~') {
                    switch (escapeSeq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DELETE_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else { 
                switch (escapeSeq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'F': return END_KEY;
                    case 'H': return HOME_KEY;
                }
            }
        } else if (escapeSeq[0] == 'O') {
            switch (escapeSeq[1]) {
                case 'F': return END_KEY;
                case 'H': return HOME_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buff[32];
    unsigned int i = 0;
    // n = Requests and reports the general status,
    // with parameter 6 to report cursor's active position,
    // and then send the cursor position reply to standard input.
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    // the reply that stored in standard inpus is an escape sequence
    // for example, "/x1b 40;141R"
    while (i < sizeof(buff)-1) {
        // store the cursor position to buff  without the "R"
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
    } else {
        *cols = winsz.ws_col;
        *rows = winsz.ws_row;
        return 0;
    }
}

// Row Operations ---
void editorAppendRow(char *s, size_t len) {
    Editor.row = realloc(Editor.row, sizeof(editorRow) * (Editor.numrows + 1));

    int at = Editor.numrows;    // set 'at' to the index of the new row we want to initialize
    Editor.row[at].size = len;
    Editor.row[at].characters = malloc(len + 1);
    memcpy(Editor.row[at].characters, s, len);  // copy a line of text from 's' to 'Editor.row[at].characters'
    Editor.row[at].characters[len] = '\0';
    Editor.numrows++;
}

// File I/O ---
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t lineLen;    // lineLen uses ssize_t so it can accept negative values (-1)

    // The while loop works because getline() returns -1 when it gets to the end of the file and there are no more lines to read
    // getline function return line length and store each line from 'fp' to pointer 'line'
    // Each time the 'getline' function is called, it replaces the line of text in 'line' with a new one.
    while ((lineLen = getline(&line, &lineCapacity, fp)) != -1) {
        // strip off the newline or carriage return at the end of the line before copying it into our erow.
        // editorRow represents one line of text, so there’s no use storing a newline character at the end of each one.
        while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r')) {
            lineLen--;
        }
        editorAppendRow(line, lineLen);
    }
    free(line);
    fclose(fp);
}

// Append Buffer ---
void abAppend(struct appendBuffer *buffer, const char *s, int length) {
    // resize the memory of buf from struct buffer and store the return at new_buff
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
void editorAddMessage(struct appendBuffer *buffer, char *message, int messageLen) {
    // Add "~", a few spaces, and a message to the buffer to be printed out later. 
    if (messageLen > Editor.screencols) messageLen = Editor.screencols;
    int padding = (Editor.screencols - messageLen) / 2;
    if (padding) {
        abAppend(buffer, "~", 1);   // add "~" to the buffer
        padding--;
    }
    while(padding != 0) {
        abAppend(buffer, " ", 1);   // add some spaces to buffer
        padding--;
    }
    abAppend(buffer, message, messageLen);  // add the message to the buffer
}

void editorScroll() {
    // Handle the scrolling up
    if (Editor.cursorY < Editor.rowOffset) {
        Editor.rowOffset = Editor.cursorY;
    }
    // Handle the scrolling down
    if (Editor.cursorY >= Editor.rowOffset + Editor.screenrows) {
        Editor.rowOffset = (Editor.cursorY - Editor.screenrows) + 1;
    }

    // Handle the scrolling right
    if (Editor.cursorX < Editor.colOffset) {
        Editor.colOffset = Editor.cursorX;
    }
    // Handle the scrolling left
    if (Editor.cursorX >= Editor.colOffset + Editor.screencols) {
        Editor.colOffset = (Editor.cursorX - Editor.screencols) + 1;
    }
}

void editorDrawRows(struct appendBuffer *buffer) {
    int y;
    for(y=0; y < Editor.screenrows; y++) {
        int filerow = y + Editor.rowOffset;
        if (filerow >= Editor.numrows) {
            // string placement based on rows
            if (Editor.numrows == 0 && y == Editor.screenrows / 3) {
            // Sector and Version
                char sectorVer[50];
                int sectorVerLen = snprintf(sectorVer, sizeof(sectorVer), "%s %s", SCTR, SCTR_VERSION);
                // string placement based on columns
                editorAddMessage(buffer, sectorVer, sectorVerLen);
            } else if (Editor.numrows == 0 && y == (Editor.screenrows / 3) + 2) {
            // Made by Ellan Aqira
                char madeBy[50];
                int madeByLen = snprintf(madeBy, sizeof(madeBy), "%s", MADE_BY);
                // string placement based on columns
                editorAddMessage(buffer, madeBy, madeByLen);
            } else if (Editor.numrows == 0 &&  y == (Editor.screenrows / 3) + 3) {
            // Sector is open source
                char isOpenSource[50];
                int isOpenSourceLen = snprintf(isOpenSource, sizeof(isOpenSource), "%s", SCTR_IS_OPEN_SOURCE);
                // string placement based on columns
                editorAddMessage(buffer, isOpenSource, isOpenSourceLen);
            } else if (Editor.numrows == 0 && y == (Editor.screenrows / 3) + 5) {
            // How to exit
                char howToExit[50];
                int howToExitLen = snprintf(howToExit, sizeof(howToExit), "%s", HOW_TO_EXIT);
                // string placement based on columns
                editorAddMessage(buffer, howToExit, howToExitLen);
            } else {
                abAppend(buffer, "~", 1);
            }
        } else {
            int len = Editor.row[filerow].size - Editor.colOffset;
            if (len < 0) len = 0;
            if (len > Editor.screencols) len = Editor.screencols;
            abAppend(buffer, &Editor.row[filerow].characters[Editor.colOffset], len);
        }

        abAppend(buffer, "\x1b[K", 3);  // erase character from the active position to the end of line

        if (y < Editor.screenrows-1) {
            abAppend(buffer, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    editorScroll();

    struct appendBuffer add_buffer = ABUF_INIT; // initialize add_buffer.buf to NULL and add_buffer.len to 0

    abAppend(&add_buffer, "\x1b[?25l", 6);  // makes the cursor invisible      
    abAppend(&add_buffer, "\x1b[H", 3);     // move the cursor to the top-left corner

    editorDrawRows(&add_buffer);

    char buf_cursor[32];
    // set the cursor position
    snprintf(buf_cursor, sizeof(buf_cursor), "\x1b[%d;%dH", (Editor.cursorY - Editor.rowOffset) + 1, (Editor.cursorX - Editor.colOffset) + 1);
    abAppend(&add_buffer, buf_cursor, strlen(buf_cursor));

    abAppend(&add_buffer, "\x1b[?25h", 6);  // makes the cursor visible

    write(STDOUT_FILENO, add_buffer.buf, add_buffer.len);   // write all stored character at buf to the screen
    abFree(&add_buffer);
}

// Input ---
void editorMoveCursor(int key) {
    editorRow *currentRow;
    
    // check if the cursor's vertical position is outside the existing rows
    if (Editor.cursorY >= Editor.numrows) {
        currentRow = NULL;
    } else {
        currentRow = &Editor.row[Editor.cursorY];
    }

    switch (key) {
        case ARROW_UP:
            if (Editor.cursorY != 0) {
                Editor.cursorY--;
            }
            break;
        case ARROW_LEFT:
            if (Editor.cursorX != 0) {
                Editor.cursorX--;
            }
            break;
        case ARROW_DOWN:
            if (Editor.cursorY < Editor.numrows) {
                Editor.cursorY++;
            }
            break;
        case ARROW_RIGHT:
            if (currentRow && Editor.cursorX < currentRow->size) {
                Editor.cursorX++;
            }
            break;
    }

    // check if the cursor's vertical position is outside the existing rows
    if (Editor.cursorY >= Editor.numrows) {
        currentRow = NULL;
    } else {
        currentRow = &Editor.row[Editor.cursorY];
    }

    // prevent the cursor to move past the end of line when moving the cursor
    // to the end of a long line, then moving it down to the next shorter line
    int rowlen = 0;
    if (currentRow != NULL) {
        rowlen = currentRow->size;
    }

    if (Editor.cursorX > rowlen) {
        Editor.cursorX = rowlen;
    }
}
int editorProcessKeypress() {
    int c = editorReadKey();
    switch(c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case HOME_KEY:
            Editor.cursorX = 0;
            break;
        case END_KEY:
            Editor.cursorX = Editor.screencols-1;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = Editor.screenrows;
                while(times--) {
                    if (c == PAGE_UP) {
                        editorMoveCursor(ARROW_UP);
                    } else {
                        editorMoveCursor(ARROW_DOWN);
                    }
                }
            }
            break;

        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

    }
    return 0;
}

// Init ---
void initEditor() {
    // pass the value of screen rows and cols to Editor struct;
    Editor.cursorX = 0;
    Editor.cursorY = 0;
    Editor.rowOffset = 0;
    Editor.colOffset = 0;
    Editor.numrows = 0;
    Editor.row = NULL;
    if (getWindowSize(&Editor.screenrows, &Editor.screencols) == -1) die("getWindowSize");
}
