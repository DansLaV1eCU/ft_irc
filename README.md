*This project has been created as part of the 42 curriculum by danslav1e, dtereshc and olcherno.*

# ft_irc

## Description

ft_irc is a basic IRC server written in C++98. The goal of the project is to understand how IRC-style communication works and to implement a server that can be used with a standard IRC client.

This implementation uses non-blocking sockets and `poll()` to manage multiple clients. It supports client registration and a minimal set of IRC commands, including authentication, nickname and username setup, channel join/management, private messages, and basic operator-only channel actions.

### Implemented commands

- `PASS`
- `NICK`
- `USER`
- `JOIN`
- `PRIVMSG`
- `TOPIC`
- `INVITE`
- `KICK`
- `MODE` with `+i`, `-i`, `+t`, `-t`, `+k`, `-k`, `+o`, `-o`, `+l`, `-l`

## Instructions

### Compilation

Build the project with:

```bash
make
```

The executable is named `ircserv`.

To remove the object files directory and the executable:

```bash
make clean
make fclean
```

To rebuild everything from scratch:

```bash
make re
```

### Execution

Run the server with a port and a password:

```bash
./ircserv <port> <password>
```

Example:

```bash
./ircserv 6667 12345
```

Then connect using a reference IRC client such as Irssi, HexChat, or another standard IRC client.

## Resources

### References

- IRCv3 documentation: https://ircv3.net/
- RFC 1459 - Internet Relay Chat Protocol: https://www.rfc-editor.org/rfc/rfc1459
- RFC 2811 - Internet Relay Chat: Channel Management: https://www.rfc-editor.org/rfc/rfc2811
- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/
- Linux `poll(2)` manual: https://man7.org/linux/man-pages/man2/poll.2.html

### AI usage

AI assistance was used to:

- draft the README structure required by the subject,
- summarize the current implementation and build workflow,
- help phrase the instructions and resources section in clear English.

The codebase itself was not generated from scratch by AI; the final content was reviewed and adjusted to match the project files and build commands.