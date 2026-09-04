#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Channel.hpp"

class Server
{
	private:
		int _port;
		std::string _password;
		int _serverFd;
		static volatile sig_atomic_t Signal;
		std::vector<Client> clients;
		std::vector<Channel> channels;
		std::vector<struct pollfd> fds;

	public:
		Server( void );
		Server( int port, const std::string& password );
		Server( const Server& other );
		Server& operator=( const Server& other );
		~Server( void );

		int getServerFd( void ) const;
		int getPort( void ) const;

		void ServerInit( void );
		void SerSocket( void );
		void AcceptNewClient( void );
		void ReceiveNewData( int fd );

		static void SignalHandler( int signum );

		void CloseFds( void );
		void ClearClients( int fd );

	private:
		Client* FindClientByFd( int fd );
		size_t CountRegisteredClients( void ) const;
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
