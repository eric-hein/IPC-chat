# IPC-chat
A basic implementation of a chat server and clients using inter-process communciation

This chat interface utilizes non-blocking communication to run a server for a host user and multiple clients to send chat messages over. The relay server implementation contains a Monitor process, which it is connected to by 2 pipes, one for each direction. The server is then connected by TCP/IP sockets for communicating with all clients.

Both programs must be called with hostname defined using the -h <hostname> command line argument. Though a default port is set, this can also be overridden with -p <portnumber>.
