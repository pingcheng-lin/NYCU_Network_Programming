#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>

using boost::asio::ip::tcp;
using namespace std;

class session
: public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
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
                handle_request();

                int childpid = fork();
                while(childpid < 0) {
                    usleep(100);
                    childpid = fork();
                }
                if(childpid > 0) {
                    socket_.close();
                } else {
                    my_setenv();
                    
                    dup2(socket_.native_handle(), 0);
                    dup2(socket_.native_handle(), 1);
                    dup2(socket_.native_handle(), 2);
                    socket_.close();

                    cout << "HTTP/1.1 200 OK\r\n";
                    fflush(stdout);

                    string path;
                    size_t pos = REQUEST_URI.find('?');
                    if(pos == string::npos)
                        path = "." + REQUEST_URI;
                    else
                        path = "." + REQUEST_URI.substr(0, pos);
                    if(execlp(path.c_str(), path.c_str(), NULL) == -1) {
                        perror("execlp");
                        exit(-1);
                    }
                }
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

    void handle_request() {
        string trash;
        stringstream temp(data_);
        
        temp >> REQUEST_METHOD;
        temp >> REQUEST_URI;
        size_t pos = REQUEST_URI.find('?');
        if(pos != string::npos)
            QUERY_STRING = REQUEST_URI.substr(pos + 1);
        temp >> SERVER_PROTOCOL;
        temp >> trash;
        temp >> HTTP_HOST;

        SERVER_ADDR = socket_.local_endpoint().address().to_string();
        SERVER_PORT = to_string(socket_.local_endpoint().port());
        REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
        REMOTE_PORT = to_string(socket_.remote_endpoint().port());
    }

    void my_setenv() {
        setenv("REQUEST_METHOD", REQUEST_METHOD.c_str(), 1);
        setenv("REQUEST_URI", REQUEST_URI.c_str(), 1);
        setenv("QUERY_STRING", QUERY_STRING.c_str(), 1);
        setenv("SERVER_PROTOCOL", SERVER_PROTOCOL.c_str(), 1);
        setenv("HTTP_HOST", HTTP_HOST.c_str(), 1);
        setenv("SERVER_ADDR", SERVER_ADDR.c_str(), 1);
        setenv("SERVER_PORT", SERVER_PORT.c_str(), 1);
        setenv("REMOTE_ADDR", REMOTE_ADDR.c_str(), 1);
        setenv("REMOTE_PORT", REMOTE_PORT.c_str(), 1);
    }

    string REQUEST_METHOD = "";
    string REQUEST_URI = "";
    string QUERY_STRING = "";
    string SERVER_PROTOCOL = "";
    string HTTP_HOST = "";
    string SERVER_ADDR = "";
    string SERVER_PORT = "";
    string REMOTE_ADDR = "";
    string REMOTE_PORT = "";

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
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
                std::make_shared<session>(std::move(socket))->start();
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

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}