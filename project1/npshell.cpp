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
        bool isUsed = false;
    private:
};

class NumPipeSuit {
    public:
        int pipe[2]; // 0: input, 1: output
        int countdown;
        bool isCountActive = false;
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
    vector<NumPipeSuit*> multiNumPipe;
    signal(SIGCHLD, Sigchld_handler); 
    while (getline(cin, commandline)) {
        // remove whitespace and parse command
        if(commandline == "") {
            cout << "% ";
            continue;
        }
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
                if (tempWord.length() != 1) {
                    NumPipeSuit* tempPipe = new NumPipeSuit;    
                    tempPipe->countdown = stoi(tempWord.substr(1));
                    tempCommand->isNumPipe = true;
                    tempCommand->staticCountdown = tempPipe->countdown;
                    multiNumPipe.push_back(tempPipe);
                }
                else {
                    PipeSuit* tempPipe = new PipeSuit;
                    tempCommand->isNumPipe = false;
                    multiPipe.push_back(tempPipe);
                }
                multiCommand.push_back(tempCommand);
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
                int childpid;
                int currentNumPipeSuite;
                int targetPipe = 0;
                
                // ordianry pipe merge
                if ((*it)->pipeType == '|' && !(*it)->isNumPipe) {
                    if(multiPipe[0]->isUsed)
                        pipe(multiPipe[1]->pipe);
                    else
                        pipe(multiPipe[0]->pipe);
                }

                // num pipe merge
                if (((*it)->pipeType == '|' || (*it)->pipeType == '!') && (*it)->isNumPipe) {
                    for(int i = 0; i < multiNumPipe.size(); i++) {
                        if(!multiNumPipe[i]->isCountActive) {
                            multiNumPipe[i]->isCountActive = true;
                            currentNumPipeSuite = i;
                            break;
                        }
                    }

                    targetPipe = currentNumPipeSuite;
                    for(int i = 0; i < multiNumPipe.size() && multiNumPipe[i]->isCountActive; i++) {
                        if(i != currentNumPipeSuite && multiNumPipe[i]->countdown == multiNumPipe[currentNumPipeSuite]->countdown) {
                            targetPipe = i;
                            break;
                        }
                    }

                    if(targetPipe == currentNumPipeSuite) {
                        pipe(multiNumPipe[targetPipe]->pipe);
                    } else {
                        multiNumPipe.erase(multiNumPipe.begin()+currentNumPipeSuite);
                    } 
                }
                
                if ((childpid = fork()) < 0) {
                    perror("can't fork");
                    exit(1);
                } else if (childpid > 0) {
                    // parent
                    (*it)->pid = childpid;

                    //remove ordinary pipe
                    if(multiPipe.size() > 0 && multiPipe[0]->isUsed) {
                        close(multiPipe[0]->pipe[0]);
                        multiPipe.erase(multiPipe.begin());
                    }
                    if((*it)->pipeType == '|' && !(*it)->isNumPipe) {
                        if(!multiPipe[0]->isUsed) {
                            close(multiPipe[0]->pipe[1]);
                            multiPipe[0]->isUsed = true;
                        }
                    }

                    //remove num pipe
                    for(int i = 0; i < multiNumPipe.size(); i++) {
                        if(multiNumPipe[i]->isCountActive && ((*it)->isNumPipe || it + 1 == multiCommand.end()) && multiNumPipe[i]->countdown == 1){
                            close(multiNumPipe[i]->pipe[1]);
                        }
                        if(multiNumPipe[i]->countdown == 0) {
                            close(multiNumPipe[i]->pipe[0]);
                            multiNumPipe.erase(multiNumPipe.begin() + i);
                            i--;
                        }
                    }
                    for(int i = 0; i < multiNumPipe.size(); i++) {
                        if (multiNumPipe[i]->isCountActive && ((*it)->isNumPipe || it + 1 == multiCommand.end())) {
                            multiNumPipe[i]->countdown--;
                        }
                    }
                    int status;
                    if (it + 1 == multiCommand.end() && !(*it)->isNumPipe)
                        waitpid((*it)->pid, &status, 0);
                    else
                        waitpid(-1, &status,WNOHANG);
                    
                } else {
                    // child
                    // command output to pipe: pipe modification
                    if((*it)->pipeType == '|' || (*it)->pipeType == '!' ) {
                        close(1);
                        if((*it)->pipeType == '!') {
                            close(2);
                            dup(multiNumPipe[targetPipe]->pipe[1]);
                        }
                        if((*it)->isNumPipe) {
                            dup(multiNumPipe[targetPipe]->pipe[1]);
                            close(multiNumPipe[targetPipe]->pipe[1]);
                        }
                        else {
                            if(multiPipe[0]->isUsed) {
                                dup(multiPipe[1]->pipe[1]);
                                close(multiPipe[1]->pipe[1]);
                            }
                            else {
                                dup(multiPipe[0]->pipe[1]);
                                close(multiPipe[0]->pipe[1]);
                            }
                        }
                    }  
                    // command input from pipe: pipe modification
                    if(multiPipe.size() > 0 && multiPipe[0]->isUsed) {
                        close(0);
                        dup(multiPipe[0]->pipe[0]);
                        close(multiPipe[0]->pipe[0]);
                    }
                    for(int i = 0; i < multiNumPipe.size(); i++) {
                        if(multiNumPipe[i]->countdown == 0) {
                            close(0);
                            dup(multiNumPipe[i]->pipe[0]);
                            close(multiNumPipe[i]->pipe[0]);
                            break;
                        }
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
                        cerr << "Unknown command: [" << (*it)->words[0] << "]." << endl;
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