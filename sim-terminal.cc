/* sim-terminal.cc
****************************************************************************************************************
****************************************************************************************************************

    Copyright (C) 2020, 2021, 2022, 2023, 2024 Askar Almatov

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License along with this program.  If not, see
    <https://www.gnu.org/licenses/>.
*/

#include <codecvt>
#include <cstring>
#include <iostream>
#include <locale>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "simcom.h"

using namespace std;

constexpr const char*       UART_DEVICE_        = "/dev/ttyS3";
constexpr const char*       GPIO_DEVICE_        = "/dev/gpiochip0";
constexpr const int         GPIO_PWRKEY_LINE_   = 203;                          // PG11 of the NanoPi Duo2
constexpr const auto        GPIO_PWRKEY_VALUE_  = GPIOD_LINE_VALUE_INACTIVE;    // inverted PWRKEY control
constexpr const char*       SERVER_IP_          = "127.0.0.1";
constexpr const unsigned    SERVER_PORT_        = 51173;
constexpr const char*       HEX_CHAR_           = "0123456789ABCDEF";

/**************************************************************************************************************/
void
tryConvert( string& line )
{
    string              result;
    string::size_type   tokenStart = 0;
    string::size_type   lookupStart = 0;

    while ( true )
    {
        string::size_type   tokenEnd = line.find( '\"', lookupStart );

        if ( string::npos != tokenEnd && (0 == tokenStart || '\"' == line.at(tokenStart)) )
        {
            ++tokenEnd;
        }

        string  token( line, tokenStart, tokenEnd - tokenStart );

        if ( token.empty() || (token.size() & 3) != 0 || token.find_first_not_of(HEX_CHAR_) != string::npos )
        {
            result.append( token );
        }
        else
        {
            static wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t>  cv;
            u16string                                                       s16;

            for ( int i = 0; i < token.size(); i += 4 )
            {
                s16.append( 1, stoi(token.substr(i,4), nullptr, 16) );
            }

            result.append( cv.to_bytes(s16) );
        }

        if ( string::npos == tokenEnd )
        {
            break;
        }

        tokenStart = lookupStart = tokenEnd;

        if ( tokenStart < line.size() && '\"' == line.at(tokenStart) )
        {
            ++lookupStart;
        }
    }

    line = move( result );
}

/**************************************************************************************************************/
int
main( int argc, char* argv[] )
{
    Simcom  sim( UART_DEVICE_, GPIO_DEVICE_, GPIO_PWRKEY_LINE_, GPIO_PWRKEY_VALUE_ );
    int     serverSocket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    int     socketOption = 1;

    setsockopt( serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &socketOption, sizeof(socketOption) );
    fcntl( serverSocket, F_SETFL, O_NONBLOCK );

    struct sockaddr_in  serverAddress;

    memset( &serverAddress, 0, sizeof(serverAddress) );
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr( SERVER_IP_ );
    serverAddress.sin_port = htons( SERVER_PORT_ );

    if ( bind(serverSocket, reinterpret_cast<const struct sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0 )
    {
        cerr << "cannot bind socket " << SERVER_IP_ << ":" << SERVER_PORT_ << "\n";
        return EXIT_FAILURE;
    }

    listen( serverSocket, 1 );
    cout << UART_DEVICE_ << " ready " << SERVER_IP_ << ":" << SERVER_PORT_ << endl;
    sim.uartWrite( "AT+CMGF=1\n" );     // set text mode

    int     clientSocket = -1;

    while ( true )
    {
        if ( clientSocket < 0 )
        {
            struct sockaddr_in  clientAddress;
            socklen_t           clientLength = sizeof( clientAddress );

            clientSocket = accept( serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientLength );
        }

        for ( auto& line : sim.uartRead() )
        {
            static bool     isSms = false;

            if ( line == "RING" )
            {
                sim.uartWrite( "ATH\n" );
            }
            else if ( line.find("+CMTI:") == 0 )
            {
                sim.uartWrite( "AT+CMGL=\"rec unread\"\nAT+CMGD=1,1\n" );
            }
            else if ( !isSms && line.find("+CMGL:") == 0 )
            {
                isSms = true;
            }
            else if ( isSms && line == "OK" )
            {
                isSms = false;
            }

            if ( isSms || line.find("+CUSD:") == 0 )
            {
                tryConvert( line );
            }

            line += '\n';
            cout.write( line.c_str(), line.size() );
            write( clientSocket, line.c_str(), line.size() );
        }

        static char     readBuffer[ 256 ];
        int             nBytes;

        nBytes = recv( clientSocket, readBuffer, sizeof(readBuffer), MSG_DONTWAIT );

        if ( nBytes > 0 )
        {
            string  clientCommand( readBuffer, nBytes - 1 );

            if ( clientCommand.compare(0, 2, "AT") == 0 )
            {
                sim.uartWrite( clientCommand + '\n' );
                continue;
            }

            cout.write( readBuffer, nBytes );

            if ( clientCommand == "kill" )
            {
                close( clientSocket );
                break;
            }
            else if ( clientCommand == "quit" )
            {
                close( clientSocket );
                clientSocket = -1;
                continue;
            }

            string  simCommand;

            if ( clientCommand == "balance" )
            {
                simCommand = "AT+CUSD=1,\"*111#\"\n";
            }
            else if ( clientCommand.compare(0, 6, "send +") == 0 )
            {
                string  number( clientCommand.erase(0, 5), 0, clientCommand.find_first_of(' ') );
                string  body( clientCommand.erase(0, number.size()), clientCommand.find_first_not_of(' ') );

                simCommand = "AT+CMGF=1\nAT+CMGS=\"" + number + "\"\n" + body + "\x1a\n";
            }
            else
            {
                write( clientSocket, "UNKNOWN\n", 8 );
                continue;
            }

            sim.uartWrite( simCommand );
        }

        usleep( 50000 );
    }

    close( serverSocket );
    return EXIT_SUCCESS;
}
