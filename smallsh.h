#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>


#define MAX_INPUT_LENGTH 2048  // defined in specs
#define MAX_ARG_COUNT 512  // defined in specs
#define MAX_FILEPATH_LENGTH 32767  // source: https://superuser.com/questions/14883/what-is-the-longest-file-path-that-windows-can-handle


struct CommandLine {
    char* command;
    char** args;
    int argCount;
    char* inFile;
    char* outFile;
    bool isBackground;
};


void printCommandPrompt();
void printToTerminal(const char*, bool);
struct CommandLine* parseCommandString(char*);


/*
* Wrapper for strcmp
* string1: any string
* string2: any string
* return: true if string1 is equal to string2; false if not
*/
bool isEqualString(char* string1, char* string2) {
    return strcmp(string1, string2) == 0;
}


/*
* Wrapper for strncmp
* prefix: any string that will be checked to be a substring of string,
*         checking LTR up to the length of string
* string: any string that will be checked to contain prefix
* return: true if prefix is a substring of string; false if not
*/
bool isPrefix(char* prefix, char* string) {
    return strncmp(prefix, string, strlen(prefix)) == 0;
}


/*
* prints the special command prompt string to the terminal
*/
void printCommandPrompt() {
    char* commandPromptText = ": ";
    printToTerminal(commandPromptText, false);

    return;
}


/*
* prints any given string to the terminal, followed by a newline
* flushes the output buffer
* text: one line to print
* isError: if true, prints with perror() instead of printf()
*/
void printToTerminal(const char* text, bool isError) {
    if (isError) {
        // print error to standard error
        perror(text);
    } else {
        // print normal text to terminal
        printf("%s", text);
    }

    // flush output buffer (output text may not reach the screen until this happens)
    fflush(NULL);

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



/*
* Returns the pid of smallsh as a string
*/
char* getPidString() {
    char* pidString = calloc(10 + 1, sizeof(char));  // allows 10 digits
    pid_t pid = getpid();

    // convert to string
    sprintf(pidString, "%d", pid);

    return pidString;
}


/*
* Replaces all instances of the pid variable ($$) in stringIn with the smallsh pid
* stringIn: string which may contain instances of "$$"
* return: the input string with instances of "$$" replaced by the smallsh pid
*/
char* expandPidVariable(char* stringIn) {
    char* pidVariable = "$$";
    char* pidString = getPidString();
    char* stringOut = calloc(strlen(stringIn) + 1, sizeof(char));
    char* stringTemp = calloc(strlen(stringIn) + 1, sizeof(char));
    int newLength = 0;
    int originalLength = strlen(stringIn);
    char* tokenPointer;
    char* token = strtok_r(stringIn, pidVariable, &tokenPointer);

    if (strlen(token) == originalLength) {
        // there are no occurrences of the pid variable
        strcpy(stringOut, stringIn);
    } else {
        // There are some occurrences of the pid variable.
        // Continue to copy segments of stringIn while substituting
        // the smallsh pid for the variable
        while (token != NULL) {
            // copy the string up to this point
            strcpy(stringTemp, stringOut);

            // Resize stringOut to fit another pid
            // Make space for stringTemp + this token + pid
            free(stringOut);  // stringOut was copied to stringTemp
            newLength = strlen(stringTemp) + strlen(token) + strlen(pidString) + 1;
            stringOut = calloc(newLength, sizeof(char));
            stringTemp = calloc(newLength, sizeof(char));  // for next iteration

            // restore stringOut from before this iteration, and append this token + pid
            strcpy(stringOut, stringTemp);
            strcat(stringOut, token);
            strcat(stringOut, pidString);  // TODO: don't do this for the last iteration
            free(stringTemp);

            // try to extract another token
            token = strtok_r(NULL, pidVariable, &tokenPointer);
        }
    }

    return stringOut;
}


/*
* gets a new command from the user
* return: user input, expanded with smallsh pid in place of $$
*/
char* getUserCommandString() {
    char* userInput = calloc(MAX_INPUT_LENGTH, sizeof(char));

    // get raw string from user
    fgets(userInput, MAX_INPUT_LENGTH + 1, stdin);

    // return input, expanded with smallsh pid in place of $$
    return expandPidVariable(userInput);
}


/*
* changes the current directory, supporting relative and absolute paths
* commandLine: pointer to a CommandLine struct which has the command line's details
*/
void handleCdCommand(struct CommandLine* commandLine) {
    // change the current directory to the new path
    chdir(commandLine->args[0]);

    return;
}


/*
* kills processes or jobs started by smallsh and terminates smallsh
*/
void handleExitCommand() {
    // kill processes or jobs started by smallsh

    // terminate smallsh
    exit(EXIT_SUCCESS);
}


/*
* executes a command not directly supported by smallsh
* (source: adapted from lecture material)
* commandLine: pointer to a CommandLine struct which has the command line's details
*/
void handleThirdPartyCommand(struct CommandLine* commandLine) {
    pid_t spawnPid = -5;
    int childStatus;
    char* childArgv[MAX_ARG_COUNT];  // must use char*[] for execvp() to work with args
    int copyIndex = 0;  // used by the loop that copies args into childArgv
    char* backgroundNoticePrefix = "background pid is ";
    char* childPidString = calloc(10, sizeof(char));  // room for 10 digits
    char* backgroundNotice = calloc(strlen(backgroundNoticePrefix) + 10, sizeof(char));  // room for 10 digits

    // fork off a child process
    spawnPid = fork();

    switch (spawnPid) {
        case -1:
            // fork() failed to create a child process
			printToTerminal("fork() failed to create a child process\n", true);
            exit(EXIT_FAILURE);
            break;
        
        case 0:
            // Only the child process will execute this, because its spawnPid is 0

            /* 
            Prepare a vector of args for execvp 
            */

            // execvp needs the first arg to be the command filename
            childArgv[0] = commandLine->command;

            // copy the args provided by user
            while (copyIndex < commandLine->argCount) {
                childArgv[copyIndex + 1] = commandLine->args[copyIndex];
                ++copyIndex;
            }

            // execvp needs these args to be terminated by a NULL pointer
            childArgv[copyIndex + 1] = NULL;
           
            /* 
            Execute the third-party command here in this child process 
            */

            // (use the PATH variable to look for non-built in commands, 
            // and allow shell scripts to be executed)
            // In case of success, the new program will terminate the process
            execvp(childArgv[0], childArgv);

            // This code will only be executed if exec returns to 
            // the original child process because of an error
            printToTerminal("error in execvp call of child process\n", true);
            exit(EXIT_FAILURE + 1);  // the child process must exit on failure as well
            break;
        
        default:
            // Only the parent process (smallsh) will execute this. Its spawnPid is the child's process ID
            if (!commandLine->isBackground) {
                // Wait for child to finish
                spawnPid = waitpid(spawnPid, &childStatus, 0);
            } else {
                // skip the wait and let the child become a zombie process (reaped in SIGCHLD handler)

                // convert number to string
                sprintf(childPidString, "%d", spawnPid);

                // compose and print notice of background process
                strcat(backgroundNotice, backgroundNoticePrefix);
                strcat(backgroundNotice, childPidString);
                strcat(backgroundNotice, "\n");
                printToTerminal(backgroundNotice, false);
            }

            break;
    }

    return;
}


/*
* handles a SIGCHLD signal (when a child process of smallsh terminates/stops/resumes)
* (source: https://stackoverflow.com/a/1512327/14257952)
* signalNumber: used by sigaction() internally
*/
void handleSIGCHLD(int signalNumber) {
    pid_t childPid;
    char* childPidString = calloc(10, sizeof(char));  // space for 10 digits
    int* terminationStatus = malloc(sizeof(int));
    char* terminationStatusString = calloc(10, sizeof(char));  // space for 10 digits
    char* notice = calloc(255, sizeof(char));

    // reap zombie, collecting info about it
    childPid = wait(terminationStatus);

    // convert numbers to strings
    sprintf(childPidString, "%d", childPid);
    sprintf(terminationStatusString, "%d", *terminationStatus);

    // construct notice to sent to terminal
    strcpy(notice, "Process ");
    strcat(notice, childPidString);
    strcat(notice, " terminated by signal ");
    strcat(notice, terminationStatusString);
    strcat(notice, "\n");

    // Notify the user of the process result,
    // without using strlen(), which is not reentrant
    write(STDOUT_FILENO, notice, 255);

    return;
}


/*
* does setup to enable handleSIGCHLD to catch SIGCHLD signals
* (adapted from lecture material)
*/
void setSIGCHLDhandler() {
    // initialize empty sigaction
    struct sigaction SIGCHLDaction = {{0}};  // gcc bug: https://stackoverflow.com/a/13758286/14257952

    // register handler function
    SIGCHLDaction.sa_handler = handleSIGCHLD;

    // block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGCHLDaction.sa_mask);

    // make no flags set
	SIGCHLDaction.sa_flags = 0;

    // install the handler to associate the handler with the signal
    sigaction(SIGCHLD, &SIGCHLDaction, NULL);

    return;
}


/*
* executes a command given to smallsh
* commandLine: pointer to a CommandLine struct which has the command line's details
*/
void executeCommand(struct CommandLine* commandLine) {
    const char commentChar = '#';

    // ignore comment lines
    if (commandLine->command[0] == commentChar) {
        // ignore this whole line; it's a comment
    } else if (isEqualString(commandLine->command, "cd")) {
        // execute the cd command
        handleCdCommand(commandLine);
    } else if (isEqualString(commandLine->command, "exit")) {
        // execute the exit command
        handleExitCommand();
    } else if (isEqualString(commandLine->command, "status")) {
        // execute the status command
        printToTerminal("status coming soon\n", false);
    } else {
        // execute a third-party command
        handleThirdPartyCommand(commandLine);
    }

    return;
}
