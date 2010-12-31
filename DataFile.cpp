/*

DataFile.cpp ... Memory-mapped file class

Copyright (C) 2009  KennyTM~

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <cstdarg>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include "DataFile.h"

using namespace std;

TRException::TRException(const char* format, ...) {
	va_list arguments;
	va_start(arguments, format);
	int string_length = vsnprintf(NULL, 0, format, arguments);
	m_error = new char[string_length+1];
	vsnprintf(m_error, string_length, format, arguments);
	va_end(arguments);
}

TRException::~TRException() throw() {
	delete[] m_error; 
}


DataFile::DataFile(const char* path) : m_fd(open(path, O_RDONLY)), m_location(0) {
	if (m_fd == -1) {
		throw TRException("DataFile::DataFile(const char*):\n\tFail to open \"%s\".", path);
	}
	
	struct stat file_stat;
	fstat(m_fd, &file_stat);
	m_filesize = file_stat.st_size;
	
	m_data = (char*)mmap(NULL, static_cast<size_t>(m_filesize), PROT_READ, MAP_SHARED, m_fd, 0);
	if (m_data == MAP_FAILED) {
		close(m_fd);
		throw TRException("DataFile::DataFile(const char*):\n\tFail to map \"%s\" into memory.", path);
	}
}
		
unsigned DataFile::read_integer() throw() {
	union {
		char as_char_array[sizeof(unsigned)];
		unsigned as_integer;
	} res;
	memcpy(&res, m_data + m_location, sizeof(unsigned));
	m_location += sizeof(unsigned);
	return res.as_integer;
}

const char* DataFile::read_string(size_t* p_string_length) throw() {
	const char* retval = m_data+m_location;
	size_t string_length = strlen(retval);
	if (p_string_length != NULL)
		*p_string_length = string_length;
	m_location += string_length + 1;
	return retval;
}
const char* DataFile::read_ASCII_string(size_t* p_string_length) throw() {
	const char* retval = m_data+m_location;
	const char* x = retval;
	
	size_t string_length = 0;
	while (*x == '\t' || *x == '\n' || *x == '\r' || (*x >= ' ' && *x <= '~')) {
		++x;
		++string_length;
	}
	
	if (p_string_length != NULL)
		*p_string_length = string_length;
	m_location += string_length;
	
	if (string_length > 0)
		return retval;
	else
		return NULL;
}

const char* DataFile::peek_ASCII_Cstring_at(off_t offset, size_t* p_string_length) const throw() {
	if (offset >= m_filesize)
		return NULL;
	
	const char* retval = m_data + offset;
	const char* x = retval;
	
	off_t string_length = 0;
	while (*x == '\t' || *x == '\n' || (*x >= ' ' && *x <= '~')) {
		++x;
		++string_length;
		if (offset + string_length >= m_filesize) {
			if (p_string_length != NULL)
				*p_string_length = 0;
			return NULL;
		}
	}
	
	if (*x == '\0') {
		if (p_string_length != NULL)
			*p_string_length = static_cast<size_t>(string_length);
		return retval;
	} else {
		if (p_string_length != NULL)
			*p_string_length = 0;
		return NULL;
	}
}

const char* DataFile::read_raw_data(size_t data_size) throw() {
	const char* retval = m_data+m_location;
	m_location += data_size;
	return retval;
}

DataFile::~DataFile() throw() {
	munmap(m_data, static_cast<size_t>(m_filesize));
	close(m_fd);
}

bool DataFile::search_forward(const char* data, size_t length) throw() {
	if (length > 0) {
		while (true) {
			if (static_cast<off_t>(length) + m_location > m_filesize) goto eof;
			
			const char* loc = reinterpret_cast<const char*>(std::memchr(m_data + m_location, data[0], m_filesize - m_location - length));
			if (loc == NULL) goto eof;
			
			m_location = loc - m_data;
			if (static_cast<off_t>(length) + m_location > m_filesize) goto eof;
			
			if (std::memcmp(m_data + m_location, data, length) == 0)
				return true;
			
			++ m_location;
		}
	} else
		return true;

	
eof:
	m_location = m_filesize;
	return false;
}
