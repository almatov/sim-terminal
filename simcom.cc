/* simcom.cc
****************************************************************************************************************
****************************************************************************************************************

    Copyright (C) 2024 Askar Almatov

    This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
    License for more details.

    You should have received a copy of the GNU General Public License along with this program.  If not, see
    <https://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "simcom.h"

using std::runtime_error;
using std::string;
using std::vector;

constexpr const char*       UART_LINE_DELIMITER_ = "\r\n";

/**************************************************************************************************************/
inline enum gpiod_line_value
toggleValue( enum gpiod_line_value value )
{
    return ( value == GPIOD_LINE_VALUE_ACTIVE )? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;
}

/**************************************************************************************************************/
Simcom::Simcom
(
    const char*             uartDevice,
    const char*             gpioDevice,
    unsigned                pwrkeyLine,
    enum gpiod_line_value   pwrkeyDefaultValue
)
{
    struct gpiod_chip*  chip = gpiod_chip_open( gpioDevice );

    if ( chip == nullptr )
    {
        throw runtime_error( "cannot open GPIO device" );
    }

    extern char*                    program_invocation_short_name;
    unsigned                        line = pwrkeyLine;
    enum gpiod_line_value           lineValue = pwrkeyDefaultValue;
    struct gpiod_line_settings*     settings = gpiod_line_settings_new();
    struct gpiod_request_config*    reqConfig = gpiod_request_config_new();
    struct gpiod_line_config*       lineConfig = gpiod_line_config_new();

    gpiod_line_settings_set_direction( settings, GPIOD_LINE_DIRECTION_OUTPUT );
    gpiod_line_settings_set_output_value( settings, lineValue );
    gpiod_line_config_add_line_settings( lineConfig, &line, 1, settings );
    gpiod_request_config_set_consumer( reqConfig, program_invocation_short_name );
    gpioRequest_ = gpiod_chip_request_lines( chip, reqConfig, lineConfig );

    if ( gpioRequest_ == nullptr )
    {
        throw runtime_error( "failed to request GPIO lines" );
    }

    gpiod_request_config_free( reqConfig );
    gpiod_line_config_free( lineConfig );
    gpiod_line_settings_free( settings );
    gpiod_chip_close( chip );

    // Power reset
    usleep( 50000 );
    lineValue = toggleValue( lineValue );
    gpiod_line_request_set_value( gpioRequest_, line, lineValue );
    usleep( 1600000 );
    lineValue = toggleValue( lineValue );
    gpiod_line_request_set_value( gpioRequest_, line, lineValue );
    usleep( 1200000 );
    lineValue = toggleValue( lineValue );
    gpiod_line_request_set_value( gpioRequest_, line, lineValue );
    usleep( 1200000 );
    lineValue = toggleValue( lineValue );
    gpiod_line_request_set_value( gpioRequest_, line, lineValue );
    usleep( 1900000 );

    // Open UART TTY
    uartFd_ = open( uartDevice, O_RDWR | O_NOCTTY | O_NDELAY );

    struct termios  term;

    if ( uartFd_ < 0 || tcgetattr(uartFd_, &term) < 0 )
    {
        throw runtime_error( "cannot open UART device" );
    }

    cfsetispeed( &term, B115200 );
    cfsetospeed( &term, B115200 );
    term.c_iflag &= ~( IXANY | IXON | IXOFF | BRKINT | ICRNL );
    term.c_oflag &= ~( OPOST );
    term.c_lflag &= ~( ISIG | ICANON | IEXTEN | ECHO );
    term.c_cflag |= ( CLOCAL | CREAD | CRTSCTS );
    term.c_cc[ VMIN ] = 1;
    term.c_cc[ VTIME ] = 10;

    if ( tcsetattr(uartFd_, TCSANOW, &term) < 0 )
    {
        throw runtime_error( "cannot set attributes to UART device" );
    }
}

/**************************************************************************************************************/
Simcom::~Simcom()
{
    close( uartFd_ );
    gpiod_line_request_release( gpioRequest_ );
}

/**************************************************************************************************************/
void
Simcom::uartWrite( const string& command )
{
    write( uartFd_, command.c_str(), command.size() );
}

/**************************************************************************************************************/
vector<string>
Simcom::uartRead()
{
    string          response;
    vector<string>  result;
    static char     readBuffer[ 256 ];
    int             nBytes = read( uartFd_, readBuffer, sizeof(readBuffer) );

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
                nBytes = read( uartFd_, readBuffer, sizeof(readBuffer) );
            }
            while ( nBytes <= 0 );
        }

        string::size_type  lineBegin = 0;
        string::size_type  lineEnd;

        do
        {
            lineEnd = response.find( UART_LINE_DELIMITER_, lineBegin );
            result.push_back( response.substr(lineBegin, lineEnd-lineBegin) );
            lineBegin = lineEnd + strlen( UART_LINE_DELIMITER_ );
        }
        while ( lineEnd < response.size() );
    }

    return move( result );
}

