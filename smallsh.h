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
    int argCount;
    char* inFile;
    char* outFile;
    bool isBackground;
};


void printCommandPrompt();
void printToTerminal(const char*);
struct CommandLine* parseCommandString(char*);


/*
* prints the special command prompt string to the terminal
*/
void printCommandPrompt() {
    char* commandPromptText = ": ";
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
* copies a source string to a destination string, dynamically allocating memory as needed
* destination: string to overwrite
* source: string to copy
*/
void strcpyDynamic(char* destination, char* source) {
    destination = calloc(strlen(source) + 1, sizeof(char));
    strcpy(destination, source);

    return;
}


/*
* reads a line from the prompt and saves parsed input to the given struct
* does not check for syntax errors (per specs)
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
    char* delimiter = " \n";  // need to add \n because fgets appends it
    char inputRedirectChar = '<';
    char outputRedirectChar = '>';
    char backgroundChar = '&';
    bool isInFileName = false;
    bool isOutFileName = false;
    bool argsAreDone = false;
    bool isSpecialChar = false;
    struct CommandLine* commandLine = malloc(sizeof(struct CommandLine));

    // initialize the CommandLine struct's fixed-size array to all null pointers
    // and initialize its other defaults
    commandLine->args = calloc(MAX_ARG_COUNT, sizeof(char*));
    commandLine->argCount = 0;
    commandLine->isBackground = false;

    // example commands for testing
    // command arg
    // command arg arg
    // command < filename
    // command arg arg < filename
    // command &
    // command arg > filename
    // command arg arg > filename &
    // command arg < filename > filename &

    // Process first token now, because it's unique.
    // It is the first that shows whether input is empty, and
    // it is the only non-optional token
    inputToken = strtok_r(stringInput, delimiter, &indexPointer);

    if (inputToken != NULL) {
        // strcpyDynamic(commandLine->command, inputToken);
        commandLine->command = calloc(strlen(inputToken) + 1, sizeof(char));
        strcpy(commandLine->command, inputToken);
    }

    // try to extract another token in case there are args or options
    inputToken = strtok_r(NULL, delimiter, &indexPointer);

    // handle remaining tokens in the command string
    while (inputToken != NULL) {
        // parsing logic:
        // if it's token #1, it's a command (already processed)
        // if it's after token #1 and no flag set for "passed a special char", it's an arg
        // if it's a < > special char, flag it
        // if it's after a special char, it's that char's thing; cancel flag
        // if it's the &, obvious

        // check first character of this token to see if it's a special character
        // if it is a special character, take note that we have passed the args section
        isSpecialChar = false;
        if (inputToken[0] == inputRedirectChar) {
            // next token will be input file name
            isInFileName = true;
            isSpecialChar = true;
        } else if (inputToken[0] == outputRedirectChar) {
            // next token will be output file name
            isOutFileName = true;
            isSpecialChar = true;
        } else if (inputToken[0] == backgroundChar) {
            // handle isBackground preference
            commandLine->isBackground = true;
            isSpecialChar = true;
        }

        // syntax rules say args come before all special characters,
        // so if we reach a special character, we know args are done
        if (isSpecialChar == true) {
            argsAreDone = true;
        }

        // check flags that depend on special characters
        if (isInFileName && !isSpecialChar) {
            // this is the name of the input file. Save it
            commandLine->inFile = calloc(strlen(inputToken) + 1, sizeof(char));
            strcpy(commandLine->inFile, inputToken);

            // make sure the next token isn't treated as the input file name!
            isInFileName = false;
        } else if (isOutFileName && !isSpecialChar) {
            // this is the name of the output file. Save it
            commandLine->outFile = calloc(strlen(inputToken) + 1, sizeof(char));
            strcpy(commandLine->outFile, inputToken);

            // make sure the next token isn't treated as the output file name!
            isOutFileName = false;
        } else if (!argsAreDone) {
            // this token is an arg. Add it to the array of args 
            // and increment the arg count so the next arg is added at the end
            commandLine->args[commandLine->argCount] = calloc(strlen(inputToken) + 1, sizeof(char));
            strcpy(commandLine->args[commandLine->argCount], inputToken);

            ++commandLine->argCount;
        }

        // try to extract another token in case there are more
        inputToken = strtok_r(NULL, delimiter, &indexPointer);
    }
    
    // return a pointer to the struct which now has all the parsed data in it
    return commandLine;
}