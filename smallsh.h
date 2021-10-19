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
void parseInput(char*, struct CommandLine*);


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
* reads a line from the prompt and save parsed input in the given struct
* inputString: one line of unprocessed user input
* commandLine: pointer to a CommandLine struct where parsed results will be saved
*/
void parseInput(char* inputString, struct CommandLine* commandLine) {
    //

    return;
}