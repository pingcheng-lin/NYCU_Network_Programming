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

struct IpAddress {
	string ipPart[4];
};

class session 
: public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket)), dst_socket_(io_context), acceptor_(io_context), resolver_(io_context)
    {}

    void start() {
        do_read();
    }

private:
    void do_read() {
		auto self(shared_from_this());
		memset(data_, 0, max_length);
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec) {
				S_IP = socket_.remote_endpoint().address().to_string();
				S_PORT = to_string(socket_.remote_endpoint().port());
				for(int i = 0; i < 4; i++)
					D_IP_int[i] = (int)data_[i + 4];
				if (D_IP_int[0] == 0 && D_IP_int[1] == 0 && D_IP_int[2] == 0 && D_IP_int[3] != 0) {
					int uidLen = 0;
					for(; (int)data_[8 + uidLen] != 0; uidLen++);

					string domain;
					for(int domainLen = 0; (int)data_[9 + uidLen + domainLen] != 0; domainLen++)
						domain += data_[9 + uidLen + domainLen];
					D_IP = domain;
				}
				else
					D_IP = to_string(D_IP_int[0]) + "." + to_string(D_IP_int[1]) + "." + to_string(D_IP_int[2]) + "." + to_string(D_IP_int[3]);

				D_PORT = to_string((int)data_[2] * 256 + (int)data_[3]);

				isAccept = true;
				CD = (int)data_[1];
				if(CD == 1)
					do_resolve();
				else
					do_bind();
			}
			});
    }

	void do_resolve() {
		auto self(shared_from_this());
		tcp::resolver::query query(D_IP, D_PORT);
		resolver_.async_resolve(query,
			[this, self](const boost::system::error_code& ec, tcp::resolver::results_type resultsEndpoint) {
			if(!ec) {
				endpoint = resultsEndpoint;
				do_connect();
			}
			else
				do_reject();
			});
	}

	void do_connect() {
		auto self(shared_from_this());
		boost::asio::async_connect(dst_socket_, endpoint,
			[this, self](boost::system::error_code ec, tcp::endpoint) {
				if (!ec) {
					D_IP = dst_socket_.remote_endpoint().address().to_string();
					size_t pos1 = 0, pos2 = 0;
					for (int i = 0; i < 4; i++) {
						pos2 = D_IP.find('.', pos1);
						D_IP_int[i] = atoi(D_IP.substr(pos1, pos2 - pos1).c_str());
						pos1 = pos2 + 1;
					}

					if(firewall())
						createSocks4Reply(true);
					else
						do_reject();
				}
				else
					do_reject();
			});
	}

	void do_bind() {
		auto self(shared_from_this());
		srand(time(nullptr));
		short port = 0;
		while(1) {
			port = rand() % 10000 + 10000;
			tcp::endpoint endpoint_(tcp::v4(), port);
			acceptor_.open(endpoint_.protocol());
			boost::system::error_code ec;
			acceptor_.bind(endpoint_, ec);
			if (!ec)
				break;
			acceptor_.close();
		}
		acceptor_.listen();
		D_PORT = to_string(port);
		createSocks4Reply(true);
	}

	bool firewall() {
		vector<IpAddress> connectRule;
		vector<IpAddress> bindRule;
		vector<IpAddress> tempVec;
		socksConf.open("socks.conf", fstream::in);
		string rule;
		while(getline(socksConf, rule)) {
			stringstream ssRule(rule);
			string words[3];
			ssRule >> words[0] >> words[1] >> words[2];

			stringstream ssWord(words[2]);
			string temp;
			IpAddress permitIp;
			for (int i = 0; i < 4; i++) {
				getline(ssWord, temp, '.');
				permitIp.ipPart[i] = temp;
			}

			if(words[1] == "c")
				connectRule.push_back(permitIp);
			else
				bindRule.push_back(permitIp);
		}
		socksConf.close();

		tempVec = CD == 1 ? connectRule : bindRule;
		if(tempVec.size() < 1)
			return false;
		for(vector<IpAddress>::iterator it = tempVec.begin(); it != tempVec.end(); it++)
			for(int i = 0; i < 4; i++)
				if((*it).ipPart[i] != "*" && (*it).ipPart[i] != to_string(D_IP_int[i]))
					return false;
		return true;
	}

	void createSocks4Reply(bool localIsAccept) {
		auto self(shared_from_this());
		isAccept = localIsAccept;
		memset(reply, 0 , sizeof(reply));
		reply[0] = 0;
		reply[1] = isAccept ? 90 : 91;
		reply[2] = 0;
		reply[3] = 0;
		reply[4] = 0;
		reply[5] = 0;
		reply[6] = 0;
		reply[7] = 0;

		if(CD == 1)
			do_reply();
		else {
			reply[2] = stoi(D_PORT) / 256;
			reply[3] = stoi(D_PORT) % 256;
			do_bindReply();
		}
	}

	void do_bindReply() {
		auto self(shared_from_this());
		boost::asio::async_write(socket_,boost::asio::buffer(reply, sizeof(reply)),
			[this, self](boost::system::error_code ec, std::size_t /*length*/) {
			if(!ec) {
				do_accept();
			}
			});
	}

	void do_accept() {
		auto self(shared_from_this());
		boost::system::error_code ec;
		acceptor_.accept(dst_socket_, ec);
		acceptor_.close();
		if(!ec)
			do_reply();
		else
			do_reject();
	}

	void do_reply() {
		auto self(shared_from_this());
		boost::asio::async_write(socket_,boost::asio::buffer(reply, sizeof(reply)),
			[this, self](boost::system::error_code ec, std::size_t /*length*/) {
			if (!ec) {
				cout << "<S_IP>: " << S_IP << endl;
				cout << "<S_PORT>: " << S_PORT << endl;
				cout << "<D_IP>: " << D_IP << endl;
				cout << "<D_PORT>: " << D_PORT << endl;
				string cmdMode = CD == 1 ? "CONNECT" : "BIND";
				cout << "<Command>: " << cmdMode << endl;
				string replyMode = isAccept ? "Accept" : "Reject";
				cout << "<Reply>: " << replyMode << endl;
				fflush(stdout);

				if(isAccept) {
					do_read_client();
					do_read_server();
				}
			}
			});
	}

	void do_reject() {
		createSocks4Reply(false);
		close_socket();
	}

	void do_read_client() {
		auto self(shared_from_this());
		memset(clientData_, '\0' , max_length);
		socket_.async_read_some(boost::asio::buffer(clientData_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec)
				do_write_server(length);
			else
				close_socket();
			});
	}

	void do_read_server() {
		auto self(shared_from_this());
		memset(serverData_, '\0' , max_length);
		dst_socket_.async_read_some(boost::asio::buffer(serverData_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length) {
			if (!ec)
				do_write_client(length);
			else
				close_socket();
			});
	}

	void do_write_client(std::size_t length) {
		auto self(shared_from_this());
		boost::asio::async_write(socket_,boost::asio::buffer(serverData_, length),
			[this, self](boost::system::error_code ec, std::size_t /*length*/) {
			if (!ec) {
				do_read_server();
			}
			else
				close_socket();
			});
	}

	void do_write_server(std::size_t length) {
		auto self(shared_from_this());
		boost::asio::async_write(dst_socket_,boost::asio::buffer(clientData_, length),
			[this, self](boost::system::error_code ec, std::size_t /*length*/) {
			if (!ec)
				do_read_client();
			else
				close_socket();
			});
	}

	void close_socket() {
		socket_.close();
		dst_socket_.close();
	}

	tcp::socket socket_;
	tcp::socket dst_socket_;
	tcp::acceptor acceptor_;
	tcp::resolver resolver_;
	tcp::resolver::results_type endpoint;
	enum { max_length = 1024 };
	unsigned char data_[max_length];
	char clientData_[max_length];
	char serverData_[max_length];

	int CD; 
	string S_IP;
	string S_PORT;
	string D_IP;
	string D_PORT;
	int D_IP_int[4];
	bool isAccept;
	unsigned char reply[8];
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

        server s(atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}