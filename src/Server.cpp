/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 20:46:27 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/21 22:02:35 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

bool Server::Signal = false;

namespace
{
std::vector<std::string> SplitWords( const std::string &line ) {
	std::vector<std::string> tokens;
	std::istringstream stream(line);
	std::string token;

	while ( stream >> token ) {
		tokens.push_back(token);
	}

	return ( tokens );
}

std::string TrimCarriageReturn( const std::string &line ) {
	if ( !line.empty() && line[line.size() - 1] == '\r' ) {
		return ( line.substr(0, line.size() - 1) );
	}

	return ( line );
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

bool StartsWith( const std::string &value, const std::string &prefix ) {
	return (value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0);
}
} // namespace

Server::Server( void ) : _port(6667), _password("password"), _serverFd(-1) {}

Server::Server( int port, const std::string &password )
	: _port( port ), _password(password), _serverFd(-1) {}

Server::Server( const Server &other ) { *this = other; }

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
	ClearClients(this->_serverFd);
	CloseFds();
}

int Server::getServerFd( void ) const { return (_serverFd); }

int Server::getPort( void ) const { return (_port); }

void Server::ServerInit( void ) {
	SerSocket();
	std::cout << GRE << "Server <" << _serverFd << "> Connected" << WHI << std::endl;
	std::cout << "Waiting to accept a connection..." << std::endl;

	while ( Server::Signal == false ) {
		if ( (poll(&fds[0], fds.size(), -1) == -1) && Server::Signal == false ) {
			throw( std::runtime_error("poll() failed") );
		}

		for ( size_t i = 0; i < fds.size(); ++i ) {
			if ( fds[i].revents & POLLIN ) {
				if ( fds[i].fd == _serverFd ) {
					AcceptNewClient();
				} else {
					ReceiveNewData(fds[i].fd);
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

	std::cout << GRE << "Client <" << incofd << "> Connected" << WHI << std::endl;
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
		if ( clients[i].GetNickname() == nickname ) {
			return ( &clients[i] );
		}
	}

	return ( NULL );
}

Channel *Server::FindChannel( const std::string &name ) {
	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( channels[i].GetName() == name ) {
			return ( &channels[i] );
		}
	}

	return ( NULL );
}

void Server::RemoveEmptyChannel( const std::string &name ) {
	for ( size_t i = 0; i < channels.size(); ++i ) {
		if ( channels[i].GetName() == name && channels[i].IsEmpty() ) {
			channels.erase(channels.begin() + i);
			
			return ;
		}
	}
}

void Server::SendToClient( int fd, const std::string &message ) {
	send(fd, message.c_str(), message.size(), 0);
}

void Server::BroadcastToChannel( Channel &channel, const std::string &message, int exceptFd ) {
	const std::vector<int> &members = channel.GetMembers();
	
	for ( size_t i = 0; i < members.size(); ++i ) {
		if ( members[i] != exceptFd ) {
			SendToClient(members[i], message);
		}
	}
}

void Server::MaybeRegisterClient( Client &client ) {
	if ( client.IsRegistered() ) {
		SendToClient(client.GetFd(), ":server 001 " + client.GetNickname() + " :Welcome to the IRC server\r\n");
	}
}

void Server::ProcessLine( Client &client, const std::string &line ) {
	std::string clean = TrimCarriageReturn(line);
	std::vector<std::string> tokens = SplitWords(clean);
	
	if ( tokens.empty() ) {
		return ;
	}

	const std::string &command = tokens[0];
	if ( command == "PASS" ) {
		if ( tokens.size() >= 2 && tokens[1] == _password ) {
			client.SetPassAccepted(true);
			SendToClient(client.GetFd(), ":server NOTICE * :Password accepted\r\n");
		} else {
			SendToClient(client.GetFd(), ":server 464 * :Password incorrect\r\n");
		}
	} else if ( command == "NICK" ) {
		if ( tokens.size() >= 2 ) {
			if (FindClientByNick(tokens[1]) != NULL && client.GetNickname() != tokens[1]) {
				SendToClient(client.GetFd(), ":server 433 * " + tokens[1] + " :Nickname is already in use\r\n");
			} else {
				client.SetNickname(tokens[1]);
				client.SetNickAccepted(true);
				MaybeRegisterClient(client);
			}
		}
	} else if ( command == "USER" ) {
		if ( tokens.size() >= 5 ) {
			client.SetUsername(tokens[1]);
			client.SetUserAccepted(true);
			std::string realname = clean;
			size_t colon = realname.find(':');
			if ( colon != std::string::npos ) {
				realname = realname.substr(colon + 1);
			}
			client.SetRealname(realname);
			MaybeRegisterClient(client);
		}
	} else if ( command == "JOIN" ) {
		if ( !client.IsRegistered() || tokens.size() < 2 ) {
			return ;
		}
		std::string channelName = tokens[1];
		std::string providedKey = tokens.size() >= 3 ? tokens[2] : "";
		if ( !StartsWith(channelName, "#") ) {
			return ;
		}
		Channel *channel = FindChannel(channelName);
		if ( channel == NULL ) {
			channels.push_back(Channel(channelName, client.GetFd()));
			channel = &channels.back();
		}
		if ( channel->IsInviteOnly() && !channel->IsInvited(client.GetNickname()) ) {
			SendToClient(client.GetFd(), ":server 473 " + client.GetNickname() + " " + channelName + " :Cannot join channel (+i)\r\n");
			return ;
		}
		if ( channel->IsFull() && !channel->HasMember(client.GetFd()) ) {
			SendToClient(client.GetFd(), ":server 471 " + client.GetNickname() + " " + channelName + " :Cannot join channel (+l)\r\n");
			return ;
		}
		if ( !channel->GetKey().empty() && channel->GetKey() != providedKey ) {
			SendToClient(client.GetFd(), ":server 475 " + client.GetNickname() + " " + channelName + " :Cannot join channel (+k)\r\n");
			return ;
		} else if ( !channel->HasMember(client.GetFd()) ) {
			channel->AddMember(client.GetFd(), false);
		}
		BroadcastToChannel(*channel, ":" + client.GetNickname() + "!" + client.GetUsername() + "@server JOIN " + channelName + "\r\n", -1);
	} else if ( command == "PRIVMSG" ) {
		if ( !client.IsRegistered() || tokens.size() < 3 ) {
			return ;
		}
		std::string target = tokens[1];
		size_t colon = clean.find(':');
		std::string message = colon == std::string::npos ? JoinFrom(tokens, 2)
														 : clean.substr(colon + 1);
		if ( StartsWith(target, "#") ) {
			Channel *channel = FindChannel(target);
			if ( channel != NULL ) {
				BroadcastToChannel(*channel, ":" + client.GetNickname() + " PRIVMSG " + target + " :" + message + "\r\n", client.GetFd());
			}
		} else {
			Client *targetClient = FindClientByNick(target);
			if ( targetClient != NULL ) {
				SendToClient(targetClient->GetFd(), ":" + client.GetNickname() + " PRIVMSG " + target + " :" + message + "\r\n");
			}
		}
	} else if ( command == "TOPIC" ) {
		if ( tokens.size() < 2 ) {
			return ;
		}
		Channel *channel = FindChannel(tokens[1]);
		if ( channel == NULL ) {
			return ;
		}
		if ( tokens.size() >= 3 ) {
			if (channel->IsTopicRestricted() && !channel->IsOperator( client.GetFd() )) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
				return ;
			}
			channel->SetTopic(JoinFrom(tokens, 2));
			BroadcastToChannel(*channel, ":server TOPIC " + channel->GetName() + " :" + channel->GetTopic() + "\r\n", -1);
		} else {
			SendToClient(client.GetFd(), ":server 332 " + client.GetNickname() + " " + channel->GetName() + " :" + channel->GetTopic() + "\r\n");
		}
	} else if ( command == "INVITE" ) {
		if ( tokens.size() < 3 ) {
			return ;
		}
		Channel *channel = FindChannel(tokens[2]);
		Client *target = FindClientByNick(tokens[1]);
		if ( channel != NULL && target != NULL ) {
			if ( !channel->IsOperator(client.GetFd()) ) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
				return ;
			}
			channel->Invite(target->GetNickname());
			SendToClient(target->GetFd(), ":server INVITE " + target->GetNickname() + " " + channel->GetName() + "\r\n");
		}
	} else if ( command == "KICK" ) {
		if ( tokens.size() < 3 ) {
			return ;
		}
		Channel *channel = FindChannel(tokens[1]);
		Client *target = FindClientByNick(tokens[2]);
		if ( channel != NULL && target != NULL ) {
			if ( !channel->IsOperator(client.GetFd()) ) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
				return ;
			}
			channel->RemoveMember(target->GetFd());
			SendToClient(target->GetFd(), ":server KICK " + channel->GetName() + " " + target->GetNickname() + "\r\n");
			RemoveEmptyChannel(channel->GetName());
		}
	} else if ( command == "MODE" ) {
		if ( tokens.size() < 3 ) {
			return ;
		}
		Channel *channel = FindChannel(tokens[1]);
		if ( channel == NULL ) {
			return ;
		}
		if ( !channel->IsOperator(client.GetFd()) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + channel->GetName() + " :You're not channel operator\r\n");
			return ;
		}
		std::string mode = tokens[2];
		if ( mode == "+i" ) {
			channel->SetInviteOnly(true);
		}
		if ( mode == "-i" ) {
			channel->SetInviteOnly(false);
		}
		if ( mode == "+t" ) {
			channel->SetTopicRestricted(true);
		}
		if ( mode == "-t" ) {
			channel->SetTopicRestricted(false);
		}
		if ( mode == "+k" && tokens.size() >= 4 ) {
			channel->SetKey(tokens[3]);
		}
		if ( mode == "-k" ) {
			channel->SetKey("");
		}
		if ( mode == "+o" && tokens.size() >= 4 ) {
			Client *target = FindClientByNick(tokens[3]);
			if ( target != NULL ) {
				channel->AddOperator(target->GetFd());
			}
		}
		if ( mode == "-o" && tokens.size() >= 4 ) {
			Client *target = FindClientByNick(tokens[3]);
			if ( target != NULL ) {
				channel->RemoveOperator(target->GetFd());
			}
		}
		if ( mode == "+l" && tokens.size() >= 4 ) {
			channel->SetUserLimit(::atoi(tokens[3].c_str()));
		}
		if ( mode == "-l" ) {
			channel->RemoveUserLimit();
		}
	}
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
		std::cout << RED << "Client <" << fd << "> Disconnected" << WHI << std::endl;

		for ( size_t i = 0; i < channels.size(); ++i ) {
			channels[i].RemoveMember(fd);
		}

		ClearClients(fd);
		close(fd);
		return ;
	}

	buff[bytes] = '\0';
	std::string buffer = client->GetBuffer();
	buffer += buff;

	std::string::size_type pos = 0;
	while ( (pos = buffer.find('\n')) != std::string::npos ) {
		std::string line = buffer.substr(0, pos + 1);
		ProcessLine(*client, line);
		buffer = buffer.substr(pos + 1);
	}
	
	client->ClearBuffer();
	client->AppendBuffer(buffer);
}

void Server::CloseFds( void ) {
	for ( size_t i = 0; i < fds.size(); ++i ) {
		close(fds[i].fd);
	}
}

void Server::ClearClients( int fd ) {
	for ( size_t i = 0; i < clients.size(); ++i ) {
		if ( clients[i].GetFd() == fd ) {
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
	(void)signum;

	std::cout << "Signal received!" << std::endl;
	Server::Signal = true;
}
