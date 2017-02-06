/*
    Copyright (c) 2010 Séverin Lemaignan (slemaign@laas.fr)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef MEMORYVIEW_EXCEPTIONS_H
#define MEMORYVIEW_EXCEPTIONS_H

#include <stdexcept>

class MemoryViewException : public std::runtime_error {
	public:
		MemoryViewException() : std::runtime_error("A generic exception has been thrown in memory view.") { }
		MemoryViewException(const char* msg) : std::runtime_error(msg) { }
		MemoryViewException(const std::string& msg) : std::runtime_error(msg.c_str()) { }
};

#endif // MEMORYVIEW_EXCEPTIONS_H
