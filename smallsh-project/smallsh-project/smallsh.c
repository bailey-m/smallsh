#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>

struct command {
	char* command;
	char* args[513];
	char* inputFile;
	char* outputFile;
	int hasAmpersandAsLast;
	int numArgs;
};

/*
* Prints each arg in a command
* Args: char args[513 elements] (need to support 512 args, but copy the command into the first arg for execvp() )
* Returns: None
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
*/
void printCommand(struct command* command) {
	printf("%s, %s, %s, %d\n", command->command,
		command->inputFile,
		command->outputFile,
		command->hasAmpersandAsLast);
	printArgs(command->args, command->numArgs);
}

// Does not do any checking that input or output files are after all commands
// Due to assignment reqs in Section 1 stating: 
// "You do not need to do any error checking on the syntax of the command line"
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
			// TODO if we need to support space in file path for cd, check for that here
			commandStruct->args[i] = calloc(strlen(token) + 1, sizeof(char));
			strcpy(commandStruct->args[i], token);
			i++;
		}
		token = strtok(NULL, space);
	}
	commandStruct->numArgs = i;
	return commandStruct;
}

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
			getcwd(cwdPath, sizeof(cwdPath)); // TODO remove this and lines below, for debugging only
			printf("current dir is: %s\n", cwdPath);
			fflush(stdout);
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
			printf("desired dir is: %s\n", filePath);
			fflush(stdout);
			int cdSuccess = chdir(filePath);
		}
		else
		{
			// If they didn't, need to concat a '/' and the desired file path onto the CWD
			char* slash = "/";
			strcat(cwdPath, slash);
			strcat(cwdPath, filePath);
			printf("desired dir is: %s\n", cwdPath);
			fflush(stdout);
			int cdSuccess = chdir(cwdPath);
		}
		int cdSuccess = chdir(cwdPath);
		if (cdSuccess != 0)
		{
			printf("Error changing directory.\n");
			fflush(stdout);
		}
		else
		{
			char* cwdPath[2049];
			getcwd(cwdPath, sizeof(cwdPath)); // TODO remove this and lines below, for debugging only
			printf("current dir is: %s\n", cwdPath);
			fflush(stdout);
		}
		memset(cwdPath, '/0', strlen(cwdPath));
	}
}

// Citation: Module 4 Exploration: Process API - Executing a New Program
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
		// In the child process
		printf("CHILD(%d) running command\n", getpid());
		fflush(stdout);

		int sourceFD = 0;
		int targetFD = 1;


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
				perror("source open()");
				fflush(stdout);
				exit(1);
			}
			// Written to terminal
			//printf("sourceFD == %d\n", sourceFD);
			//fflush(stdout);

			// Redirect stdin to source file
			int result = dup2(sourceFD, 0);
			if (result == -1) {
				perror("source dup2()");
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
				perror("target open() error");
				fflush(stdout);
				exit(1);
			}
			//printf("targetFD == %d\n", targetFD); // Written to terminal
			//fflush(stdout);

			// Redirect stdout to target file
			int result = dup2(targetFD, 1);
			if (result == -1) {
				perror("target dup2() error");
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
			perror("execvp error");
			fflush(stdout);
			exit(1); // TODO is this being set correctly?? check the test script
		}
		else 
		{
			exit(2);
		}
		break;
	default:
		// In the parent process
		// Don't wait child process to terminate if it's a BG command

		// Wait for child process to terminate after running a command if it's a FG command
		if (!command->hasAmpersandAsLast)
		{
			spawnPid = waitpid(spawnPid, &childStatus, 0);
			printf("PARENT(%d): child(%d) terminated. Exiting\n", getpid(), spawnPid);
			fflush(stdout);
			spawnPid = childStatus; // set spawnPid to be the exit status of the foreground process, for printing on next 'status' command
			// TODO check if this is the right exit status
		}
		
		// Return spawnPid back to displayPrompt loop so that we can check for its termination
		return spawnPid;
		break;
	}
	memset(executableCommand, '/0', strlen(executableCommand));
	//free(executableCommand);
}

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
			perror("error");
			fflush(stdout);
		}
		// If waitpid() returns the PID, the child process has terminated
		else
		{
			// Print process ID and exit status
			printf("Child process %d terminated with exit status %d.\n", childPid, childStatus); // TODO check if this is the right exit status
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

	while (isExiting == 0)
	{
		// Check if any of the currently running child processes have terminated
		numChildProcesses = checkChildProcessesForTermination(childProcessesRunning, numChildProcesses);
		printf("\n: ");
		fflush(stdout);
		scanf("%[^\n]%*c", &commandInput); // TODO handle blank new line entry
		
		if (!isBlankLine(&commandInput) && !isComment(&commandInput)) {
			char* expandedCommandInput = expandPids(commandInput);
			strcpy(commandInput, expandedCommandInput);
			struct command* command = parseCommand(commandInput);
			printCommand(command); // TODO remove this line and line below, for debugging only
			fflush(stdout);

			// Check for exit
			if (strncmp(command->command, exit, 5) == 0)
			{
				// TODO kill all processes created by this shell
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
				// Otherwise, store it as the FG process exit status 
				else
				{
					lastForegroundExitStatus = mostRecentChildProcess; // TODO check if this is the right exit status
				}
			}
		}
		memset(commandInput, '\0', sizeof(commandInput));
	}
}

int main()
{
	displayPrompt();
	return 0;
}