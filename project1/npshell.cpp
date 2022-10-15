#include <iostream>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
using namespace std;

void err_sys(const char* x) {
    perror(x);
    exit(1);
}

class CommandSuit {
    public:
        vector<string> words;
        char pipeType;
        int pid;
        int staticCountdown;
        bool isNumPipe;
        string directFile;
    private:
};

class PipeSuit {
    public:
        int pipe[2]; // 0: input, 1: output
        int countdown;
        // bool isNumPipe;
    private:
};

void Sigchld_handler(int sig) {
    int status;
    while(waitpid(-1, &status,WNOHANG) > 0);
}

int main() {
    setenv("PATH", "bin:.", 1);
    cout << "% ";
    string commandline;
    vector<CommandSuit*> multiCommand;
    vector<PipeSuit*> multiPipe;
    if (SIGCHLD, Sigchld_handler); 
    while (getline(cin, commandline)) {
        // remove whitespace and parse command
        stringstream ss;
        ss << commandline;
        string tempWord;
        CommandSuit *tempCommand = nullptr;
        bool isLast = false; // check if '>' is the last command
        while (ss >> tempWord) {
            isLast = false;
            if (tempCommand == nullptr)
                tempCommand = new CommandSuit;

            if (tempWord == ">") {
                tempCommand->pipeType = '>';
                ss >> tempWord;
                tempCommand->directFile = tempWord;
                multiCommand.push_back(tempCommand);
                tempCommand = nullptr;
                isLast = true;
            }
            else if (tempWord[0] == '|' || tempWord[0] == '!') {
                tempCommand->pipeType = tempWord[0];
                PipeSuit* tempPipe = new PipeSuit;
                if (tempWord.length() != 1) {
                    tempPipe->countdown = stoi(tempWord.substr(1));
                    tempCommand->isNumPipe = true;
                }
                else {
                    tempPipe->countdown = 1;
                    tempCommand->isNumPipe = false;
                }
                tempCommand->staticCountdown = tempPipe->countdown;
                multiPipe.push_back(tempPipe);
                multiCommand.push_back(tempCommand);
                //delete tempCommand;
                tempCommand = nullptr;
            }
            else
                tempCommand->words.push_back(tempWord);
        }
        if (!isLast && tempWord[0] != '|' && tempWord[0] != '!')
            multiCommand.push_back(tempCommand);

        // handle commandline
        if (multiCommand.size() && multiCommand[0]->words[0] == "exit")
            exit(0);
        else if (multiCommand.size() && multiCommand[0]->words[0] == "setenv") {
            setenv(multiCommand[0]->words[1].c_str(), multiCommand[0]->words[2].c_str(), 1);
            delete multiCommand[0];
            multiCommand.clear();
        }
        else if (multiCommand.size() && multiCommand[0]->words[0] == "printenv") {
            char* outputEnv = getenv(multiCommand[0]->words[1].c_str());
            if (outputEnv != NULL)
                cout << (string)outputEnv << endl;
            delete multiCommand[0];
            multiCommand.clear();
        }
        else {
            string execPath = getenv("PATH");
            for (vector<CommandSuit*>:: iterator it = multiCommand.begin(); it != multiCommand.end(); it++) {
                int childpid, pipe1[2];
                if (pipe(pipe1))
                    err_sys("can't create pipes");

                if ((childpid = fork()) < 0) {
                    err_sys("can't fork");
                } else if (childpid > 0) {
                    // parent
                    (*it)->pid = childpid;
                    int status;
                    if (it + 1 == multiCommand.end())
                        waitpid((*it)->pid, &status, 0);
                    else
                        waitpid((*it)->pid, &status, WNOHANG);
                } else {
                    // child
                    // command output to pipe
                    if ((*it)->pipeType == '|') {
                        close(1);
                        bool isSamePipe = false;
                        for(int i = 0; i < multiPipe.size(); i++) {
                            if(multiPipe[i]->countdown == (*it)->staticCountdown) {
                                dup(multiPipe[i]->pipe[1]);
                                close(multiPipe[i]->pipe[1]);
                                isSamePipe = true;
                                break;
                            }
                        }
                        if(!isSamePipe) {
                            dup(pipe1[1]);
                            close(pipe1[1]);
                        }
                    }
                    // command input to pipe
                    for(int i = 0; i < multiPipe.size(); i++) {
                        if(multiPipe[i]->countdown == 1) {
                            close(0);
                            dup(multiPipe[i]->pipe[0]);
                            close(multiPipe[i]->pipe[0]);
                        }
                        if ((*it)->isNumPipe || it + 1 == multiCommand.end())
                            multiPipe[i]->countdown--;
                    }

                    // command redirection
                    if((*it)->pipeType == '>') {
                        int fileFd = open((*it)->directFile.c_str(), O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC, S_IRUSR|S_IWUSR);
                        close(1);
                        dup(fileFd);
                        close(fileFd);
                    }

                    // execute command
                    char **argv = new char*[128]();
                    for(int i = 0; i < (*it)->words.size()+1; i++) {
                        *(argv+i) = new char[256]();
                        if(i == (*it)->words.size())
                            *(argv+i) = NULL;
                        else
                            strcpy(*(argv+i), (*it)->words[i].c_str());
                    }
                    if(execvp((*it)->words[0].c_str(), argv) == -1) {
                        cerr << "Unknown command: [" << (*it)->words[0] << "].\n";
                        exit(1);
                    }
                    
                    exit(0);
                }
            }
            multiCommand.clear();
        }
        cout << "% ";
    }
    return 0;
}