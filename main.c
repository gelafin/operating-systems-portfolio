// If you are not compiling with the gcc option --std=gnu99, then
// #define _GNU_SOURCE or you might get a compiler warning
// sources for fgets() knowledge: https://www.educative.io/edpresso/how-to-use-the-fgets-function-in-c
//      and https://stackoverflow.com/a/59019657/14257952
#define _GNU_SOURCE
#include "./smallsh.h"

/*
*   Runs an interactive shell program
*   Compile the program as follows:
*       gcc --std=gnu99 -o smallsh main.c
*/
int main(int argc, char* argv[]) {
    // test basic i/o
    printf("\tDEBUG: testing input\n");
    char* userInput = calloc(MAX_INPUT_LENGTH, sizeof(char));

    for (int index = 0; index < 3; index++) {
        printCommandPrompt();
        fgets(userInput, MAX_INPUT_LENGTH + 1, stdin);
        printf("\tDEBUG: Your raw command is %s\n", userInput);
        struct CommandLine* commandLine = parseCommandString(userInput);
        printf("\tDEBUG: handling command %s...\n", commandLine->command);
        executeCommand(commandLine);

        // clean up placeholder (memory leaks allowed at this point in class)
        free(commandLine);
    }

    return EXIT_SUCCESS;
}
