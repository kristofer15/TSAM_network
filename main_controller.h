#include <sys/time.h>

// Less typing
typedef NetworkHandler::Command Command;
typedef NetworkHandler::Message Message;

class MainController {
public:
    MainController(NetworkHandler &network, UserHandler &users, AccessControl &access) :
        network(network), users(users), access(access) {
            this->server_id = "V_Group_51";
            md5 = {"1", "2", "3", "4", "5"};
    }

    ~MainController() {}

    void run() {
        while(true) {
            Command command = network.get_message();

            if(command.tokens.size() <= 0 || command.role == "") {
                continue;
            }
            match_command(command);
        }
    }

private:

    struct Response {
        long int timestamp;
        std::vector<std::string> sent_tokens;
        std::vector<std::string> received_tokens;
    };

    NetworkHandler network;
    UserHandler users;
    AccessControl access;
    std::string server_id;
    std::vector<std::string> md5;

    std::map<int, Response> responses;

    void match_command(Command &command) {
        std::string c = command.tokens[0];
        Message m;

        // Check if the command token is whitelisted for this role
        if(!access.permit(command.role, command.tokens[0])) {
            Message m;
            m.to = command.from;
            m.message = "Operation not permitted/recognized";
            network.message(m);
            return;
        }

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
            m2.to = command.from;
            m2.message = "Message sent";
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

            if(users.remove_user(command.from) == "") {
                m.message = "Already disconnected";
            }
            else {
                m.message = "Leaving";
            }

            network.message(m);
            return;
        }
        else if(c == "ID") {
            m.to = command.from;
            m.message = server_id;
            network.message(m);
            return;
        }
        else if(c == "FETCH") {
            m.to = command.from;

            if(command.tokens.size() == 2) {
                int i = stoi(command.tokens[1]);
                if(i < 1 || i > 5) {
                    m.message = "Index out of range";
                }
                else {
                    m.message = md5[i-1];
                }
            }
            else {
                m.message = "Invalid number of arguments";
            }

            network.message(m);
            return;
        }
        else if(c == "CMD") {
            // SPECIAL. Requires a response

            // If token 1 is not a known server ID, assume that it is a command meant for us

            // Commands meant for us need to be stored so a response is handled properly
        }
        else {  // Don't go here. Validate first.
            std::cout << "Command not implemented" << std::endl;
        }
    }
};