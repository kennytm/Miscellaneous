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
	
	m_data = static_cast<unsigned char*>(mmap(NULL, static_cast<size_t>(m_filesize), PROT_READ, MAP_SHARED, m_fd, 0));
	if (m_data == MAP_FAILED) {
		close(m_fd);
		throw TRException("DataFile::DataFile(const char*):\n\tFail to map \"%s\" into memory.", path);
	}
}
		
unsigned DataFile::read_integer() throw() {
	unsigned res;
	memcpy(&res, m_data + m_location, sizeof(unsigned));
	m_location += sizeof(unsigned);
	return res;
}

const char* DataFile::read_string(size_t* p_string_length) throw() {
	const char* retval = reinterpret_cast<const char*>(m_data+m_location);
	size_t string_length = strlen(retval);
	if (p_string_length != NULL)
		*p_string_length = string_length;
	m_location += string_length + 1;
	return retval;
}
const char* DataFile::read_ASCII_string(size_t* p_string_length) throw() {
	const char* retval = reinterpret_cast<const char*>(m_data+m_location);
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
	
	const char* retval = reinterpret_cast<const char*>(m_data + offset);
	const char* x = retval;
	
	off_t string_length = 0;
	while (*x == '\t' || *x == '\n' || *x == '\r' || (*x >= ' ' && *x <= '~')) {
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

const unsigned char* DataFile::read_raw_data(size_t data_size) throw() {
	const unsigned char* retval = m_data+m_location;
	m_location += data_size;
	return retval;
}

DataFile::~DataFile() throw() {
	munmap(m_data, static_cast<size_t>(m_filesize));
	close(m_fd);
}

bool DataFile::search_forward(const unsigned char* data, size_t length) throw() {
	if (length > 0) {
		while (true) {
			if (static_cast<off_t>(length) + m_location > m_filesize) goto eof;
			
			const unsigned char* loc = static_cast<const unsigned char*>(std::memchr(m_data + m_location, data[0], m_filesize - m_location - length));
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


#if UNITTEST
#include <cstdlib>
#include <stdexcept>

struct Foo {
    double e;
    unsigned char a;
    unsigned char b;
    unsigned short c;
    float f;
};

#define XSTR(x) #x
#define STR(x) XSTR(x)
#define ASSERT(expr) if(!(expr)) { throw std::logic_error("Assert failed: " #expr " on line " STR(__LINE__)); }

int main () {
    char filename[] = "/tmp/DF_unittest_XXXXXX";
    int fd = mkstemp(filename);
    
    unsigned char info[] = {
        0x78, 0x56, 0x34, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE4, 0xBF,
        0x41, 0x58, 0xD2, 0x04, 0x00, 0xE4, 0x40, 0x46
    };
    write(fd, info, sizeof(info));
    
    try {
        DataFile f (filename);
        ASSERT(f.data() != NULL);
        ASSERT(f.filesize() == sizeof(info));
        ASSERT(f.tell() == 0);
        f.seek(4);
        ASSERT(f.tell() == 4);
        f.retreat(4);
        ASSERT(f.tell() == 0);
        f.advance(4);
        ASSERT(f.tell() == 4);
        f.rewind();
        ASSERT(f.tell() == 0);
        ASSERT(!f.is_eof());
        f.seek(sizeof(info));
        ASSERT(f.is_eof());
        
        f.rewind();
        ASSERT(f.read_integer() == 0x00345678u);
        ASSERT(f.read_integer() == 0);
        ASSERT(f.read_integer() == 0xbfe40000u);
        ASSERT(f.read_char() == 'A');
        ASSERT(f.read_char() == 'X');
        const unsigned char* raw_data = f.read_raw_data(3);
        ASSERT(f.tell() == 17);
        ASSERT(raw_data[0] == 0xd2 && raw_data[1] == 4 && raw_data[2] == 0);
        
        f.retreat(5);
        ASSERT(f.tell() == 12);
        size_t length = 0;
        const char* string = f.read_string(&length);
        ASSERT(!strcmp(string, "AX\xD2\x04"));
        ASSERT(length == 4);
        ASSERT(f.tell() == 17);
        f.retreat(5);
        string = f.read_ASCII_string(&length);
        ASSERT(length == 2);
        ASSERT(!strncmp(string, "AX", length));
        ASSERT(f.tell() == 14);
        
        string = f.peek_ASCII_Cstring_at(0, &length);
        ASSERT(length == 3);
        ASSERT(!strncmp(string, "xV4", length));
        ASSERT(f.tell() == 14);
        
        f.rewind();
        ASSERT(f.copy_data<unsigned short>() == 0x5678);
        ASSERT(f.tell() == 2);
        f.seek(sizeof(info) - 3);
        ASSERT(f.copy_data<unsigned short>() == 0x40e4);
        ASSERT(f.read_data<unsigned short>() == NULL);
        ASSERT(f.tell() == sizeof(info) - 1);
        f.seek(4);
        const Foo* foo = f.peek_data<Foo>();
        ASSERT(foo->e == -0.625 && foo->a == 'A' && foo->b == 'X' && foo->c == 1234 && foo->f == 12345.0f);
        ASSERT(f.tell() == 4);
        ASSERT(!memcmp(f.peek_data<Foo>(), foo, sizeof(*foo)));
        ASSERT(f.tell() == 4);
        ASSERT(f.peek_data<Foo>(1) == NULL);
        ASSERT(f.tell() == 4);
        ASSERT(*(f.peek_data_at<unsigned>(0)) == 0x00345678u);
        ASSERT(f.tell() == 4);
        ASSERT(f.peek_data_at<unsigned>(sizeof(info) - 3) == NULL);
        ASSERT(f.tell() == 4);
        unsigned char target[] = {0xE4, 0xBF};
        ASSERT(f.search_forward(target, sizeof(target)));
        ASSERT(f.tell() == 10);
        f.advance(1);
        ASSERT(!f.search_forward(target, sizeof(target)));
        ASSERT(f.is_eof());
    } catch (std::logic_error e) {
        printf("Unit test failed with exception:\n%s\n\n", e.what());
    }

    close(fd);
    unlink(filename);
    
    printf("Unit test finished.\n");

    return 0;
}
#endif

