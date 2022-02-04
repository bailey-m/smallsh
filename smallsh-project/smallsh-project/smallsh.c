#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

struct command {
	char* command;
	char* args[512];
	char* inputFile;
	char* outputFile;
	int hasAmpersandAsLast;
	int numArgs;
};

/*
* Prints each arg in a command
* Args: char args[512 elements][2048 chars/element] // TODO update this when final size is decided
* Returns: None
*/
void printArgs(char* args[512], int numArgs)
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
	token = strtok(NULL, space);

	// Get args / input/output files
	int i = 0;
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

char* expandPids(char commandInput[2049]) // TODO should this be pointer returned? // TODO fix the copying of strings before and after
{
	// iterate with while loop
	char dollarSign[2] = "$";
	int i = 0;
	int foundFirstDollarSign = 0;
	int strlengg = strlen(commandInput);
	while (i < strlen(commandInput))
	{
		if (strncmp(&commandInput[i], dollarSign, 1) == 0)
		{
			if (foundFirstDollarSign) {
				char expandedInput[2049];
				
				pid_t myPid = getpid();
				char pidString[10];
				snprintf(pidString, 10, "%d", myPid); // convert PID to string
				int indexLastCharBeforeExpansion = i - 1;
				int indexFirstCharAfterExpansion = indexLastCharBeforeExpansion + strlen(pidString);
				
				strncpy(expandedInput, commandInput, indexLastCharBeforeExpansion); // copy portion of string before $$
				strcat(&expandedInput, pidString); // append PID string to portion of string before $$
				// append portion of string after $$
				strncpy(expandedInput + sizeof(char) * indexFirstCharAfterExpansion, commandInput + sizeof(char) * (i+1), strlen(commandInput) - (i+1));

				// Set i back to i-1 (the first new PID character) to resume checking
				i = i - 1;
				foundFirstDollarSign = 0;
				strcpy(commandInput, expandedInput);
				memset(expandedInput, '\0', sizeof(expandedInput));
				//free(expandedInput);
			}
			else
			{
				foundFirstDollarSign = 1;
			}
		}
		i++;
	}
	return commandInput;
}

void displayPrompt(void)
{
	int isExiting = 0;
	char commandInput[2049]; // Supports command line with a max length of 2048 chars
	char* exit = "exit";
	char* cd = "cd";
	char* status = "status";
	char* HOME = "HOME";

	while (isExiting == 0)
	{
		printf("\n: ");
		fflush(stdout);
		scanf(" %s", &commandInput); // TODO handle new line entry
		//printf("\n");
		
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
				// If no args following cd, CD to directory specified by HOME env var
				if (command->numArgs == 0)
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

				}

			}
			// Check for status
			else if (strncmp(commandInput, status, 7) == 0)
			{

			}
			// All other commands
			else
			{

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