#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

#include "message_parser.h"

class NetworkHandler {

public:

    struct Command {
        int from;
        std::string role;
        std::vector<std::string> tokens;
    };

    struct Message {
        int to;
        std::string message;
    };

    struct Server {
        int socket;
        std::string IP;
        int port;
    };

    NetworkHandler(int network_port, int info_port, int control_port) {
        this->control_port = control_port;
        this->network_port = network_port;
        this->info_port = info_port;

        // Connected clients
        this->client_sockets["control"] = std::vector<int>();
        this->client_sockets["network"] = std::vector<int>();
        this->client_sockets["info"] = std::vector<int>();

        this->keep_running = true;
        this->control_socket = -1;
        this->network_socket = -1;
        this->info_socket = -1;
        this->top_socket = -1;

        setup_sockets();
    }

    ~NetworkHandler() {}

    Command get_message() {
        keep_running = true;
        struct timeval t;

        Command command;

        t.tv_sec = 10;
        reset_socket_set();

        if(select(top_socket+1, &socket_set, NULL, NULL, &t) < 0) {
            error("Unable to select");
        }

        // 3 Ports that allow connections
        // Your role is determined by which port you connected through
        if(FD_ISSET(control_socket, &socket_set)) {
            std::cout << "Got a control request" << std::endl;

            int client_socket = accept_connection(control_socket);

            client_sockets["control"].push_back(client_socket);
        }

        if(FD_ISSET(network_socket, &socket_set)) {
            std::cout << "Got a network request" << std::endl;

            int client_socket = accept_connection(network_socket);
            client_sockets["network"].push_back(client_socket);

            // TODO: Track connected server
        }

        if(FD_ISSET(info_socket, &socket_set)) {
            std::cout << "Got an info request" << std::endl;

            // TODO abstract from get_message()

            socklen_t clilen;
            struct sockaddr_in cli_addr;
            bzero((char *) &cli_addr, sizeof(cli_addr));
            clilen = sizeof(cli_addr);

            char buffer[256];
            bzero(buffer, 256);

            // read content into buffer from an incoming client
		    int len = recvfrom(info_socket, buffer, sizeof(buffer), 0,
		                   (struct sockaddr *)&cli_addr, &clilen);

            std::string response = "";
            for(auto const& server: known_servers) {
                response += server.first + ","                  + 
                             server.second.IP + ","             +
                             std::to_string(server.second.port) + ";\n";   
            }

		    sendto(info_socket, response.c_str(), len, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
        }

        // Get messages from all clients
        for(auto socket_type : {"control", "network", "info"}) {
            for(int client_socket : client_sockets[socket_type]) {

                if(FD_ISSET(client_socket, &socket_set)) {
                    command.from = client_socket;
                    command.role = socket_type;
                    command.tokens = message_parser.tokenize(read_socket(client_socket));

                    // TODO: Multithreading and command queues
                    return command;
                }
            }
        }

        return command;
    }

    void message(Message m) {
        char start = 1; // SOH
        char end = 4;   // EOT

        m.message = start + m.message + end;
        write(m.to, m.message.c_str(), m.message.length());
    }

    void stop() {
        keep_running = false;
    }

    void heartbeat() {

    }

    std::map<std::string, Server> get_servers() {
        return known_servers;
    }

    Server get_server(std::string id) {
        return known_servers[id];
    }

    bool connect_to_server(Server server) {
        struct sockaddr_in destination_in;
        struct hostent *destination;

        destination = gethostbyname(server.IP.c_str());
        if(destination == NULL) {
            return false;
        }

        bzero((char *) &destination_in, sizeof(destination_in));

        // destination info
        destination_in.sin_family = AF_INET;
        destination_in.sin_port = htons(server.port);
        bcopy((char *)destination->h_addr,
        (char *)&destination_in.sin_addr.s_addr,
        destination->h_length);

        int server_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        

        connect(server_socket, (struct sockaddr *) &destination_in, sizeof(destination_in));
 
        int error_code;
        socklen_t error_code_size = sizeof(error_code);
        getsockopt(server_socket, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
        
        // Socket is not connected
        if(error_code == 111) {
            return false;
        }

        client_sockets["network"].push_back(server_socket);
        server.socket = server_socket;
        known_servers[std::to_string(server.port)] = server;
        
        // Message m {server_socket, "ID"};
        // message(m);
        
        return true;
    }

private:
    int control_port;
    int control_socket;
    int network_port;
    int network_socket;
    int info_port;
    int info_socket;
    std::map<std::string, std::vector<int>> client_sockets;
    int top_socket;
    fd_set socket_set;

    std::map<std::string, Server> known_servers;    //Keys: Server ID - Values: Server structs
    bool keep_running;

    MessageParser message_parser;
    
    void setup_sockets() {
        control_socket = setup_tcp(control_port);
        network_socket = setup_tcp(network_port);
        info_socket = setup_udp(info_port);

        top_socket = std::max(control_socket, network_socket);
        top_socket = std::max(top_socket, info_socket);
    }

    int setup_tcp(int port) {
        struct sockaddr_in serv_addr;
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

        if(fd < 0) {
            error("Unable to open socket");
        }

        make_reusable(fd);

        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if(bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            error("Unable to bind tcp socket");
        }

        if(listen(fd, 1) < 0) {
            error("Unable to listen on socket");
        }

        return fd;
    }

    int setup_udp(int port) {
        struct sockaddr_in serv_addr;
        int fd = socket(AF_INET, SOCK_DGRAM, 0);

        if(fd < 0) {
            error("Unable to open socket");
        }

        make_reusable(fd);

        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if(bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            error("Unable to bind udp socket");
        }

        return fd;
    }

    void make_reusable(int socket) {
        int enable = 1;

        if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            error("Unable to modify socket");
        }
    }

    void reset_socket_set() {
        FD_ZERO(&socket_set);

        // Reset client sockets
        for(auto socket_type : {"control", "network", "info"}) {
            for(int client_socket : client_sockets[socket_type]) {
                if(client_socket > 0) {
                    FD_SET(client_socket, &socket_set);
                }
            }
        }

        // Reset server sockets
        if(control_socket > 0) {
            FD_SET(control_socket, &socket_set);
        }
        else {
            error("Invalid control socket");
        }

        if(network_socket > 0) {
            FD_SET(network_socket, &socket_set);
        }
        else {
            error("Invalid network socket");            
        }

        if(info_socket > 0) {
            FD_SET(info_socket, &socket_set);
        }
        else {
            error("Invalid info socket");            
        }
    }

    int accept_connection(int socket) {
        // setup client address info
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        bzero((char *) &cli_addr, sizeof(cli_addr));
        clilen = sizeof(cli_addr);

        int client_socket = accept(socket, (struct sockaddr *) &cli_addr, &clilen);
        top_socket = std::max(client_socket, top_socket);
        return client_socket;
    }

    void remove_client(int socket) {
        close_socket(socket);

        // Remove client fd. Take care not to deep copy the vector.
        for(auto socket_type : {"control", "network", "info"}) {
            auto *v = &client_sockets[socket_type];
            v->erase(std::remove(v->begin(), v->end(), socket), v->end());
        }
    }

    std::string read_socket(int socket) {
        char buffer[256];
        bzero(buffer, 256);

        if(read(socket, buffer, 255) <= 0) {  
            std::cout << "Client disconnected" << std::endl;
            remove_client(socket);
            return "LEAVE";
        }

        return buffer;
    }

    void close_socket(int fd) {
        if (fd >= 0) {
            if (close(fd) < 0) {
                error("Unable to close socket");
            }
        }
    }

    // TODO: Remove
    void error(const char *msg) {
        perror(msg);
        exit(0);
    }
};
