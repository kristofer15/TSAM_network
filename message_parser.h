#include <iostream>
#include <sstream>
#include <vector>

class MessageParser {

public:

    MessageParser() { }

    std::vector<std::string> tokenize(std::string message, char delimiter=' ') {

        // sstream from the assignments example server:
        std::stringstream stream(message);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(stream, token, delimiter)) {
            tokens.push_back(token);
        }

        return tokens;
    }
};