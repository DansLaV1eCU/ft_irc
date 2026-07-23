/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/06 22:26:31 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/23 22:21:09 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string> //-> for std::string
#include <sys/socket.h> //-> for socket()
#include <sys/types.h> //-> for socket()
#include <netinet/in.h> //-> for sockaddr_in
#include <fcntl.h> //-> for fcntl()
#include <unistd.h> //-> for close()
#include <arpa/inet.h> //-> for inet_ntoa()
#include <poll.h> //-> for poll()
#include <csignal> //-> for signal()
//-------------------------------------------------------//
#define RED "\e[1;31m" //-> for red color
#define WHI "\e[0;37m" //-> for white color
#define GRE "\e[1;32m" //-> for green color
#define YEL "\e[1;33m" //-> for yellow color
//-------------------------------------------------------//

class Client { //-> class for client
	private:
		int 		_fd; //-> client file descriptor
		std::string _ipAddress; //-> client ip address
		std::string _buffer; //-> buffered incoming data
		std::string _nickname; //-> client nickname
		std::string _username; //-> client username
		std::string _realname; //-> client real name
		bool		_passAccepted; //-> PASS command accepted
		bool		_nickAccepted; //-> NICK command accepted
		bool		_userAccepted; //-> USER command accepted
		bool		_clientDisconnected; //-> client connected state
		std::string _outBuffer;

	public:
					Client( void ); //-> default constructor
					Client( const Client& other ); //-> copy constructor
					~Client( void ); //-> destructor
		Client& 	operator=( const Client& other ); //-> copy assignment operator

		int 		GetFd( void ) const; //-> getter for fd
		std::string GetIpAddress( void ) const; //-> getter for ip address
		std::string GetBuffer( void ) const; //-> getter for input buffer
		const std::string& GetOutBuffer( void ) const;
		std::string GetNickname( void ) const; //-> getter for nickname
		std::string GetUsername( void ) const; //-> getter for username
		std::string GetRealname( void ) const; //-> getter for realname
		bool 		HasAcceptedPass( void ) const; //-> getter for PASS state
		bool 		HasNickname( void ) const; //-> getter for NICK state
		bool 		HasUsername( void ) const; //-> getter for USER state
		bool 		IsRegistered( void ) const; //-> getter for registration state
		bool 		IsDisconnected( void ) const; //-> getter for disconnected state

		void 		SetFd( int fd ); //-> setter for fd
		void 		setIpAdd( std::string ipAddress ); //-> setter for ipadd
		void 		AppendBuffer( const std::string& data ); //-> append incoming data
		void 		ClearBuffer( void ); //-> clear input buffer
		void 		SetNickname( const std::string& nickname ); //-> setter for nickname
		void 		SetUsername( const std::string& username ); //-> setter for username
		void 		SetRealname( const std::string& realname ); //-> setter for realname
		void 		SetPassAccepted( bool accepted ); //-> setter for PASS state
		void 		SetNickAccepted( bool accepted ); //-> setter for NICK state
		void 		SetUserAccepted( bool accepted ); //-> setter for USER state
		void 		AppendOutBuffer( const std::string& data );
        void 		EraseOutBuffer( size_t count );
		void 		SetDisconnected( bool disconnected ); //-> setter for disconnected state
};


#endif
