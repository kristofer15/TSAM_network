#include <sys/time.h>
#include <ctime>

// Less typing
typedef NetworkHandler::Command Command;
typedef NetworkHandler::Message Message;
typedef NetworkHandler::Server Server;

class MainController {
public:
    MainController(NetworkHandler &network, UserHandler &users, AccessControl &access) :
        network(network), users(users), access(access){
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
            // Request a heartbeat. Has a frequency limit.
            network.heartbeat();

            // Cleanup servers that have timed out
            network.cleanup();

            // Listen for messages
            network.get_message();

            // Read and handle commands
            Command command;
            do {
                command = network.consume_command();

                if(command.from != -1 && command.tokens.size() != 0 && command.role != "") {

                    std::cout << command.tokens[0] << std::endl;
                    std::cout << command.tokens[0].length() << std::endl;
                    handle_command(command);
                }
            }
            while(command.from != -1);
        }
    }

private:

    // Monitor sent tokens that require a response. So it can be handled approppriately
    // TODO: Add sender socket to improve I/O
    struct Response {
        long int timestamp;
        std::vector<std::string> sent_tokens;
        std::vector<std::string> received_tokens;
    };

    NetworkHandler network;
    UserHandler users;
    AccessControl access;
    MessageParser message_parser;
    std::string server_id;
    std::vector<std::string> md5;

    std::map<int, Response> responses;

    std::string handle_command(Command &command) {
        std::string c = command.tokens[0];
        Message m;

        // Check if the command token is whitelisted for this role
        if(!access.permit(command.role, command.tokens[0])) {
            // This appears to be a response

            // Don't message this. Leads to shaming loops.
            return "Operation not permitted/recognized";
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

            // These returns typically aren't picked up.
            // All flow control branches need a return however as
            //  CMD uses some of then.
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
                m.message = "From " + sender + ";";

                for(int i = 2; i < command.tokens.size(); i++) {
                    m.message += " " + command.tokens[i];
                }
            }

            network.message(m);

            // Also notify sender
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
            m.message += "," + network.get_server_ip();
            m.message += "," + std::to_string(network.get_network_port());
            std::cout << "Returning ID: " << m.message << std::endl;
            network.message(m);

            return m.message;
        }
        else if(c == "LISTSERVERS") {
            m.to = command.from;
            m.message = "";
            for(auto const& server: network.get_servers()) {

                // show only one hop servers
                if(server.second.distance == 1) {
                    m.message += server.second.id + ","         + 
                             server.second.ip + ","             +
                             std::to_string(server.second.port) + ";"; 
                }  
            }

            network.message(m);
            return m.message;
        }
        else if(c == "LISTROUTES") {
            // only gives 1-hop servers so far

            m.to = command.from;
            m.message = routing_table();

            network.message(m);
            return m.message;
        }
        else if(c == "ADDSERVER") {
            m.to = command.from;

            if(command.tokens.size() == 3) {
     
                try {
                    std::string ip = command.tokens[1];
                    int port = stoi(command.tokens[2]);
                    Server server = network.connect_to_server(ip, port);

                    request_id(server.socket);

                } catch (std::runtime_error msg) {
                    m.message = msg.what(); 
                }     
            }
            else { m.message = "Invalid number of arguments"; }

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
        else if(c == "KEEPALIVE") {

            // Track server activity
            Server* s = &network.get_server(command.from);
            s->last_comms = network.timestamp();

            return "Heartbeat received";

        }
        else if(c == "CMD") {

            if(command.tokens.size() < 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);
                return m.message;
            }

            // Commands intended for us are indicated with our ID or no id in the second field
            if(command.tokens[1] == "" || command.tokens[1] == server_id) {
                Command delegate;
                delegate.from = -1;
                delegate.role = "network";
                delegate.tokens  = command.delegate_tokens;

                m.to = command.from;

                // Create response header
                m.message = "RSP," + command.tokens[2] + ',' + server_id;

                // Commands sent
                for(auto token : command.delegate_tokens) {
                    m.message += ',' + token;
                }

                // Command result
                m.message += ',' + handle_command(delegate);

                network.message(m);
                return m.message;
            }

            if(forward_command(command)) {
                std::cout << "Command forwarded" << std::endl;
                return "Command forwarded";
            }
            
            std::cout << "Command forwarding failed" << std::endl;
            return "Command forwarding failed";

        }
        else if(c == "RSP") {

            if(!awaiting_response_from(command.from)) {
                std::cout << "Received unwarranted RSP. Possibly forged" << std::endl;
                return "Unwarranted RSP";
            }

            if(command.tokens.size() < 4) {
                m.to = command.from;
                m.message = "Invalid number of arguments";
                network.message(m);

                std::cout << "Received an invalid RSP" << std::endl;
                return m.message;
            }

            // Commands intended for us are indicated with our ID or no id in the second field
            if(command.tokens[1] == "" || command.tokens[1] == server_id) {
                return handle_response(command);
            }

            if(forward_command(command)) {
                std::cout << "Response forwarded" << std::endl;
                return "Response forwarded";
            }

            std::cout << "Response forwarding failed" << std::endl;
            return "Response forwarding failed";
        }
        else if(c == "META_REQUEST_ID") {
            std::cout << "Meta request" << std::endl;
            // std::cout << "Requesting ID from " << command.tokens[1] << std::endl;

            request_id(stoi(command.tokens[1]));
            // std::cout << "ID requested" << std::endl;
            return "ID requested";
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
            // Keep a response struct so that the following RSP is allowed through
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

    // Specifically handle RSP commands
    std::string handle_response(Command &command) {
        std::string c = responses[command.from].sent_tokens[0];

        // Use a pointer so that we can modify the object
        Server* s;

        if(network.contains_server(command.from)) {
            s = &network.get_server(command.from);            
        }
        else {
            return "Received RSP from an unidentified client";
        }

        // This is a response to an ID request. Code gets a little unorganized as the network is unorganized
        // in messaging protocols. We need to handle several common methods of transmitting an ID.
        if(c == "ID") {
            int id_index = 0;

            if(command.delegate_tokens[id_index] == "ID") {
                if(command.delegate_tokens.size() < 2) {

                    // Some people assume you'll pick up the ID from the RSP header...
                    s->id = command.tokens[2];
                    std::cout << "Picked up ID" << std::endl;
                }
                else {

                    // This response is probably formatted "COMMAND,ID,IP,PORT"
                    s->id = command.delegate_tokens[1];

                    // Try to get these if they're missing
                    if(command.delegate_tokens.size() > 2 && (s->ip == "" || s->port == 0)) {
                        s->ip = command.delegate_tokens[id_index+1];
                        s->port = std::stoi(command.delegate_tokens[id_index+2]);
                    }
                }
            }
            else {
                s->id = command.delegate_tokens[0];
            }

            // Response received
            responses.erase(command.from);
        }
        else if(c == "LISTROUTES") {
            // Unable to finish in time :(
            // Have this instead: http://i.imgur.com/0kvtMLE.gif

            // just relays response to all registered clients
            std::vector<int> control_sockets = network.get_control_sockets();
            Message results;
            results.message = "";
            for(auto const& token : command.delegate_tokens) {
                if(token != "LISTROUTES") results.message += token;
            }
            
            for(auto const& socket : control_sockets) {
                if(socket > 0) {
                    results.to = socket;
                    network.message(results);
                }
            }
            // Response received
            responses.erase(command.from);
        }

        return "RECEIVED RESPONSE";
    }

    // Request and ID from another server using a CMD command.
    // Create a response struct for tracking
    void request_id(int socket) {
        responses[socket] = {unix_timestamp(), {"ID"}, {}};

        Message demand_id;
        demand_id.to = socket;
        demand_id.message = "CMD,,"+server_id+",ID";
        network.message(demand_id);
    }

    bool awaiting_response_from(int fd) {
        // TODO: Implement
        // We're currently just trusting the network. If it says an RSP is intended for us,
        //  we assume that it's a response to a command.
        return true;
        // return responses.count(fd) ? true : false;
    }

    long unix_timestamp() {
        time_t t = std::time(0);
        long int now = static_cast<long int> (t);
        return now;
    }

    std::string routing_table() {
        std::string table = server_id + ";";

        // generate header
        for(auto const& server: network.get_servers()) {
            table += server.second.id + ";";           
        }
        table += "\n" + server_id + ";" + "-;";

        // generate routes
        for(auto const& server: network.get_servers()) {
            if(server.second.distance == 1) {
                table += server_id + ";";                           
            }
            else {
                for(int i = 0; i < server.second.intermediates.size(); i++) {
                    if(i > 0) table += "-";
                    table += server.second.intermediates[i];
                }
                table += ";";                         
            }
        }

        // generate timestamp ISO 8601
        std::time_t now = unix_timestamp();
        ctime(&now);
        char buf[sizeof "2011-10-08T07:07:09Z"];
        strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

        // add timestamp
        table += buf;

        return table;
    }
};