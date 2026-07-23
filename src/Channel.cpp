/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/20 20:52:13 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/20 20:52:14 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Channel.hpp"

Channel::Channel( void ) : _inviteOnly(false), _topicRestricted(false), _userLimit(0) {
}

Channel::Channel( const std::string& name, int creatorFd ) : _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(0) {
	AddMember(creatorFd, true);
}

Channel::Channel( const Channel& other ) {
	*this = other;
}

Channel::~Channel( void ) {
}

Channel& Channel::operator=( const Channel& other ) {
	if ( this != &other ) {
		_name = other._name;
		_topic = other._topic;
		_key = other._key;
		_members = other._members;
		_operators = other._operators;
		_invited = other._invited;
		_inviteOnly = other._inviteOnly;
		_topicRestricted = other._topicRestricted;
		_userLimit = other._userLimit;
	}
	return ( *this );
}

const std::string& Channel::GetName( void ) const { return (_name); }
const std::string& Channel::GetTopic( void ) const { return (_topic); }
const std::string& Channel::GetKey( void ) const { return (_key); }
const std::vector<int>& Channel::GetMembers( void ) const { return (_members); }
bool Channel::IsInviteOnly( void ) const { return (_inviteOnly); }
bool Channel::IsTopicRestricted( void ) const { return (_topicRestricted); }
int Channel::GetUserLimit( void ) const { return (_userLimit); }

bool Channel::HasMember( int fd ) const {
	for ( std::vector<int>::const_iterator it = _members.begin(); it != _members.end(); ++it ) {
		if ( *it == fd )
			return ( true );
	}
	return ( false );
}

bool Channel::IsOperator( int fd ) const {
	for ( std::vector<int>::const_iterator it = _operators.begin(); it != _operators.end(); ++it ) {
		if ( *it == fd )
			return ( true );
	}
	return ( false );
}

bool Channel::IsInvited( const std::string& nickname ) const {
	for ( std::vector<std::string>::const_iterator it = _invited.begin(); it != _invited.end(); ++it ) {
		if ( *it == nickname )
			return ( true );
	}
	return ( false );
}

bool Channel::IsEmpty( void ) const {
	return ( _members.empty() );
}

bool Channel::HasUserLimit( void ) const {
	return ( _userLimit > 0 );
}

bool Channel::IsFull( void ) const {
	return ( HasUserLimit() && static_cast<int>(_members.size()) >= _userLimit );
}

void Channel::SetTopic( const std::string& topic ) {
	_topic = topic;
}

void Channel::SetKey( const std::string& key ) {
	_key = key;
}

void Channel::SetInviteOnly( bool enabled ) {
	_inviteOnly = enabled;
}

void Channel::SetTopicRestricted( bool enabled ) {
	_topicRestricted = enabled;
}

void Channel::SetUserLimit( int limit ) {
	_userLimit = limit > 0 ? limit : 0;
}

void Channel::RemoveUserLimit( void ) {
	_userLimit = 0;
}

void Channel::AddMember( int fd, bool asOperator ) {
	if ( !HasMember(fd) )
		_members.push_back(fd);
	if ( asOperator )
		AddOperator(fd);
}

void Channel::RemoveMember( int fd ) {
	for ( std::vector<int>::iterator it = _members.begin(); it != _members.end(); ++it ) {
		if ( *it == fd ) {
			_members.erase(it);
			break;
		}
	}
	RemoveOperator(fd);
}

void Channel::AddOperator( int fd ) {
	if ( IsOperator(fd) )
		return ;
	_operators.push_back(fd);
}

void Channel::RemoveOperator( int fd ) {
	for ( std::vector<int>::iterator it = _operators.begin(); it != _operators.end(); ++it ) {
		if ( *it == fd ) {
			_operators.erase(it);
			break;
		}
	}
}

void Channel::Invite( const std::string& nickname ) {
	if ( !IsInvited(nickname) )
		_invited.push_back(nickname);
}

void Channel::RemoveInvite( const std::string& nickname ) {
	for ( std::vector<std::string>::iterator it = _invited.begin(); it != _invited.end(); ++it ) {
		if ( *it == nickname ) {
			_invited.erase(it);
			break;
		}
	}
}
