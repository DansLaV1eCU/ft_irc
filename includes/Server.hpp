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
		static bool Signal; //-> static boolean for signal
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
		void MaybeRegisterClient( Client& client );
		void SendWelcomeMessages( Client& client );
};

#endif