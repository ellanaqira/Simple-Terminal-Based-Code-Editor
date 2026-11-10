
/*** Includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


/*** Defines ***/
#define SCTR_VERSION "0.0.1"
#define SCTR "Ssector - version"
#define MADE_BY "Made by Ellan Aqira"
#define SCTR_IS_OPEN_SOURCE "Sector is open source"
#define HOW_TO_EXIT "type  :ctrl + q     to exit"

// fg colors ---
#define FG_DEFAULT			39
#define FG_BLACK			30
#define FG_RED				31
#define FG_GREEN			32
#define FG_YELLOW			33
#define FG_BLUE				34
#define FG_MAGENTA			35
#define FG_CYAN				36
#define FG_LIGHT_GRAY		37
#define FG_DARK_GRAY		90
#define FG_LIGHT_RED		91
#define FG_LIGHT_GREEN		92
#define FG_LIGHT_YELLOW		93
#define FG_LIGHT_BLUE		94
#define FG_LIGHT_MAGENTA	95
#define FG_LIGHT_CYAN		96
#define FG_WHITE			97

// bg colors ---
#define BG_DEFAULT			49
#define BG_BLACK			40
#define BG_RED				41
#define BG_GREEN			42
#define BG_YELLOW			43
#define BG_BLUE				44
#define BG_MAGENTA			45
#define BG_CYAN				46
#define BG_LIGHT_GRAY		47
#define BG_DARK_GRAY		100
#define BG_LIGHT_RED		101
#define BG_LIGHT_GREEN		102
#define BG_LIGHT_YELLOW		103
#define BG_LIGHT_BLUE		104
#define BG_LIGHT_MAGENTA	105
#define BG_LIGHT_CYAN		106
#define BG_WHITE			107

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// --------- [appearance] ---------
#define TAB_STOP 4

#define FG_EDITOR_COLOR TOSTRING(FG_LIGHT_CYAN)
#define BG_EDITOR_COLOR TOSTRING(BG_BLUE)

#define LEFT_BORDER "▍"
#define FG_LEFT_BORDER_COLOR TOSTRING(FG_LIGHT_YELLOW)
#define BG_LEFT_BORDER_COLOR TOSTRING(BG_BLUE)

#define BOTTOM_BORDER "▂"
#define FG_BOTTOM_BORDER_COLOR TOSTRING(FG_LIGHT_YELLOW)
#define BG_BOTTOM_BORDER_COLOR TOSTRING(BG_BLUE)

#define BOTTOM_LEFT_CORNER "🭎"
#define FG_BOTTOM_LEFT_CORNER_COLOR TOSTRING(FG_LIGHT_YELLOW)
#define BG_BOTTOM_LEFT_CORNER_COLOR TOSTRING(BG_BLUE)

#define FG_STATUS_BAR_COLOR TOSTRING(FG_BLUE)
#define BG_STATUS_BAR_COLOR TOSTRING(BG_CYAN)
#define FG_ROW_STATUS_COLOR TOSTRING(FG_WHITE)
#define BG_ROW_STATUS_COLOR TOSTRING(BG_RED)
// --------------------------------

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0};
#define EXTENSION_BUFFER_SIZE 32

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
	int render_size;
	char *characters;
	char *render;
} editorRow;
// Editor Configuration ---
struct editorConfig {
	int cursorX, cursorY;
	int renderX;
	int rowOffset;
	int colOffset;
	int screenrows;
	int screencols;
	int numrows;
	editorRow *row;
	char *filename;
	char status_msg[80];
	time_t status_msg_time;
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
int editorRowCxToRx(editorRow *row, int cursorX);
void editorUpdateRow(editorRow *row);
void editorAppendRow(char *s, size_t len);
// File I/O ---
void editorOpen(char *name_of_the_file);
// Append Buffer ---
void abAppend(struct appendBuffer *buffer, const char *s, int length);
void abFree(struct appendBuffer *buffer);
// Output ---
void editorAddMessage(struct appendBuffer *buffer, char *message, int messageLen);
void editorScroll();
void editorDrawRows(struct appendBuffer *buffer);
void editorDrawStatusBar(struct appendBuffer *buffer);
void editorDrawMessageBar(struct appendBuffer *buffer);
void editorRefreshScreen();
void editorSetStatusMessage(const char *fmt, ...);
// Input ---
void editorMoveCursor(int key);
int editorProcessKeypress();
// Init ---
void initEditor();


int main(int argc, char *argv[]) {
	enableRawMode();
	initEditor();
	if (argc >= 2) {
		editorOpen(argv[1]);
	}

	editorSetStatusMessage("HELP: Ctrl+Q = quit");

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
	printf("row = %d + 3 = %d\r\n", Editor.screenrows, Editor.screenrows + 3);
	printf("col = %d + 1 = %d\r\n", Editor.screencols, Editor.screencols + 1);
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
	size_t i = 0;
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
int editorRowCxToRx(editorRow *row, int cursorX) {
	int renderX = 0;
	int j;

	for (j=0; j<cursorX; j++) {
		if (row->characters[j] == '\t') {
			renderX += (TAB_STOP - 1) - (renderX % TAB_STOP);
		}
		renderX++;
	}
	return renderX;
}

void editorUpdateRow(editorRow *row) {
	int num_of_tabs = 0;
	for (int i=0; i < row->size; i++) {
		if (row->characters[i] == '\t') {
			num_of_tabs++;
		}
	}

	free(row->render);
	// row->size already counts 1 for each tab, so we multiply the number of tabs by (TAB_STOP-1)
	//and add that to row->size to get the maximum amount of memory we’ll need for the rendered row
	row->render = malloc(row->size + (num_of_tabs*(TAB_STOP-1)) + 1);
	
	int i;
	int index = 0;
	for (i=0; i < row->size; i++) {
		if (row->characters[i] == '\t') {
			row->render[index] = ' ';
			index++;
			while (index % TAB_STOP != 0) {
				row->render[index] = ' ';
				index++;
			}
		} else {
			row->render[index] = row->characters[i];
			index++;
		}
	}
	row->render[index] = '\0';
	row->render_size = index;
}

void editorAppendRow(char *s, size_t len) {
	Editor.row = realloc(Editor.row, sizeof(editorRow) * (Editor.numrows + 1));

	int at = Editor.numrows;    // set 'at' to the index of the new row we want to initialize
	Editor.row[at].size = len;
	Editor.row[at].characters = malloc(len + 1);
	memcpy(Editor.row[at].characters, s, len);  // copy a line of text from 's' to 'Editor.row[at].characters'
	Editor.row[at].characters[len] = '\0';

	Editor.row[at].render_size = 0;
	Editor.row[at].render = NULL;
	editorUpdateRow(&Editor.row[at]);
	
	Editor.numrows++;
}

// File I/O ---
void editorOpen(char *name_of_the_file) {
	free(Editor.filename);
	Editor.filename = strdup(name_of_the_file);
	
	FILE *fp = fopen(name_of_the_file, "r");
	if (!fp) die("fopen");

	char *line = NULL;
	size_t lineCapacity = 0;
	ssize_t lineLen;    // lineLen uses ssize_t so it can accept negative values (-1)

	// The while loop works because getline() returns -1 when it gets to the end of the file and there are no more lines to read
	// getline function return line length and store each line from 'fp' to pointer 'line'
	// Each time the 'getline' function is called, it replaces the line of text in 'line' with a new one.
	while ((lineLen = getline(&line, &lineCapacity, fp)) != -1) {
		// strip off the newline or carriage return at the end of the line before copying it into our editorRow.
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
		abAppend(buffer, "\x1b[" FG_LEFT_BORDER_COLOR ";" BG_LEFT_BORDER_COLOR "m" , 9);
		abAppend(buffer, ""LEFT_BORDER"", 3);
		abAppend(buffer, "\x1b[m", 3);
		abAppend(buffer, "\x1b[" FG_EDITOR_COLOR  ";" BG_EDITOR_COLOR "m", 9);	// set fg and bg color 
		abAppend(buffer, "~", 1);

		padding--;
	}
	while(padding != 0) {
		abAppend(buffer, " ", 1);
		padding--;
	}
	abAppend(buffer, message, messageLen);
}

void editorScroll() {
	Editor.renderX = 0;

	if (Editor.cursorY < Editor.numrows) {
		Editor.renderX = editorRowCxToRx(&Editor.row[Editor.cursorY], Editor.cursorX);
	}

	// Handle the scrolling up
	if (Editor.cursorY < Editor.rowOffset) {
		Editor.rowOffset = Editor.cursorY;
	}

	// Handle the scrolling down
	if (Editor.cursorY >= Editor.rowOffset + Editor.screenrows) {
		Editor.rowOffset = (Editor.cursorY - Editor.screenrows) + 1;
	}

	// Handle the scrolling right
	if (Editor.renderX < Editor.colOffset) {
		Editor.colOffset = Editor.renderX;
	}
	// Handle the scrolling left
	if (Editor.renderX >= Editor.colOffset + Editor.screencols) {
		Editor.colOffset = (Editor.renderX - Editor.screencols) + 1;
	}
}

void editorDrawRows(struct appendBuffer *buffer) {
	int y;
	for(y=0; y < Editor.screenrows; y++) {
		int filerow = y + Editor.rowOffset;
		if (filerow >= Editor.numrows) {
			if (Editor.numrows == 0 && y == Editor.screenrows / 3) {
				char sectorVer[50];
				int sectorVerLen = snprintf(sectorVer, sizeof(sectorVer), "%s %s", SCTR, SCTR_VERSION);
				editorAddMessage(buffer, sectorVer, sectorVerLen);
			} else if (Editor.numrows == 0 && y == (Editor.screenrows / 3) + 2) {
				char madeBy[50];
				int madeByLen = snprintf(madeBy, sizeof(madeBy), "%s", MADE_BY);
				editorAddMessage(buffer, madeBy, madeByLen);
			} else if (Editor.numrows == 0 &&  y == (Editor.screenrows / 3) + 3) {
				char isOpenSource[50];
				int isOpenSourceLen = snprintf(isOpenSource, sizeof(isOpenSource), "%s", SCTR_IS_OPEN_SOURCE);
				editorAddMessage(buffer, isOpenSource, isOpenSourceLen);
			} else if (Editor.numrows == 0 && y == (Editor.screenrows / 3) + 5) {
				char howToExit[50];
				int howToExitLen = snprintf(howToExit, sizeof(howToExit), "%s", HOW_TO_EXIT);
				editorAddMessage(buffer, howToExit, howToExitLen);
			} else {
				abAppend(buffer, "\x1b[" FG_LEFT_BORDER_COLOR ";" BG_LEFT_BORDER_COLOR "m" , 9);
				abAppend(buffer, ""LEFT_BORDER"", 3);
				abAppend(buffer, "\x1b[m", 3);
				abAppend(buffer, "\x1b[" FG_EDITOR_COLOR  ";" BG_EDITOR_COLOR "m", 9);	// set fg and bg color 
				abAppend(buffer, "~", 1);
			}
		} else {
			int len = Editor.row[filerow].render_size - Editor.colOffset;
			if (len < 0) len = 0;
			if (len > Editor.screencols) len = Editor.screencols;
			abAppend(buffer, "\x1b[" FG_LEFT_BORDER_COLOR ";" BG_LEFT_BORDER_COLOR "m" , 9);
			abAppend(buffer, ""LEFT_BORDER"", 3);
			abAppend(buffer, "\x1b[m", 3);
			abAppend(buffer, "\x1b[" FG_EDITOR_COLOR  ";" BG_EDITOR_COLOR "m", 9);	// set fg and bg color 
			abAppend(buffer, &Editor.row[filerow].render[Editor.colOffset], len);
		}

		abAppend(buffer, "\x1b[K", 3);  // erase character from the active position to the end of line

		abAppend(buffer, "\r\n", 2);
	}
}

void editorDrawStatusBar(struct appendBuffer *buffer) {
	char file_status[80], row_status[80];
	int maxlen_of_file_name = 20;
	size_t filename_len = 0;
	int len = 0;
	int bottom_border_len = 0;
	int rowlen = 0;

	char filetype[EXTENSION_BUFFER_SIZE];
	int index_filetype = 0;

	if (Editor.filename != NULL) {
		filename_len = strlen(Editor.filename);	

		 for (int i=0; i<(int)filename_len; i++) {
		 		if (Editor.filename[i] == '.') {
		 			while (i<(int)filename_len) {
		 				filetype[index_filetype] = Editor.filename[i];
		 				i++;
		 				index_filetype++;
		 			}
		 		}
		 	}
		 filetype[index_filetype] = '\0';
	}

	#define len_visible 14
	if (Editor.filename) {
		if ((int)filename_len <= maxlen_of_file_name) {
			len = snprintf(file_status, sizeof(file_status), " %.20s - %d line(s)", Editor.filename, Editor.numrows);
			rowlen = snprintf(row_status, sizeof(row_status), " %d/%d ", Editor.cursorY + 1, Editor.numrows);
		} else {
			// determining the appropriate length for displaying a filename
			// so that the total number of characters shown based on the
			// format: "%.*s..%s", is equals exactly 20
			int num_of_periods = 2;
			if (index_filetype >= maxlen_of_file_name) {
				len = snprintf(file_status, sizeof(file_status), " %.17s... - %d line(s)", Editor.filename, Editor.numrows);
				rowlen = snprintf(row_status, sizeof(row_status), " %d/%d ", Editor.cursorY + 1, Editor.numrows);
			} else {
				int len_visible_filename = maxlen_of_file_name - num_of_periods  - index_filetype;
				len = snprintf(file_status, sizeof(file_status), " %.*s..%s - %d line(s)", len_visible_filename, Editor.filename, filetype, Editor.numrows);
				rowlen = snprintf(row_status, sizeof(row_status), " %d/%d ", Editor.cursorY + 1, Editor.numrows);
			}
		}
	} else {
		len = snprintf(file_status, sizeof(file_status), " %.20s - %d line(s)", "[No Name]", Editor.numrows);
		rowlen = snprintf(row_status, sizeof(row_status), " %d/%d ", Editor.cursorY + 1, Editor.numrows);
	}
	
	if (len > Editor.screencols) len = Editor.screencols;
	abAppend(buffer, "\x1b[" FG_BOTTOM_LEFT_CORNER_COLOR ";" BG_BOTTOM_LEFT_CORNER_COLOR "m", 9);
	abAppend(buffer, "" BOTTOM_LEFT_CORNER "", 4);
	
	abAppend(buffer, "\x1b[" FG_BOTTOM_BORDER_COLOR ";" BG_BOTTOM_BORDER_COLOR "m", 9);
	while (bottom_border_len < (Editor.screencols)) {
		abAppend(buffer, "" BOTTOM_BORDER  "", 3);
		bottom_border_len++;
	}
	abAppend(buffer, "\r\n", 2);

	abAppend(buffer, "\x1b[" FG_STATUS_BAR_COLOR ";" BG_STATUS_BAR_COLOR "m", 9);
	abAppend(buffer, file_status, len);
	while (len < Editor.screencols+1) {
		if (((Editor.screencols+1) - len) == rowlen) {
			abAppend(buffer, "\x1b[" FG_ROW_STATUS_COLOR ";" BG_ROW_STATUS_COLOR "m", 9);
			abAppend(buffer, row_status, rowlen);
			break;
		} else {
			abAppend(buffer, " ", 1);
			len++;
		}
	}
	abAppend(buffer, "\x1b[m", 3);
	abAppend(buffer, "\r\n", 2);
}

void editorDrawMessageBar(struct appendBuffer *buffer) {
	abAppend(buffer, "\x1b[K", 3);
	abAppend(buffer, " ", 1);
	int message_len = strlen(Editor.status_msg);
	if (message_len > Editor.screencols) message_len = Editor.screencols;
	if (message_len && time(NULL) - Editor.status_msg_time < 5) {
		abAppend(buffer, Editor.status_msg, message_len);
	}
}

void editorRefreshScreen() {
	editorScroll();

	struct appendBuffer add_buffer = ABUF_INIT; // initialize add_buffer.buf to NULL and add_buffer.len to 0

	abAppend(&add_buffer, "\x1b[?25l", 6);  	// makes the cursor invisible      
	abAppend(&add_buffer, "\x1b[H", 3);     	// move the cursor to the top-left corner

	editorDrawRows(&add_buffer);
	editorDrawStatusBar(&add_buffer);
	editorDrawMessageBar(&add_buffer);

	char buf_cursor[32];
	// set the cursor position
	snprintf(buf_cursor, sizeof(buf_cursor), "\x1b[%d;%dH", (Editor.cursorY - Editor.rowOffset) + 1, (Editor.renderX - Editor.colOffset) + 2);
	abAppend(&add_buffer, buf_cursor, strlen(buf_cursor));

	abAppend(&add_buffer, "\x1b[?25h", 6);  // makes the cursor visible

	write(STDOUT_FILENO, add_buffer.buf, add_buffer.len);   // write all stored character at buf to the screen
	abFree(&add_buffer);
}

void editorSetStatusMessage(const char *fmt, ...) {
	va_list arg_ptr;
	va_start(arg_ptr, fmt);
	vsnprintf(Editor.status_msg, sizeof(Editor.status_msg), fmt, arg_ptr);
	va_end(arg_ptr);
	Editor.status_msg_time = time(NULL);
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
			// allow the user to press ← at the beginning of the line to move to the end of the previous line
			} else if (Editor.cursorY > 0) {
				Editor.cursorY--;
				Editor.cursorX = Editor.row[Editor.cursorY].size;
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
			// allow the user to press → at the end of a line to go to the beginning of the next line
			} else if (currentRow && Editor.cursorX == currentRow->size) {
				Editor.cursorY++;
				Editor.cursorX = 0;
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
			Editor.cursorX = Editor.row[Editor.cursorY].size;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				if (c == PAGE_UP) {
					Editor.cursorY = Editor.rowOffset;
				} else if (c == PAGE_DOWN) {
					Editor.cursorY = Editor.rowOffset + Editor.screenrows - 1;
					if (Editor.cursorY > Editor.numrows) Editor.cursorY = Editor.numrows;
				}

				int times = Editor.screenrows;
				while (times--) {
					if (c == PAGE_UP) {
						editorMoveCursor(ARROW_UP);
					} else if (c == PAGE_DOWN) {
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
	Editor.renderX = 0;
	Editor.rowOffset = 0;
	Editor.colOffset = 0;
	Editor.numrows = 0;
	Editor.row = NULL;
	Editor.filename = NULL;
	Editor.status_msg[0] = '\0';
	Editor.status_msg_time = 0;
	
	if (getWindowSize(&Editor.screenrows, &Editor.screencols) == -1) die("getWindowSize");
	Editor.screenrows -= 3;
	Editor.screencols -= 1;
}
