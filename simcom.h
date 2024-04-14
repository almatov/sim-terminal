/* simcom.h
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

#include <string>
#include <vector>
#include <gpiod.h>

/**************************************************************************************************************/
class Simcom
{
public:
                                        Simcom
                                        (
                                            const char*             uartDevice,
                                            const char*             gpioDevice,
                                            unsigned                pwrkeyLine,
                                            enum gpiod_line_value   pwrkeyDefaultValue
                                        );

                                        ~Simcom();

    void                                uartWrite( const std::string& );
    std::vector<std::string>            uartRead();

protected:
    struct gpiod_line_request*          gpioRequest_;
    int                                 uartFd_;
};
