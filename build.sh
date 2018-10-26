cd "$(dirname "$0")"
mkdir -p bin/
g++ -std=c++11 -pthread server.cpp -o bin/tsamvgroup51
