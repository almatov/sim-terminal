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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include <fcntl.h>
#include <gpiod.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace std;

constexpr const char*       SERVER_IP_          = "127.0.0.1";
constexpr const unsigned    SERVER_PORT_        = 51173;
constexpr const char*       TTY_DEVICE_         = "/dev/ttyS3";
constexpr const char*       TTY_LINE_DELIMITER_ = "\r\n";
constexpr const char*       HEX_CHAR_           = "0123456789ABCDEF";
constexpr const char*       GPIO_DEVICE_        = "/dev/gpiochip0";
constexpr const int         GPIO_LINE_PWRKEY_   = 203;                  // PG11 of the NanoPi Duo2

/**************************************************************************************************************/
void
powerReset()
{
    struct gpiod_chip*  chip = gpiod_chip_open( GPIO_DEVICE_ );

    if ( chip == nullptr )
    {
        throw runtime_error( "cannot open GPIO device" );
    }

    extern char*                    program_invocation_short_name;
    unsigned                        line = GPIO_LINE_PWRKEY_;
    struct gpiod_line_settings*     settings = gpiod_line_settings_new();
    struct gpiod_request_config*    reqConfig = gpiod_request_config_new();
    struct gpiod_line_config*       lineConfig = gpiod_line_config_new();

    gpiod_line_settings_set_direction( settings, GPIOD_LINE_DIRECTION_OUTPUT );
    gpiod_line_config_add_line_settings( lineConfig, &line, 1, settings );
    gpiod_request_config_set_consumer( reqConfig, program_invocation_short_name );

    struct gpiod_line_request*      gpioRequest = gpiod_chip_request_lines( chip, reqConfig, lineConfig );

    if ( gpioRequest == nullptr )
    {
        throw runtime_error( "failed to request GPIO lines" );
    }

    gpiod_request_config_free( reqConfig );
    gpiod_line_config_free( lineConfig );
    gpiod_line_settings_free( settings );
    gpiod_chip_close( chip );

    enum gpiod_line_value           lineValue = GPIOD_LINE_VALUE_ACTIVE;

    gpiod_line_request_set_value( gpioRequest, line, lineValue );
    usleep( 1600000 );
    lineValue = ( lineValue == GPIOD_LINE_VALUE_ACTIVE )? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
    gpiod_line_request_set_value( gpioRequest, line, lineValue );
    usleep( 1200000 );
    lineValue = ( lineValue == GPIOD_LINE_VALUE_ACTIVE )? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
    gpiod_line_request_set_value( gpioRequest, line, lineValue );
    usleep( 1200000 );
    lineValue = ( lineValue == GPIOD_LINE_VALUE_ACTIVE )? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
    gpiod_line_request_set_value( gpioRequest, line, lineValue );
    usleep( 1900000 );
    gpiod_line_request_release( gpioRequest );
}

/**************************************************************************************************************/
vector<string>
ttyRead( int ttyFd )
{
    string          response;
    vector<string>  result;
    static char     readBuffer[ 256 ];
    int             nBytes = read( ttyFd, readBuffer, sizeof(readBuffer) );

    if ( nBytes > 0 )
    {
        while ( true )
        {
            response.append( readBuffer, nBytes );

            if ( readBuffer[nBytes - 1] == '\n' )
            {
                break;
            }

            do
            {
                usleep( 10000 );
                nBytes = read( ttyFd, readBuffer, sizeof(readBuffer) );
            }
            while ( nBytes <= 0 );
        }

        string::size_type  lineBegin = 0;
        string::size_type  lineEnd;

        do
        {
            lineEnd = response.find( TTY_LINE_DELIMITER_, lineBegin );
            result.push_back( response.substr(lineBegin, lineEnd-lineBegin) );
            lineBegin = lineEnd + strlen( TTY_LINE_DELIMITER_ );
        }
        while ( lineEnd < response.size() );
    }

    return move( result );
}

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
    powerReset();

    struct termios  term;
    int             ttyFd = open( TTY_DEVICE_, O_RDWR | O_NOCTTY | O_NDELAY );

    if ( ttyFd < 0 || tcgetattr(ttyFd, &term) < 0 )
    {
        cerr << "cannot open " << TTY_DEVICE_ << " device\n";
        return EXIT_FAILURE;
    }

    cfsetispeed( &term, B115200 );
    cfsetospeed( &term, B115200 );
    term.c_iflag &= ~( IXANY | IXON | IXOFF | BRKINT | ICRNL );
    term.c_oflag &= ~( OPOST );
    term.c_lflag &= ~( ISIG | ICANON | IEXTEN | ECHO );
    term.c_cflag |= ( CLOCAL | CREAD | CRTSCTS );
    term.c_cc[ VMIN ] = 1;
    term.c_cc[ VTIME ] = 10;

    if ( tcsetattr(ttyFd, TCSANOW, &term) < 0 )
    {
        cerr << "cannot set attributes to " << TTY_DEVICE_ << " device\n";
        return EXIT_FAILURE;
    }

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
    cout << TTY_DEVICE_ << " ready " << SERVER_IP_ << ":" << SERVER_PORT_ << endl;

    int     clientSocket = -1;

    while ( true )
    {
        if ( clientSocket < 0 )
        {
            struct sockaddr_in  clientAddress;
            socklen_t           clientLength = sizeof( clientAddress );

            clientSocket = accept( serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientLength );
        }

        for ( auto& line : ttyRead(ttyFd) )
        {
            static bool     isSms = false;

            if ( line == "RING" )
            {
                write( ttyFd, "ATH\n", 4 );
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
            if ( memcmp(readBuffer, "AT", 2) == 0 )
            {
                write( ttyFd, readBuffer, nBytes );
                continue;
            }

            cout.write( readBuffer, nBytes );

            string  command( readBuffer, nBytes - 1 );

            if ( command == "kill" )
            {
                close( clientSocket );
                break;
            }
            else if ( command == "quit" )
            {
                close( clientSocket );
                clientSocket = -1;
                continue;
            }

            string  ttyCommand;

            if ( command == "list" )
            {
                ttyCommand = "AT+CMGF=1\nAT+CMGL=\"all\"\n";
            }
            else if ( command == "delete" )
            {
                ttyCommand = "AT+CMGD=1,1\n";
            }
            else if ( command == "balance" )
            {
                ttyCommand = "AT+CUSD=1,\"*111#\"\n";
            }
            else if ( command.compare(0, 6, "send +") == 0 )
            {
                string  number( command.erase(0, 5), 0, command.find_first_of(' ') );
                string  body( command.erase(0, number.size()), command.find_first_not_of(' ') );;

                ttyCommand = "AT+CMGF=1\nAT+CMGS=\"" + number + "\"\n" + body + "\x1a\n";
            }
            else
            {
                write( clientSocket, "UNKNOWN\n", 8 );
                continue;
            }

            write( ttyFd, ttyCommand.c_str(), ttyCommand.size() );
        }

        usleep( 50000 );
    }

    close( serverSocket );
    close( ttyFd );
    return EXIT_SUCCESS;
}
