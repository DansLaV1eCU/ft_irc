/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 20:46:27 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/22 21:47:49 by danslav1e        ###   ########.fr       */
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

void Server::ProcessLine( Client& client, const std::string& line ) {
	std::string cleanLine = TrimCarriageReturn( line );
	std::vector<std::string> tokens = SplitWords( cleanLine );

	if ( tokens.empty() ) {
		return ;
	}

	std::string command = tokens[0];
	bool wasRegistered = client.IsRegistered();

	if ( command == "PASS" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 * PASS :Not enough parameters\r\n");
			return ;
		}
		if ( tokens[1] == _password ) {
			client.SetPassAccepted( true );
		} else {
			SendToClient(client.GetFd(), ":server 464 * :Password incorrect\r\n");
		}
	} else if ( command == "NICK" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 431 * :No nickname given\r\n");
			return ;
		}
		if ( FindClientByNick( tokens[1] ) != NULL ) {
			SendToClient(client.GetFd(), ":server 433 * " + tokens[1] + " :Nickname is already in use\r\n");
			return ;
		}
		client.SetNickname( tokens[1] );
		client.SetNickAccepted( true );
	} else if ( command == "USER" ) {
		if ( tokens.size() < 5 ) {
			SendToClient(client.GetFd(), ":server 461 * USER :Not enough parameters\r\n");
			return ;
		}
		client.SetUsername( tokens[1] );
		client.SetRealname( JoinFrom(tokens, 4) );
		client.SetUserAccepted( true );
	} else if ( !client.IsRegistered() ) {
		SendToClient(client.GetFd(), ":server 451 * :You have not registered\r\n");
		return ;
	} else if ( command == "PING" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " PING :Not enough parameters\r\n");
			return ;
		}
		SendToClient(client.GetFd(), "PONG " + tokens[1] + "\r\n");
	} else if ( command == "JOIN" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " JOIN :Not enough parameters\r\n");
			return ;
		}
		std::string chanName = tokens[1];
		std::string key = tokens.size() >= 3 ? tokens[2] : "";

		if ( chanName.empty() || chanName[0] != '#' ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}

		Channel* channel = FindChannel( chanName );
		std::string joinMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " JOIN " + chanName + "\r\n";

		if ( channel == NULL ) {
			Channel newChan(chanName, client.GetFd());
			if ( !key.empty() ) {
				newChan.SetKey( key );
			}
			channels.push_back( newChan );
			channel = FindChannel( chanName );

			SendToClient(client.GetFd(), joinMsg);
			SendToClient(client.GetFd(), ":server 353 " + client.GetNickname() + " = " + chanName + " :@" + client.GetNickname() + "\r\n");
			SendToClient(client.GetFd(), ":server 366 " + client.GetNickname() + " " + chanName + " :End of /NAMES list.\r\n");
		} else {
			if ( channel->HasMember( client.GetFd() ) ) {
				return ;
			}
			if ( channel->IsInviteOnly() && !channel->IsInvited( client.GetNickname() ) ) {
				SendToClient(client.GetFd(), ":server 473 " + client.GetNickname() + " " + chanName + " :Cannot join channel (+i)\r\n");
				return ;
			}
			if ( !channel->GetKey().empty() && channel->GetKey() != key ) {
				SendToClient(client.GetFd(), ":server 475 " + client.GetNickname() + " " + chanName + " :Cannot join channel (+k)\r\n");
				return ;
			}
			if ( channel->IsFull() ) {
				SendToClient(client.GetFd(), ":server 471 " + client.GetNickname() + " " + chanName + " :Cannot join channel (+l)\r\n");
				return ;
			}

			channel->AddMember( client.GetFd(), false );
			BroadcastToChannel(*channel, joinMsg, -1);

			if ( !channel->GetTopic().empty() ) {
				SendToClient(client.GetFd(), ":server 332 " + client.GetNickname() + " " + chanName + " :" + channel->GetTopic() + "\r\n");
			}

			std::string namesList = "";
			const std::vector<int>& members = channel->GetMembers();
			for ( size_t i = 0; i < members.size(); ++i ) {
				Client* memberClient = FindClientByFd( members[i] );
				if ( memberClient != NULL ) {
					if ( i > 0 ) {
						namesList += " ";
					}
					if ( channel->IsOperator( members[i] ) ) {
						namesList += "@";
					}
					namesList += memberClient->GetNickname();
				}
			}
			SendToClient(client.GetFd(), ":server 353 " + client.GetNickname() + " = " + chanName + " :" + namesList + "\r\n");
			SendToClient(client.GetFd(), ":server 366 " + client.GetNickname() + " " + chanName + " :End of /NAMES list.\r\n");
		}
	} else if ( command == "PART" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " PART :Not enough parameters\r\n");
			return ;
		}
		std::string chanName = tokens[1];
		Channel* channel = FindChannel( chanName );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}
		if ( !channel->HasMember( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + chanName + " :You're not on that channel\r\n");
			return ;
		}
		std::string reason = tokens.size() > 2 ? JoinFrom(tokens, 2) : "Leaving";
		if ( StartsWith(reason, ":") ) {
			reason = reason.substr(1);
		}
		std::string partMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " PART " + chanName + " :" + reason + "\r\n";
		BroadcastToChannel(*channel, partMsg, -1);
		channel->RemoveMember( client.GetFd() );
		RemoveEmptyChannel( chanName );
	} else if ( command == "PRIVMSG" ) {
		if ( tokens.size() < 3 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " PRIVMSG :Not enough parameters\r\n");
			return ;
		}
		std::string target = tokens[1];
		std::string message = JoinFrom(tokens, 2);
		if ( StartsWith(message, ":") ) {
			message = message.substr(1);
		}
		std::string fullMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " PRIVMSG " + target + " :" + message + "\r\n";

		if ( target[0] == '#' ) {
			Channel* channel = FindChannel( target );
			if ( channel == NULL ) {
				SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + target + " :No such channel\r\n");
				return ;
			}
			if ( !channel->HasMember( client.GetFd() ) ) {
				SendToClient(client.GetFd(), ":server 404 " + client.GetNickname() + " " + target + " :Cannot send to channel\r\n");
				return ;
			}
			BroadcastToChannel(*channel, fullMsg, client.GetFd());
		} else {
			Client* targetClient = FindClientByNick( target );
			if ( targetClient == NULL ) {
				SendToClient(client.GetFd(), ":server 401 " + client.GetNickname() + " " + target + " :No such nick/channel\r\n");
				return ;
			}
			SendToClient(targetClient->GetFd(), fullMsg);
		}
	} else if ( command == "KICK" ) {
		if ( tokens.size() < 3 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " KICK :Not enough parameters\r\n");
			return ;
		}
		std::string chanName = tokens[1];
		std::string targetNick = tokens[2];
		std::string reason = tokens.size() > 3 ? JoinFrom(tokens, 3) : "Kicked by operator";
		if ( StartsWith(reason, ":") ) {
			reason = reason.substr(1);
		}

		Channel* channel = FindChannel( chanName );
		if ( channel == NULL ) {
			SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + chanName + " :No such channel\r\n");
			return ;
		}
		if ( !channel->IsOperator( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + chanName + " :You're not channel operator\r\n");
			return ;
		}
		Client* targetClient = FindClientByNick( targetNick );
		if ( targetClient == NULL || !channel->HasMember( targetClient->GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 441 " + client.GetNickname() + " " + targetNick + " " + chanName + " :They isn't on that channel\r\n");
			return ;
		}

		std::string kickMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " KICK " + chanName + " " + targetNick + " :" + reason + "\r\n";
		BroadcastToChannel(*channel, kickMsg, -1);
		channel->RemoveMember( targetClient->GetFd() );
		RemoveEmptyChannel( chanName );
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
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + chanName + " :You're not on that channel\r\n");
			return ;
		}
		if ( channel->IsInviteOnly() && !channel->IsOperator( client.GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + chanName + " :You're not channel operator\r\n");
			return ;
		}
		if ( channel->HasMember( targetClient->GetFd() ) ) {
			SendToClient(client.GetFd(), ":server 443 " + client.GetNickname() + " " + targetNick + " " + chanName + " :is already on channel\r\n");
			return ;
		}

		SendToClient(client.GetFd(), ":server 341 " + client.GetNickname() + " " + targetNick + " " + chanName + "\r\n");
		SendToClient(targetClient->GetFd(), ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " INVITE " + targetNick + " :" + chanName + "\r\n");
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
			SendToClient(client.GetFd(), ":server 442 " + client.GetNickname() + " " + chanName + " :You're not on that channel\r\n");
			return ;
		}

		if ( tokens.size() == 2 ) {
			if ( channel->GetTopic().empty() ) {
				SendToClient(client.GetFd(), ":server 331 " + client.GetNickname() + " " + chanName + " :No topic is set\r\n");
			} else {
				SendToClient(client.GetFd(), ":server 332 " + client.GetNickname() + " " + chanName + " :" + channel->GetTopic() + "\r\n");
			}
		} else {
			if ( channel->IsTopicRestricted() && !channel->IsOperator( client.GetFd() ) ) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + chanName + " :You're not channel operator\r\n");
				return ;
			}
			std::string newTopic = JoinFrom(tokens, 2);
			if ( StartsWith(newTopic, ":") ) {
				newTopic = newTopic.substr(1);
			}
			channel->SetTopic( newTopic );
			std::string topicMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " TOPIC " + chanName + " :" + newTopic + "\r\n";
			BroadcastToChannel(*channel, topicMsg, -1);
		}
	} else if ( command == "MODE" ) {
		if ( tokens.size() < 2 ) {
			SendToClient(client.GetFd(), ":server 461 " + client.GetNickname() + " MODE :Not enough parameters\r\n");
			return ;
		}
		std::string target = tokens[1];
		if ( target[0] == '#' ) {
			Channel* channel = FindChannel( target );
			if ( channel == NULL ) {
				SendToClient(client.GetFd(), ":server 403 " + client.GetNickname() + " " + target + " :No such channel\r\n");
				return ;
			}
			if ( tokens.size() == 2 ) {
				std::string modes = "+";
				if ( channel->IsInviteOnly() ) {
					modes += "i";
				}
				if ( channel->IsTopicRestricted() ) {
					modes += "t";
				}
				if ( !channel->GetKey().empty() ) {
					modes += "k";
				}
				if ( channel->HasUserLimit() ) {
					modes += "l";
				}
				SendToClient(client.GetFd(), ":server 324 " + client.GetNickname() + " " + target + " " + modes + "\r\n");
				return ;
			}

			if ( !channel->IsOperator( client.GetFd() ) ) {
				SendToClient(client.GetFd(), ":server 482 " + client.GetNickname() + " " + target + " :You're not channel operator\r\n");
				return ;
			}

			std::string mode = tokens[2];
			if ( mode == "+i" ) {
				channel->SetInviteOnly( true );
			} else if ( mode == "-i" ) {
				channel->SetInviteOnly( false );
			} else if ( mode == "+t" ) {
				channel->SetTopicRestricted( true );
			} else if ( mode == "-t" ) {
				channel->SetTopicRestricted( false );
			} else if ( mode == "+k" && tokens.size() >= 4 ) {
				channel->SetKey( tokens[3] );
			} else if ( mode == "-k" ) {
				channel->SetKey( "" );
			} else if ( mode == "+o" && tokens.size() >= 4 ) {
				Client* targetClient = FindClientByNick( tokens[3] );
				if ( targetClient != NULL && channel->HasMember( targetClient->GetFd() ) ) {
					channel->AddOperator( targetClient->GetFd() );
				}
			} else if ( mode == "-o" && tokens.size() >= 4 ) {
				Client* targetClient = FindClientByNick( tokens[3] );
				if ( targetClient != NULL && channel->HasMember( targetClient->GetFd() ) ) {
					channel->RemoveOperator( targetClient->GetFd() );
				}
			} else if ( mode == "+l" && tokens.size() >= 4 ) {
				channel->SetUserLimit( ::atoi(tokens[3].c_str()) );
			} else if ( mode == "-l" ) {
				channel->RemoveUserLimit();
			} else {
				return ;
			}

			std::string modeMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " MODE " + target + " " + mode;
			if ( tokens.size() >= 4 ) {
				modeMsg += " " + tokens[3];
			}
			modeMsg += "\r\n";
			BroadcastToChannel(*channel, modeMsg, -1);
		}
	} else if ( command == "QUIT" ) {
		std::string reason = tokens.size() > 1 ? JoinFrom(tokens, 1) : "Client Quit";
		if ( StartsWith(reason, ":") ) {
			reason = reason.substr(1);
		}

		std::string quitMsg = ":" + client.GetNickname() + "!" + client.GetUsername() + "@" + client.GetIpAddress() + " QUIT :" + reason + "\r\n";
		for ( size_t i = 0; i < channels.size(); ++i ) {
			if ( channels[i].HasMember( client.GetFd() ) ) {
				BroadcastToChannel(channels[i], quitMsg, client.GetFd());
				channels[i].RemoveMember( client.GetFd() );
			}
		}

		SendToClient(client.GetFd(), "ERROR :Closing Link: (" + client.GetUsername() + "@" + client.GetIpAddress() + ") [" + reason + "]\r\n");
		ClearClients( client.GetFd() );
		close( client.GetFd() );
		return ;
	} else {
		std::string nick = client.GetNickname().empty() ? "*" : client.GetNickname();
		SendToClient(client.GetFd(), ":server 421 " + nick + " " + command + " :Unknown command\r\n");
	}

	if ( !wasRegistered && client.IsRegistered() ) {
		SendWelcomeMessages( client );
	}
}

void Server::SendWelcomeMessages( Client& client ) {
	std::string nick = client.GetNickname();
	
	SendToClient(client.GetFd(), ":server 001 " + nick + " :Welcome to the ft_irc Network, " + nick + "\r\n");
	SendToClient(client.GetFd(), ":server 002 " + nick + " :Your host is server, running version 1.0\r\n");
	SendToClient(client.GetFd(), ":server 003 " + nick + " :This server was created today\r\n");
	SendToClient(client.GetFd(), ":server 004 " + nick + " :server 1.0 i t k o l\r\n");
	
	// Простейшая реализация MOTD
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
