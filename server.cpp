#include "network_handler.h"
#include "user_handler.h"
#include "access_control.h"

// Less typing
typedef NetworkHandler::Command Command;
typedef NetworkHandler::Message Message;

void help(std::string file_name="server") {
    std::cout << "Usage:" << std::endl
              << file_name << " {TCP PORT} {UDP PORT}" << std::endl;
}

void match_command(Command &command, NetworkHandler &network, UserHandler &users) {
    std::string c = command.tokens[0];
    Message m;

    if(c == "CONNECT") {
        m.to = command.from;

        if(command.tokens.size() != 2) {
            m.message = "Invalid number of arguments";
        }
        else if(users.connected(command.from)) {
            m.message = "You are already connected";
        }
        else if(users.id(command.tokens[1])) {
            m.message = "Name already taken";
        }
        else if(users.connect_user(command.from, command.tokens[1])) {
            m.message = "Connected as " + command.tokens[1];
            std::cout << command.tokens[1] << " connected" << std::endl;
        }
        else {
            m.message = "Failed to connect";
            std::cout << "Failed to connect";
        }

        network.message(m);
        return;
    }
    else if(c == "MSG") {
        if(command.tokens.size() < 2) {
            m.to = command.from;
            m.message = "Missing recipient and/or message";
            network.message(m);
            return;
        }

        std::string sender = users.user_name(command.from);
        if(sender == "") {
            m.to = command.from;
            m.message = "You must connect before sending messages";
            network.message(m);
            return;
        }

        int recipient = users.id(command.tokens[1]);
        if(!recipient) {
            m.to = command.from;
            m.message = "No such user";
        }
        else {

            m.to = recipient;
            m.message = "From " + sender + ":";

            for(int i = 2; i < command.tokens.size(); i++) {
                m.message += " " + command.tokens[i];
            }
        }

        network.message(m);

        Message m2;
        m.to = command.from;
        m.message = "Message sent";
        network.message(m);
        return;
    } 
    else if(c == "WHO") {
        m.to = command.from;
        m.message = "";

        auto user_list = users.user_list();
        for(int i = 0; i < user_list.size(); i++) {

            if(i != 0) {
                m.message += "\n";
            }

            m.message += user_list[i];
        }

        network.message(m);
        return;
    }
    else if(c == "LEAVE") {
        m.to = command.from;
        m.message = users.remove_user(command.from);

        if(m.message == "") {
            m.message = "Already disconnected";
        }

        network.message(m);
        return;
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

    NetworkHandler network(atoi(argv[1]), atoi(argv[2]));
    UserHandler users;
    AccessControl access;

    while(true) {
        Command command = network.get_message();

        if(command.tokens.size() <= 0 || command.role == "") {
            continue;
        }
        if(access.permit(command.role, command.tokens[0])) {
            match_command(command, network, users);
        }
        else {
            // TODO: Move to match_command
            Message m;
            m.to - command.from;
            m.message = "Operation not permitted/recognized";
            network.message(m);
        }
    }
}
