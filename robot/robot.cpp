/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   robot.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/24 00:53:18 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/24 00:53:19 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <string>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <sstream>

std::string ToLower( std::string str ) {
    for ( size_t i = 0; i < str.length(); ++i ) {
        str[i] = std::tolower(str[i]);
    }
    return ( str );
}

bool ContainsBadWord( const std::string& message ) {
    std::string lowerMsg = ToLower(message);
    const char* badWords[] = {"fuck", "blyat", "kurwa"};
    size_t wordsCount = sizeof(badWords) / sizeof(badWords[0]);

    for ( size_t i = 0; i < wordsCount; ++i ) {
        if ( lowerMsg.find(badWords[i]) != std::string::npos ) {
            return ( true );
        }
    }
    return ( false );
}

void SendData( int sock, const std::string &data ) {
    send(sock, data.c_str(), data.length(), 0);
}

int main( int argc, char **argv ) {
    if ( argc != 5 ) {
        std::cerr << "Usage: ./bot <IP> <PORT> <PASSWORD> <#CHANNEL>" << std::endl;
        return (1);
    }

    std::string ip = argv[1];
    int port = std::atoi(argv[2]);
    std::string password = argv[3];
    std::string channel = argv[4];
    std::string botName = "ModBot";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
        std::cerr << "Error: Socket creation failed" << std::endl;
        return (1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if ( inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0 ) {
        std::cerr << "Error: Invalid address" << std::endl;
        return (1);
    }

    if ( connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ) {
        std::cerr << "Error: Connection failed" << std::endl;
        return (1);
    }

    SendData(sock, "PASS " + password + "\r\n");
    SendData(sock, "NICK " + botName + "\r\n");
    SendData(sock, "USER " + botName + " 0 * :I am ModBot\r\n");
    SendData(sock, "JOIN " + channel + "\r\n");

    std::map<std::string, int> userWarnings;
    char buffer[4096];
    std::string dataBuffer = "";

    while ( true ) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if ( bytes_received <= 0 ) {
            std::cerr << "Disconnected from server." << std::endl;
            break;
        }

        dataBuffer += buffer;
        size_t pos;

        while ( (pos = dataBuffer.find("\r\n")) != std::string::npos ) {
            std::string line = dataBuffer.substr(0, pos);
            dataBuffer.erase(0, pos + 2);

            if ( line.find("PING ") == 0 ) {
                std::string token = line.substr(5);
                SendData(sock, "PONG " + token + "\r\n");
                continue;
            }

            size_t privmsgPos = line.find(" PRIVMSG ");
            if ( privmsgPos != std::string::npos ) {
                size_t exclPos = line.find('!');
                if ( line[0] != ':' || exclPos == std::string::npos ) {
					continue;
				}

                std::string sender = line.substr(1, exclPos - 1);

                if ( sender == botName ) {
					continue;
				}

                size_t msgStart = line.find(" :", privmsgPos);
                if ( msgStart == std::string::npos ) {
					continue;
				}

                std::string target = line.substr(privmsgPos + 9, msgStart - (privmsgPos + 9));
                std::string message = line.substr(msgStart + 2);

                if ( target == channel && ContainsBadWord(message) ) {
                    userWarnings[sender]++;
                    int warnings = userWarnings[sender];

                    if ( warnings < 3 ) {
                        std::stringstream ss;
                        ss << "PRIVMSG " << channel << " :" << sender 
                           << ", please do not use bad words. Warning " << warnings << "/3\r\n";
                        SendData(sock, ss.str());
                    } else {
                        SendData(sock, "KICK " + channel + " " + sender + " :3/3 warnings. Bye!\r\n");
                        userWarnings[sender] = 0;
                    }
                }
            }
        }
    }

    close(sock);
    return ( 0 );
}