#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

#include "message_parser.h"

#define MAX_SERVERS 5

class NetworkHandler {

public:

    struct Command {
        int from;
        std::string role;
        std::vector<std::string> tokens;
        std::vector<std::string> delegate_tokens;
        std::string raw;
    };

    struct Message {
        int to;
        std::string message;
    };

    struct Server {
        int socket;
        std::string id;
        std::string ip;
        int port;
        std::vector<std::string> intermediates;
        int distance;
        long int last_comms;
    };

    NetworkHandler(int network_port, int info_port, int control_port) {
        this->control_port = control_port;
        this->network_port = network_port;
        this->info_port = info_port;

        // Connected clients
        this->client_sockets["control"] = std::vector<int>();
        this->client_sockets["network"] = std::vector<int>();
        this->client_sockets["info"] = std::vector<int>();

        this->control_socket = -1;
        this->network_socket = -1;
        this->info_socket = -1;
        this->top_socket = -1;

        this->server_timeout = 300; // 5m
        this->last_heartbeat = 0;
        this->heartbeat_interval = 60; // 1m

        setup_sockets();
    }

    ~NetworkHandler() {}

    Command get_message() {
        Command command;

        struct timeval t;
        t.tv_sec = 10;
        t.tv_usec = 0;

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

            if(known_servers.size() < MAX_SERVERS) {
                int client_socket = accept_connection(network_socket, true);
                client_sockets["network"].push_back(client_socket);

                command.from = -1;
                command.role = "root";
                command.tokens = {"META_REQUEST_ID", std::to_string(client_socket)};
            }
            else {
                // accept connection to flush socket
                int client_socket = accept_connection(network_socket);
                close_socket(client_socket);
            }

            return command;
        }

        if(FD_ISSET(info_socket, &socket_set)) {
            std::cout << "Got an info request" << std::endl;

            // TODO abstract from get_message()
            command.from = info_socket;
            command.role = "info";
            command.tokens = {"LISTSERVERS"};
        }

        // Get messages from all clients
        for(auto socket_type : {"control", "network", "info"}) {
            for(int client_socket : client_sockets[socket_type]) {

                if(FD_ISSET(client_socket, &socket_set)) {
                    command.raw = read_socket(client_socket);
                    command.from = client_socket;
                    command.role = socket_type;

                    // The message contains commas
                    // Assume that this is a meta command with a delegate command trailing
                    if(command.raw.find(',') != std::string::npos) {
                        command.tokens = message_parser.tokenize(command.raw, ',');
                        command.delegate_tokens = message_parser.tokenize(command.tokens.back(), ' ');

                        // The delegate command appears to be comma delimited
                        if(command.tokens.size() > 4 && command.delegate_tokens.size() == 1) {
                            
                            // Erase old delegate since it's probably just the last word in a multi word command
                            std::vector<std::string> v;
                            command.delegate_tokens = v;

                            for(int i = 3; i < command.tokens.size(); i++) {
                                command.delegate_tokens.push_back(command.tokens[i]);
                            }
                        }
                    }
                    else {
                        command.tokens = message_parser.tokenize(command.raw, ' ');
                    }

                    return command;
                }
            }
        }

        return command;
    }

    // Check the timestamp of last comms to determine if a server is alive
    bool is_alive(Server &server) {
        return (timestamp() - server.last_comms) < server_timeout;
    }

    void message(Message m) {
        char start = 1; // SOH
        char end = 4;   // EOT

        m.message = start + m.message + end;
        if(m.to == info_socket) {
            echo_udp(m);
        }
        else {
            write(m.to, m.message.c_str(), m.message.length());            
        }
    }

    bool heartbeat() {
        if((timestamp() - last_heartbeat) < heartbeat_interval) {
            return false;
        }

        Message m;
        m.message = "KEEPALIVE";

        for(auto sp : known_servers) {
            if(sp.second.distance == 1) {
                m.to = sp.first;
                message(m);
            }
        }

        last_heartbeat = timestamp();
        return true;
    }

    // Drop servers that haven't sent comms in too long
    void cleanup() {
        std::vector<std::string> erased_1hop_servers;

        // First iteration: Remove 1-hop servers
        for(auto sp : known_servers) {

            // 1 hop server
            if(sp.second.distance == 1) {
                if((timestamp() - sp.second.last_comms) > server_timeout) {
                    known_servers.erase(sp.first);
                    erased_1hop_servers.push_back(sp.second.id);
                }
            }
        }

        // Second iteration: Remove servers whose first intermediate is a removed 1-hop server
        for(auto sp : known_servers) {
            for(std::string erased_server : erased_1hop_servers) {
                if(sp.second.intermediates.size() > 0 && sp.second.intermediates[0] == erased_server) {
                    known_servers.erase(sp.first);
                }
            }
        }
    }

    std::map<int, Server> get_servers() {
        return known_servers;
    }

    bool contains_server(int socket) {
        return known_servers.count(socket) ? true : false;
    }

    Server& get_server(int socket) {
        return known_servers[socket];
    }

    Server find_server(std::string id) {
        for(auto it = known_servers.begin(); it != known_servers.end(); ++it) {
            if(it->second.id == id) {
                return it->second;
            }
        }

        Server non_server;
        return non_server;
    }

    Server connect_to_server(const std::string ip, const int port) {
        struct sockaddr_in destination_in;
        struct hostent *destination;

        destination = gethostbyname(ip.c_str());
        if(destination == NULL) { throw std::runtime_error("Destination IP unavailable"); }

        bzero((char *) &destination_in, sizeof(destination_in));

        // destination info
        destination_in.sin_family = AF_INET;
        destination_in.sin_port = htons(port);
        bcopy((char *)destination->h_addr,
        (char *)&destination_in.sin_addr.s_addr,
        destination->h_length);

        int server_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        make_reusable(server_socket);
        
        connect(server_socket, (struct sockaddr *) &destination_in, sizeof(destination_in));
 
        int error_code;
        socklen_t error_code_size = sizeof(error_code);
        getsockopt(server_socket, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
        
        // Socket is not connected
        if(error_code == 111) { throw std::runtime_error("Unable to connect to server"); }

        // fill new server info and return
        Server server;
        server.socket = server_socket;
        server.ip = ip;
        server.port = port;
        server.distance = 1;
        server.last_comms = timestamp();    // Special case. Set timestamp to when we sent a message
                                            // Typically only set these when comms are received.

        // add new server to known_servers
        known_servers[server_socket] = server;
        client_sockets["network"].push_back(server_socket);
        
        return server;
    }

    int get_network_port() {
        std::cout << "Returning port: " << network_port << std::endl;
        return network_port;
    }

    std::string get_server_ip() {
        if(server_ip == "") {
            server_ip = get_local_address();
        }

        return server_ip;
    }

    long timestamp() {
        time_t t = std::time(0);
        long int now = static_cast<long int> (t);
        return now;
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

    std::map<int, Server> known_servers;    //Keys: Server fd - Values: Server structs
    long int server_timeout;
    std::string server_ip;

    long int last_heartbeat;
    long int heartbeat_interval;

    MessageParser message_parser;

    void echo_udp(const Message m) {
        // client info
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        bzero((char *) &cli_addr, sizeof(cli_addr));
        clilen = sizeof(cli_addr);

        // need buffer purely as recvfrom argument
        char buffer[256];
        bzero(buffer, 256);

        // recvfrom to fill clients info
        int len = recvfrom(m.to, buffer, sizeof(buffer), 0,
                       (struct sockaddr *)&cli_addr, &clilen);

        sendto(m.to, m.message.c_str(), strlen(m.message.c_str()), 0, 
            (struct sockaddr *)&cli_addr, sizeof(cli_addr));
    }
    
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
                    top_socket = std::max(client_socket, top_socket);
                }
            }
        }

        // Reset server sockets
        if(control_socket > 0) {
            FD_SET(control_socket, &socket_set);
            top_socket = std::max(control_socket, top_socket);
        }
        else {
            error("Invalid control socket");
        }

 
        if(network_socket > 0) {
            FD_SET(network_socket, &socket_set);
            top_socket = std::max(network_socket, top_socket);
        }
        else {
            error("Invalid network socket");            
        }
      
        if(info_socket > 0) {
            FD_SET(info_socket, &socket_set);
            top_socket = std::max(info_socket, top_socket);
        }
        else {
            error("Invalid info socket");            
        }
    }

    int accept_connection(int socket, bool server=false) {
        // setup client address info
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        bzero((char *) &cli_addr, sizeof(cli_addr));
        clilen = sizeof(cli_addr);

        int client_socket = accept(socket, (struct sockaddr *) &cli_addr, &clilen);

        if(server) {
            Server s;
            s.socket = client_socket;
            s.ip = ""; //inet_ntoa(cli_addr.sin_addr);
            s.port = 0; // ntohs(cli_addr.sin_port);
            s.distance = 1;
            s.last_comms = timestamp();

            known_servers[client_socket] = s;
        }

        return client_socket;
    }

    std::string read_socket(int socket) {
        char buffer[256];
        bzero(buffer, 256);

        if(read(socket, buffer, 255) <= 0) {  
            std::cout << "Client disconnected" << std::endl;
            remove_client(socket);
            return "LEAVE";
        }

        return trim_message(buffer);
    }

    std::string trim_message(std::string s) {
        std::string trimmed = s;

        // Temporarily trimming transmission symbols
        // Validate messages with them later

        while(trimmed[trimmed.length()-1] == '\n' || trimmed[trimmed.length()-1] == 4) {
            trimmed.erase(trimmed.length()-1);
        }

        if(trimmed[0] == 1) {
            trimmed.erase(0, 1);
        }

        return trimmed;
    }

    void remove_client(int socket) {
        close_socket(socket);

        // Remove client fd. Take care not to deep copy the vector.
        for(auto socket_type : {"control", "network", "info"}) {
            auto *v = &client_sockets[socket_type];
            v->erase(std::remove(v->begin(), v->end(), socket), v->end());
        }

        // Remove if known server
        known_servers.erase(socket);
    }

    void close_socket(int fd) {
        if (fd >= 0) {
            // clear socket from socket_set
            FD_CLR(fd, &socket_set);
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

    std::string get_local_address() {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        int sock = socket ( AF_INET, SOCK_DGRAM, 0);
    
        const char* kGoogleDnsIp = "8.8.8.8";
        int dns_port = 53;
    
        struct sockaddr_in serv;
    
        memset( &serv, 0, sizeof(serv) );
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
        serv.sin_port = htons( dns_port );
    
        int err = connect( sock , (const struct sockaddr*) &serv , sizeof(serv) );
    
        struct sockaddr_in name;
        socklen_t namelen = sizeof(name);
        err = getsockname(sock, (struct sockaddr*) &name, &namelen);
        close(sock);

        char ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET, &name.sin_addr, ip, sizeof ip);
        return ip;
    }
};
