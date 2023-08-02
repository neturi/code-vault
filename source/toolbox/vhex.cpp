/*
Copyright c1997-2011 Trygve Isaacson. All rights reserved.
This file is part of the Code Vault version 3.3
http://www.bombaydigital.com/
*/

/** @file */

#include "vhex.h"

#include "vtextiostream.h"
#include "vbinaryiostream.h"
#include "vchar.h"

// static
void VHex::bufferToHexString(const Vu8* buffer, Vs64 bufferLength, VString& s, bool wantLeading0x) {
    int hexStringLength = (int)(bufferLength * 2);  // note we don't support string lengths > 32 bits

    if (wantLeading0x)
        hexStringLength += 2;

    s.preflight(hexStringLength);

    char*   hexStringBuffer = s.buffer();
    int     hexStringIndex = 0;
    int     bufferIndex = 0;
    char    highNibbleChar;
    char    lowNibbleChar;

    if (wantLeading0x) {
        hexStringBuffer[hexStringIndex++] = '0';
        hexStringBuffer[hexStringIndex++] = 'x';
    }

    while (bufferIndex < bufferLength) {
        VHex::byteToHexChars(buffer[bufferIndex++], &highNibbleChar, &lowNibbleChar);

        hexStringBuffer[hexStringIndex++] = highNibbleChar;
        hexStringBuffer[hexStringIndex++] = lowNibbleChar;
    }

    s.postflight(hexStringLength);
}

// static
void VHex::hexStringToBuffer(const VString& hexDigits, Vu8* buffer, bool hasLeading0x) {
    int     digitsIndex = 0;
    int     bufferIndex = 0;
    int     numDigits = hexDigits.length();
    bool    oddNumHexDigits = (numDigits % 2) != 0;
    char    highNibbleChar = '0';
    char    lowNibbleChar;

    if (hasLeading0x) {
        digitsIndex += 2;
    }

    while (digitsIndex < numDigits) {
        if (oddNumHexDigits) {
            // First time through, use high nibble of 0, treat next digit as low nibble.
            lowNibbleChar = hexDigits.charAt(digitsIndex++);
            oddNumHexDigits = false; // done compensating for odd length, don't do again
        } else {
            highNibbleChar = hexDigits.charAt(digitsIndex++);
            lowNibbleChar = hexDigits.charAt(digitsIndex++);
        }

        buffer[bufferIndex++] = VHex::hexCharsToByte(highNibbleChar, lowNibbleChar);
    }
}

// static
void VHex::stringToHex(const VString& text, VString& hexDigits, bool wantLeading0x) {
    VHex::bufferToHexString(text.getDataBufferConst(), static_cast<Vs64>(text.length()), hexDigits, wantLeading0x);
}

// static
void VHex::hexToString(const VString& hexDigits, VString& text, bool hasLeading0x) {
    int outputLength = hexDigits.length() / 2;

    if (hasLeading0x) {
        --outputLength;
    }

    text.preflight(outputLength);
    VHex::hexStringToBuffer(hexDigits, text.getDataBuffer(), hasLeading0x);
    text.postflight(outputLength);
}

// static
void VHex::byteToHexString(Vu8 byteValue, VString& s) {
    char hexChars[3];

    hexChars[2] = 0;
    VHex::byteToHexChars(byteValue, &hexChars[0], &hexChars[1]);

    s = hexChars;
}

// static
void VHex::byteToHexChars(Vu8 byteValue, char* highNibbleChar, char* lowNibbleChar) {
    *highNibbleChar = VHex::nibbleToHexChar(static_cast<Vu8>(byteValue >> 4));
    *lowNibbleChar = VHex::nibbleToHexChar(static_cast<Vu8>(byteValue & 0x0F));
}

// static
Vu8 VHex::hexStringToByte(const char* twoHexDigits) {
    Vu8 highNibble = VHex::hexCharToNibble(twoHexDigits[0]);
    Vu8 lowNibble = VHex::hexCharToNibble(twoHexDigits[1]);

    return static_cast<Vu8>((highNibble << 4) | lowNibble);
}

// static
Vu8 VHex::hexCharsToByte(char highNibbleChar, char lowNibbleChar) {
    Vu8 highNibble = VHex::hexCharToNibble(highNibbleChar);
    Vu8 lowNibble = VHex::hexCharToNibble(lowNibbleChar);

    return static_cast<Vu8>((highNibble << 4) | lowNibble);
}

// static
char VHex::nibbleToHexChar(Vu8 nibbleValue) {
    nibbleValue &= 0x0F;    // ensure that the high 4 bits are zero

    if (nibbleValue < 0x0A)
        return static_cast<char>('0' + nibbleValue);
    else
        return static_cast<char>('A' + (nibbleValue - 0x0A));
}

// static
Vu8 VHex::hexCharToNibble(char hexChar) {
    if ((hexChar >= '0') && (hexChar <= '9'))
        return static_cast<Vu8>(hexChar - '0');
    else if ((hexChar >= 'A') && (hexChar <= 'F'))
        return static_cast<Vu8>(0x0A + hexChar - 'A');
    else if ((hexChar >= 'a') && (hexChar <= 'f'))
        return static_cast<Vu8>(0x0A + hexChar - 'a');
    else
        return 0;
}

// static
void VHex::bufferToPrintableASCIIString(const Vu8* buffer, Vs64 bufferLength, VString& s) {
    for (int i = 0; i < (int) bufferLength; ++i) {
        char asciiValue = static_cast<char>(buffer[i]);
        if ((asciiValue <= 0x20) || (asciiValue > 0x7E)) {
            asciiValue = '.';
        }
        s += asciiValue;
    }
}

// static
void VHex::readHexDump(VTextIOStream& inputStream, VBinaryIOStream& outputStream) {
    VString line;

    do {
        inputStream.readLine(line);

        if (line.isEmpty()) {
            break;
        }

        // Remove typical leading indent spaces.
        line.trim();

        // Lines we want must start with either of these offset formats:
        //   NNNNNNNN: (where N is a decimal digit)
        //   0xNNNNNNNN: (where N is a hexadecimal digit)
        // Anything else is a line to be skipped.

        int nextHexByteOffset = -1;
        if (line.startsWith("0x") && (line.length() > 10) && (line[10] == ':')) {
            nextHexByteOffset = 11;
        } else if (line.startsWith("0") && (line.length() > 8) && (line[8] == ':')) {
            nextHexByteOffset = 9;
        } else {
            continue; // skip this line
        }

        // Read each ' xx' triplet from the line. Space, hex digit, hex digit.
        // Anything else indicates end of this line's hex data.
        while (nextHexByteOffset > 0) {
            if (line.length() < nextHexByteOffset + 3) {
                break;
            } else if (line[nextHexByteOffset] != ' ') {
                break;
            } else if (!(VChar(line[nextHexByteOffset+1]).isHexadecimal()) || !(VChar(line[nextHexByteOffset+2]).isHexadecimal())) {
                break;
            }

            // Now we know we have a space and two hex digits we can decode.
            Vu8 byteValue = VHex::hexCharsToByte(line[nextHexByteOffset+1], line[nextHexByteOffset+2]);
            outputStream.writeU8(byteValue);

            nextHexByteOffset += 3;
        }

    } while (line.isNotEmpty());
}

VHex::VHex(VTextIOStream* outputStream, int numBytesPerRow, int indentCount, bool labelsInHex, bool showASCIIValues)
    : mOutputStream(outputStream)
    , mNumBytesPerRow(numBytesPerRow)
    , mIndentCount(indentCount)
    , mLabelsInHex(labelsInHex)
    , mShowASCIIValues(showASCIIValues)
    , mStartColumn(0)
    , mOffset(0)
    , mPendingBufferUsed(0)
    , mPendingBuffer(NULL)
    , mLineBuffer()
    {
    mPendingBuffer = new Vu8[mNumBytesPerRow];
}

VHex::~VHex() {
    delete [] mPendingBuffer;
    mOutputStream = NULL; // we don't own it, so don't delete it
}

void VHex::printHex(const Vu8* buffer, Vs64 length, Vs64 offset) {
    while (length > 0) {
        if (mPendingBufferUsed == mNumBytesPerRow - mStartColumn) {
            this->_printPending();
        }

        mPendingBuffer[mPendingBufferUsed++] = buffer[offset++];

        --length;
    }

    this->flush();
}

void VHex::reset() {
    this->_printPending();

    mStartColumn = 0;
    mOffset = 0;
}

void VHex::flush() {
    this->_printPending();
}

void VHex::_printPending() {
    if (mPendingBufferUsed > 0) {
        char highNibbleChar;
        char lowNibbleChar;

        mLineBuffer = VString::EMPTY();

        // Add spaces to indent.
        for (int i = 0; i < mIndentCount; ++i) {
            mLineBuffer += ' ';
        }

        // Add the label.
        if (mLabelsInHex) {
            VString label(VSTRING_ARGS("0x%08X: ", (int) mOffset));
            mLineBuffer += label;
        } else {
            VString label(VSTRING_ARGS("%08lld: ", mOffset));
            mLineBuffer += label;
        }

        // If we're starting mid-row, add spaces to indent.
        for (int i = 0; i < mStartColumn; ++i) {
            mLineBuffer += "   ";
        }

        // Now append our hex data.
        for (int i = 0; i < mPendingBufferUsed; ++i) {
            VHex::byteToHexChars(mPendingBuffer[i], &highNibbleChar, &lowNibbleChar);
            mLineBuffer += highNibbleChar;
            mLineBuffer += lowNibbleChar;
            mLineBuffer += ' ';
        }

        // Now do the ASCII stuff if necessary.
        if (mShowASCIIValues) {
            // If we're starting mid-row, add spaces to indent.
            for (int i = mPendingBufferUsed; i < mNumBytesPerRow - mStartColumn; ++i) {
                mLineBuffer += "   ";
            }

            mLineBuffer += "   ";

            for (int i = 0; i < mStartColumn; ++i) {
                mLineBuffer += ' ';
            }

            // Append the ASCII data
            for (int i = 0; i < mPendingBufferUsed; ++i) {
                char    asciiValue = (char) mPendingBuffer[i];

                if ((asciiValue <= 0x20) || (asciiValue > 0x7E)) {
                    asciiValue = '.';
                }

                mLineBuffer += asciiValue;
            }
        }

        // Keep track of column in case of split lines
        if (mNumBytesPerRow == 0) {
            mStartColumn = 0;
        } else {
            mStartColumn = (mStartColumn + mPendingBufferUsed) % mNumBytesPerRow;
        }

        // Reset pending indicators
        mOffset += mPendingBufferUsed;
        mPendingBufferUsed = 0;

        // Finally, shove the string out to the stream or output pipe.
        if (mOutputStream == NULL) {
            std::cout << mLineBuffer.chars() << std::endl;
        } else {
            mOutputStream->writeLine(mLineBuffer);
        }
    }
}

