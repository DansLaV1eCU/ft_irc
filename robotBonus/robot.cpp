#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

#define WARN_LIMIT 3

std::string ToLower( std::string str ) {
	for ( size_t i = 0; i < str.length(); ++i ) {

		str[i] = static_cast<char>( std::tolower( static_cast<unsigned char>(str[i]) ) );
	}
	return ( str );
}

bool EqualsIgnoreCase( const std::string& a, const std::string& b ) {
	return ( ToLower(a) == ToLower(b) );
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

bool SendData( int sock, const std::string& data ) {
	size_t sent = 0;

	while ( sent < data.length() ) {
		ssize_t written = send(sock, data.c_str() + sent, data.length() - sent, 0);

		if ( written <= 0 ) {
			return ( false );
		}
		sent += static_cast<size_t>(written);
	}
	return ( true );
}

int ConnectTo( const std::string& host, const std::string& port ) {
	struct addrinfo hints;
	struct addrinfo* result = NULL;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ( getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0 || result == NULL ) {
		std::cerr << "Error: cannot resolve " << host << std::endl;
		return ( -1 );
	}

	int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if ( sock < 0 ) {
		std::cerr << "Error: socket creation failed" << std::endl;
		freeaddrinfo(result);
		return ( -1 );
	}
	if ( connect(sock, result->ai_addr, result->ai_addrlen) < 0 ) {
		std::cerr << "Error: connection to " << host << ":" << port << " failed" << std::endl;
		close(sock);
		freeaddrinfo(result);
		return ( -1 );
	}

	freeaddrinfo(result);
	return ( sock );
}

struct Message {
	std::string sender;
	std::string command;
	std::vector<std::string> params;
};

Message ParseLine( const std::string& line ) {
	Message msg;
	std::string rest = line;

	if ( !rest.empty() && rest[0] == ':' ) {
		std::string::size_type space = rest.find(' ');

		if ( space == std::string::npos ) {
			return ( msg );
		}

		std::string prefix = rest.substr(1, space - 1);
		std::string::size_type excl = prefix.find('!');

		msg.sender = ( excl == std::string::npos ) ? prefix : prefix.substr(0, excl);
		rest = rest.substr(space + 1);
	}

	std::string head = rest;
	std::string trailing;
	bool hasTrailing = false;
	std::string::size_type colon = rest.find(" :");

	if ( colon != std::string::npos ) {
		head = rest.substr(0, colon);
		trailing = rest.substr(colon + 2);
		hasTrailing = true;
	}

	std::istringstream stream(head);
	std::string token;
	while ( stream >> token ) {
		msg.params.push_back(token);
	}
	if ( hasTrailing ) {
		msg.params.push_back(trailing);
	}

	if ( !msg.params.empty() ) {
		msg.command = ToLower(msg.params[0]);
		for ( size_t i = 0; i < msg.command.size(); ++i ) {
			msg.command[i] = static_cast<char>( std::toupper( static_cast<unsigned char>(msg.command[i]) ) );
		}
		msg.params.erase(msg.params.begin());
	}

	return ( msg );
}

int main( int argc, char **argv ) {
	if ( argc != 5 ) {
		std::cerr << "Usage: ./modbot <host> <port> <password> <#channel>" << std::endl;
		return ( 1 );
	}

	std::string host = argv[1];
	std::string port = argv[2];
	std::string password = argv[3];
	std::string channel = argv[4];
	std::string botName = "ModBot";

	if ( channel.empty() || channel[0] != '#' ) {
		std::cerr << "Error: channel must start with '#'" << std::endl;
		return ( 1 );
	}

	int sock = ConnectTo(host, port);
	if ( sock < 0 ) {
		return ( 1 );
	}

	if ( !SendData(sock, "PASS " + password + "\r\n")
		|| !SendData(sock, "NICK " + botName + "\r\n")
		|| !SendData(sock, "USER " + botName + " 0 * :I am ModBot\r\n") ) {
		std::cerr << "Error: registration failed" << std::endl;
		close(sock);
		return ( 1 );
	}

	std::map<std::string, int> userWarnings;
	std::string dataBuffer;
	char buffer[4096];
	bool registered = false;
	bool warnedAboutOps = false;
	bool running = true;

	while ( running ) {
		std::memset(buffer, 0, sizeof(buffer));

		ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if ( bytes_received <= 0 ) {
			std::cerr << "Disconnected from server." << std::endl;
			break ;
		}
		dataBuffer.append(buffer, static_cast<size_t>(bytes_received));

		std::string::size_type pos;
		while ( running && (pos = dataBuffer.find('\n')) != std::string::npos ) {
			std::string line = dataBuffer.substr(0, pos);

			dataBuffer.erase(0, pos + 1);
			while ( !line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n') ) {
				line.erase(line.size() - 1);
			}
			if ( line.empty() ) {
				continue ;
			}

			Message msg = ParseLine(line);

			if ( msg.command == "PING" ) {
				SendData(sock, "PONG :" + ( msg.params.empty() ? std::string("server") : msg.params[0] ) + "\r\n");
				continue ;
			}
			if ( msg.command == "ERROR" ) {
				std::cerr << "Server closed the link: " << ( msg.params.empty() ? "" : msg.params[0] ) << std::endl;
				running = false;
				continue ;
			}

			if ( msg.command == "001" ) {
				registered = true;
				SendData(sock, "JOIN " + channel + "\r\n");
				continue ;
			}

			if ( msg.command == "433" && !registered ) {
				botName += "_";
				if ( botName.length() > 20 ) {
					std::cerr << "Error: cannot find a free nickname" << std::endl;
					running = false;
					continue ;
				}
				std::cerr << "Nickname in use, retrying as " << botName << std::endl;
				SendData(sock, "NICK " + botName + "\r\n");
				continue ;
			}

			if ( msg.command == "464" ) {
				std::cerr << "Error: password incorrect" << std::endl;
				running = false;
				continue ;
			}

			if ( msg.command == "482" ) {
				std::cerr << "Warning: " << botName << " is not a channel operator, cannot kick."
						  << " Run \"/mode " << channel << " +o " << botName << "\" to enable kicking."
						  << std::endl;
				if ( !warnedAboutOps ) {
					warnedAboutOps = true;
					SendData(sock, "PRIVMSG " + channel + " :I need operator status to enforce warnings ("
						+ "/mode " + channel + " +o " + botName + ")\r\n");
				}
				continue ;
			}

			if ( msg.command == "403" || msg.command == "473" || msg.command == "475" || msg.command == "471" ) {
				std::cerr << "Error: cannot join " << channel << " ("
						  << ( msg.params.empty() ? "" : msg.params[msg.params.size() - 1] ) << ")" << std::endl;
				running = false;
				continue ;
			}

			if ( msg.command == "NICK" && !msg.params.empty() ) {
				std::string oldNick = msg.sender;
				std::string newNick = msg.params[0];

				if ( EqualsIgnoreCase(oldNick, botName) ) {
					botName = newNick;
				} else if ( userWarnings.count(ToLower(oldNick)) != 0 ) {
					userWarnings[ToLower(newNick)] = userWarnings[ToLower(oldNick)];
					userWarnings.erase(ToLower(oldNick));
				}
				continue ;
			}

			if ( msg.command == "KICK" && msg.params.size() >= 2 && EqualsIgnoreCase(msg.params[1], botName) ) {
				std::cerr << "Kicked from " << channel << ", leaving." << std::endl;
				running = false;
				continue ;
			}

			if ( msg.command != "PRIVMSG" || msg.params.size() < 2 ) {
				continue ;
			}

			std::string target = msg.params[0];
			std::string message = msg.params[1];

			if ( msg.sender.empty() || EqualsIgnoreCase(msg.sender, botName) ) {
				continue ;
			}
			if ( !EqualsIgnoreCase(target, channel) || !ContainsBadWord(message) ) {
				continue ;
			}

			std::string key = ToLower(msg.sender);
			int warnings = ++userWarnings[key];

			if ( warnings < WARN_LIMIT ) {
				std::stringstream ss;

				ss << "PRIVMSG " << channel << " :" << msg.sender
				   << ", please do not use bad words. Warning " << warnings << "/" << WARN_LIMIT << "\r\n";
				SendData(sock, ss.str());
			} else {
				std::stringstream ss;

				ss << "KICK " << channel << " " << msg.sender << " :"
				   << WARN_LIMIT << "/" << WARN_LIMIT << " warnings. Bye!\r\n";
				SendData(sock, ss.str());
				userWarnings.erase(key);
			}
		}
	}

	close(sock);
	return ( 0 );
}
