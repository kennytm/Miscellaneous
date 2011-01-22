#
#    lzss.py ... Decompress an lzss-compressed file.
#    Copyright (C) 2011  KennyTM~ <kennytm@gmail.com>
#    
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#    
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#    
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

from struct import Struct
from itertools import cycle, islice
import os


class RingBuffer(object):
    def __init__(self, size):
        assert not (size & (size-1))
        self.content = bytearray(size)
        self.size = size
        self.cur = 0
    
    def append(self, byte):
        cur = self.cur
        self.content[cur] = byte
        self.cur = (cur + 1) & (self.size - 1)

    def extend(self, bs):
        cur = self.cur
        size = self.size
        
        bslen = len(bs)
        excess = bslen + cur - size
        content = self.content
        
        if excess < 0:
            content[cur : cur+bslen] = bs
            cur += bslen
        elif excess < size:
            first_part_len = size - cur
            content[cur:] = bs[:first_part_len]
            content[:excess] = bs[first_part_len:]
            cur = excess
        else:
            excess &= size-1
            content[excess:] = bs[-size:-excess]
            content[:excess] = bs[-excess:]
            cur = excess
            
        self.cur = cur
            
            
    def _lift_limited(self, begin, end):
        cur = self.cur
        content_length = end-begin
        should_extend = True
        
        if begin >= cur or cur >= end:
            content = self.content[begin:end]
            should_extend = begin != cur
        else:
            content = islice(cycle(self.content[begin:cur]), content_length)
        
        if should_extend:
            content = bytes(content)
            self.extend(content)
        else:
            self.cur = end
        return content
            

    def lift(self, begin, end):
        size = self.size
        size_bits = size.bit_length() - 1
        _lift_limited = self._lift_limited
        
        boundary_count = (end >> size_bits) - (begin >> size_bits)
        begin &= size-1
        end &= size-1
        
        if boundary_count == 0:
            yield _lift_limited(begin, end)
        
        else:
            for i in range(boundary_count+1):
                if i == 0:
                    yield _lift_limited(begin, size)
                elif i == boundary_count:
                    yield _lift_limited(0, end)
                else:
                    yield _lift_limited(0, size)


def decompress_lzss(fin, fout):
    N = 4096
    F = 18
    THRESHOLD = 2

    rb = RingBuffer(N)
    rb.extend(b' ' * (N-F))

    flags = 0
    
    write_res = bytearray()
    read_res = fin.read()
    length = len(read_res)
    
    idx = 0
    percentage = 0
    target = 0
    
    rba = rb.append
    rbl = rb.lift
    
    try:
        while True:
            if idx >= target:
                print ("<Info> Decompressing LZSS... ({0}%)".format(percentage), end="\r")
                percentage += 1
                target = (percentage * length) // 100
            
                fout.write(write_res)
                write_res = bytearray()
                fw = write_res.append
                fw2 = write_res.extend
        
            flags >>= 1
            if not (flags & 0x100):
                flags = 0xff00 | read_res[idx]
                idx += 1
            if length < 0:
                break
            if flags & 1:
                c = read_res[idx]
                idx += 1
                fw(c)
                rba(c)
            else:
                i = read_res[idx]
                j = read_res[idx+1]
                idx += 2
                i |= (j & 0xf0) << 4
                j = (j & 0x0f) + THRESHOLD
                res = rbl(i, i+j+1)
                for content in res:
                    fw2(content)
                    
    except IndexError:
        pass
        
    print("")    
    fout.write(write_res)

    


def decompressor(fin):
    magic = fin.read(8)
    if magic == b'complzss':
        fin.seek(0x180, os.SEEK_SET)
        return decompress_lzss
    elif magic == b'iBootIm\0':
        return None # decompress_iBootIm
    else:
        return None
    

    
if __name__ == '__main__':
    rb = RingBuffer(8)
    
    rb.extend(b'abcde')
    assert rb.content == b'abcde\0\0\0'
    assert rb.cur == 5
    
    rb.extend(b'fghij')
    assert rb.content == b'ijcdefgh'
    assert rb.cur == 2
    
    rb.extend(b'0123456789')
    assert rb.content == b'67892345'
    assert rb.cur == 4
    
    rb.append(32)
    assert rb.content == b'6789 345'
    assert rb.cur == 5
    
    rb.extend(b'abcdefghijklmnopqrstuvwxyz')
    assert rb.content == b'tuvwxyzs'
    assert rb.cur == 7

    rb.append(48)
    assert rb.content == b'tuvwxyz0'
    assert rb.cur == 0
    
    res = b''.join(rb.lift(0, 254))
    assert res == b'tuvwxyz0' * 31 + b'tuvwxy'
    assert rb.content == b'tuvwxyz0'
    assert rb.cur == 6
    
    res = b''.join(rb.lift(1, 5))
    assert res == b'uvwx'
    assert rb.content == b'wxvwxyuv'
    assert rb.cur == 2
    
    res = b''.join(rb.lift(0, 3))
    assert res == b'wxw'
    assert rb.content == b'wxwxwyuv'
    assert rb.cur == 5
    
    rb.extend(b'01234567')
    assert rb.content == b'34567012'
    assert rb.cur == 5
    
    res = b''.join(rb.lift(7, 70))
    assert rb.content == b'72343456'
    assert rb.cur == 4
    assert res == b'234567234567234567234567234567234567234567234567234567234567234'
    
    res = b''.join(rb.lift(3, 57))  
    assert rb.content == b'44444444'
    assert rb.cur == 2
    assert res == b'4' * 54

