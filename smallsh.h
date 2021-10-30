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
#define MAX_BG_CHILDREN 100  // defined in specs


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


// globals used to track PIDs of background child processes
// syntax reminder from https://stackoverflow.com/a/201116/14257952
pid_t GLOBAL_backgroundChildrenPids[MAX_BG_CHILDREN] = {0};
int GLOBAL_lastForegroundChildStatus = 0;  // default to 0 per specs


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
* Redirects stdin to point to a given file
* If sourceFile isn't provided, stdin will be redirected to /dev/null
* sourceFile: path of the file to use for stdin
* (adapted from lesson material)
* (source of courage to open /dev/null: https://stackoverflow.com/a/14846891/14257952)
*/
void redirectStdin(char* sourceFile) {
    // check for redirecting to /dev/null
    char* redirectPath = sourceFile ? sourceFile : "/dev/null";

    // open source file
    int sourceFD = open(redirectPath, O_RDONLY);

    if (sourceFD == -1) { 
        printf("cannot open %s for input\n", redirectPath);
        fflush(NULL);
		exit(1);
	}

    // redirect stdin to source file
	int result = dup2(sourceFD, 0);  // 0 is used for stdin default
	if (result == -1) { 
        printToTerminal("couldn't redirect stdin to input file via dup2(), but it was a good file\n", true);
		exit(2); 
	}

    return;
}


/*
* Redirects stdout to point to a given file
* If outputFile isn't provided, stdout will be redirected to /dev/null
* outputFile: path of the file to use for stdout
* (adapted from lesson material)
* (source of courage to open /dev/null: https://stackoverflow.com/a/14846891/14257952)
*/
void redirectStdout(char* outputFile) {
    // check for redirecting to /dev/null
    char* redirectPath = outputFile ? outputFile : "/dev/null";

    // open output file
    int outputFD = open(redirectPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (outputFD == -1) { 
        printf("cannot open %s for output\n", redirectPath);
        fflush(NULL);
		exit(1);
	}

    // redirect stdout to output file
	int result = dup2(outputFD, 1);  // 1 is used for stdout default
	if (result == -1) { 
        printToTerminal("couldn't redirect stdout to output file via dup2(), but it was a good file\n", true);
		exit(2); 
	}

    return;
}


/*
* reads a line from the prompt and saves parsed input to the given struct
* does not check for syntax errors (per specs)
* does not support quoting, so arguments with spaces are not possible (per specs)
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
    char* delimiter = " ";
    char inputRedirectChar = '<';
    char outputRedirectChar = '>';
    char backgroundChar = '&';
    bool isInFileName = false;
    bool isOutFileName = false;
    bool argsAreDone = false;
    bool isSpecialChar = false;
    struct CommandLine* commandLine = malloc(sizeof(struct CommandLine));

    printf("\tDEBUG: in parseCommandString\n");
    fflush(NULL);

    // initialize the CommandLine struct's fixed-size array to all null pointers
    // and initialize its other defaults
    commandLine->args = calloc(MAX_ARG_COUNT, sizeof(char*));
    commandLine->argCount = 0;
    commandLine->isBackground = false;
    commandLine->inFile = NULL;
    commandLine->outFile = NULL;

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
* Imitates strtok() but uses a string delimiter
* (adapted from https://stackoverflow.com/a/29848367/14257952)
* str: input string to tokenize
* delim: delimiter
* return: the next segment of str originally ending with delim (the final segment is returned regardless)
*/
char* strtokm(char *str, const char *delim, bool *isFinalSegment)
{
    static char *tok;
    static char *next;
    char *temp;

    // check for invalid delimiter param
    if (delim == NULL) return NULL;

    // extract the next token segment, starting at the beginning of str
    // if str was provided
    tok = (str) ? str : next;
    if (tok == NULL) return NULL;

    // check for occurrence of delimiter in this token segment
    temp = strstr(tok, delim);

    if (temp) {
        // move next pointer past the delimiter and reset m
        next = temp + strlen(delim);
        *temp = '\0';
    } else {
        // this token segment doesn't have the delimiter, so it's the end
        next = NULL;
    }

    if (next == NULL) {
        *isFinalSegment = true;
    }

    return tok;
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
    bool* isFinalSegment = malloc(sizeof(bool));
    *isFinalSegment = false;
    char* token = strtokm(stringIn, pidVariable, isFinalSegment);

    if (strlen(token) == originalLength) {
        // there are no occurrences of the pid variable
        strcpy(stringOut, stringIn);
    } else {
        // There are some occurrences of the pid variable.
        // Continue to copy segments of stringIn while substituting
        // the smallsh pid for the variable
        while (token) {
            // copy the string up to this point
            strcpy(stringTemp, stringOut);

            // Resize stringOut to fit another pid
            // Make space for stringTemp + this token + pid
            free(stringOut);  // stringOut was copied to stringTemp
            newLength = strlen(stringTemp) + strlen(token) + strlen(pidString) + 1;
            stringOut = calloc(newLength, sizeof(char));

            // restore stringOut from before this iteration
            strcpy(stringOut, stringTemp);
            strcat(stringOut, token);

            // Check for the special case of the final segment, because strtokm() will need to 
            // return a token for the final segment even if there isn't a delimiter in it
            if (!*isFinalSegment) {
                // append the pid, because the delimiter was found in this token
                strcat(stringOut, pidString);
            }

            // for next iteration
            free(stringTemp);
            stringTemp = calloc(newLength, sizeof(char));

            // try to extract another token
            token = strtokm(NULL, pidVariable, isFinalSegment);
        }
    }

    return stringOut;
}


/*
* gets a new command from the user
* return: user input, expanded with smallsh pid in place of $$
*/
char* getUserCommandString() {
    printf("\tDEBUG: in getUserCommandString\n");
    fflush(NULL);

    char* userInput = calloc(MAX_INPUT_LENGTH, sizeof(char));

    // get raw string from user
    fgets(userInput, MAX_INPUT_LENGTH + 1, stdin);

    // remove \n appended by fgets (source: https://stackoverflow.com/a/28462221/14257952)
    userInput[strcspn(userInput, "\n")] = 0;

    // return input, expanded with smallsh pid in place of $$
    return expandPidVariable(userInput);
}


/*
* Changes the current directory, supporting relative and absolute paths
* If no argument is given, changes to the user's home directory
* commandLine: pointer to a CommandLine struct which has the command line's details
*/
void handleCdCommand(struct CommandLine* commandLine) {
    // handle the command with no argument
    if (!commandLine->args[0]) {
        // change the current directory to the HOME directory
        chdir(getenv("HOME"));
    } else {
        // change the current directory to the specified path
        chdir(commandLine->args[0]);
    }

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
* Prints the exit status of the last foreground process run by smallsh
* If no foreground command has been run yet, prints 0
*/
void handleStatusCommand() {
    // print notice of exit status for last foreground child
    printf("exit value %d\n", GLOBAL_lastForegroundChildStatus);
    fflush(NULL);

    return;
}


/*
* ignores a SIGINT signal (when a process receives a ctrl+Z interrupt signal)
* signalNumber: used by sigaction() internally
*/
void ignoreSIGINT(int signalNumber) {
    // do nothing, overriding default SIGINT handler behavior
    char* debugMessage = "DEBUG: Caught SIGINT, ignoring\n";
	write(STDOUT_FILENO, debugMessage, 31);
    fflush(NULL);

    return;
}


/*
* does setup to enable ignoreSIGINT to catch SIGINT signals
* (adapted from lecture material)
*/
void setSIGINThandler() {
    // initialize empty sigaction
    struct sigaction SIGINTaction = {{0}};  // gcc bug: https://stackoverflow.com/a/13758286/14257952

    // register handler function
    SIGINTaction.sa_handler = ignoreSIGINT;

    // block all catchable signals while handler is running
	sigfillset(&SIGINTaction.sa_mask);

    // make no flags set
	SIGINTaction.sa_flags = 0;

    // install the handler to associate the handler with the signal
    sigaction(SIGINT, &SIGINTaction, NULL);

    return;
}


/*
* resets SIGINT signal handling behavior to default
* (adapted from lecture material)
* (inspired by https://stackoverflow.com/a/24804019/14257952)
*/
void resetSIGINThandler() {
    // initialize empty sigaction
    struct sigaction SIGINTaction = {{0}};  // gcc bug: https://stackoverflow.com/a/13758286/14257952

    // register handler function back to default
    SIGINTaction.sa_handler = SIG_DFL;

    // block all catchable signals while handler is running
	sigfillset(&SIGINTaction.sa_mask);

    // make no flags set
	SIGINTaction.sa_flags = 0;

    // install the handler to associate the handler with the signal
    sigaction(SIGINT, &SIGINTaction, NULL);

    return;
}


/*
* registers signal handlers for a new background child process
*/
void registerNewBgChildSignals() {
    // ignore sigint
    setSIGINThandler();

    return;
}


/*
* adds a PID to the global array tracking background child PIDs
* pid_in: pid to add to the global list
*/
void registerNewBgChildPid(pid_t pid_in) {
    // add this process id to the list, at the first 0
    for (int index = 0; index < MAX_BG_CHILDREN; ++index) {
        // find the spot in the array with the first 0
        if (GLOBAL_backgroundChildrenPids[index] == 0) {
            // add the pid to the list
            GLOBAL_backgroundChildrenPids[index] = pid_in;

            // DEBUG
            printf("\tDEBUG: started tracking new pid %d at index %d: %d\n", pid_in, index, GLOBAL_backgroundChildrenPids[index]);

            break;
        }
    }

    return;
}


/*
* Removes a PID from the global array tracking background child PIDs
* If the provided PID isn't in the array, nothing happens
* pid_in: pid to remove from the global list
*/
void unregisterBgChildPid(pid_t pid_in) {
    // remove this process id from the list, if found
    for (int index = 0; index < MAX_BG_CHILDREN; ++index) {
        // find the spot in the array with the given pid
        if (GLOBAL_backgroundChildrenPids[index] == pid_in) {
            // Remove the pid from the list.
            // No need to shift the remaining elements, because the function that adds
            // elements will add in the first 0 spot
            GLOBAL_backgroundChildrenPids[index] = 0;

            break;
        }
    }

    return;
}


/*
* Checks whether a PID is in the global array tracking background child PIDs
* pid_in: pid to check for in the global list
* return: true if the given PID is in the gloabl list; false if not
*/
bool isTrackedBgChild(pid_t pid_in) {
    // check the list for this process id
    for (int index = 0; index < MAX_BG_CHILDREN; ++index) {
        // DEBUG
        printf("\tDEBUG: pid at index %d: %d\tpid given: %d\n", index, GLOBAL_backgroundChildrenPids[index], pid_in);
        fflush(NULL);
        // find the spot in the array with the given pid
        if (GLOBAL_backgroundChildrenPids[index] == pid_in) {
            // pid was found
            return true;
        }
    }

    // pid was not found in the above loop
    return false;
}


/*
* does everything that needs to be done by a new child process
*/
void handleNewBgChild() {
    // background children must handle signals differently (per specs)
    registerNewBgChildSignals();

    return;
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
    char* childPidString = calloc(10, sizeof(char));  // room for 9 digits
    char* backgroundNotice = calloc(strlen(backgroundNoticePrefix) + 10, sizeof(char));  // room for 9 digits

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

            if (commandLine->isBackground) {
                handleNewBgChild();
            } else {
                // this is a foreground child, so SIGINT shouldn't be blocked (per specs)
                resetSIGINThandler();
            }

            // Redirect input if the user asked to
            // Else, if it's background, suppress input (per specs)
            if (commandLine->inFile) {
                redirectStdin(commandLine->inFile);
            } else if (commandLine->isBackground) {
                redirectStdin(NULL);
            }

            // Redirect output if the user asked to
            // Else, if it's background, suppress output (per specs)
            if (commandLine->outFile) {
                redirectStdout(commandLine->outFile);
            } else if (commandLine->isBackground) {
                redirectStdout(NULL);
            }

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

                // track background children
                registerNewBgChildPid(spawnPid);
                
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
    // write(STDOUT_FILENO, "\n\tDEBUG: caught SIGCHLD\n", 24);

    pid_t childPid;
    char* childPidString = calloc(10, sizeof(char));  // space for 10 digits
    int* terminationStatus = malloc(sizeof(int));
    char* terminationStatusString = calloc(10, sizeof(char));  // space for 10 digits
    char* notice = calloc(255, sizeof(char));

    // reap zombie, collecting info about it
    childPid = waitpid(-1, terminationStatus, WNOHANG);
    if (childPid == -1) {
        perror("sigchld handler: \n");
        fflush(NULL);
    }

    // if it was a foreground process that just ended, record its status without a notice to the user
    if (!isTrackedBgChild(childPid)) {
        // DEBUG
        sprintf(childPidString, "%d", childPid);
        strcpy(notice, "foreground child pid ");
        strcat(notice, childPidString);
        strcat(notice, "\n");
        // get length of notice
        int noticeLength = 0;
        char* checkCharPointer = notice;
        while (*checkCharPointer != '\0') {
            ++noticeLength;
            ++checkCharPointer;
        }
        write(STDOUT_FILENO, notice, noticeLength);

        GLOBAL_lastForegroundChildStatus = *terminationStatus;
    } else {
        // this was a background process that just ended
        // construct notice to sent to terminal
        sprintf(childPidString, "%d", childPid);
        sprintf(terminationStatusString, "%d", *terminationStatus);
        strcpy(notice, "background pid ");
        strcat(notice, childPidString);
        strcat(notice, " is done: exit value ");
        strcat(notice, terminationStatusString);
        strcat(notice, "\n");
        
        // get length of notice
        int noticeLength = 0;
        char* checkCharPointer = notice;
        while (*checkCharPointer != '\0') {
            ++noticeLength;
            ++checkCharPointer;
        }

        // print the notice
        write(STDOUT_FILENO, notice, noticeLength);
    }

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

    // block all catchable signals while ignore_SIGINT is running
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

    printf("\n\tDEBUG: in executeCommand(), executing command %s\n", commandLine->command);
    fflush(NULL);

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
        handleStatusCommand();
    } else {
        // execute a third-party command
        handleThirdPartyCommand(commandLine);
    }

    return;
}
