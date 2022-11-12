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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map> 
using namespace std;

#define QLEN 30
#define BUFSIZE 4096

class UserPipe {
    public:
        int pipe[2];
        int src;
        int dst;
};

class WhoInfo {
    public:
        bool isExist;
        int fd;
        string nickname;
        string address;
        map<string, string> env;
    private:
};

class CommandSuit {
    public:
        vector<string> words;
        char pipeType;
        int pid;
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

int msock;
fd_set rfds, afds; // read file descriptor set, active file descriptor set
WhoInfo usersInfo[QLEN];

void Sigchld_handler(int sig) {
    int status;
    while(waitpid(-1, &status,WNOHANG) > 0);
}

void broadcast(string msg) {
    int nfds = getdtablesize();

    for(int fd = 0; fd < 30; fd++) {
        if(fd != msock && FD_ISSET(fd, &afds)) {
            write(fd, msg.c_str(), msg.length());
        }
    }
}

int npshell(int srcIndex) {
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
            return -1;
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
        else if(multiCommand.size() && multiCommand[0]->words[0] == "who") {
            cout << "<ID>\n<nickname>\t<IP:port>\t<indicate me>\n";
            for(int index = 0; index < 30; index++)
                if(usersInfo[index].isExist) {
                    cout << index + 1 << "\n" << usersInfo[index].nickname << "\t" << usersInfo[index].address;
                    if(srcIndex == index)
                        cout << "\t<-me";
                    cout << endl;
                }
        }
        else if(multiCommand.size() && multiCommand[0]->words[0] == "name") {
            string newName = multiCommand[0]->words[1];
            bool isSame = false;
            for(int index = 0; index < 30; index++) {
                if(srcIndex != index && usersInfo[index].isExist && newName == usersInfo[index].nickname) {
                    cout << "*** User '" << newName << "' already exists. ***\n";
                    isSame = true;
                    break;
                }
            }
            if(!isSame) {
                usersInfo[srcIndex].nickname = newName;
                string temp = "*** User from " + usersInfo[srcIndex].address + "is named '" +  usersInfo[srcIndex].nickname + "'. ***\n";
                broadcast(temp);
            }
        }
        else if(multiCommand.size() && multiCommand[0]->words[0] == "tell") {
            int dstIndex = stoi(multiCommand[0]->words[1]) - 1;
            string msg = "";
            for(int i = 1; i < multiCommand[0]->words.size(); i++) {
                msg += multiCommand[0]->words[i];
                if(i + 1 != multiCommand[0]->words.size())
                    msg += " ";
            } 
            if(usersInfo[srcIndex].isExist)
                cout << "*** " << usersInfo[srcIndex].nickname << " told you ***: " << msg << "\n";
            else
                cout << "*** Error: user #" << srcIndex + 1 << " does not exist yet. ***\n";
        }
        else if(multiCommand.size() && multiCommand[0]->words[0] == "yell") {
            string msg = "";
            for(int i = 1; i < multiCommand[0]->words.size(); i++) {
                msg += multiCommand[0]->words[i];
                if(i + 1 != multiCommand[0]->words.size())
                    msg += " ";
            }
            string temp = "*** " + usersInfo[srcIndex].nickname + " yelled ***: " + msg + "\n";
            broadcast(temp);
        }
        else {
            string execPath = getenv("PATH");
            for (vector<CommandSuit*>:: iterator it = multiCommand.begin(); it != multiCommand.end(); it++) {
                int shellChildpid;
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
                shellChildpid = fork();
                while (shellChildpid < 0) {
                    usleep(1000);
                    shellChildpid = fork();
                }
                if (shellChildpid > 0) {
                    // parent
                    (*it)->pid = shellChildpid;

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
                    
                    setenv("PATH", "bin:.", 1);
                    if(execvp((*it)->words[0].c_str(), argv) == -1) {
                        cerr << "Unknown command: [" << (*it)->words[0] << "]." << endl;
                        exit(1);
                    }
                    return 0;
                }
            }
            multiCommand.clear();
        }
        usleep(10000);
        cout << "% ";
    }
    return 0;
}


int passiveTCP(int port) {
    int sockfd, newsockfd, cli_len, childpid;
    struct sockaddr_in srv_addr, cli_addr;

    // Open a TCP socket
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket");
        exit(1);
    }

    // Build our local address so that the client can send to us
    bzero((char*) &srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port = htons(port);
    
    int flag = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    if(bind(sockfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
        perror("server: can't bind local address");
        exit(1);
    }

    if(listen(sockfd, QLEN) < 0) {
        perror("listen");
        exit(1);
    }

    return sockfd;
}

void initInfo(int index) {
    usersInfo[index].isExist = false;
    usersInfo[index].fd = -1;
    usersInfo[index].nickname = "(no name)";
    usersInfo[index].address = "";
    usersInfo[index].env.clear();
}

int main(int argc, char *argv[]) {
    struct sockaddr_in fsin;
    int alen;
    int fd, nfds;
    msock = passiveTCP(atoi(argv[1]));

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    for(int index = 0; index < 30; index++)
        initInfo(index);

    while(1) {
        memcpy(&rfds, &afds, sizeof(rfds));
        if(select(nfds, &rfds, (fd_set*) 0, (fd_set*) 0, (struct timeval*) 0) < 0) {
            perror("select");
            exit(1);
        }

        if(FD_ISSET(msock, &rfds)) {
            int ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr*) &fsin, (socklen_t*) &alen);
            if(ssock < 0) {
                perror("server: accept error");
                exit(1);
            }
            FD_SET(ssock, &afds);
            cout << "****************************************\n** Welcome to the information server. **\n****************************************\n";

            int index;
            for(index = 0; index < QLEN; index++) {
                if(!usersInfo[index].isExist)
                    break;
            }
            usersInfo[index].isExist = true;
            usersInfo[index].fd = ssock;
            usersInfo[index].nickname = "(no name)";
            string ip = inet_ntoa(fsin.sin_addr);
            usersInfo[index].address = ip + ":" + to_string(ntohs(fsin.sin_port));
            usersInfo[index].env["PATH"] = "bin:.";

            string temp = "*** User '" + usersInfo[index].nickname + "' entered from " + usersInfo[index].address + ". ***\n";
            broadcast(temp);
        }

        for(int index = 0; index < 30; index++) {
            npshell(index);
        }
        // for(fd = 0; fd < nfds; fd++) {
        //     if(fd != msock && FD_ISSET(fd, &rfds))
        //         if(npshell(fd) == -1) { //exit
        //             for(int i = 0; i < usersInfo.size(); i++) {
        //                 if(usersInfo[i].fd == fd) {
        //                     string temp = "*** User '" + usersInfo[i].nickname + "' left. ***\n";
        //                     broadcast(temp);
        //                     usersInfo.erase(usersInfo.begin() + i);
        //                 }
        //             }
        //             close(fd);
        //             FD_CLR(fd, &afds);
        //         }
        // }
    }
    return 0;
}