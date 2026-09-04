#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <csignal>

#define RED "\e[1;31m"
#define WHI "\e[0;37m"
#define GRE "\e[1;32m"
#define YEL "\e[1;33m"

class Client {
	private:
		int 		_fd;
		std::string _ipAddress;
		std::string _buffer;
		std::string _nickname;
		std::string _username;
		std::string _realname;
		bool		_passAccepted;
		bool		_nickAccepted;
		bool		_userAccepted;
		bool		_clientDisconnected;
		std::string _outBuffer;

	public:
					Client( void );
					Client( const Client& other );
					~Client( void );
		Client& 	operator=( const Client& other );

		int 		GetFd( void ) const;
		std::string GetIpAddress( void ) const;
		std::string GetBuffer( void ) const;
		const std::string& GetOutBuffer( void ) const;
		std::string GetNickname( void ) const;
		std::string GetUsername( void ) const;
		std::string GetRealname( void ) const;
		bool 		HasAcceptedPass( void ) const;
		bool 		HasNickname( void ) const;
		bool 		HasUsername( void ) const;
		bool 		IsRegistered( void ) const;
		bool 		IsDisconnected( void ) const;

		void 		SetFd( int fd );
		void 		setIpAdd( std::string ipAddress );
		void 		AppendBuffer( const std::string& data );
		void 		ClearBuffer( void );
		void 		SetNickname( const std::string& nickname );
		void 		SetUsername( const std::string& username );
		void 		SetRealname( const std::string& realname );
		void 		SetPassAccepted( bool accepted );
		void 		SetNickAccepted( bool accepted );
		void 		SetUserAccepted( bool accepted );
		void 		AppendOutBuffer( const std::string& data );
        void 		EraseOutBuffer( size_t count );
		void 		SetDisconnected( bool disconnected );
};

#endif
