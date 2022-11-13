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

class UserPipeSuit {
    public:
        int pipe[2]; // 0: input, 1: output
        int senderId;
        int recverId;
    private:
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
        char pipeType; //#: >?, %: <?
        int pid;
        bool isNumPipe;
        string directFile;
        int senderId = -1;
        int recverId = -1;
        bool isSendNull = false;
        bool isRecvNull = false;
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
    for(int fd = 0; fd < 30; fd++) {
        if(fd != msock && FD_ISSET(fd, &afds)) {
            write(fd, msg.c_str(), msg.length());
        }
    }
}

vector<UserPipeSuit*> userPipe;
vector<NumPipeSuit*>* multiNumPipeForUser[30];

void initInfo(int index) {
    usersInfo[index].isExist = false;
    usersInfo[index].fd = -1;
    usersInfo[index].nickname = "(no name)";
    usersInfo[index].address = "";
    usersInfo[index].env.clear();

    delete multiNumPipeForUser[index];
    multiNumPipeForUser[index] = new vector<NumPipeSuit*>;
}


void npshell(int srcIndex) {
    // dup2(usersInfo[srcIndex].fd, 0);
    dup2(usersInfo[srcIndex].fd, 1);
    dup2(usersInfo[srcIndex].fd, 2);
    vector<CommandSuit*> multiCommand;
    vector<PipeSuit*> multiPipe;
    vector<NumPipeSuit*>* multiNumPipe = multiNumPipeForUser[srcIndex];
    
    signal(SIGCHLD, Sigchld_handler);
    clearenv();
    for(map<string, string>::iterator it = usersInfo[srcIndex].env.begin(); it != usersInfo[srcIndex].env.end(); it++) {
        setenv(it->first.c_str(), it->second.c_str(), 1);
    }

    char input[15000];
    memset(input, 0, sizeof(input));
    fflush(0);
    recv(usersInfo[srcIndex].fd, input, sizeof(input), MSG_DONTWAIT);
    string commandline(input);
    // remove whitespace and parse command
    
    if(commandline[commandline.length() - 1] == '\n')
        commandline.erase(commandline.length()-1);
    if(commandline[commandline.length() - 1] == '\r')
        commandline.erase(commandline.length()-1);
    if(commandline == "")
        return;

    stringstream ss;
    ss << commandline;
    string tempWord;
    CommandSuit *tempCommand = nullptr;
    bool isLast = false; // check if '>' is the last command
    bool doubleUserPipe = false;
    bool isUserPipe = false;
    while (ss >> tempWord) {
        isLast = false;
        if (tempCommand == nullptr)
            tempCommand = new CommandSuit;
        
        if (tempWord == ">") {
            isUserPipe = false;
            tempCommand->pipeType = '>';
            ss >> tempWord;
            tempCommand->directFile = tempWord;
            multiCommand.push_back(tempCommand);
            tempCommand = nullptr;
            isLast = true;
        }
        else if (tempWord[0] == '>' || tempWord[0] == '<') {
            bool isError = false;
            string userID = tempWord.substr(1);
            if(stoi(userID) > 30 || !usersInfo[stoi(userID) - 1].isExist) {
                string temp = "*** Error: user #" + userID + " does not exist yet. ***\n";
                write(usersInfo[srcIndex].fd, temp.c_str(), temp.length());
                isError = true;
            }
            if(tempWord[0] == '>') {
                bool isExist = false;
                for(int i = 0; i < userPipe.size(); i++) {
                    if(userPipe[i]->recverId == stoi(userID) && userPipe[i]->senderId == srcIndex + 1)
                        isExist = true;
                }
                if(isExist) {
                    string temp = "*** Error: the pipe #" + to_string(srcIndex + 1) + "->#" + userID + " already exists. ***\n";
                    write(usersInfo[srcIndex].fd, temp.c_str(), temp.length());
                    isError = true;
                }
                tempCommand->pipeType = '#';
                tempCommand->senderId = srcIndex + 1;
                tempCommand->recverId = stoi(userID);
                UserPipeSuit* tempPipe = new UserPipeSuit;
                tempPipe->senderId = srcIndex + 1;
                tempPipe->recverId = stoi(userID);
                if(isError)
                    tempCommand->isSendNull = true;
                userPipe.push_back(tempPipe);
                pipe(userPipe[userPipe.size()-1]->pipe);
                if(!isError) {
                    string temp = "*** " + usersInfo[srcIndex].nickname + " (#" + to_string(srcIndex + 1) + ") just piped '" + commandline 
                        + "' to " + usersInfo[stoi(userID) - 1].nickname + " (#" + userID + ") ***\n";
                    broadcast(temp);
                }
                isLast = true;  
            }
            else {
                bool isExist = false;
                for(int i = 0; i < userPipe.size(); i++) {
                    if(userPipe[i]->recverId == srcIndex + 1 && userPipe[i]->senderId == stoi(userID))
                        isExist = true;
                }
                if(!isExist) {
                    string temp = "*** Error: the pipe #" + userID + "->#" + to_string(srcIndex + 1) + " does not exist yet. ***\n";
                    write(usersInfo[srcIndex].fd, temp.c_str(), temp.length());
                    isError = true;
                    UserPipeSuit* tempPipe = new UserPipeSuit;
                    tempPipe->senderId = stoi(userID);
                    tempPipe->recverId = srcIndex + 1;
                    if(isError)
                        tempCommand->isRecvNull = true;
                    pipe(tempPipe->pipe);
                    userPipe.push_back(tempPipe);
                }
                tempCommand->pipeType = '$';
                tempCommand->senderId = stoi(userID);
                tempCommand->recverId = srcIndex + 1;
                if(!isError) {
                    string temp = "*** " + usersInfo[srcIndex].nickname + " (#" + to_string(srcIndex + 1) + ") just received from " 
                        + usersInfo[stoi(userID) - 1].nickname + " (#" + userID + ") by '" + commandline + "' ***\n";
                    broadcast(temp);
                }
                isLast = true;
            }
            if(isUserPipe) {
                tempCommand->pipeType = '@';
                isUserPipe = false;
            }
            multiCommand.push_back(tempCommand);
            tempCommand = nullptr;
            isUserPipe = true;
        }
        else if (tempWord[0] == '|' || tempWord[0] == '!') {
            isUserPipe = false;
            tempCommand->pipeType = tempWord[0];
            if (tempWord.length() != 1) {
                NumPipeSuit* tempPipe = new NumPipeSuit;    
                tempPipe->countdown = stoi(tempWord.substr(1));
                tempCommand->isNumPipe = true;
                (*multiNumPipe).push_back(tempPipe);
            }
            else {
                PipeSuit* tempPipe = new PipeSuit;
                tempCommand->isNumPipe = false;
                multiPipe.push_back(tempPipe);
            }
            multiCommand.push_back(tempCommand);
            tempCommand = nullptr;
        }
        else {
            isUserPipe = false;
            tempCommand->words.push_back(tempWord);
        }
    }
    if (!isLast && tempWord[0] != '|' && tempWord[0] != '!')
        multiCommand.push_back(tempCommand);

    // handle commandline
    if (multiCommand.size() && multiCommand[0]->words[0] == "exit") {
        string temp = "*** User '" + usersInfo[srcIndex].nickname + "' left. ***\n";
        close(usersInfo[srcIndex].fd);
        close(1);
        close(2);
        dup2(0, 1);
        dup2(0, 2);
        broadcast(temp);
        FD_CLR(usersInfo[srcIndex].fd, &afds);
        initInfo(srcIndex);
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0){}
    }
    else if (multiCommand.size() && multiCommand[0]->words[0] == "setenv") {
        usersInfo[srcIndex].env[multiCommand[0]->words[1]] = multiCommand[0]->words[2];
        setenv(multiCommand[0]->words[1].c_str(), multiCommand[0]->words[2].c_str(), 1);
        delete multiCommand[0];
        multiCommand.clear();
    }
    else if (multiCommand.size() && multiCommand[0]->words[0] == "printenv") {
        char* outputEnv = getenv(multiCommand[0]->words[1].c_str());
        if (outputEnv != NULL) {
            cout << (string)outputEnv << endl;
            fflush(stdin);
        }
        delete multiCommand[0];
        multiCommand.clear();
    }
    else if(multiCommand.size() && multiCommand[0]->words[0] == "who") {
        cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        for(int index = 0; index < 30; index++)
            if(usersInfo[index].isExist) {
                cout << index + 1 << "\t" << usersInfo[index].nickname << "\t" << usersInfo[index].address;
                if(srcIndex == index)
                    cout << "\t<-me";
                cout << endl;
            }
        fflush(0);
    }
    else if(multiCommand.size() && multiCommand[0]->words[0] == "name") {
        string newName = multiCommand[0]->words[1];
        bool isSame = false;
        for(int index = 0; index < 30; index++) {
            if(srcIndex != index && usersInfo[index].isExist && newName == usersInfo[index].nickname) {
                cout << "*** User '" << newName << "' already exists. ***\n";
                fflush(0);
                isSame = true;
                break;
            }
        }
        if(!isSame) {
            usersInfo[srcIndex].nickname = newName;
            string temp = "*** User from " + usersInfo[srcIndex].address + " is named '" +  usersInfo[srcIndex].nickname + "'. ***\n";
            broadcast(temp);
        }
    }
    else if(multiCommand.size() && multiCommand[0]->words[0] == "tell") {
        int dstIndex = stoi(multiCommand[0]->words[1]) - 1;
        string msg = commandline.substr(commandline.find(" ", 5) + 1);
        if(usersInfo[dstIndex].isExist) {
            string temp =  "*** " + usersInfo[srcIndex].nickname + " told you ***: " + msg + "\n";
            write(usersInfo[dstIndex].fd, temp.c_str(), temp.length());
        }
        else
            cout << "*** Error: user #" << dstIndex + 1 << " does not exist yet. ***\n";
            fflush(0);
    }
    else if(multiCommand.size() && multiCommand[0]->words[0] == "yell") {
        string msg = commandline.substr(commandline.find(" ") + 1);
        string temp = "*** " + usersInfo[srcIndex].nickname + " yelled ***: " + msg + "\n";
        broadcast(temp);
    }
    else {
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
                for(int i = 0; i < (*multiNumPipe).size(); i++) {
                    if(!(*multiNumPipe)[i]->isCountActive) {
                        (*multiNumPipe)[i]->isCountActive = true;
                        currentNumPipeSuite = i;
                        break;
                    }
                }

                targetPipe = currentNumPipeSuite;
                for(int i = 0; i < (*multiNumPipe).size() && (*multiNumPipe)[i]->isCountActive; i++) {
                    if(i != currentNumPipeSuite && (*multiNumPipe)[i]->countdown == (*multiNumPipe)[currentNumPipeSuite]->countdown) {
                        targetPipe = i;
                        break;
                    }
                }

                if(targetPipe == currentNumPipeSuite) {
                    pipe((*multiNumPipe)[targetPipe]->pipe);
                } else {
                    (*multiNumPipe).erase((*multiNumPipe).begin()+currentNumPipeSuite);
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
                for(int i = 0; i < (*multiNumPipe).size(); i++) {
                    if((*multiNumPipe)[i]->isCountActive && ((*it)->isNumPipe || it + 1 == multiCommand.end()) && (*multiNumPipe)[i]->countdown == 1){
                        close((*multiNumPipe)[i]->pipe[1]);
                    }
                    if((*multiNumPipe)[i]->countdown == 0) {
                        close((*multiNumPipe)[i]->pipe[0]);
                        (*multiNumPipe).erase((*multiNumPipe).begin() + i);
                        i--;
                    }
                }
                for(int i = 0; i < (*multiNumPipe).size(); i++) {
                    if ((*multiNumPipe)[i]->isCountActive && ((*it)->isNumPipe || it + 1 == multiCommand.end())) {
                        (*multiNumPipe)[i]->countdown--;
                    }
                }

                // user pipe
                if((*it)->pipeType == '#' || (*it)->pipeType == '@') {
                    int i = 0;
                    for(; i < userPipe.size(); i++) {
                        if(userPipe[i]->senderId == (*it)->senderId && userPipe[i]->recverId == (*it)->recverId)
                            break;
                    }
                    close(userPipe[i]->pipe[1]);
                }
                if((*it)->pipeType == '$' || (*it)->pipeType == '@') {
                    int i = 0;
                    for(; i < userPipe.size(); i++) {
                        if(userPipe[i]->senderId == (*it)->senderId && userPipe[i]->recverId == (*it)->recverId)
                            break;
                    }
                    close(userPipe[i]->pipe[0]);
                    userPipe.erase(userPipe.begin() + i);
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
                        dup((*multiNumPipe)[targetPipe]->pipe[1]);
                    }
                    if((*it)->isNumPipe) {
                        dup((*multiNumPipe)[targetPipe]->pipe[1]);
                        close((*multiNumPipe)[targetPipe]->pipe[1]);
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
                for(int i = 0; i < (*multiNumPipe).size(); i++) {
                    if((*multiNumPipe)[i]->countdown == 0) {
                        close(0);
                        dup((*multiNumPipe)[i]->pipe[0]);
                        close((*multiNumPipe)[i]->pipe[0]);
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

                // user pipe
                if((*it)->pipeType == '#' || (*it)->pipeType == '@') {
                    int i = 0;
                    for(; i < userPipe.size(); i++) {
                        if(userPipe[i]->senderId == (*it)->senderId && userPipe[i]->recverId == (*it)->recverId)
                            break;
                    }
                    int devNull = open("/dev/null", O_RDWR);
                    if((*it)->isSendNull) {
                        dup2(devNull, 1);
                        close(devNull);
                    }
                    else {
                        dup2(userPipe[i]->pipe[1], 1);
                        close(userPipe[i]->pipe[1]);
                    }
                }
                if((*it)->pipeType == '$' || (*it)->pipeType == '@') {
                    int i = 0;
                    for(; i < userPipe.size(); i++) {
                        if(userPipe[i]->senderId == (*it)->senderId && userPipe[i]->recverId == (*it)->recverId)
                            break;
                    }
                    int devNull = open("/dev/null", O_RDWR);
                    if((*it)->isRecvNull) {
                        dup2(devNull, 0);
                        close(devNull);
                    }
                    else {
                        dup2(userPipe[i]->pipe[0], 0);
                        close(userPipe[i]->pipe[0]);
                    }
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
            }
        }
        multiCommand.clear();
    }
    write(usersInfo[srcIndex].fd, "% ", 2);
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

int main(int argc, char *argv[]) {
    struct sockaddr_in fsin;
    int alen;
    int fd;
    msock = passiveTCP(atoi(argv[1]));

    FD_ZERO(&afds);
    FD_SET(msock, &afds);

    for(int index = 0; index < 30; index++)
        initInfo(index);

    while(1) {
        memcpy(&rfds, &afds, sizeof(rfds));
        int max = msock;
        for(int index = 0; index < 30; index++)
            if(usersInfo[index].fd > max)
                max = usersInfo[index].fd;
        struct timeval tv = {0, 0};
        if(select(max + 1, &rfds, (fd_set*) 0, (fd_set*) 0, &tv) < 0) {
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
            string welcome = "****************************************\n** Welcome to the information server. **\n****************************************\n";
            write(ssock, welcome.c_str(), welcome.length());

            int index;
            for(index = 0; index < QLEN; index++) {
                if(!usersInfo[index].isExist) {
                    usersInfo[index].isExist = true;
                    usersInfo[index].fd = ssock;
                    usersInfo[index].nickname = "(no name)";
                    string ip = inet_ntoa(fsin.sin_addr);
                    usersInfo[index].address = ip + ":" + to_string(ntohs(fsin.sin_port));
                    usersInfo[index].env["PATH"] = "bin:.";

                    string temp = "*** User '" + usersInfo[index].nickname + "' entered from " + usersInfo[index].address + ". ***\n";
                    broadcast(temp);
                    break;
                }
            }
            write(ssock, "% ", 2);
        }

        for(int index = 0; index < 30; index++)
            if(usersInfo[index].isExist) {
                npshell(index);
            }
    }
    return 0;
}