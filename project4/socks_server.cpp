#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <fstream>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_context io_context;

class session
: public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket)), dst_socket_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), 0)), resolver(io_context)
    {
        socksConf.open("socks.conf", fstream::in);
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
            if (!ec)
            {
                S_IP = socket_.local_endpoint().address().to_string();
                S_PORT = to_string(socket_.local_endpoint().port());
                D_IP = (int)data_[2] * 256 + (int)data_[3];
                if((int)data_[4] == 0 && (int)data_[5] == 0 && (int)data_[6] == 0 && (int)data_[7] != 0) {

                } 
                else {

                }
                
                D_PORT;
                if((int)data_[1] == 1)
                    Command = "CONNECT";
                else
                    Command = "BIND";
                Reply = "ACCEPT"; //firewall
                do_resolve();
            }
            });
    }

    void do_resolve() {
        auto self(shared_from_this());
        tcp::resolver::query query(D_IP, D_PORT);
        resolver.async_resolve(query,
            [this, self](boost::system::error_code ec, tcp::resolver::iterator it) {
            if(!ec) {
                do_connect(it);
            } else {
                cout << "Error (do_resolve): " << ec.message() << "\n";
            }
            });
    }

    void do_connect(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        socket_.async_connect(*it, 
            [this, self](const boost::system::error_code ec) {
            if(!ec) {
                do_read();
            } else {
                cout << "Error (do_connect): " << ec.message() << "\n";
            }
            });
    }

    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
            if (!ec)
            {
                do_read();
            }
            });
    }

    void showMsg() {
        cout << "<S_IP>: " << S_IP << endl;
        cout << "<S_PORT>: " << S_PORT << endl;
        cout << "<D_IP>: " << D_IP << endl;
        cout << "<D_PORT>: " << D_PORT << endl;
        cout << "<Command>: " << Command << endl;
        cout << "<Reply>: " << Reply << endl;
        fflush(stdout);
    }

    tcp::socket socket_;
    tcp::socket dst_socket_;
    tcp::acceptor acceptor_;
    tcp::resolver resolver;
    enum { max_length = 1024 };
    unsigned char data_[max_length];
    char clientData_[max_length];
    char serverData_[max_length];
    string S_IP;
    string S_PORT;
    string D_IP;
    string D_PORT;
    string Command;
    string Reply;
    fstream socksConf;
};

class server
{
public:
    server(short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
            if (!ec)
            {
                io_context.notify_fork(boost::asio::io_service::fork_prepare);
                if(fork() == 0) {
                    io_context.notify_fork(boost::asio::io_service::fork_child);
                    acceptor_.close();
                    std::make_shared<session>(std::move(socket))->start();
                }
                else {
                    io_context.notify_fork(boost::asio::io_service::fork_parent);
                    socket.close();
                }
            }

            do_accept();
            }); 
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        server s(std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}