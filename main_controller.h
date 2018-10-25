#include <sys/time.h>
#include <ctime>

// Less typing
typedef NetworkHandler::Command Command;
typedef NetworkHandler::Message Message;
typedef NetworkHandler::Server Server;

class MainController {
public:
    MainController(NetworkHandler &network, UserHandler &users, AccessControl &access) :
        network(network), users(users), access(access) {
            this->server_id = "V_Group_51";
            this->md5 = {
                "ca23ba209cc33678530392b7197fda4d",
                "a3eaaf35761efa2a09437854e2caf4b3",
                "f0f9b8ff2096179b21848c8ffeca7c10",
                "be5d5d37542d75f93a87094459f76678",
                "6f96cfdfe5ccc627cadf24b41725caa4"};
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

    long unix_timestamp()
    {
        time_t t = std::time(0);
        long int now = static_cast<long int> (t);
        return now;
    }

    std::string handle_command(Command &command) {
        std::string c = command.tokens[0];
        Message m;

        // Check if the command token is whitelisted for this role
        if(!access.permit(command.role, command.tokens[0])) {
            // This appears to be a response
            if(awaiting_response_from(command.from)) {
                handle_response(command);
                return "Not implemented";
            }
            else {
                Message m;
                m.to = command.from;
                m.message = "Operation not permitted/recognized";
                network.message(m);
                return m.message;
            }
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

            std::cout << "ID command from: " << command.from << std::endl;

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
            m.to = command.from;

            if(command.tokens.size() == 3) {
                // TODO: connect_to_server should take address info and return a server struct
                // TODO: Only succeed once ID has been returned. Needs to be async. Don't expect a response here
                
                try {
                    std::string ip = command.tokens[1];
                    int port = stoi(command.tokens[2]);
                    Server server = network.connect_to_server(ip, port);

                    responses[server.socket] = {unix_timestamp(), {"ID"}, {}};

                    Message demand_id = {server.socket, "CMD,,server_id,ID"};
                    demand_id.to = server.socket;
                    demand_id.message = "CMD,,"+server_id+",ID";
                    network.message(demand_id);

                    m.message = "Successfully connected to: " + 
                                server.ip + " "               + 
                                std::to_string(server.port);

                } catch (std::runtime_error msg) {
                    m.message = msg.what(); 
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

            if(command.tokens.size() < 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);
                return m.message;
            }

            // Commands intended for are indicated with our ID or no id in the second field
            if(command.tokens[1] == "" || command.tokens[1] == server_id) {
                Command delegate;
                delegate.from = -1;
                delegate.role = "network";
                delegate.tokens  = command.delegate_tokens;

                m.to = command.from;

                m.message = "RSP," + command.tokens[2] + "," + server_id + "," + handle_command(delegate);
                network.message(m);
                return m.message;
            }

            if(forward_command(command)) {
                std::cout << "Command forwarded" << std::endl;
                return "Command forwarded";
            }
            else {
                std::cout << "Command forwarding failed" << std::endl;
            }
        }
        else if(c == "RSP") {
            if(command.tokens.size() < 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);

                std::cout << "Received an invalid RSP" << std::endl;
                return m.message;
            }

            // Commands intended for are indicated with our ID or no id in the second field
            if(command.tokens[1] == "" || command.tokens[1] == server_id) {
                return handle_response(command);
            }

            if(forward_command(command)) {
                std::cout << "Response forwarded" << std::endl;
                return "Response forwarded";
            }
            else {
                std::cout << "Response forwarding failed" << std::endl;
            }
        }
        else {  // Don't go here. Validate first.
            std::cout << "Command not implemented" << std::endl;
            return "Command not implemented";
        }
    }

    bool forward_command(Command &command) {
        Message m;

        // Forward the message
        Server target = network.find_server(command.tokens[1]);
        if(target.id != "") {
            m.to = target.socket;

            // We don't have a direct connection
            if(target.distance > 1) {

                // Delegate to the first intermediate (1-hop)
                m.to = network.find_server(target.intermediates[0]).socket;
            }

            m.message = command.raw;
            network.message(m);

            // We need a response
            if(command.tokens[0] == "CMD") {
                Response response;
                response.timestamp = -1;
                response.sent_tokens = command.delegate_tokens;
                responses[m.to] = response;
            }

            // Message forwarded
            return true;
        }
        else {
            // Target server not found
            return false;
        }
    }

    bool awaiting_response_from(int fd) {
        return responses.count(fd) ? true : false;
    }

    std::string handle_response(Command &command) {
        std::string c = responses[command.from].sent_tokens[0];
        for(auto token : command.tokens) {
            std::cout << token << std::endl;
        }
        std::cout << "RECEIVED RESPONSE" << std::endl;
        return "RECEIVED RESPONSE";
    }
};