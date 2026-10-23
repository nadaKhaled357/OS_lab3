#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
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

int logFile = -1;

void writeLog(int pid)
{
    char line[100];
    snprintf(line, sizeof(line), "Child process %d terminated\n", pid);
    write(logFile, line, strlen(line));
}

void childFinished(int sig)
{
    int pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        writeLog(pid);
    }
}

std::string getWord(std::vector<Token> &tokens, int &i)
{
    std::string word = tokens[i].value;

    if (word[0] == '"')
    {
        while (tokens[i + 1].type != TOKEN_EOF && !(word.size() > 1 && word[word.size() - 1] == '"'))
        {
            i++;
            word = word + " " + tokens[i].value;
        }
    }

    std::string result = "";
    for (int k = 0; k < word.size(); k++)
    {
        if (word[k] != '"')
        {
            result = result + word[k];
        }
    }
    return result;
}

bool isWord(Token &token)
{
    return token.type == TOKEN_COMMAND || token.type == TOKEN_ARGUMENT;
}

void parseError(const char *message, SimpleCommand *current)
{
    printf("myshell: %s\n", message);
    if (current != NULL)
    {
        Command::_currentCommand.insertSimpleCommand(current);
    }
    Command::_currentCommand.clear();
}

void parse(std::vector<Token> &tokens)
{
    Command &command = Command::_currentCommand;
    SimpleCommand *current = NULL;
    bool afterPipe = false;

    for (int i = 0; tokens[i].type != TOKEN_EOF; i++)
    {
        TokenType type = tokens[i].type;

        if (type == TOKEN_PIPE)
        {
            if (current == NULL)
            {
                parseError("syntax error: missing command before |", current);
                return;
            }
            command.insertSimpleCommand(current);
            current = NULL;
            afterPipe = true;
        }
        else if (type == TOKEN_BACKGROUND)
        {
            if (tokens[i + 1].type != TOKEN_EOF)
            {
                parseError("syntax error: & must be at the end of the command", current);
                return;
            }
            command._background = 1;
        }
        else if (type == TOKEN_REDIRECT || type == TOKEN_APPEND || type == TOKEN_INPUT ||
                 type == TOKEN_ERROR || type == TOKEN_REDIRECT_AND_ERROR)
        {
            if (!isWord(tokens[i + 1]))
            {
                std::string message = "syntax error: missing file name after " + tokens[i].value;
                parseError(message.c_str(), current);
                return;
            }
            i++;
            std::string name = getWord(tokens, i);

            if (type == TOKEN_INPUT)
            {
                if (command._inputFile)
                    free(command._inputFile);
                command._inputFile = strdup(name.c_str());
            }
            else if (type == TOKEN_ERROR)
            {
                if (command._errFile)
                    free(command._errFile);
                command._errFile = strdup(name.c_str());
            }
            else
            {
                if (command._outFile)
                    free(command._outFile);
                command._outFile = strdup(name.c_str());
                command._append = 0;

                if (type == TOKEN_APPEND || type == TOKEN_REDIRECT_AND_ERROR)
                {
                    command._append = 1;
                }
                if (type == TOKEN_REDIRECT_AND_ERROR)
                {
                    command._out_error = 1;
                    if (command._errFile)
                        free(command._errFile);
                    command._errFile = strdup(name.c_str());
                }
            }
        }
        else
        {
            std::string word = getWord(tokens, i);
            if (current == NULL)
            {
                current = new SimpleCommand();
            }
            current->insertArgument(strdup(word.c_str()));
            afterPipe = false;
        }
    }

    if (afterPipe)
    {
        parseError("syntax error: missing command after |", current);
        return;
    }

    if (current != NULL)
    {
        command.insertSimpleCommand(current);
    }

    if (command._numberOfSimpleCommands == 0)
    {
        if (command._outFile || command._inputFile || command._errFile || command._background)
        {
            parseError("syntax error: missing command", NULL);
        }
        return;
    }

    command.execute();
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
        delete _simpleCommands[i];
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
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
           _out_error == 1 ? _errFile : "default", _background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    print();

    char *first = _simpleCommands[0]->_arguments[0];

    if (strcmp(first, "exit") == 0)
    {
        printf("Good bye!!\n");
        clear();
        exit(0);
    }

    if (strcmp(first, "cd") == 0)
    {
        char *dir;
        if (_simpleCommands[0]->_numberOfArguments > 1)
        {
            dir = _simpleCommands[0]->_arguments[1];
        }
        else
        {
            dir = getenv("HOME");
        }

        if (chdir(dir) != 0)
        {
            printf("myshell: cd: %s: %s\n", dir, strerror(errno));
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
            printf("myshell: %s: %s\n", _inputFile, strerror(errno));
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

        if (outFd < 0)
        {
            printf("myshell: %s: %s\n", _outFile, strerror(errno));
            if (inFd >= 0)
                close(inFd);
            clear();
            return;
        }
    }

    if (_out_error == 1)
    {
        errFd = outFd;
    }
    else if (_errFile)
    {
        errFd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (errFd < 0)
        {
            printf("myshell: %s: %s\n", _errFile, strerror(errno));
            if (inFd >= 0)
                close(inFd);
            if (outFd >= 0)
                close(outFd);
            clear();
            return;
        }
    }

    int previousRead = -1;
    int lastPid = -1;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        int fds[2];
        fds[0] = -1;
        fds[1] = -1;

        if (i < _numberOfSimpleCommands - 1)
        {
            if (pipe(fds) < 0)
            {
                printf("myshell: pipe: %s\n", strerror(errno));
                break;
            }
        }

        int pid = fork();

        if (pid < 0)
        {
            printf("myshell: fork: %s\n", strerror(errno));
            break;
        }

        if (pid == 0)
        {
            signal(SIGINT, SIG_DFL);

            if (i == 0 && inFd >= 0)
                dup2(inFd, 0);
            if (i > 0)
                dup2(previousRead, 0);

            if (i < _numberOfSimpleCommands - 1)
                dup2(fds[1], 1);
            else if (outFd >= 0)
                dup2(outFd, 1);

            if (i == _numberOfSimpleCommands - 1 && errFd >= 0)
                dup2(errFd, 2);

            if (inFd >= 0)
                close(inFd);
            if (outFd >= 0)
                close(outFd);
            if (errFd >= 0 && errFd != outFd)
                close(errFd);
            if (previousRead >= 0)
                close(previousRead);
            if (fds[0] >= 0)
                close(fds[0]);
            if (fds[1] >= 0)
                close(fds[1]);

            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
            fprintf(stderr, "myshell: %s: %s\n", _simpleCommands[i]->_arguments[0],
                    errno == ENOENT ? "command not found" : strerror(errno));
            _exit(1);
        }

        lastPid = pid;

        if (previousRead >= 0)
            close(previousRead);
        if (fds[1] >= 0)
            close(fds[1]);
        previousRead = fds[0];
    }

    if (previousRead >= 0)
        close(previousRead);
    if (inFd >= 0)
        close(inFd);
    if (outFd >= 0)
        close(outFd);
    if (errFd >= 0 && errFd != outFd)
        close(errFd);

    if (!_background && lastPid > 0)
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
    logFile = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);

    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);

        if (!std::getline(std::cin, input))
        {
            printf("\n");
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
