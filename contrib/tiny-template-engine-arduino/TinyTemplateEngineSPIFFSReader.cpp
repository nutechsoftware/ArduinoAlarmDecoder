/*
MIT License

Copyright (c) 2019 full-stack-ex

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "TinyTemplateEngineSPIFFSReader.h"
#include <string.h>

TinyTemplateEngineSPIFFSReader::TinyTemplateEngineSPIFFSReader(File *fd):
    Reader(),
    _fd(fd),
    _buffer({0}) {}

void TinyTemplateEngineSPIFFSReader::reset() {
    _fd->seek(0);
}

TinyTemplateEngine::Line TinyTemplateEngineSPIFFSReader::nextLine() {
    if (! _fd->available()) return TinyTemplateEngine::Line(0, 0);
    // consume bytes looking for \n or if > 200 any non template character.
    uint8_t i = 0;
    char c;
    do {
      c = _fd->read();
      _buffer[i++] = c;
      if (c == '\n')
       break;
      if ( i > 200 ) {
        if (c != '{' && c != '}' && c != '$' && (c <= '0' || c >='9'))
          break;
      }
      // guard
      if ( i == sizeof(_buffer)-1) {
        break;
      }
    } while(_fd->available());
    return TinyTemplateEngine::Line(_buffer, i);
}
