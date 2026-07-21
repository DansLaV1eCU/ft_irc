/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: danslav1e <danslav1e@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/05 22:58:47 by danslav1e         #+#    #+#             */
/*   Updated: 2026/07/20 18:23:05 by danslav1e        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <cstdlib>
#include <iostream>

int main( int argc, char **argv ) {
	if (argc != 3) {
		std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
		return (1);
	}

	int port = ::atoi(argv[1]);
	std::string password = argv[2];
	Server server(port, password);

	std::cout << "Server:" << std::endl;

	try {
		server.ServerInit();
	}
	catch ( const std::exception& e ) {
		std::cerr << "Error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}