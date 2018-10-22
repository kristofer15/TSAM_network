#include "network_handler.h"

void help(std::string file_name="server") {
    std::cout << "Usage:" << std::endl
              << file_name << " {TCP PORT} {UDP PORT}" << std::endl;
}


int main(int argc, char* argv[]) {

    if(argc < 3) {
        help(argv[0]);
        return 0;
    }

    NetworkHandler handler(atoi(argv[1]), atoi(argv[2]));

    handler.monitor_sockets();
}

int generate_key(std::string& role) {

    if(role == "control") {
        return 0b100;
    }
    else if(role == "network") {
        return 0b010;
    }
    else if(role == "info") {
        return 0b001;
    }
    else {
        return 0b000;
    }
}