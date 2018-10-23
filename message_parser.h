#include <iostream>
#include <sstream>
#include <vector>

class MessageParser {

public:

    MessageParser() { }

    std::vector<std::string> tokenize(std::string message) {
        std::stringstream stream(message);
        std::string token;
        std::vector<std::string> tokens;

        // sstream from the assignments example server:
        while (stream >> token) {
            tokens.push_back(token);
        }

        return tokens;
    }
};