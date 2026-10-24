#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

FILE *logFile;

void writeLog(int pid)
{
    fprintf(logFile, "Child process %d terminated\n", pid);
    fflush(logFile);
}

void childFinished(int sig)
{
    int pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        writeLog(pid);
    }
}

void parse(std::vector<Token> &tokens)
{
    Command currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    for (int i = 0; tokens[i].type != TOKEN_EOF; i++)
    {
        TokenType type = tokens[i].type;
        std::string word = tokens[i].value;

        if (type == TOKEN_PIPE)
        {
            if (currentSimpleCommand->_numberOfArguments == 0)
            {
                printf("myshell: syntax error near |\n");
                currentCommand.clear();
                return;
            }
            currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
            if (tokens[i + 1].type == TOKEN_EOF)
            {
                printf("myshell: missing command after |\n");
                currentCommand.clear();
                return;
            }
        }
        else if (type == TOKEN_BACKGROUND)
        {
            currentCommand._background = 1;
        }
        else if (type == TOKEN_REDIRECT || type == TOKEN_APPEND || type == TOKEN_INPUT ||
                 type == TOKEN_ERROR || type == TOKEN_REDIRECT_AND_ERROR)
        {
            i++;
            if (tokens[i].type != TOKEN_COMMAND && tokens[i].type != TOKEN_ARGUMENT)
            {
                printf("myshell: missing file name after %s\n", word.c_str());
                currentCommand.clear();
                return;
            }
            char *file = strdup(tokens[i].value.c_str());

            if (type == TOKEN_INPUT)
            {
                currentCommand._inputFile = file;
            }
            else if (type == TOKEN_ERROR)
            {
                currentCommand._errFile = file;
            }
            else
            {
                currentCommand._outFile = file;
                if (type == TOKEN_APPEND || type == TOKEN_REDIRECT_AND_ERROR)
                {
                    currentCommand._append = 1;
                }
                if (type == TOKEN_REDIRECT_AND_ERROR)
                {
                    currentCommand._out_error = 1;
                    currentCommand._errFile = strdup(file);
                }
            }
        }
        else
        {
            if (word[0] == '"')
            {
                while (tokens[i + 1].type != TOKEN_EOF && word[word.size() - 1] != '"')
                {
                    i++;
                    word = word + " " + tokens[i].value;
                }
            }
            std::string clean = "";
            for (int k = 0; k < word.size(); k++)
            {
                if (word[k] != '"')
                {
                    clean = clean + word[k];
                }
            }
            currentSimpleCommand->insertArgument(strdup(clean.c_str()));
        }
    }

    if (currentSimpleCommand->_numberOfArguments > 0)
    {
        currentCommand.insertSimpleCommand(currentSimpleCommand);
    }

    if (currentCommand._numberOfSimpleCommands > 0)
    {
        currentCommand.execute();
    }
    else
    {
        if (currentCommand._outFile || currentCommand._inputFile || currentCommand._errFile)
        {
            printf("myshell: missing command\n");
        }
        currentCommand.clear();
    }
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _background = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    print();

    char *first = _simpleCommands[0]->_arguments[0];

    if (strcmp(first, "exit") == 0)
    {
        printf("Good bye!!\n");
        exit(0);
    }

    if (strcmp(first, "cd") == 0)
    {
        char *dir = getenv("HOME");
        if (_simpleCommands[0]->_numberOfArguments > 1)
        {
            dir = _simpleCommands[0]->_arguments[1];
        }
        if (chdir(dir) != 0)
        {
            printf("myshell: cd: %s: No such file or directory\n", dir);
        }
        clear();
        return;
    }

    int inFd = -1;
    int outFd = -1;
    int errFd = -1;

    if (_inputFile)
    {
        inFd = open(_inputFile, O_RDONLY);
        if (inFd < 0)
        {
            printf("myshell: %s: No such file or directory\n", _inputFile);
            clear();
            return;
        }
    }
    if (_outFile)
    {
        if (_append)
            outFd = open(_outFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            outFd = open(_outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if (_out_error == 1)
    {
        errFd = outFd;
    }
    else if (_errFile)
    {
        errFd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    int n = _numberOfSimpleCommands;
    int previous = -1;
    int lastPid = 0;

    for (int i = 0; i < n; i++)
    {
        int fds[2] = {-1, -1};
        if (i < n - 1)
        {
            pipe(fds);
        }

        int pid = fork();
        if (pid == 0)
        {
            signal(SIGINT, SIG_DFL);

            if (i == 0 && inFd >= 0)
                dup2(inFd, 0);
            if (i > 0)
                dup2(previous, 0);
            if (i < n - 1)
                dup2(fds[1], 1);
            else if (outFd >= 0)
                dup2(outFd, 1);
            if (i == n - 1 && errFd >= 0)
                dup2(errFd, 2);

            if (inFd >= 0)
                close(inFd);
            if (outFd >= 0)
                close(outFd);
            if (errFd >= 0 && errFd != outFd)
                close(errFd);
            if (previous >= 0)
                close(previous);
            if (fds[0] >= 0)
                close(fds[0]);
            if (fds[1] >= 0)
                close(fds[1]);

            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            printf("myshell: %s: command not found\n", _simpleCommands[i]->_arguments[0]);
            exit(1);
        }

        lastPid = pid;
        if (previous >= 0)
            close(previous);
        if (fds[1] >= 0)
            close(fds[1]);
        previous = fds[0];
    }

    if (inFd >= 0)
        close(inFd);
    if (outFd >= 0)
        close(outFd);
    if (errFd >= 0 && errFd != outFd)
        close(errFd);

    if (_background == 0)
    {
        int result = waitpid(lastPid, NULL, 0);
        if (result > 0)
        {
            writeLog(result);
        }
    }

    clear();
}

void Command::prompt()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, childFinished);
    logFile = fopen("log.txt", "a");

    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);
        if (!std::getline(std::cin, input))
        {
            break;
        }
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    Command::_currentCommand.prompt();
    return 0;
}
