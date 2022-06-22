
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <vector>
#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

using namespace std;

static struct option    LONG_OPTIONS_[] =
{
    { "delete",     no_argument,    nullptr, 'd' },
    { "balance",    no_argument,    nullptr, 'b' },
    { "version",    no_argument,    nullptr, 'v' },
    { "help",       no_argument,    nullptr, 'h' },
    { nullptr,      0,              nullptr,  0  }
};

static const char       TTY_DEVICE_[]       = "/dev/ttyS2";
static const char       LINE_DELIMITER_[]   = "\r\n";
static const char       HEX_CHAR_[]         = "0123456789ABCDEF";

static auto&&           LIST_COMMANDS_ =
{
    "AT\n",
    "AT+CMGF=1\n",
    "AT+CMGL=\"all\"\n"
};

static auto&&           DELETE_COMMANDS_ =
{
    "AT\n",
    "AT+CMGD=1,1\n"
};

static auto&&           BALANCE_COMMANDS_ =
{
    "AT\n",
    "AT+CUSD=1,\"*111#\"\n"
};


/**************************************************************************************************************/
vector<string>
readPort( int fd )
{
    string          response;
    static char     readBuffer[ 128 ];
    int             nBytes = 0;

    do
    {
        nBytes = read( fd, readBuffer, sizeof(readBuffer) );

        if ( nBytes > 0 )
        {
            response.append( readBuffer, nBytes );
        }
    }
    while ( nBytes < 0 || nBytes == sizeof(readBuffer) ); 

    vector<string>      result;
    string::size_type   lineBegin = 0;
    string::size_type   lineEnd;

    do
    {
        lineEnd = response.find( LINE_DELIMITER_, lineBegin );
        result.push_back( response.substr(lineBegin, lineEnd-lineBegin) );
        lineBegin = lineEnd + strlen( LINE_DELIMITER_ );
    }
    while ( lineEnd < response.size() );

    return move( result );
}

/**************************************************************************************************************/
int
main( int argc, char* argv[] )
{
    struct termios  term;
    int             fd = open( TTY_DEVICE_, O_RDWR | O_NOCTTY | O_NDELAY );

    if ( fd < 0 || tcgetattr(fd, &term) < 0 )
    {
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

    if ( tcsetattr(fd, TCSANOW, &term) < 0 )
    {
        return EXIT_FAILURE;
    }

    cout << "Ready" << endl;

    auto&   commands = LIST_COMMANDS_;
    int     wait = 1;
    int     opt;

    while ( (opt = getopt_long(argc,argv,"dbvh",LONG_OPTIONS_,nullptr)) != -1 )
    {
        switch ( opt )
        {
            case 'd':
                commands = DELETE_COMMANDS_;
                break;

            case 'b':
                commands = BALANCE_COMMANDS_;
                wait = 5;
        }
    }

    for ( auto cmd : commands )
    {
        usleep( 100000 );
        write( fd, cmd, strlen(cmd) );
    }

    sleep( wait );

    wstring_convert<codecvt_utf8<char16_t>, char16_t>   cv;

    for ( auto& line : readPort(fd) )
    {
        if ( !line.empty() && line.find_first_not_of(HEX_CHAR_) == string::npos )
        {
            u16string   s16;

            for ( int i = 0; i < line.size(); i += 4 )
            {
                s16.append( 1, stoi(line.substr(i,4), nullptr, 16) );
            }

            cout << cv.to_bytes( s16 ) << endl;
        }
        else
        {
            cout << line << endl;
        }
    }

    close( fd );

    return EXIT_SUCCESS;
}
