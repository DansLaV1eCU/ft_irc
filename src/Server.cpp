#include "../includes/Server.hpp"
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cerrno>

volatile sig_atomic_t Server::Signal = 0;

namespace
{

std::vector<std::string> SplitParams( const std::string &line ) {
	std::vector<std::string> tokens;
	std::string rest = line;

	if ( !rest.empty() && rest[0] == ':' ) {
		std::string::size_type space = rest.find(' ');

		if ( space == std::string::npos ) {
			return ( tokens );
		}
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
		tokens.push_back(token);
	}
	if ( hasTrailing ) {
		tokens.push_back(trailing);
	}

	return ( tokens );
}

std::vector<std::string> SplitList( const std::string &value, char separator ) {
	std::vector<std::string> items;
	std::string current;

	for ( size_t i = 0; i < value.size(); ++i ) {
		if ( value[i] == separator ) {
			if ( !current.empty() ) {
				items.push_back(current);
			}
			current.clear();
		} else {
			current += value[i];
		}
	}
	if ( !current.empty() ) {
		items.push_back(current);
	}

	return ( items );
}

std::string ToUpper( const std::string &value ) {
	std::string result = value;

	for ( size_t i = 0; i < result.size(); ++i ) {
		result[i] = static_cast<char>( std::toupper( static_cast<unsigned char>(result[i]) ) );
	}
	return ( result );
}

bool EqualsIgnoreCase( const std::string &a, const std::string &b ) {
	if ( a.size() != b.size() ) {
		return ( false );
	}
	for ( size_t i = 0; i < a.size(); ++i ) {
		if ( std::tolower( static_cast<unsigned char>(a[i]) ) != std::tolower( static_cast<unsigned char>(b[i]) ) ) {
			return ( false );
		}
	}
	return ( true );
}

std::string TrimCarriageReturn( const std::string &line ) {
	std::string result = line;

	while ( !result.empty() && (result[result.size() - 1] == '\n' || result[result.size() - 1] == '\r') ) {
		result.erase(result.size() - 1);
	}
	return ( result );
}

std::string JoinFrom( const std::vector<std::string> &tokens, size_t start ) {
	std::string result;

	for ( size_t i = start; i < tokens.size(); ++i ) {
		if ( i > start ) {
			result += " ";
		}
		result += tokens[i];
	}

	return ( result );
}

std::string ToString( long value ) {
	std::ostringstream stream;

	stream << value;
	return ( stream.str() );
}

bool IsValidChannelName( const std::string &name ) {
	if ( name.size() < 2 || name.size() > 50 || name[0] != '#' ) {
		return ( false );
	}
	for ( size_t i = 1; i < name.size(); ++i ) {
		unsigned char c = static_cast<unsigned char>(name[i]);

		if ( c <= 32 || c == 127 || name[i] == ',' || name[i] == ':' ) {
			return ( false );
		}
	}
	return ( true );
}

bool IsValidNickname( const std::string &nick ) {
	const std::string special = "[]\\`_^{|}";

	if ( nick.empty() || nick.size() > 30 ) {
		return ( false );
	}
	if ( !std::isalpha( static_cast<unsigned char>(nick[0]) ) && special.find(nick[0]) == std::string::npos ) {
		return ( false );
	}
	for ( size_t i = 1; i < nick.size(); ++i ) {
		unsigned char c = static_cast<unsigned char>(nick[i]);

		if ( std::isalnum(c) || nick[i] == '-' || special.find(nick[i]) != std::string::npos ) {
			continue ;
		}
		return ( false );
	}
	return ( true );
}
}

Server::Server( void ) : _port(6667), _password("password"), _serverFd(-1) {

}

Server::Server( int port, const std::string &password ) : _port( port ), _password(password), _serverFd(-1) {

}

Server::Server( const Server &other ) {
	*this = other;
}

Server &Server::operator=( const Server &other ) {
	if ( this != &other ) {
		_port = other._port;
		_password = other._password;
		_serverFd = other._serverFd;
		clients = other.clients;
		channels = other.channels;
		fds = other.fds;
	}

	return ( *this );
}

Server::~Server( void ) {
	CloseFds();
}

int Server::getServerFd( void ) const {
	return (_serverFd);
}

int Server::getPort( void ) const {
	return (_port);
}

void Server::ServerInit( void ) {
	SerSocket();
	std::cout << GRE << "Server listening on port " << _port
			  << " (listening socket fd " << _serverFd << ", 0 client(s) online)" << WHI << std::endl;
	std::cout << "Waiting to accept a connection..." << std::endl;

	while ( Server::Signal == 0 ) {
		if ( poll(&fds[0], fds.size(), -1) == -1 ) {
			if ( Server::Signal != 0 ) {
				break ;
			}
			if ( errno == EINTR ) {
				continue ;
			}
			throw( std::runtime_error("poll() failed") );
		}

		for ( size_t i = 0; i < fds.size(); ++i ) {
			if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				int errorFd = fds[i].fd;

				if (errorFd == _serverFd) {
					throw ( std::runtime_error("Fatal error on server socket") );
				}

				DisconnectClient(errorFd, "Error/Hangup");
				--i;

				continue;
			}
			if ( fds[i].revents & POLLIN ) {
				if ( fds[i].fd == _serverFd ) {
					AcceptNewClient();
				} else {
					size_t len_before = clients.size();

					ReceiveNewData(fds[i].fd);
					if ( clients.size() != len_before ) {
						--i;

						continue;
					}
				}
			}

			if ( fds[i].revents & POLLOUT ) {
				Client* client = FindClientByFd(fds[i].fd);
				if ( client != NULL && !client->GetOutBuffer().empty() ) {
					const std::string& outMsg = client->GetOutBuffer();
					ssize_t bytesSent = send(fds[i].fd, outMsg.c_str(), outMsg.size(), 0);

					if ( bytesSent > 0 ) {
						client->EraseOutBuffer(bytesSent);
						if ( client->GetOutBuffer().empty() ) {
							fds[i].events &= ~POLLOUT;
						}
					} else if ( bytesSent == -1 && errno != EAGAIN && errno != EWOULDBLOCK ) {
						DisconnectClient(fds[i].fd, "Send error");
						--i;

						continue;
					}
				}
			}

			Client* client = FindClientByFd(fds[i].fd);
			if ( client != NULL && client->IsDisconnected() ) {
				if ( client->GetOutBuffer().empty() ) {
					DisconnectClient(fds[i].fd, "Quit");
					--i;

					continue;
				}
			}
		}
	}
}

void Server::SerSocket( void ) {
	struct sockaddr_in add;
	struct pollfd newPoll;

	add.sin_family = AF_INET;
	add.sin_port = htons(_port);
	add.sin_addr.s_addr = INADDR_ANY;
	for ( size_t i = 0; i < sizeof(add.sin_zero); ++i ) {
		add.sin_zero[i] = 0;
	}

	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if ( _serverFd == -1 ) {
		throw( std::runtime_error("failed to create socket") );
	}

	int optval = 1;
	if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval )) == -1) {
		throw( std::runtime_error("failed to set SO_REUSEADDR") );
	}
	if ( fcntl(_serverFd, F_SETFL, O_NONBLOCK) == -1 ) {
		throw( std::runtime_error("failed to set O_NONBLOCK") );
	}
	if ( bind(_serverFd, (struct sockaddr *)&add, sizeof(add)) == -1 ) {
		throw( std::runtime_error("failed to bind socket") );
	}
	if ( listen(_serverFd, SOMAXCONN) == -1 ) {
		throw( std::runtime_error("listen() failed") );
	}

	newPoll.fd = _serverFd;
	newPoll.events = POLLIN;
	newPoll.revents = 0;
	fds.push_back(newPoll);
}

void Server::AcceptNewClient( void ) {
	struct sockaddr_in cliadd;
	struct pollfd newPoll;
	socklen_t len = sizeof(cliadd);

	int incofd = accept(_serverFd, (sockaddr *)&cliadd, &len);
	if ( incofd == -1 ) {
		if ( errno != EAGAIN && errno != EWOULDBLOCK ) {
			std::cout << RED << "accept() failed" << WHI << std::endl;
		}
		return ;
	}

	if ( fcntl(incofd, F_SETFL, O_NONBLOCK) == -1 ) {
		std::cout << RED << "fcntl() failed" << WHI << std::endl;
		close(incofd);
		return ;
	}

	newPoll.fd = incofd;
	newPoll.events = POLLIN;
	newPoll.revents = 0;

	Client cli;
	cli.SetFd(incofd);
	cli.setIpAdd(inet_ntoa(cliadd.sin_addr));
	clients.push_back(cli);
	fds.push_back(newPoll);

}

size_t Server::CountRegisteredClients( void ) const {
	size_t total = 0;

	for ( size_t i = 0; i < clients.size(); ++i ) {
		if ( clients[i].IsRegistered() ) {
			++total;
		}
	}

	return ( total );
}

Client *Server::FindClientByFd( int fd ) {
	for ( size_t i = 0; i < clients.size(); ++i ) {
		if ( clients[i].GetFd() == fd ) {
			return ( &clients[i] );
		}
	}

	return ( NULL );
}

Client *Server::FindClientByNick( const std::string &nickname ) {
	for ( size_t i = 0; i < clients.size(); ++i ) {
		if ( EqualsIgnoreCase( clients[i].GetNickname(), nickname ) ) {
			return ( &clients[i] );
		}
	}

	return ( NULL );
}

Channel *Server::FindChannel( const std::string &name ) {
	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( EqualsIgnoreCase( channels[i].GetName(), name ) ) {
			return ( &channels[i] );
		}
	}

	return ( NULL );
}

void Server::RemoveEmptyChannel( const std::string &name ) {
	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( EqualsIgnoreCase( channels[i].GetName(), name ) && channels[i].IsEmpty() ) {
			channels.erase(channels.begin() + i);

			return ;
		}
	}
}

void Server::SendToClient( int fd, const std::string &message ) {
	Client* client = FindClientByFd(fd);
	if (client != NULL) {
		client->AppendOutBuffer(message);
		for ( size_t i = 0; i < fds.size(); ++i ) {
			if ( fds[i].fd == fd ) {
				fds[i].events |= POLLOUT;
				break;
			}
		}
	}
}

void Server::BroadcastToChannel( Channel &channel, const std::string &message, int exceptFd ) {
	const std::vector<int> &members = channel.GetMembers();

	for ( size_t i = 0; i < members.size(); ++i ) {
		if ( members[i] != exceptFd ) {
			SendToClient(members[i], message);
		}
	}
}

std::string Server::BuildNamesList( Channel &channel ) {
	const std::vector<int> &members = channel.GetMembers();
	std::string namesList;

	for ( size_t i = 0; i < members.size(); ++i ) {
		Client* memberClient = FindClientByFd( members[i] );

		if ( memberClient == NULL ) {
			continue ;
		}
		if ( !namesList.empty() ) {
			namesList += " ";
		}
		if ( channel.IsOperator( members[i] ) ) {
			namesList += "@";
		}
		namesList += memberClient->GetNickname();
	}

	return ( namesList );
}

void Server::JoinOneChannel( Client &client, const std::string &chanName, const std::string &key ) {
	if ( !IsValidChannelName( chanName ) ) {
		SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
		return ;
	}

	Channel* channel = FindChannel( chanName );

	std::string realName = ( channel != NULL ) ? channel->GetName() : chanName;
	std::string joinMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " JOIN " + realName + "\r\n";

	if ( channel == NULL ) {

		Channel newChan(chanName, client.GetFd());

		channels.push_back( newChan );
		channel = FindChannel( chanName );

		SendToClient(client.GetFd(), joinMsg);
		SendToClient(client.GetFd(), ":server 353 " + client.GetNickname() + " = " + chanName + " :@" + client.GetNickname() + "\r\n");
		SendToClient(client.GetFd(), ":server 366 " + client.GetNickname() + " " + chanName + " :End of /NAMES list.\r\n");
		return ;
	}

	if ( channel->HasMember( client.GetFd() ) ) {
		return ;
	}
	if ( channel->IsInviteOnly() && !channel->IsInvited( client.GetNickname() ) ) {
		SendToClient(client.GetFd(), ":server 473 " + client.GetNickname() + " " + channel->GetName() + " :Cannot join channel (+i)\r\n");
		return ;
	}
	if ( !channel->GetKey().empty() && channel->GetKey() != key ) {
		SendToClient(client.GetFd(), ":server 475 " + client.GetNickname() + " " + channel->GetName() + " :Cannot join channel (+k)\r\n");
		return ;
	}
	if ( channel->IsFull() ) {
		SendToClient(client.GetFd(), ":server 471 " + client.GetNickname() + " " + channel->GetName() + " :Cannot join channel (+l)\r\n");
		return ;
	}

	channel->AddMember( client.GetFd(), false );
	channel->RemoveInvite( client.GetNickname() );
	BroadcastToChannel(*channel, joinMsg, -1);

	if ( !channel->GetTopic().empty() ) {
		SendToClient(client.GetFd(), ":server 332 " + client.GetNickname() + " " + channel->GetName() + " :" + channel->GetTopic() + "\r\n");
	}
	SendToClient(client.GetFd(), ":server 353 " + client.GetNickname() + " = " + channel->GetName() + " :" + BuildNamesList(*channel) + "\r\n");
	SendToClient(client.GetFd(), ":server 366 " + client.GetNickname() + " " + channel->GetName() + " :End of /NAMES list.\r\n");
}

void Server::PartOneChannel( Client &client, const std::string &chanName, const std::string &reason ) {
	Channel* channel = FindChannel( chanName );

	if ( channel == NULL ) {
		SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
		return ;
	}
	if ( !channel->HasMember( client.GetFd() ) ) {
		SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + channel->GetName() + " :You're not on that channel\r\n");
		return ;
	}

	std::string partMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress()
		+ " PART " + channel->GetName() + " :" + reason + "\r\n";
	std::string realName = channel->GetName();

	BroadcastToChannel(*channel, partMsg, -1);
	channel->RemoveMember( client.GetFd() );
	channel->RemoveInvite( client.GetNickname() );
	RemoveEmptyChannel( realName );
}

void Server::DeliverMessage( Client &client, const std::string &target, const std::string &message, bool isNotice ) {
	std::string command = isNotice ? "NOTICE" : "PRIVMSG";
	std::string prefix = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " " + command + " ";

	if ( target[0] == '#' ) {
		Channel* channel = FindChannel( target );

		if ( channel == NULL ) {
			if ( !isNotice ) {
				SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + target + " :No such channel\r\n");
			}
			return ;
		}
		if ( !channel->HasMember( client.GetFd() ) ) {
			if ( !isNotice ) {
				SendToClient(client.GetFd(), ":server 404 " + client.GetNickname() + " " + target + " :Cannot send to channel\r\n");
			}
			return ;
		}

		BroadcastToChannel(*channel, prefix + channel->GetName() + " :" + message + "\r\n", client.GetFd());
		return ;
	}

	Client* targetClient = FindClientByNick( target );
	if ( targetClient == NULL ) {
		if ( !isNotice ) {
			SendToClient(client.GetFd(), ":server 401 " + client.GetNickname() + " " + target + " :No such nick/channel\r\n");
		}
		return ;
	}

	SendToClient(targetClient->GetFd(), prefix + targetClient->GetNickname() + " :" + message + "\r\n");
}

void Server::BroadcastToPeers( Client &client, const std::string &message ) {
	std::vector<int> notified;

	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( !channels[i].HasMember( client.GetFd() ) ) {
			continue ;
		}

		const std::vector<int> &members = channels[i].GetMembers();
		for ( size_t j = 0; j < members.size(); ++j ) {
			if ( members[j] == client.GetFd() ) {
				continue ;
			}

			bool alreadyAdded = false;
			for ( size_t k = 0; k < notified.size(); ++k ) {
				if ( notified[k] == members[j] ) {
					alreadyAdded = true;
					break ;
				}
			}
			if ( !alreadyAdded ) {
				notified.push_back( members[j] );
			}
		}
	}

	for ( size_t i = 0; i < notified.size(); ++i ) {
		SendToClient( notified[i], message );
	}
}

void Server::RenameInvites( const std::string &oldNick, const std::string &newNick ) {
	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( channels[i].IsInvited( oldNick ) ) {
			channels[i].RemoveInvite( oldNick );
			channels[i].Invite( newNick );
		}
	}
}

void Server::ProcessLine( Client& client, const std::string& line ) {
	std::string cleanLine = TrimCarriageReturn( line );
	std::vector<std::string> tokens = SplitParams( cleanLine );

	if ( tokens.empty() ) {
		return ;
	}

	std::string command = ToUpper( tokens[0] );
	bool wasRegistered = client.IsRegistered();
	std::string replyNick = client.GetNickname().empty() ? "*" : client.GetNickname();

	if ( command == "CAP" ) {
		if ( tokens.size() >= 2 ) {
			std::string sub = ToUpper( tokens[1] );

			if ( sub == "LS" ) {
				SendToClient(client.GetFd(), ":server CAP * LS :\r\n");
			} else if ( sub == "LIST" ) {
				SendToClient(client.GetFd(), ":server CAP * LIST :\r\n");
			} else if ( sub == "REQ" ) {
				SendToClient(client.GetFd(), ":server CAP * NAK :" + ( tokens.size() > 2 ? tokens[2] : std::string("") ) + "\r\n");
			}
		}
		return ;
	}

	if ( command == "QUIT" ) {
		std::string reason = tokens.size() > 1 ? JoinFrom(tokens, 1) : "Client Quit";
		std::string quitMsg = ":" + replyNick + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " QUIT :" + reason + "\r\n";

		BroadcastToPeers( client, quitMsg );

		SendToClient(client.GetFd(), "ERROR :Closing Link: (" + client.GetUsername() + "@" + client.GetIpAddress() + ") [" + reason + "]\r\n");
		client.SetDisconnected( true );

		return ;
	}

	if ( command == "PING" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 409 " + replyNick + " :No origin specified\r\n");
			return ;
		}
		SendToClient(client.GetFd(), ":server PONG server :" + tokens[1] + "\r\n");
		return ;
	}

	if ( command == "PONG" ) {
		return ;
	}

	if ( command == "PASS" ) {
		if ( wasRegistered ) {
			SendToClient(client.GetFd(), ":server 462 " + replyNick + " :You may not reregister\r\n");
			return ;
		}
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + replyNick + " PASS :Not enough parameters\r\n");
			return ;
		}
		if ( tokens[1] == _password ) {
			client.SetPassAccepted( true );
		} else {
			SendToClient(client.GetFd(), ":server 464 " + replyNick + " :Password incorrect\r\n");
		}
	} else if ( command == "NICK" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 431 " + replyNick + " :No nickname given\r\n");
			return ;
		}

		std::string newNick = tokens[1];
		if ( !IsValidNickname( newNick ) ) {
			SendToClient(client.GetFd(), ":server 432 " + replyNick + " " + newNick + " :Erroneous nickname\r\n");
			return ;
		}

		Client* holder = FindClientByNick( newNick );

		if ( holder == &client ) {
			if ( client.GetNickname() == newNick ) {
				return ;
			}
		} else if ( holder != NULL ) {
			SendToClient(client.GetFd(), ":server 433 " + replyNick + " " + newNick + " :Nickname is already in use\r\n");
			return ;
		}

		std::string oldNick = client.GetNickname();
		client.SetNickname( newNick );
		client.SetNickAccepted( true );

		if ( wasRegistered ) {
			std::string nickMsg = ":" + oldNick + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " NICK :" + newNick + "\r\n";

			SendToClient( client.GetFd(), nickMsg );
			BroadcastToPeers( client, nickMsg );
			RenameInvites( oldNick, newNick );
		}
	} else if ( command == "USER" ) {
		if ( wasRegistered ) {
			SendToClient(client.GetFd(), ":server 462 " + replyNick + " :You may not reregister\r\n");
			return ;
		}
		if ( tokens.size() < 5 ) {
			SendToClient(client.GetFd(), ":server 461 " + replyNick + " USER :Not enough parameters\r\n");
			return ;
		}
		client.SetUsername( tokens[1] );
		client.SetRealname( JoinFrom(tokens, 4) );
		client.SetUserAccepted( true );
	} else if ( !client.IsRegistered() ) {
		SendToClient(client.GetFd(), ":server 451 " + replyNick + " :You have not registered\r\n");
		return ;
	} else if ( command == "NAMES" ) {
		if ( tokens.size() > 1 ) {
			std::string chanName = tokens[1];
			Channel* channel = FindChannel( chanName );
			if ( channel != NULL ) {
				SendToClient(client.GetFd(), ":server 353 " + client.GetNickname() + " = " + channel->GetName() + " :" + BuildNamesList(*channel) + "\r\n");
			}
			SendToClient(client.GetFd(), ":server 366 " + client.GetNickname() + " "
				+ ( channel != NULL ? channel->GetName() : chanName ) + " :End of /NAMES list.\r\n");
		}
	}

	else if ( command == "WHO" ) {
		std::string chanName = tokens.size() > 1 ? tokens[1] : std::string();
		Channel* channel = chanName.empty() ? NULL : FindChannel( chanName );

		if ( channel != NULL ) {
			const std::vector<int> &members = channel->GetMembers();

			for ( size_t i = 0; i < members.size(); ++i ) {
				Client* member = FindClientByFd( members[i] );

				if ( member == NULL ) {
					continue ;
				}
				std::string flags = channel->IsOperator( members[i] ) ? "H@" : "H";
				SendToClient(client.GetFd(), ":server 352 " + client.GetNickname() + " " + channel->GetName() + " "
					+ member->GetUsername() + " " + member->GetIpAddress() + " server " + member->GetNickname()
					+ " " + flags + " :0 " + member->GetRealname() + "\r\n");
			}
		}
		SendToClient(client.GetFd(), ":server 315 " + client.GetNickname() + " "
			+ ( chanName.empty() ? "*" : chanName ) + " :End of /WHO list.\r\n");
	}
	else if ( command == "JOIN" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " JOIN :Not enough parameters\r\n");
			return ;
		}

		std::vector<std::string> names = SplitList( tokens[1], ',' );
		std::vector<std::string> keys = tokens.size() >= 3 ? SplitList( tokens[2], ',' ) : std::vector<std::string>();

		for ( size_t i = 0; i < names.size(); ++i ) {
			JoinOneChannel( client, names[i], i < keys.size() ? keys[i] : std::string("") );
		}
	} else if ( command == "PART" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " PART :Not enough parameters\r\n");
			return ;
		}

		std::string reason = tokens.size() > 2 ? JoinFrom(tokens, 2) : "Leaving";
		std::vector<std::string> names = SplitList( tokens[1], ',' );

		for ( size_t i = 0; i < names.size(); ++i ) {
			PartOneChannel( client, names[i], reason );
		}
	} else if ( command == "PRIVMSG" || command == "NOTICE" ) {
		bool isNotice = ( command == "NOTICE" );

		if ( tokens.size() < 2 ) {
			if ( !isNotice ) {
				SendToClient(client.GetFd(), ":server 411 " + client.GetNickname() + " :No recipient given (" + command + ")\r\n");
			}
			return ;
		}
		if ( tokens.size() < 3 || tokens[2].empty() ) {
			if ( !isNotice ) {
				SendToClient(client.GetFd(), ":server 412 " + client.GetNickname() + " :No text to send\r\n");
			}
			return ;
		}

		std::string message = JoinFrom(tokens, 2);
		std::vector<std::string> targets = SplitList( tokens[1], ',' );

		for ( size_t i = 0; i < targets.size(); ++i ) {
			DeliverMessage( client, targets[i], message, isNotice );
		}

	} else if ( command == "KICK" ) {
		if ( tokens.size() < 3 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " KICK :Not enough parameters\r\n");
			return ;
		}
		std::string chanName = tokens[1];
		std::string targetNick = tokens[2];
		std::string reason = tokens.size() > 3 ? JoinFrom(tokens, 3) : "Kicked by operator";

		Channel* channel = FindChannel( chanName );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}
		if ( !channel->HasMember( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + channel->GetName() + " :You're not on that channel\r\n");
			return ;
		}
		if ( !channel->IsOperator( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
			return ;
		}
		Client* targetClient = FindClientByNick( targetNick );
		if ( targetClient == NULL || !channel->HasMember( targetClient->GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 441 " + client.GetNickname() + " " + targetNick + " " + channel->GetName() + " :They aren't on that channel\r\n");
			return ;
		}

		std::string kickMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress()
			+ " KICK " + channel->GetName() + " " + targetClient->GetNickname() + " :" + reason + "\r\n";
		std::string realName = channel->GetName();

		BroadcastToChannel(*channel, kickMsg, -1);
		channel->RemoveMember( targetClient->GetFd() );
		channel->RemoveInvite( targetClient->GetNickname() );
		RemoveEmptyChannel( realName );
	} else if ( command == "INVITE" ) {
		if ( tokens.size() < 3 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " INVITE :Not enough parameters\r\n");
			return ;
		}
		std::string targetNick = tokens[1];
		std::string chanName = tokens[2];

		Client* targetClient = FindClientByNick( targetNick );
		if ( targetClient == NULL ) {
			SendToClient(client.GetFd(), ":server 401 " + client.GetNickname() + " " + targetNick + " :No such nick/channel\r\n");
			return ;
		}
		Channel* channel = FindChannel( chanName );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}
		if ( !channel->HasMember( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + channel->GetName() + " :You're not on that channel\r\n");
			return ;
		}
		if ( channel->IsInviteOnly() && !channel->IsOperator( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
			return ;
		}
		if ( channel->HasMember( targetClient->GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 443 " + client.GetNickname() + " " + targetClient->GetNickname() + " " + channel->GetName() + " :is already on channel\r\n");
			return ;
		}

		channel->Invite( targetClient->GetNickname() );

		SendToClient(client.GetFd(), ":server 341 " + client.GetNickname() + " " + targetClient->GetNickname() + " " + channel->GetName() + "\r\n");
		SendToClient(targetClient->GetFd(), ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress()
			+ " INVITE " + targetClient->GetNickname() + " :" + channel->GetName() + "\r\n");
	} else if ( command == "TOPIC" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " TOPIC :Not enough parameters\r\n");
			return ;
		}
		std::string chanName = tokens[1];
		Channel* channel = FindChannel( chanName );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}
		if ( !channel->HasMember( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + channel->GetName() + " :You're not on that channel\r\n");
			return ;
		}

		if ( tokens.size() == 2 ) {
			if ( channel->GetTopic().empty() ) {
				SendToClient(client.GetFd(), ":server 331 " + client.GetNickname() + " " + channel->GetName() + " :No topic is set\r\n");
			} else {
				SendToClient(client.GetFd(), ":server 332 " + client.GetNickname() + " " + channel->GetName() + " :" + channel->GetTopic() + "\r\n");
			}
		}
		else {
			if ( channel->IsTopicRestricted() && !channel->IsOperator( client.GetFd() ) ) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
				return ;
			}
			std::string newTopic = JoinFrom(tokens, 2);
			channel->SetTopic( newTopic );
			std::string topicMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress()
				+ " TOPIC " + channel->GetName() + " :" + newTopic + "\r\n";
			BroadcastToChannel(*channel, topicMsg, -1);
		}
	} else if ( command == "MODE" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " MODE :Not enough parameters\r\n");
			return ;
		}
		std::string target = tokens[1];

		if ( target.empty() ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " MODE :Not enough parameters\r\n");
			return ;
		}

		if ( target[0] != '#' ) {
			if ( target != client.GetNickname() ) {
				SendToClient(client.GetFd(), ":server 502 " + client.GetNickname() + " :Cannot change mode for other users\r\n");
			} else {
				SendToClient(client.GetFd(), ":server 221 " + client.GetNickname() + " +i\r\n");
			}
			return ;
		}

		Channel* channel = FindChannel( target );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + target + " :No such channel\r\n");
			return ;
		}

		std::string modeArg = tokens.size() > 2 ? tokens[2] : std::string();
		bool hasSign = ( modeArg.find('+') != std::string::npos || modeArg.find('-') != std::string::npos );

		if ( tokens.size() == 2 || !hasSign ) {
			bool answered = false;

			if ( modeArg.find('b') != std::string::npos ) {
				SendToClient(client.GetFd(), ":server 368 " + client.GetNickname() + " " + channel->GetName() + " :End of channel ban list\r\n");
				answered = true;
			}
			if ( modeArg.find('e') != std::string::npos ) {
				SendToClient(client.GetFd(), ":server 349 " + client.GetNickname() + " " + channel->GetName() + " :End of channel exception list\r\n");
				answered = true;
			}
			if ( modeArg.find('I') != std::string::npos ) {
				SendToClient(client.GetFd(), ":server 347 " + client.GetNickname() + " " + channel->GetName() + " :End of channel invite list\r\n");
				answered = true;
			}

			if ( !answered ) {
				std::string modes = "+";
				std::string args = "";

				if ( channel->IsInviteOnly() ) {
					modes += "i";
				}
				if ( channel->IsTopicRestricted() ) {
					modes += "t";
				}
				if ( !channel->GetKey().empty() ) {
					modes += "k";

					if ( channel->HasMember( client.GetFd() ) ) {
						args += " " + channel->GetKey();
					}
				}
				if ( channel->HasUserLimit() ) {
					modes += "l";
					args += " " + ToString( channel->GetUserLimit() );
				}
				SendToClient(client.GetFd(), ":server 324 " + client.GetNickname() + " " + channel->GetName() + " " + modes + args + "\r\n");
			}
			return ;
		}

		if ( !channel->IsOperator( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
			return ;
		}

		ApplyChannelModes( client, *channel, tokens );
	} else {
		std::string nick = client.GetNickname().empty() ? "*" : client.GetNickname();
		SendToClient(client.GetFd(), ":server 421 " + nick + " " + command + " :Unknown command\r\n");
	}

	if ( !wasRegistered && client.IsRegistered() ) {
		SendWelcomeMessages( client );

		std::cout << GRE << "Client <" << client.GetFd() << "> Connected as "
				  << client.GetNickname() << " -- " << CountRegisteredClients()
				  << " client(s) online" << WHI << std::endl;
	}
}

void Server::ApplyChannelModes( Client &client, Channel &channel, const std::vector<std::string> &tokens ) {
	const std::string &modeString = tokens[2];
	std::string appliedModes;
	std::string appliedArgs;
	char lastSign = 0;
	char sign = '+';
	size_t argIndex = 3;

	for ( size_t i = 0; i < modeString.size(); ++i ) {
		char c = modeString[i];

		if ( c == '+' || c == '-' ) {
			sign = c;
			continue ;
		}

		bool needsArg = ( c == 'o' || ( ( c == 'k' || c == 'l' ) && sign == '+' ) );
		std::string arg;

		if ( needsArg ) {
			if ( argIndex >= tokens.size() ) {
				SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " MODE :Not enough parameters\r\n");
				continue ;
			}
			arg = tokens[argIndex++];
		} else if ( c == 'k' && argIndex < tokens.size() ) {
			arg = tokens[argIndex++];
		}

		bool applied = true;
		switch ( c ) {
			case 'i':
				channel.SetInviteOnly( sign == '+' );
				break ;
			case 't':
				channel.SetTopicRestricted( sign == '+' );
				break ;
			case 'k':
				channel.SetKey( sign == '+' ? arg : std::string("") );
				break ;
			case 'l':
				if ( sign == '+' ) {
					int limit = ::atoi(arg.c_str());

					if ( limit <= 0 ) {
						SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " MODE :Not enough parameters\r\n");
						applied = false;
						break ;
					}
					channel.SetUserLimit( limit );
				} else {
					channel.RemoveUserLimit();
				}
				break ;
			case 'o': {
				Client* targetClient = FindClientByNick( arg );

				if ( targetClient == NULL || !channel.HasMember( targetClient->GetFd() ) ) {
					SendToClient(client.GetFd(), ":server 441 " + client.GetNickname() + " " + arg + " " + channel.GetName() + " :They aren't on that channel\r\n");
					applied = false;
					break ;
				}
				if ( sign == '+' ) {
					channel.AddOperator( targetClient->GetFd() );
				} else {
					channel.RemoveOperator( targetClient->GetFd() );
				}
				break ;
			}
			default:
				SendToClient(client.GetFd(), ":server 472 " + client.GetNickname() + " " + std::string(1, c) + " :is unknown mode char to me\r\n");
				applied = false;
				break ;
		}

		if ( !applied ) {
			continue ;
		}
		if ( sign != lastSign ) {
			appliedModes += sign;
			lastSign = sign;
		}
		appliedModes += c;
		if ( !arg.empty() ) {
			appliedArgs += " " + arg;
		}
	}

	if ( appliedModes.empty() ) {
		return ;
	}

	std::string modeMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress()
		+ " MODE " + channel.GetName() + " " + appliedModes + appliedArgs + "\r\n";
	BroadcastToChannel( channel, modeMsg, -1 );
}

void Server::SendWelcomeMessages( Client& client ) {
	std::string nick = client.GetNickname();

	SendToClient(client.GetFd(), ":server 001 " + nick + " :Welcome to the ft_irc Network, " + nick + "\r\n");
	SendToClient(client.GetFd(), ":server 002 " + nick + " :Your host is server, running version 1.0\r\n");
	SendToClient(client.GetFd(), ":server 003 " + nick + " :This server was created today\r\n");
	SendToClient(client.GetFd(), ":server 004 " + nick + " server 1.0 i itkol\r\n");
	SendToClient(client.GetFd(), ":server 005 " + nick + " CHANTYPES=# PREFIX=(o)@ CHANMODES=,k,l,it :are supported by this server\r\n");

	SendToClient(client.GetFd(), ":server 375 " + nick + " :- server Message of the day - \r\n");
	SendToClient(client.GetFd(), ":server 372 " + nick + " :- Welcome to 42 IRC Server!\r\n");
	SendToClient(client.GetFd(), ":server 372 " + nick + " :- Please respect other users.\r\n");
	SendToClient(client.GetFd(), ":server 376 " + nick + " :End of /MOTD command.\r\n");
}

void Server::ReceiveNewData( int fd ) {
	Client *client = FindClientByFd(fd);
	if ( client == NULL ) {
		return ;
	}

	char buff[512 + 1];
	ssize_t bytes = recv(fd, buff, sizeof(buff) - 1, 0);
	if ( bytes <= 0 ) {
		if ( bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ) {
			return ;
		}
		DisconnectClient(fd, bytes == 0 ? "Connection closed" : "Read error");
		return ;
	}

	if ( client->GetBuffer().size() + bytes > 1024 ) {
		SendToClient(fd, "ERROR :Closing Link: Input line was too long\r\n");
		DisconnectClient(fd, "Input line too long");
		return ;
	}

	std::string buffer = client->GetBuffer();
	buffer.append(buff, static_cast<size_t>(bytes));

	std::string::size_type pos = 0;
	while ( (pos = buffer.find('\n')) != std::string::npos ) {
		if ( client->IsDisconnected() ) {
        	break;
    	}
		std::string line = buffer.substr(0, pos + 1);
		ProcessLine(*client, line);
		buffer = buffer.substr(pos + 1);
	}

	client->ClearBuffer();
	client->AppendBuffer(buffer);
}

void Server::RemoveAllEmptyChannels( void ) {
	size_t i = 0;

	while ( i < channels.size() ) {
		if ( channels[i].IsEmpty() ) {
			channels.erase( channels.begin() + i );
		} else {
			++i;
		}
	}
}

void Server::DisconnectClient( int fd, const std::string &reason ) {

	Client* leaving = FindClientByFd( fd );
	bool wasAnnounced = ( leaving != NULL && leaving->IsRegistered() );
	std::string nick = ( leaving != NULL ) ? leaving->GetNickname() : std::string();

	for ( size_t i = 0; i < channels.size(); ++i ) {
		channels[i].RemoveMember( fd );
	}
	RemoveAllEmptyChannels();

	ClearClients( fd );
	close( fd );

	if ( !wasAnnounced ) {
		return ;
	}

	std::cout << RED << "Client <" << fd << "> (" << nick << ") Disconnected";
	if ( !reason.empty() ) {
		std::cout << " (" << reason << ")";
	}
	std::cout << " -- " << CountRegisteredClients() << " client(s) online" << WHI << std::endl;
}

void Server::CloseFds( void ) {
	for ( size_t i = 0; i < fds.size(); ++i ) {
		if ( fds[i].fd >= 0 ) {
			close(fds[i].fd);
		}
	}
	fds.clear();
	clients.clear();
	_serverFd = -1;
}

void Server::ClearClients( int fd ) {
	for ( size_t i = 0; i < clients.size(); ++i ) {
		if ( clients[i].GetFd() == fd ) {
			const std::string nick = clients[i].GetNickname();

			if ( !nick.empty() ) {
				for ( size_t j = 0; j < channels.size(); ++j ) {
					channels[j].RemoveInvite( nick );
				}
			}
			clients.erase(clients.begin() + i);
			break;
		}
	}
	for ( size_t i = 0; i < fds.size(); ++i ) {
		if ( fds[i].fd == fd ) {
			fds.erase(fds.begin() + i);
			break;
		}
	}
}

void Server::SignalHandler( int signum ) {
	const char message[] = "\nSignal received!\n";

	(void)signum;

	if ( write(STDOUT_FILENO, message, sizeof(message) - 1) == -1 ) {

	}
	Server::Signal = 1;
}
