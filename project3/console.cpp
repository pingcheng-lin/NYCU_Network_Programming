#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
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
            }
            });
    }

    void do_connect(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        socket_.async_connect(*it, 
            [this, self](const boost::system::error_code ec) {
            if(!ec) {
                do_read();
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
                cout << "<script>document.getElementById('s" << to_string(targetIndex) << "').innerHTML += '" << replaceChar(temp) << "';</script>" << endl;
                memset(data_, '\0' , max_length);
                
                if(temp.find("% ") == string::npos)
                    do_read();
                else
                    do_write();
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
        cout << "<script>document.getElementById('s" << to_string(targetIndex) << "').innerHTML += '<b>" << replaceChar(temp) << "</b>';</script>" << endl;
        async_write(socket_, boost::asio::buffer(temp.c_str(), temp.length()), 
            [this, self](boost::system::error_code ec, size_t /*length*/) {
            if(!ec) {
                if(nps[targetIndex].isUsed)
                    do_read();
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

void handleQuery() {
    string query = getenv("QUERY_STRING");
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
    cout << "Content-type: text/html\r\n\r\n "
    << "<!DOCTYPE html> "
    << "<html lang=\"en\"> "
    << "<head> "
    <<     "<meta charset=\"UTF-8\" /> "
    <<     "<title>NP Project 3 Sample Console</title> "
    <<     "<link "
    <<     "rel=\"stylesheet\" "
    <<     "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\" "
    <<     "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\" "
    <<     "crossorigin=\"anonymous\" "
    <<     "/> "
    <<     "<link "
    <<     "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\" "
    <<     "rel=\"stylesheet\" "
    <<     "/> "
    <<     "<link "
    <<     "rel=\"icon\" "
    <<     "type=\"image/png\" "
    <<     "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\" "
    <<     "/> "
    <<     "<style> "
    <<     "* { "
    <<         "font-family: 'Source Code Pro', monospace; "
    <<         "font-size: 1rem !important; "
    <<     "} "
    <<     "body { "
    <<         "background-color: #212529; "
    <<     "} "
    <<     "pre { "
    <<         "color: #cccccc; "
    <<     "} "
    <<     "b { "
    <<         "color: #01b468; "
    <<     "} "
    <<     "</style> "
    << "</head> "
    << "<body> "
    <<     "<table class=\"table table-dark table-bordered\"> "
    <<     "<thead> "
    <<         "<tr> ";
    for(int index = 0; index < 5 && nps[index].isUsed; index++)
        cout << "<th scope=\"col\">" << nps[index].hostname << ":" << nps[index].port << "</th> ";
    cout
    <<         "</tr> "
    <<     "</thead> "
    <<     "<tbody> "
    <<         "<tr> ";
    for(int index = 0; index < 5 && nps[index].isUsed; index++)
        cout << "<td><pre id=\"s" << to_string(index) << "\" class=\"mb-0\"></pre></td> ";
    cout
    <<         "</tr> "
    <<     "</tbody> "
    <<     "</table> "
    << "</body> "
    << "</html> ";
}

int main(int argc, char* argv[])
{
    try
    {
        boost::asio::io_context io_context;
        handleQuery();
        renderWebsite();
        for(int index = 0; index < 5 && nps[index].isUsed; index++)
            make_shared<client>(index, io_context)->start();

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}