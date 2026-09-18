
/*** Includes ***/
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


/*** Data ***/
struct termios orig_termios;


/*** Functions Declaration ***/
void enableRawMode();
void disableRawMode();
void die(const char *s);


/*** Main ***/
int main() {
    enableRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
            die("read");
        }

        if (iscntrl(c)) {           // check if input is a control character
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') {
            break;
        }
    }

    return 0;
}


/*** Function Definition ***/
void enableRawMode() {
    printf("\r\n# Entering Raw-Mode\r\n");

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {     // fetch current terminal settings (attributes) and copies them into orig_termios struct
        die("tcgetattr");
    }
    atexit(disableRawMode);                     // execute disableRawMode if program ends normally

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {   // apply changes to the terminal
        die("tcsetattr");
    }
}

void disableRawMode() {
    printf("# Exit Raw-Mode\r\n\r\n");
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

void die(const char *s) {
    perror(s);
    exit(1);
}