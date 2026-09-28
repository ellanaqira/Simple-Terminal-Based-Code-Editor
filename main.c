/*** Includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** Data ***/
struct termios OrigTermSet;


/*** Functions Declaration ***/
void enableRawMode();
void disableRawMode();
void die(const char *s);


/*** Main ***/
int main() {
    enableRawMode();

    while (1) {
        char c = '\0';

        /* Task: see how read can return error message */

        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }
    return 0;
}


/*** Functions Definition ***/
void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &OrigTermSet) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &OrigTermSet) == -1) die("tcgetattr");

    struct termios ModifTermSet = OrigTermSet;
    ModifTermSet.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    ModifTermSet.c_oflag &= ~(OPOST);
    ModifTermSet.c_cflag |= (CS8);
    ModifTermSet.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    ModifTermSet.c_cc[VMIN] = 0;
    ModifTermSet.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &ModifTermSet) == -1) die("tcsetattr");

    atexit(disableRawMode);
}
