#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <sstream>
#include <fstream>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;
using namespace std;

class npInfo {
public:
    bool isUsed = false;
    string hostname;
    string port;
    string file;
private:
};

npInfo nps[5];
boost::asio::io_context io_context;
boost::asio::io_context io_context_socket;
tcp::socket web_socket_(io_context_socket);

class client 
: public enable_shared_from_this<client>
{
public:
    client(int index, boost::asio::io_context& io_context)
        : targetIndex(index), socket_(io_context), resolver(io_context) {
            fs.open("./test_case/" + nps[targetIndex].file, fstream::in);
        }
    
    void start() {
        do_resolve();
    }            
private:
    void do_resolve() {
        auto self(shared_from_this());
        tcp::resolver::query query(nps[targetIndex].hostname, nps[targetIndex].port);
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

    void do_read() {
        auto self(shared_from_this());
        memset(data_, '\0' , max_length);
        socket_.async_read_some(boost::asio::buffer(data_, max_length), 
            [this, self](boost::system::error_code ec, size_t length) {
            if(!ec) {
                string temp(data_);
                string html = "<script>document.getElementById('s" + to_string(targetIndex) + "').innerHTML += '" + replaceChar(temp) + "';</script>\n";
                do_web_write(html);
                memset(data_, '\0' , max_length);
                
                if(temp.find("% ") == string::npos)
                    do_read();
                else
                    do_write();
            } else {
                cout << "Error (do_read): " << ec.message() << "\n";
            }
            });
    }

    void do_write() {
        auto self(shared_from_this());
        string temp;
        getline(fs, temp);
        if(temp.find("exit") != string::npos) {
            nps[targetIndex].isUsed = false;
            fs.close();
        }
        temp += "\n";
        string html = "<script>document.getElementById('s" + to_string(targetIndex) + "').innerHTML += '<b>" + replaceChar(temp) + "</b>';</script>\n";
        do_web_write(html);
        async_write(socket_, boost::asio::buffer(temp.c_str(), temp.length()), 
            [this, self](boost::system::error_code ec, size_t /*length*/) {
            if(!ec) {
                if(nps[targetIndex].isUsed)
                    do_read();
            } else {
                cout << "Error (do_write): " << ec.message() << "\n";
            }
            });
    }

    void do_web_write(string temp)
    {
        auto self(shared_from_this());
        async_write(web_socket_, boost::asio::buffer(temp.c_str(), temp.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
            if (!ec)
            {
            } else {
                cout << "Error (do_web_write): " << ec.message() << "\n";
            }
            });
    }

    string replaceChar(string target) {
        boost::replace_all(target, "&", "&amp;");
        boost::replace_all(target, "\"", "&quot;");
        boost::replace_all(target, "\'", "&apos;");
        boost::replace_all(target, "<", "&lt;");
        boost::replace_all(target, ">", "&gt;");
        boost::replace_all(target, "\r", "");
        boost::replace_all(target, "\n", "<br>");
        return target;
    }

    int targetIndex;
    fstream fs;
    tcp::socket socket_;
    tcp::resolver resolver;
    enum { max_length = 15000 };
    char data_[max_length];  
};


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
                do_my_write("HTTP/1.1 200 OK\r\n");
                if(REQUEST_URI.find("panel.cgi") != string::npos)
                    panel_cgi();
                else if(REQUEST_URI.find("console.cgi") != string::npos)
                    console_cgi();
            } else {
                cout << "Error (do_read): " << ec.message() << "\n";
            }
            });
    }

    void do_my_write(string temp)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(temp.c_str(), temp.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
            if (!ec)
            {
            } else {
                cout << "Error (do_my_write): " << ec.message() << "\n";
            }
            });
    }

    void panel_cgi() {
        string html = ""
            "Content-type: text/html\r\n\r\n "
                "<!DOCTYPE html> "
                "<html lang=\"en\"> "
                "<head> "
                    "<title>NP Project 3 Panel</title> "
                    "<link "
                    "rel=\"stylesheet\" "
                    "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\" "
                    "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\" "
                    "crossorigin=\"anonymous\" "
                    "/> "
                    "<link "
                    "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" "
                    "rel=\"stylesheet\" "
                    "/> "
                    "<link "
                    "rel=\"icon\" "
                    "type=\"image/png\" "
                    "href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\" "
                    "/> "
                    "<style> "
                    "* { "
                    "    font-family: 'Source Code Pro', monospace; "
                    "} "
                    "</style> "
                "</head> "
                "<body class=\"bg-secondary pt-5\"> "
                    "<form action=\"console.cgi\" method=\"GET\"> "
                    "<table class=\"table mx-auto bg-light\" style=\"width: inherit\"> "
                        "<thead class=\"thead-dark\"> "
                        "<tr> "
                            "<th scope=\"col\">#</th> "
                            "<th scope=\"col\">Host</th> "
                            "<th scope=\"col\">Port</th> "
                            "<th scope=\"col\">Input File</th> "
                        "</tr> "
                        "</thead> "
                        "<tbody> ";
        for(int i = 0; i < 5; i++) {
            html += "<tr> "
                        "<th scope=\"row\" class=\"align-middle\">Session " + to_string(i + 1) + "</th> "
                        "<td> "
                        "<div class=\"input-group\"> "
                            "<select name=\"h" + to_string(i) + "\" class=\"custom-select\"> "
                            "<option></option> "
                            "<option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option> "
                            "<option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option> "
                            "<option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option> "
                            "<option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option> "
                            "<option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option> "
                            "<option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option> "
                            "<option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option> "
                            "<option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option> "
                            "<option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option> "
                            "<option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option> "
                            "<option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option> "
                            "<option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option> "
                            "</select> "
                            "<div class=\"input-group-append\"> "
                            "<span class=\"input-group-text\">.cs.nctu.edu.tw</span> "
                            "</div>"
                        "</div> "
                        "</td> "
                        "<td> "
                        "<input name=\"p" + to_string(i) + "\" type=\"text\" class=\"form-control\" size=\"5\" /> "
                        "</td> "
                        "<td> "
                        "<select name=\"f" + to_string(i) + "\" class=\"custom-select\"> "
                            "<option></option> "
                            "<option value=\"t1.txt\">t1.txt</option> "
                            "<option value=\"t2.txt\">t2.txt</option> "
                            "<option value=\"t3.txt\">t3.txt</option> "
                            "<option value=\"t4.txt\">t4.txt</option> "
                            "<option value=\"t5.txt\">t5.txt</option> "
                        "</select> "
                        "</td> "
                    "</tr> ";
        }
        html += "<tr> "
                    "<td colspan=\"3\"></td> "
                    "<td> "
                    "<button type=\"submit\" class=\"btn btn-info btn-block\">Run</button> "
                    "</td> "
                "</tr> "
                "</tbody> "
            "</table> "
            "</form> "
        "</body> "
        "</html> ";
        do_my_write(html);
    }

    void console_cgi() {
        try
        {
            handleQuery(QUERY_STRING);
            renderWebsite();
            web_socket_ = std::move(socket_);
            for(int index = 0; index < 5 && nps[index].isUsed; index++)
                make_shared<client>(index, io_context)->start();
        }
        catch (std::exception& e)
        {
            std::cerr << "Exception: " << e.what() << "\n";
        }
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

    void handleQuery(string query) {
        size_t pos1 = 0, pos2 = 0;
        string temp;
        for(int index = 0; index < 5; index++) {
            pos2 = query.find('&', pos1);
            temp = query.substr(pos1, pos2 - pos1);
            pos1 = pos2 + 1;
            if(temp.length() > 3)
                nps[index].hostname = temp.substr(3);

            pos2 = query.find('&', pos1);
            temp = query.substr(pos1, pos2 - pos1);
            pos1 = pos2 + 1;
            if(temp.length() > 3)
                nps[index].port = temp.substr(3);

            pos2 = query.find('&', pos1);
            temp = query.substr(pos1, pos2 - pos1);
            pos1 = pos2 + 1;
            if(temp.length() > 3) {
                nps[index].file = temp.substr(3);
                nps[index].isUsed = true;
            }
        }
    }


    void renderWebsite() {
        string html = "Content-type: text/html\r\n\r\n "
                    "<!DOCTYPE html> "
                    "<html lang=\"en\"> "
                    "<head> "
                        "<meta charset=\"UTF-8\" /> "
                        "<title>NP Project 3 Sample Console</title> "
                        "<link "
                        "rel=\"stylesheet\" "
                        "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\" "
                        "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\" "
                        "crossorigin=\"anonymous\" "
                        "/> "
                        "<link "
                        "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" "
                        "rel=\"stylesheet\" "
                        "/> "
                        "<link "
                        "rel=\"icon\" "
                        "type=\"image/png\" "
                        "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" "
                        "/> "
                        "<style> "
                        "* { "
                            "font-family: 'Source Code Pro', monospace; "
                            "font-size: 1rem !important; "
                        "} "
                        "body { "
                            "background-color: #212529; "
                        "} "
                        "pre { "
                            "color: #cccccc; "
                        "} "
                        "b { "
                            "color: #01b468; "
                        "} "
                        "</style> "
                    "</head> "
                    "<body> "
                        "<table class=\"table table-dark table-bordered\"> "
                        "<thead> "
                            "<tr> ";
        for(int index = 0; index < 5 && nps[index].isUsed; index++)
            html += "<th scope=\"col\">" + nps[index].hostname + ":" + nps[index].port + "</th> ";
        html += "</tr> "
            "</thead> "
            "<tbody> "
                "<tr> ";
        for(int index = 0; index < 5 && nps[index].isUsed; index++)
            html += "<td><pre id=\"s" + to_string(index) + "\" class=\"mb-0\"></pre></td> ";
        html += "</tr> "
            "</tbody> "
            "</table> "
        "</body> "
        "</html> ";
        do_my_write(html);
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
            } else {
                cout << "Error (accept): " << ec.message() << "\n";
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

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}