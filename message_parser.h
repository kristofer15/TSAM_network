#include <iostream>
#include <sstream>
#include <vector>

class MessageParser {

public:
    struct Command {
        // from/to?
        std::string role;
        std::vector<std::string> tokens;
    };

    MessageParser() { }

    Command parse(const std::string role, std::string message) {
        std::stringstream stream(message);
        std::string token;
        std::vector<std::string> tokens;

        // sstream from the assignments example server:
        while (stream >> token) {
            tokens.push_back(token);
        }

        Command command;
        command.role = role;
        command.tokens = tokens;

        return command;
    }
};