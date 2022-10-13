#include <iostream>
#include <queue>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
using namespace std;

void err_sys(const char* x) {
    perror(x);
    exit(1);
}

class CommandSuit {
    public:
        vector<string> words;
        char pipeType;
    private:
};

class PipeSuit {
    public:
        int pipe[2];
        int countdown;
    private:
};

int main() {
    setenv("PATH", "bin:.", 1);
    cout << "% ";
    string commandline;
    vector<CommandSuit*> multiCommand;
    vector<PipeSuit*> multiPipe;
    while (getline(cin, commandline)) {
        // remove whitespace and parse command
        stringstream ss;
        ss << commandline;
        string tempWord;
        CommandSuit *tempCommand = nullptr;
        while (ss >> tempWord) {
            if (tempCommand == nullptr)
                tempCommand = new CommandSuit;

            if (tempWord == ">") {
                tempCommand->pipeType = '>';
                multiCommand.push_back(tempCommand);
                //delete tempCommand;
                tempCommand = nullptr;
            }
            else if (tempWord[0] == '|' || tempWord[0] == '!') {
                tempCommand->pipeType = tempWord[0];
                if (tempWord.length() != 1) {
                    PipeSuit* tempPipe = new PipeSuit;
                    tempPipe->countdown = stoi(tempWord.substr(1));
                    multiPipe.push_back(tempPipe);
                }
                multiCommand.push_back(tempCommand);
                //delete tempCommand;
                tempCommand = nullptr;
            }
            else
                tempCommand->words.push_back(tempWord);
        }
        multiCommand.push_back(tempCommand);

        // handle commandline
        if (multiCommand.size() && multiCommand[0]->words[0] == "exit")
            exit(0);
        else if (multiCommand.size() && multiCommand[0]->words[0] == "setenv") {
            setenv(multiCommand[0]->words[1].c_str(), multiCommand[0]->words[2].c_str(), 1);
            delete multiCommand[0];
            multiCommand.erase(multiCommand.begin());
        }
        else if (multiCommand.size() && multiCommand[0]->words[0] == "printenv") {
            char* outputEnv = getenv(multiCommand[0]->words[1].c_str());
            if (outputEnv != NULL)
                cout << (string)outputEnv << endl;
            delete multiCommand[0];
            multiCommand.erase(multiCommand.begin());
        }
        else {
            // for(int i = 0; i < multiCommand.size(); i++) {
            //     for(int j = 0; j < multiCommand[i]->words.size(); j++)
            //         cout << multiCommand[i]->words[j] << endl;
            // }
            
            string execPath = getenv("PATH");
            // int childpid, pipe1[2], pipe2[2];
            // if (pipe(pipe1) || )
            //     err_sys("can't create pipes");

            // if ((childpid = fork()) < 0) {
            //     err_sys("can't fork");
            // } else if (childpid > 0) {
            //     // parent
            // } else {
            //     // child
            //     execv();
            // }
        }


        cout << "% ";
    }
    return 0;
}