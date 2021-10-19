#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>


#define MAX_INPUT_LENGTH 2048  // defined in specs
#define MAX_ARG_COUNT 512  // defined in specs


struct CommandLine {
    char* command;
    char** args;
    char* inputFile;
    char* outputFile;
    bool isBackground;
};


void printCommandPrompt();
void printToTerminal(const char*);
struct CommandLine* parseCommandString(char*);


/*
* prints the special command prompt string to the terminal
*/
void printCommandPrompt() {
    const char* commandPromptText = ": ";
    printToTerminal(commandPromptText);

    return;
}


/*
* prints any given string to the terminal, followed by a newline
* flushes the output buffer
* text: one line to print
*/
void printToTerminal(const char* text) {
    // print text to terminal
    printf("%s", text);

    // flush output buffer (output text may not reach the screen until this happens)
    fflush(NULL);

    return;
}


/*
* reads a line from the prompt and saves parsed input to the given struct
* command syntax is
*       command [arg1 arg2 ...] [< input_file] [> output_file] [&]
*   where square-bracketed items are optional. Note that special characters
*   must still be surrounded by spaces. 
*   The < redirects input and the > redirects output.
*   Input redirection can appear before or after output redirection.
*   The & is only special as the last character,
*   where it means "run command in the background"
*   Any instance of $$ is expanded into the smallsh process id
* inputString: one line of unprocessed user input
* return: pointer to a CommandLine struct where parsed results will be saved
*/
struct CommandLine* parseCommandString(char* stringInput) {
    char* indexPointer;
    char* inputToken;
    const char* delimiter = " ";
    struct CommandLine* commandLine;

    // process first token now to check for empty input
    char* inputToken = strtok_r(stringInput, delimiter, &indexPointer);

    // break string into tokens
    while (inputToken != NULL) {
        // handle command

        // handle args

        // handle input redirection

        // handle output redirection

        // handle isBackground preference

        // try to extract another token in case there are more
        char* inputToken = strtok_r(stringInput, delimiter, &indexPointer);
    }
    

    return;
}