# ![FuqTuP](ftp.png)

File Transfer Protocol Server / Client implementation using socket programming. Made as a part of Networks Lab. Refer to the problem statement [here](Problem_Statement.pdf).

## Features

* One server can handle multiple clients, on the same network. Transfer between two systems on the same network with ease.
* Simultaneous download to 2 different clients possible.
* Can handle any type of file - pdf, text, video, audio.
* Client can force exit with user interrupt (Ctrl+C)


The following commands are available on the client side:

* `fput filename` - upload a file to the server. Multiple clients can connect to a server.
* `fget filename` - Fetch a file uploaded / already available on the server.
* `servls` - Get a list of files available on the server location.
* `servcd loc` - Change directory at location.
* `servpwd` - Get present working directory at server.
* `clils` - Read files at current directory for client.
* `clicd loc` - Change directory at client.
* `clipwd` - Get present working directory at client.
* `quit` - Close the connection.
* `clear` - clear the screen for client.

## Usage

### Server

```
gcc ftp_server.cpp -o server
./server portnumber verbose
```
verbose is an optional parameter to print details of the operations.

### Client

```
gcc ftp_client.cpp -o client
./client server_ip portnumber
```

## Members
* Surya Midatala [@kingofools](https://github.com/kingofools)
* Kaustubh Hiware [@kaustubhhiware](https://github.com/kaustubhhiware)
