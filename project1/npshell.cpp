#include <iostream>
using namespace std;

int main() {
    string command;

    cout << "% ";
    while (cin >> command) {
        if (command == "exit")
            exit(0);
        cout << "% ";
    }
    return 0;
}