/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2013 Bolton Software Ltd.
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file COPYING that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CCryptoStream.h"
#include "CLog.h"
#include <sstream>
#include <string>

using namespace CryptoPP;

CCryptoStream::CCryptoStream(IEventQueue& eventQueue, synergy::IStream* stream, bool adoptStream) :
	CStreamFilter(eventQueue, stream, adoptStream),
	m_key(NULL),
	m_keyLength(0)
{
}

CCryptoStream::~CCryptoStream()
{
}

UInt32
CCryptoStream::read(void* out, UInt32 n)
{
	assert(m_key != NULL);
	LOG((CLOG_DEBUG4 "crypto: read %i (decrypt)", n));

	byte* cypher = new byte[n];
	int result = getStream()->read(cypher, n);
	if (result == 0) {
		// nothing to read.
		return 0;
	}

	if (result != n) {
		LOG((CLOG_ERR "crypto: decrypt failed, only %i of %i bytes", result, n));
		return 0;
	}

	logBuffer("cypher", cypher, n);
	m_decryption.ProcessData(static_cast<byte*>(out), cypher, n);
	logBuffer("plaintext", static_cast<byte*>(out), n);
	delete[] cypher;
	return result;
}

void
CCryptoStream::write(const void* in, UInt32 n)
{
	assert(m_key != NULL);
	LOG((CLOG_DEBUG4 "crypto: write %i (encrypt)", n));

	logBuffer("plaintext", static_cast<byte*>(const_cast<void*>(in)), n);
	byte* cypher = new byte[n];
	m_encryption.ProcessData(cypher, static_cast<const byte*>(in), n);
	logBuffer("cypher", cypher, n);
	getStream()->write(cypher, n);
	delete[] cypher;
}

void
CCryptoStream::setKeyWithIv(const byte* key, size_t length, const byte* iv)
{
	logBuffer("iv", key, length);
	logBuffer("key", iv, CRYPTO_IV_SIZE);

	m_encryption.SetKeyWithIV(key, length, iv);
	m_decryption.SetKeyWithIV(key, length, iv);

	m_key = key;
	m_keyLength = length;
}

void
CCryptoStream::setIv(const byte* iv)
{
	assert(m_key != NULL);
	logBuffer("iv", iv, CRYPTO_IV_SIZE);
	m_encryption.SetKeyWithIV(m_key, m_keyLength, iv);
	m_decryption.SetKeyWithIV(m_key, m_keyLength, iv);
}

void
CCryptoStream::newIv(byte* out)
{
	m_autoSeedRandomPool.GenerateBlock(out, CRYPTO_IV_SIZE);
	setIv(out);
}

void
CCryptoStream::logBuffer(const char* name, const byte* buf, int length)
{
	if (CLOG->getFilter() < kDEBUG4) {
		return;
	}

	std::stringstream ss;
	ss << "crypto: " << name << ":";

	char buffer[4];
	for (int i = 0; i < length; i++) {
		sprintf(buffer, " %02X", buf[i]);
		ss << buffer;
	}

	LOG((CLOG_DEBUG4 "%s", ss.str().c_str()));
}
