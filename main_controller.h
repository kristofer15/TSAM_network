#include <sys/time.h>

// Less typing
typedef NetworkHandler::Command Command;
typedef NetworkHandler::Message Message;
typedef NetworkHandler::Server Server;

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

            if(command.tokens.size() == 0 || command.role == "") {
                continue;
            }
            handle_command(command);
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

    std::string handle_command(Command &command) {
        std::string c = command.tokens[0];
        Message m;

        bool awaited = awaiting_response_from(command.from);

        // Check if the command token is whitelisted for this role
        if(!access.permit(command.role, command.tokens[0]) && !awaited) {
            Message m;
            m.to = command.from;
            m.message = "Operation not permitted/recognized";
            network.message(m);
            return m.message;
        }
        else if(awaited) {
            // This non-command is from a server from which a response is due
            handle_response(command);
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
            return m.message;
        }
        else if(c == "MSG") {
            if(command.tokens.size() < 2) {
                m.to = command.from;
                m.message = "Missing recipient and/or message";
                network.message(m);
                return m.message;
            }

            std::string sender = users.user_name(command.from);
            if(sender == "") {
                m.to = command.from;
                m.message = "You must connect before sending messages";
                network.message(m);
                return m.message;
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
            return m.message;
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
            return m.message;
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
            return m.message;
        }
        else if(c == "ID") {
            m.to = command.from;
            m.message = server_id;
            network.message(m);
            return m.message;
        }
        else if(c == "LISTSERVERS") {

            m.to = command.from;
            m.message = "";
            for(auto const& server: network.get_servers()) {
                m.message += server.second.id + ","                 + 
                             server.second.ip + ","             +
                             std::to_string(server.second.port) + ";";   
            }

            network.message(m);
            return m.message;
        }
        else if(c == "ADDSERVER") {
            // TODO responsibility of main controller concering adding servers
            m.to = command.from;

            if(command.tokens.size() == 3) {
                Server server = {"", command.tokens[1], stoi(command.tokens[2])};
                
                if(network.connect_to_server(server)){
                    m.message = "Successfully connected to: " + 
                                server.ip + " "               + 
                                std::to_string(server.port);
                }
                else {
                    m.message = "Unable to connect server"; 
                }
            }
            else {
                m.message = "Invalid number of arguments";
                
            }

            network.message(m);
            return m.message;
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
            return m.message;
        }
        else if(c == "CMD") {
            std::string response_message;

            // Commands intended for us are indicated wtih 3 tokens or
            // 4 tokens where the second is our ID
            if(command.tokens.size() == 3 || (command.tokens.size() == 4 && command.tokens[1] == server_id)) {
                Command delegate;
                delegate.from = -1;
                delegate.role = "network";
                delegate.tokens  = command.delegate_tokens;

                m.to = command.from;
                response_message = handle_command(delegate);
            }
            else if(command.tokens.size() != 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);
                return m.message;
            }

            // TODO: Forward message

            m.message = "RSP," + command.tokens[1] + "," + server_id + "," + response_message; 
            network.message(m);
            return m.message;

            // If token 1 is not a known server ID, assume that it is a command meant for us

            // Commands meant for us need to be stored so a response is handled properly
        }
        else if(c == "RSP") {
            // Responses intended for us are indicated with 3 tokens or
            // 4 tokens where the second is our ID
            if(command.tokens.size() == 3 || (command.tokens.size() == 4 && command.tokens[1] == server_id)) {
                handle_response(command);
            }
            else if(command.tokens.size() != 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);

                std::cout << "Received an invalid RSP" << std::endl;
                return m.message;
            }

            // TODO: Forward 
            return "Response procesed";
        }
        else {  // Don't go here. Validate first.
            std::cout << "Command not implemented" << std::endl;
            return "Command not implemented";
        }
    }

    bool awaiting_response_from(int fd) {
        return responses.count(fd) ? true : false;
    }

    void handle_response(Command &command) {
        std::string c = responses[command.from].sent_tokens[0];        
        std::cout << "RECEIVED RESPONSE" << std::endl;
    }
};