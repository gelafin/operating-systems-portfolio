// If you are not compiling with the gcc option --std=gnu99, then
// #define _GNU_SOURCE or you might get a compiler warning
#define _GNU_SOURCE
#include "./smallsh.h"

/*
*   Runs an interactive shell program
*   Compile the program as follows:
*       gcc --std=gnu99 -o smallsh main.c
*/
int main(int argc, char* argv[]) {
    // test basic i/o
    printf("testing 3 inputs");
    char* userInput = calloc(MAX_INPUT_LENGTH, sizeof(char));

    for (int index = 0; index < 3; index++) {
        printCommandPrompt();
        scanf("%s", userInput);
        printf("you entered %s\n", userInput);
    }

    return EXIT_SUCCESS;
}
