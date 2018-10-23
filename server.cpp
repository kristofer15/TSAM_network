#include "network_handler.h"
#include "access_control.h"

// Less typing
typedef NetworkHandler::Command Command;

void help(std::string file_name="server") {
    std::cout << "Usage:" << std::endl
              << file_name << " {TCP PORT} {UDP PORT}" << std::endl;
}

void match_command(Command &command) {
    std::string c = command.tokens[0];

    if(c == "CONNECT") {

    } 
    else if(c == "WHO") {

    }
    else if(c == "MSG") {

    }
    else if(c == "LEAVE") {

    }
    else {
        std::cout << "Command not recognized" << std::endl;
    }
}

int main(int argc, char* argv[]) {

    if(argc < 3) {
        help(argv[0]);
        return 0;
    }

    NetworkHandler handler(atoi(argv[1]), atoi(argv[2]));
    AccessControl access;

    while(true) {
        NetworkHandler::Command command = handler.get_message();

        if(command.tokens.size() <= 0 || command.role == "") {
            continue;
        }

        if(access.permit(command.role, command.tokens[0])) {
            std::cout << "Access granted" << std::endl;
            match_command(command);
        }
    }
}
