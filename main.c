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
    setSIGINThandler();

    while (true) {
        printCommandPrompt();

        printf(" DEBUG: getting new command...\n");
        fflush(NULL);

        struct CommandLine* commandLine = parseCommandString(getUserCommandString());

        // handle empty input (also caused by signals interrupting fgets)
        if (commandLine->command) {
            printf("\tDEBUG: handling command %s...", commandLine->command);
            fflush(NULL);

            executeCommand(commandLine);
        }

        // clean up zombies
        reapAll();
    }

    return EXIT_SUCCESS;
}
