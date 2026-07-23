/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/05 22:58:47 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/23 19:46:47 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <cstdlib>
#include <iostream>
#include <csignal>
#include <sstream>

// Проверка порта на соответствие RFC (1-65535) и отсутствие нечисловых символов
static bool IsValidPort( const std::string& portStr ) {
	if ( portStr.empty() ) {
		return ( false );
	}
	for ( size_t i = 0; i < portStr.length(); ++i ) {
		if ( !std::isdigit(portStr[i]) ) {
			return ( false );
		}
	}

	long port;
	std::istringstream stream(portStr);
	stream >> port;

	return ( port > 0 && port <= 65535 );
}

int main( int argc, char **argv ) {
	if ( argc != 3 ) {
		std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
		return ( 1 );
	}

	std::string portStr = argv[1];
	if ( !IsValidPort(portStr) ) {
		std::cerr << "Error: Invalid port. Must be a number between 1 and 65535." << std::endl;
		return ( 1 );
	}

	std::string password = argv[2];
	if ( password.empty() ) {
		std::cerr << "Error: Password cannot be empty." << std::endl;
		return ( 1 );
	}

	// Перехват сигналов завершения (Ctrl+C, Ctrl+\) для корректного вызова деструкторов
	signal( SIGINT, Server::SignalHandler );
	signal( SIGQUIT, Server::SignalHandler );
	// Игнорирование SIGPIPE для предотвращения падения при отправке в закрытый сокет
	signal( SIGPIPE, SIG_IGN );

	int port = std::atoi(portStr.c_str());
	Server server(port, password);

	std::cout << "Server initialization starting..." << std::endl;

	try {
		server.ServerInit();
	}
	catch ( const std::exception& e ) {
		std::cerr << "Error: " << e.what() << std::endl;
		return ( 1 );
	}

	std::cout << "Server shutdown gracefully." << std::endl;
	return ( 0 );
}