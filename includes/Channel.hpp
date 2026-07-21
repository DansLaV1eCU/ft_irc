#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>

class Channel
{
	private:
		std::string _name;
		std::string _topic;
		std::string _key;
		std::vector<int> _members;
		std::vector<int> _operators;
		std::vector<std::string> _invited;
		bool _inviteOnly;
		bool _topicRestricted;
		int _userLimit;

	public:
		Channel( void );
		Channel( const std::string& name, int creatorFd );
		Channel( const Channel& other );
		~Channel( void );
		Channel& operator=( const Channel& other );

		const std::string& GetName( void ) const;
		const std::string& GetTopic( void ) const;
		const std::string& GetKey( void ) const;
		const std::vector<int>& GetMembers( void ) const;
		bool IsInviteOnly( void ) const;
		bool IsTopicRestricted( void ) const;
		int GetUserLimit( void ) const;
		bool HasMember( int fd ) const;
		bool IsOperator( int fd ) const;
		bool IsInvited( const std::string& nickname ) const;
		bool IsEmpty( void ) const;
		bool HasUserLimit( void ) const;
		bool IsFull( void ) const;

		void SetTopic( const std::string& topic );
		void SetKey( const std::string& key );
		void SetInviteOnly( bool enabled );
		void SetTopicRestricted( bool enabled );
		void SetUserLimit( int limit );
		void RemoveUserLimit( void );
		void AddMember( int fd, bool asOperator );
		void RemoveMember( int fd );
		void AddOperator( int fd );
		void RemoveOperator( int fd );
		void Invite( const std::string& nickname );
		void RemoveInvite( const std::string& nickname );
};

#endif