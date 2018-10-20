#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <unistd.h>

#include <iostream>

class NetworkHandler {

public:
    NetworkHandler(int control_port=4055, int network_port=4044, int info_port=4045) {
        this->control_port = control_port;
        this->network_port = network_port;
        this->info_port = info_port;
        this->keep_running = true;
        this->control_socket = -1;
        this->network_socket = -1;
        this->info_socket = -1;
        this->top_socket = -1;

        setup_sockets();
    }

    void monitor_sockets() {
        keep_running = true;
        struct timeval t;

        while(keep_running) {
            t.tv_sec = 10;

            reset_socket_set();

            if(select(top_socket+1, &socket_set, NULL, NULL, &t) < 0) {
                // TOOO: Throw exception
                // Select failed
                return;
            }

            std::cout << "Selected" << std::endl;

            if(FD_ISSET(control_socket, &socket_set)) {
                std::cout << "Got a control request" << std::endl;
                read_socket(control_socket);
            }

            if(FD_ISSET(network_socket, &socket_set)) {
                std::cout << "Got a network request" << std::endl;
                read_socket(network_socket);
            }

            if(FD_ISSET(info_socket, &socket_set)) {
                std::cout << "Got an info request" << std::endl;
            }
        }
    }

    std::string read_socket(int socket) {
        char buffer[256];
        bzero(buffer, 256);

        if(read(socket, buffer, 255) <= 0) {   
            // TODO: deal with nonsignals
        }

        return buffer;
    }

    void stop() {
        keep_running = false;
    }

private:
    int control_port;
    int control_socket;
    int network_port;
    int network_socket;
    int info_port;
    int info_socket;

    int top_socket;
    fd_set socket_set;

    int keep_running;

    
    void setup_sockets() {
        control_socket = setup_tcp(control_port);
        network_socket = setup_tcp(network_port);
        info_socket = setup_udp(info_port);

        top_socket = std::max(control_socket, network_socket);
        top_socket = std::max(top_socket, info_socket);

        std::cout << control_socket << std::endl;
        std::cout << network_socket << std::endl;
        std::cout << info_socket << std::endl;
    }

    int setup_tcp(int port) {
        struct sockaddr_in serv_addr;
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

        if(fd < 0) {
            // TODO: Throw exception
            // Unable to open socket
        }

        make_reusable(fd);

        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(port);

        if(bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            // TODO: Throw exception
            // Unable to bind socket
        }

        if(listen(fd, 1) < 0) {
            // TODO: Throw exception
            // Unable to listen to socket
        }

        return fd;
    }

    int setup_udp(int port) {
        return -1;
    }

    void make_reusable(int socket) {
        int enable = 1;

        if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
            // TODO: Throw exception
            // could not modify socket
        }
    }

    void reset_socket_set() {
        FD_ZERO(&socket_set);

        // TODO reset client sockets

        // Reset server sockets
        if(control_socket > 0) {
            FD_SET(control_socket, &socket_set);
        }
        else {
            // TODO: Throw exception
            // control_socket invalid
        }

        if(network_socket > 0) {
            FD_SET(network_socket, &socket_set);
        }
        else {
            // TODO: Throw exception
            // network_socket invalid
        }

        if(info_socket > 0) {
            FD_SET(info_socket, &socket_set);
        }
        else {
            // TODO: Throw exception
            // info_socket invalid
        }
    }
};
