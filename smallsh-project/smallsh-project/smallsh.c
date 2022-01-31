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
};

/*
* Prints each arg in a command
* Args: char args[512 elements][2048 chars/element] // TODO update this when final size is decided
* Returns: None
*/
void printArgs(char* args[512])
{
	for (int i = 0; i < 512; i++)
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
	printArgs(command->args);
}

// Does not do any checking that input or output files are after all commands
// Due to assignment reqs in Section 1 stating: 
// "You do not need to do any error checking on the syntax of the command line"
struct command* parseCommand(char commandInput[2049])
{
	// TODO check for comments, blank lines, and $$ expansion
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
	return commandStruct;
}

void displayPrompt(void)
{
	int isExiting = 0;
	char commandInput[2049]; // Supports command line with a max length of 2048 chars
	char exit = "exit";
	char cd = "cd";
	char status = "status";
	char* HOME = "HOME";

	while (isExiting == 0)
	{
		printf("\n: ");
		fflush(stdout);
		scanf(" %[^\n]", commandInput);
		printf("\n");

		// TODO remove following statements before turning in, used for debugging only
		//printf("you said: %s", commandInput);
		//fflush(stdout);
		struct command* command = parseCommand(commandInput);
		memset(commandInput, '\0', sizeof(commandInput));
		printCommand(command);
		
		/*
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
			if (strcmp(command->args[0], "") == 0)
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

		} */
	}
}

int main()
{
	displayPrompt();
	return 0;
}