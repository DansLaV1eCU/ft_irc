#include "../includes/Client.hpp"

Client::Client( void ) : _fd(-1), _passAccepted(false), _nickAccepted(false), _userAccepted(false), _clientDisconnected( false ) {
}

Client::Client( const Client& other ) {
	*this = other;
}

Client& Client::operator=( const Client& other ) {
	if ( this != &other ) {
		this->_fd = other._fd;
		this->_ipAddress = other._ipAddress;
		this->_buffer = other._buffer;
		this->_nickname = other._nickname;
		this->_username = other._username;
		this->_realname = other._realname;
		this->_passAccepted = other._passAccepted;
		this->_nickAccepted = other._nickAccepted;
		this->_userAccepted = other._userAccepted;
		this->_outBuffer = other._outBuffer;
		this->_clientDisconnected = other._clientDisconnected;
	}
	return ( *this );
}

Client::~Client( void ) {
}

int Client::GetFd( void ) const {
	return ( this->_fd );
}

std::string Client::GetIpAddress( void ) const {
	return ( this->_ipAddress );
}

std::string Client::GetBuffer( void ) const {
	return ( this->_buffer );
}

std::string Client::GetNickname( void ) const {
	return ( this->_nickname );
}

std::string Client::GetUsername( void ) const {
	return ( this->_username );
}

std::string Client::GetRealname( void ) const {
	return ( this->_realname );
}

bool Client::HasAcceptedPass( void ) const {
	return ( this->_passAccepted );
}

bool Client::HasNickname( void ) const {
	return ( !this->_nickname.empty() && this->_nickAccepted );
}

bool Client::HasUsername( void ) const {
	return ( !this->_username.empty() && this->_userAccepted );
}

bool Client::IsRegistered( void ) const {
	return ( this->HasAcceptedPass() && this->HasNickname() && this->HasUsername() );
}

void Client::SetFd( int fd ) {
	this->_fd = fd;
}

void Client::setIpAdd( std::string ipAddres ) {
	this->_ipAddress = ipAddres;
}

void Client::AppendBuffer( const std::string& data ) {
	this->_buffer += data;
}

void Client::ClearBuffer( void ) {
	this->_buffer.clear();
}

void Client::SetNickname( const std::string& nickname ) {
	this->_nickname = nickname;
}

void Client::SetUsername( const std::string& username ) {
	this->_username = username;
}

void Client::SetRealname( const std::string& realname ) {
	this->_realname = realname;
}

void Client::SetPassAccepted( bool accepted ) {
	this->_passAccepted = accepted;
}

void Client::SetNickAccepted( bool accepted ) {
	this->_nickAccepted = accepted;
}

void Client::SetUserAccepted( bool accepted ) {
	this->_userAccepted = accepted;
}

const std::string& Client::GetOutBuffer( void ) const {
	return ( this->_outBuffer );
}

void Client::AppendOutBuffer( const std::string& data ) {
	this->_outBuffer += data;
}

void Client::EraseOutBuffer( size_t count ) {
	this->_outBuffer.erase(0, count);
}

bool Client::IsDisconnected( void ) const {
	return ( this->_clientDisconnected );
}

void Client::SetDisconnected( bool disconnected ) {
	this->_clientDisconnected = disconnected;
}
