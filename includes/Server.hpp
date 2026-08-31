/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/06 22:25:20 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/22 21:34:37 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Channel.hpp"

class Server //-> class for server
{
	private:
		int _port; //-> server port
		std::string _password; //-> server password
		int _serverFd; //-> server socket file descriptor
		static volatile sig_atomic_t Signal; //-> set from the signal handler; sig_atomic_t is the only type safe to touch there
		std::vector<Client> clients; //-> vector of clients
		std::vector<Channel> channels; //-> vector of channels
		std::vector<struct pollfd> fds; //-> vector of pollfd
		
	public:
		Server( void ); //-> default constructor
		Server( int port, const std::string& password ); //-> configured constructor
		Server( const Server& other ); //-> copy constructor
		Server& operator=( const Server& other ); //-> copy assignment operator
		~Server( void ); //-> destructor
		
		int getServerFd( void ) const; //-> get server socket file descriptor
		int getPort( void ) const; //-> get server port

		void ServerInit( void ); //-> server initialization
		void SerSocket( void ); //-> server socket creation
		void AcceptNewClient( void ); //-> accept new client
		void ReceiveNewData( int fd ); //-> receive new data from a registered client

		static void SignalHandler( int signum ); //-> signal handler
	
		void CloseFds( void ); //-> close file descriptors
		void ClearClients( int fd ); //-> clear clients

	private:
		Client* FindClientByFd( int fd );
		Client* FindClientByNick( const std::string& nickname );
		Channel* FindChannel( const std::string& name );
		void RemoveEmptyChannel( const std::string& name );
		void SendToClient( int fd, const std::string& message );
		void BroadcastToChannel( Channel& channel, const std::string& message, int exceptFd );
		void ProcessLine( Client& client, const std::string& line );
		void SendWelcomeMessages( Client& client );
		void BroadcastToPeers( Client& client, const std::string& message );
		void ApplyChannelModes( Client& client, Channel& channel, const std::vector<std::string>& tokens );
		void RenameInvites( const std::string& oldNick, const std::string& newNick );
		void RemoveAllEmptyChannels( void );
		void DisconnectClient( int fd, const std::string& reason );
		void JoinOneChannel( Client& client, const std::string& chanName, const std::string& key );
		void PartOneChannel( Client& client, const std::string& chanName, const std::string& reason );
		void DeliverMessage( Client& client, const std::string& target, const std::string& message, bool isNotice );
		std::string BuildNamesList( Channel& channel );
};

#endif