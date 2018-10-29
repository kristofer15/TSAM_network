# TSAM Network
A botnet server which implements a small API for receiving/sending messages between other servers in the botnetn.  
This is an assigment for the course Computer Networks (TSAM) in Reykjavik University.  
API is listed in the project's description which is not present in the repository.

## Development

All development was done on **Manjaro Linux** OS.

## Compiling

Compiling can be done using the *build.sh* script

```
// make sure you are in root of repository
cd TSAM_network/

// make sure build.sh is executable if it's not already
chmod +x build.sh

// run build script
./build.sh
```

## Running the server

After compiling you can run the server using the following command

```
// again make sure you are in root of repository
cd TSAM_network/

// run the server
./bin/server <TCP port> <UDP port>
```
## Client commands

Connecting to another server using NetCat
```
// server's client port hardcoded to 4050
ncat <server IP/domain name> 4050 
```


Connecting server to another server on the network.
```
// make sure you are connected as a client to a botnet server
ADDSERVER <server IP/domain name> <open tcp port>
```
