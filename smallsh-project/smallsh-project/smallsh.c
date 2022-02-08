#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#define _POSIX_C_SOURCE 200809L

// Structure of command line entry
struct command {
	char* command;
	char* args[513];
	char* inputFile;
	char* outputFile;
	int hasAmpersandAsLast;
	int numArgs;
};

// Global boolean variable for whether or not a SIGTSTP (Ctrl-Z) has been received
int receivedSIGTSTP = 0;

/*
* Prints each arg in a command
* Args: char args[513 elements] (need to support 512 args, but copy the command into the first arg for execvp() )
* Returns: None
* Used primarily for debugging.
*/
void printArgs(char* args[513], int numArgs)
{
	for (int i = 0; i < numArgs; i++)
	{
		printf("arg %d: %s\n", i, args[i]);
		fflush(stdout);
	}
}

/*
* Prints each struct field for the given command
* Args: struct command*
* Returns: None
* Citation: Adapted from Assignment 1 Resources
* Used primarily for debugging.
*/
void printCommand(struct command* command) {
	printf("%s, %s, %s, %d\n", command->command,
		command->inputFile,
		command->outputFile,
		command->hasAmpersandAsLast);
	printArgs(command->args, command->numArgs);
}

/* Parses command line input and breaks down entry into command, args, I/O files.
* Does not do any checking that input or output files are after all commands
* Due to assignment reqs in Section 1 stating: 
* "You do not need to do any error checking on the syntax of the command line"
* Args: char[2049]
* Returns: struct command*
*/
struct command* parseCommand(char commandInput[2049])
{
	// TODO check for $$ expansion
	const char* space = " ";
	const char* inputFileChar = "<";
	const char* outputFileChar = ">";
	const char* ampersand = "&";
	
	struct command* commandStruct = malloc(sizeof(struct command));
	commandStruct->hasAmpersandAsLast = 0;

	// Get command (first string before space)
	char* token;
	token = strtok(commandInput, space);
	commandStruct->command = calloc(strlen(token) + 1, sizeof(char));
	strcpy(commandStruct->command, token);

	// Add command as first element of args array, since execvp()/our method for executing child processes requires this
	commandStruct->args[0] = calloc(strlen(token) + 1, sizeof(char));
	strcpy(commandStruct->args[0], token);
	token = strtok(NULL, space);

	// Get args / input/output files
	int i = 1;
	while (token != NULL)
	{
		// Check for < 
		if (strncmp(token, inputFileChar, 1) == 0)
		{
			token = strtok(NULL, space); // Get next token (file name)
			commandStruct->inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandStruct->inputFile, token);
		}
		// Check for >
		else if (strncmp(token, outputFileChar, 1) == 0)
		{
			token = strtok(NULL, space); // Get next token (file name)
			commandStruct->outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandStruct->outputFile, token);
		}
		// Check for &
		else if (strncmp(token, ampersand, 1) == 0)
		{
			commandStruct->hasAmpersandAsLast = 1;
		}
		else
		{
			// Add to args array
			commandStruct->args[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandStruct->args[i], token);
			i++;
		}
		token = strtok(NULL, space);
	}
	commandStruct->numArgs = i;
	return commandStruct;
}

/* Iterates through each character of the command line entry and checks for non-whitespace chars.
* If no non-whitespace chars are found, returns 1 (True), otherwise, returns 0 (False)
* Args: char[2049]
* Returns: int (1 or 0)
*/
int isBlankLine(char commandInput[2049])
{
	char space[2] = " ";
	for (int i = 0; i < strlen(commandInput); i++)
	{
		// If a non-blank character is encountered, return 0 (False)
		if (strncmp(&commandInput[i], space, 1) != 0)
		{
			return 0;
		}
	}
	return 1;
}

/* Iterates through command line to compare the first non-whitespace char to #.
* If the first non-whitespace char = #, then the line is a comment, and returns 1 (True), otherwise, returns 0 (False)
* Args: char[2049]
* Returns: int (1 or 0)
*/
int isComment(char commandInput[2049])
{
	char pound[2] = "#";
	char space[2] = " ";
	// Iterate through potential blank space before comment
	for (int i = 0; i < strlen(commandInput); i++) {
		// If the char is not a blank space
		if (strncmp(&commandInput[i], space, 1) != 0)
		{
			// If the char is a #, return 1 (True)
			if (strncmp(&commandInput[i], pound, 1) == 0)
			{
				return 1;
			}
			// Otherwise, return 0 (False)
			else
			{
				return 0;
			}
		}
	}
	return 0;
}

/* Iterates through the command line entry to look for instances of '$$'
* Replaces any instances of '$$' in-place with the current process ID,
* Then resumes checking the rest of the line until the end is reached.
* Args: char[2049]
* Returns: char*, the newly PID-expanded command line entry.
*/
char* expandPids(char commandInput[2049]) 
{
	char dollarSign[2] = "$";
	int i = 0;
	int foundFirstDollarSign = 0;
	
	while (i < strlen(commandInput))
	{
		// Compare each char in the command to '$'
		if (strncmp(&commandInput[i], dollarSign, 1) == 0)
		{
			if (foundFirstDollarSign) {
				char expandedInput[2049];
				
				pid_t myPid = getpid();
				char pidString[10];
				snprintf(pidString, 10, "%d", myPid); // convert PID to string
				int indexLastCharBeforeExpansion = i - 1;
				int indexFirstCharAfterExpansion = indexLastCharBeforeExpansion + strlen(pidString);
				
				// copy portion of string before $$
				strncpy(expandedInput, commandInput, indexLastCharBeforeExpansion);
				// append PID string to portion of string before $$
				strcat(&expandedInput, pidString);
				// append portion of string after $$
				strncpy(expandedInput + sizeof(char) * indexFirstCharAfterExpansion, commandInput + sizeof(char) * (i+1), strlen(commandInput) - (i+1));

				// Set i back to i-1 (the first new PID character) to resume checking
				i = i - 1;
				foundFirstDollarSign = 0;
				strcpy(commandInput, expandedInput);
				memset(expandedInput, '\0', sizeof(expandedInput));
				//free(expandedInput); // TODO test this
			}
			else
			{
				foundFirstDollarSign = 1;
			}
		}
		// Each time a char != "$", reset the foundFirst$ to 0
		// So that only consecutive $ (i.e. $$) trigger pid expansion
		else 
		{
			foundFirstDollarSign = 0;
		}
		i++;
	}
	return commandInput;
}

/* Performs the 'cd' command using chdir().
* If no arguments given, changes dir to the path specified by the OS's $HOME env variable.
* Otherwise, changes dir to the directory specified in the first argument following the command.
* Supports absolute and relative paths.
* Args: struct command*
* Returns: None
*/
void changeDir(struct command* command)
{
	char* HOME = "HOME";
	// If no args following cd, CD to directory specified by HOME env var
	if (command->numArgs == 1)
	{
		char* homeDir = getenv(HOME);
		int cdSuccess = chdir(homeDir);
		if (cdSuccess != 0)
		{
			printf("Error changing directory.\n");
			fflush(stdout);
		}
		else
		{
			char* cwdPath[2049];
			//getcwd(cwdPath, sizeof(cwdPath)); // TODO remove this and lines below, for debugging only
			//printf("current dir is: %s\n", cwdPath);
			//fflush(stdout);
		}
	}
	// If args following CD
	else
	{
		// Get CWD
		char* cwdPath[2049];
		getcwd(cwdPath, sizeof(cwdPath));
		// Get desired directory
		char* filePath = malloc(sizeof(command->args[1]));
		strcpy(filePath, command->args[1]);
		// Check if user included absolute path
		if (strncmp(cwdPath, filePath, strlen(cwdPath)) == 0) // TODO not sure if comparing against CWD is correct?? we may just want to check for first char being /
		{
			// If they did, we don't need to concat the full CWD onto the desired directory
			//printf("desired dir is: %s\n", filePath);
			//fflush(stdout);
			int cdSuccess = chdir(filePath);
		}
		else
		{
			// If they didn't, need to concat a '/' and the desired file path onto the CWD
			char* slash = "/";
			strcat(cwdPath, slash);
			strcat(cwdPath, filePath);
			//printf("desired dir is: %s\n", cwdPath);
			//fflush(stdout);
			int cdSuccess = chdir(cwdPath);
		}
		int cdSuccess = chdir(cwdPath);
		if (cdSuccess != 0)
		{
			printf("Error changing directory.\n");
			fflush(stdout);
		}
		/*
		else
		{
			char* cwdPath[2049];
			getcwd(cwdPath, sizeof(cwdPath)); // TODO remove this and lines below, for debugging only
			printf("current dir is: %s\n", cwdPath);
			fflush(stdout);
		} */
		memset(cwdPath, '/0', strlen(cwdPath));
	}
}

/*
* Uses fork() to create a child process, then uses execvp() to execute the child process.
* If the user entered a '&' as the last arg in their command line entry,
* the process is run in the background - i.e. the program does not wait for it to complete
* execution before prompting the user again. 
* 
* If the user did not enter a '&' as the last arg, the process is run in the foreground,
* i.e. the program waits for process completion before prompting the user again.
* 
* Can redirect input and output if I/O redirect files are specified in the command.
* For background processes, if I/O files are not specified, redirects I/O to /dev/null.
* 
* Args: struct command*
* Returns: pid_t (the PID of the child process it executed).
* Citation: Module 4 Exploration : Process API - Executing a New Program
*/
pid_t createFork(struct command* command)
{
	int childStatus;
	char* executableCommand = malloc(sizeof(command->command));
	strcpy(executableCommand, command->command);

	// Parent forks off a child
	pid_t spawnPid = fork();

	switch (spawnPid) {
	case -1:
		// Error while forking
		perror("fork()\n");
		fflush(stdout);
		exit(1);
		break;
	case 0:
	{
		// In the child process

		// Foreground and background child processes ignore SIGTSTP
		struct sigaction ignore_action = { 0 };
		ignore_action.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &ignore_action, NULL);
		signal(SIGTSTP, SIG_IGN);

		// If background process, ignore SIG_INT
		if (command->hasAmpersandAsLast) {
			sigaction(SIGINT, &ignore_action, NULL);
			signal(SIGTSTP, SIG_IGN);
		}

		// Check for input redirection (or set input redirection to /dev/null if BG process and no specified input file)
		if (command->inputFile || command->hasAmpersandAsLast)
		{
			char* inputFile = malloc(257);
			if (command->inputFile)
			{
				strcpy(inputFile, command->inputFile);
			}
			else if (command->hasAmpersandAsLast)
			{
				inputFile = "/dev/null";
			}

			// Open source file
			int sourceFD = open(inputFile, O_RDONLY);
			if (sourceFD == -1) {
				perror("Cannot open the source file");
				fflush(stdout);
				exit(1);
			}
			// Written to terminal
			//printf("sourceFD == %d\n", sourceFD);
			//fflush(stdout);

			// Redirect stdin to source file
			int result = dup2(sourceFD, 0);
			if (result == -1) {
				perror("Cannot read from the source file");
				fflush(stdout);
				exit(2);
			}
		}

		// Check for output redirection (or set output redirection to /dev/null if BG process and no specified output file)
		if (command->outputFile || command->hasAmpersandAsLast)
		{
			char* outputFile = malloc(257);
			if (command->outputFile)
			{
				strcpy(outputFile, command->outputFile);
			}
			else if (command->hasAmpersandAsLast)
			{
				outputFile = "/dev/null";
			}

			// Open target file
			int targetFD = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (targetFD == -1) {
				perror("Cannot open the target file");
				fflush(stdout);
				exit(1);
			}
			//printf("targetFD == %d\n", targetFD); // Written to terminal
			//fflush(stdout);

			// Redirect stdout to target file
			int result = dup2(targetFD, 1);
			if (result == -1) {
				perror("Cannot write to the target file");
				fflush(stdout);
				exit(2);
			}
		}

		// Child will use execvp() to run command
		// Execvp looks for the command in PATH specified variable
		int execStatus = execvp(executableCommand, command->args);
		// exec only returns if there is an error
		if (execStatus == -1)
		{
			perror("Cannot find command");
			fflush(stdout);
			exit(1); // TODO is this being set correctly?? check the test script
		}
		else
		{
			exit(2);
		}
		break;
	}
	default:
		// In the parent process
		// Don't wait child process to terminate if it's a BG command

		// Wait for child process to terminate after running a command if it's a FG command
		if (!command->hasAmpersandAsLast)
		{
			spawnPid = waitpid(spawnPid, &childStatus, 0);
			//printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
			//fflush(stdout);
			spawnPid = childStatus; // set spawnPid to be the exit status of the foreground process, for printing on next 'status' command
			// TODO check if this is the right exit status
		}
		else
		{
			printf("Background PID %d has started", spawnPid);
		}
		
		// Return spawnPid back to displayPrompt loop so that we can check for its termination
		return spawnPid;
		break;
	}
	memset(executableCommand, '/0', strlen(executableCommand));
	//free(executableCommand);
}

/* Loops through an array of background child process PIDs
* and checks if they have terminated. If they have, prints a message with the PID
* stating that it has terminated, and removes the PID from the array.
* Args: pid_t[200] (array of child PIDs)
* Returns: int (number of PIDs in the array that have NOT yet terminated)
*/
int checkChildProcessesForTermination(pid_t childProcessesRunning[200], int numChildProcesses)
{
	pid_t nonTerminatedChildren[200];
	int numNonTerminatedChildren = 0;
	for (int i = 0; i < numChildProcesses; i++)
	{
		int childStatus;
		// using waitpid() to check for termination (from Linux manpages):
		// if WNOHANG was specified and one or more child(ren) specified by pid exist, 
		// but have not yet changed state, then 0 is returned
		// 
		// Therefore: if waitpid() returns 0 -> PID is still running
		pid_t childPid = childProcessesRunning[i];
		pid_t pidStatus = waitpid(childPid, &childStatus, WNOHANG);
		if (pidStatus == 0)
		{
			nonTerminatedChildren[numNonTerminatedChildren] = childPid;
			numNonTerminatedChildren++;
		}
		// If waitpid() returns -1, there was an error
		else if (pidStatus == -1)
		{
			printf("Error checking PID %d\n", childPid);
			//perror("error");
			fflush(stdout);
		}
		// If waitpid() returns the PID, the child process has terminated
		else
		{
			// Print process ID and exit status
			printf("Child process %d terminated with exit status %d.\n", childPid, childStatus); // TODO check if this is the right exit status
			fflush(stdout);
		}
	}

	// Copy nonTerminatedChildren back over into original input array, childProcessesRunning
	for (int i = 0; i < 200; i++)
	{
		if (i < numNonTerminatedChildren)
		{
			childProcessesRunning[i] = nonTerminatedChildren[i];
		}
		else
		{
			childProcessesRunning[i] = 0;
		}
	}

	return numNonTerminatedChildren;
}

/* Iterates through array of child process PIDs and kills each process.
* Invoked immediately before shell exit.
* Args: pid_t[200] (array of child process PIDs), int (number of currently running child processes).
* Returns: None
*/
void killChildProcesses(pid_t childProcessesRunning[200], int numChildProcesses)
{
	for (int i = 0; i < numChildProcesses; i++)
	{
		kill(childProcessesRunning[i], SIGKILL);
	}
}

/* Checks if SIGTSTP was called since the last time it was checked and displays the correct message.
* Args: int
* Returns: None
*/
void checkForSIGTSTP(int oldReceivedSIGTSTP)
{
	if (oldReceivedSIGTSTP != receivedSIGTSTP)
	{
		if (receivedSIGTSTP == 1)
		{
			printf("SIGTSTP received. Entering foreground only mode.\n");
			fflush(stdout);
		}
		else
		{
			printf("SIGTSTP received. Background processes now permitted.\n");
			fflush(stdout);
		}
	}
}

/*
* Uses a while loop to continuously prompt the user for command line input.
* Retrieves user input and uses parseCommand() to assign user input to a command struct.
* Has built-in handling of commands 'status', 'cd', and 'exit'.
* Passes all other commands to createFork().
* Args: None
* Returns: None
*/
void displayPrompt(void)
{
	int isExiting = 0;
	char commandInput[2049]; // Supports command line with a max length of 2048 chars
	char* exit = "exit";
	char* cd = "cd";
	char* status = "status";
	int lastForegroundExitStatus = 0;
	int numChildProcesses = 0;
	pid_t childProcessesRunning[200]; // 'ulimit -a' on os1 states a maximum of 200 user processes
	int oldReceivedSIGTSTP = receivedSIGTSTP; // used to check for differences in receivedTSTP value

	while (isExiting == 0)
	{
		
		// Check if any of the currently running child processes have terminated
		numChildProcesses = checkChildProcessesForTermination(childProcessesRunning, numChildProcesses);

		printf("\n: ");
		fflush(stdout);
		scanf("%[^\n]%*c", &commandInput); 

		// Check for SIGTSTP from command line
		checkForSIGTSTP(oldReceivedSIGTSTP);
		oldReceivedSIGTSTP = receivedSIGTSTP;
		
		if (!isBlankLine(&commandInput) && !isComment(&commandInput)) {
			char* expandedCommandInput = expandPids(commandInput);
			strcpy(commandInput, expandedCommandInput);
			struct command* command = parseCommand(commandInput);

			// If SIGTSTP is currently in foreground-only mode, set ampersand boolean to 0
			if (receivedSIGTSTP) {
				command->hasAmpersandAsLast = 0;
			}
			//printCommand(command); // TODO remove this line and line below, for debugging only
			//fflush(stdout);

			// Check for exit
			if (strncmp(command->command, exit, 5) == 0)
			{
				killChildProcesses(childProcessesRunning, numChildProcesses);
				isExiting = 1;
			}
			// Check for cd
			else if (strncmp(command->command, cd, 3) == 0)
			{
				changeDir(command);
			}
			// Check for status
			else if (strncmp(commandInput, status, 7) == 0)
			{
				printf("Exit status: %d\n", lastForegroundExitStatus);
				fflush(stdout);
			}
			// All other commands
			else
			{
				pid_t mostRecentChildProcess = createFork(command);
				// If this was a BG process, keep track of the child PID
				if (command->hasAmpersandAsLast)
				{
					// Add the running PID to an array of PIDs
					childProcessesRunning[numChildProcesses] = mostRecentChildProcess;
					numChildProcesses++;
				}
				// Otherwise, store it as the FG process exit status (this is used in built-in 'status' command)
				else
				{
					lastForegroundExitStatus = mostRecentChildProcess;

					// Check for SIGTSTP after termination of last foreground process
					checkForSIGTSTP(oldReceivedSIGTSTP);
					oldReceivedSIGTSTP = receivedSIGTSTP;
				}
			}
		}
		memset(commandInput, '\0', sizeof(commandInput));
	}
}

/*
* Handles SIGINT (Ctrl-C) by printing a message.
* SIGINT will then go on to kill any foreground processes.
* Args: int (signal number)
* Returns: None
*/
void handle_SIGINT(int signo)
{
	char* message = "Caught SIGINT, process terminated by signal 2.\n";
	fflush(stdout);
	write(STDOUT_FILENO, message, 39);
}

/*
* Handles SIGTSTP (Ctrl-Z) by printing a message.
* SIGINT will then go on to kill any foreground processes.
* Args: int (signal number)
* Returns: None
*/
void handle_SIGTSTP(int signo)
{
	if (receivedSIGTSTP == 0)
	{
		receivedSIGTSTP = 1;
	}
	else
	{
		receivedSIGTSTP = 0;
	}
} 

int main()
{
	// Signal handler installation taken from Module 5 Explorations

	// SIGINT HANDLER

	// Initialize SIGINT_action struct to be empty
	struct sigaction SIGINT_action = { 0 };
	// Register handle_SIGINT as the signal handler
	SIGINT_action.sa_handler = handle_SIGINT;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
	// Install our signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

	

	// SIGTSTP HANDLER
	

	// Initialize SIGITSTP_action struct to be empty
	struct sigaction SIGTSTP_action = { 0 };
	// Register handle_SIGTSTP as the signal handler
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGTSTP is running
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = 0;

	// Install our signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL); 

	displayPrompt();
	return 0;
}