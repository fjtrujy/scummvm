/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/debug.h"
#include "common/stream.h"
#include "common/substream.h"
#include "common/textconsole.h"
#include "graphics/surface.h"

#include "chewy/atds.h"
#include "chewy/chewy.h"
#include "chewy/resource.h"

namespace Chewy {

Resource::Resource(Common::String filename) {
	const uint32 headerGeneric = MKTAG('N', 'G', 'S', '\0');
	const uint32 headerTxtDec  = MKTAG('T', 'C', 'F', '\0');
	const uint32 headerTxtEnc  = MKTAG('T', 'C', 'F', '\1');
	const uint32 headerSprite  = MKTAG('T', 'A', 'F', '\0');

	filename.toLowercase();
	_stream.open(filename);

	uint32 header = _stream.readUint32BE();
	bool isText = (header == headerTxtDec || header == headerTxtEnc);
	bool isSprite = (header == headerSprite);
	bool isSpeech = filename.contains("speech.tvp");

	if (header != headerGeneric && !isSprite && !isText)
		error("Invalid resource - %s", filename.c_str());

	if (isText) {
		_resType = kResourceTCF;
		_encrypted = (header == headerTxtEnc);
	} else if (isSprite) {
		initSprite(filename);
		return;
	} else {
		_resType = (ResourceType)_stream.readUint16LE();
		_encrypted = false;
	}

	if (filename.contains("atds.tap"))
		_encrypted = true;

	_chunkCount = _stream.readUint16LE();
	_chunkList.reserve(_chunkCount);

	for (uint i = 0; i < _chunkCount; i++) {
		Chunk cur;
		cur.size = _stream.readUint32LE();

		if (!isText) {
			cur.type = (ResourceType)_stream.readUint16LE();
			cur.num = 0;
		} else {
			cur.type = kResourceUnknown;
			cur.num = _stream.readUint16LE();
		}	

		cur.pos = _stream.pos();

		// WORKAROUND: Patch invalid speech sample
		if (isSpeech && i == 2277 && cur.size == 57028) {
			cur.size = 152057;
			_stream.skip(cur.size);
			_chunkList.push_back(cur);
			_chunkList.push_back(cur);
			continue;
		}

		_stream.skip(cur.size);
		_chunkList.push_back(cur);
	}

	_spriteCorrectionsCount = 0;
	_spriteCorrectionsTable = nullptr;
}

Resource::~Resource() {
	_chunkList.clear();
	_stream.close();

	delete[] _spriteCorrectionsTable;
	_spriteCorrectionsTable = nullptr;
}

uint32 Resource::getChunkCount() const {
	return _chunkList.size();
}

Chunk *Resource::getChunk(uint num) {
	assert(num < _chunkList.size());

	return &_chunkList[num];
}

uint32 Resource::findLargestChunk(uint start, uint end) {
	uint32 maxSize = 0;
	for (uint i = start; i < end; i++) {
		if (_chunkList[i].size > maxSize)
			maxSize = _chunkList[i].size;
	}
	return maxSize;
}

uint8 *Resource::getChunkData(uint num) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	uint8 *data = new uint8[chunk->size];

	_stream.seek(chunk->pos, SEEK_SET);
	_stream.read(data, chunk->size);
	if (_encrypted)
		decrypt(data, chunk->size);

	return data;
}

void Resource::initSprite(Common::String filename) {
	_resType = kResourceTAF;
	_encrypted = false;
	/*screenMode = */_stream.readUint16LE();
	_chunkCount = _stream.readUint16LE();
	_allSize = _stream.readUint32LE();
	_stream.read(_spritePalette, 3 * 256);
	uint32 nextSpriteOffset = _stream.readUint32LE();
	_spriteCorrectionsCount = _stream.readUint16LE();

	// Sometimes there's a filler byte
	if ((int32)nextSpriteOffset == _stream.pos() + 1)
		_stream.skip(1);

	if ((int32)nextSpriteOffset != _stream.pos())
		error("Invalid sprite resource - %s", filename.c_str());

	for (uint i = 0; i < _chunkCount; i++) {
		Chunk cur;

		cur.pos = _stream.pos();
		cur.type = kResourceTAF;

		_stream.skip(2 + 2 + 2);
		nextSpriteOffset = _stream.readUint32LE();
		uint32 spriteImageOffset = _stream.readUint32LE();
		_stream.skip(1);

		if ((int32)spriteImageOffset != _stream.pos())
			error("Invalid sprite resource - %s", filename.c_str());

		cur.size = nextSpriteOffset - cur.pos - 15;

		_stream.skip(cur.size);
		_chunkList.push_back(cur);

		if (_stream.err())
			error("Sprite stream error - %s", filename.c_str());
	}

	_spriteCorrectionsTable = new uint16[_chunkCount * 2];

	for (uint i = 0; i < _chunkCount; i++) {
		_spriteCorrectionsTable[i * 2] = _stream.readUint16LE();
		_spriteCorrectionsTable[i * 2 + 1] = _stream.readUint16LE();
	}
}

void Resource::unpackRLE(uint8 *buffer, uint32 compressedSize, uint32 uncompressedSize) {
	uint32 outPos = 0;

	for (uint i = 0; i < (compressedSize) / 2 && outPos < uncompressedSize; i++) {
		uint8 count = _stream.readByte();
		uint8 value = _stream.readByte();
		for (uint8 j = 0; j < count && outPos < uncompressedSize; j++) {
			buffer[outPos++] = value;
		}
	}
}

void Resource::decrypt(uint8 *data, uint32 size) {
	uint8 *c = data;

	for (uint i = 0; i < size; i++) {
		*c = -(*c);
		++c;
	}
}

TAFChunk *SpriteResource::getSprite(uint num) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	TAFChunk *taf = new TAFChunk();

	_stream.seek(chunk->pos, SEEK_SET);

	taf->compressionFlag = _stream.readUint16LE();
	taf->width = _stream.readUint16LE();
	taf->height = _stream.readUint16LE();
	_stream.skip(4 + 4 + 1);

	taf->data = new uint8[taf->width * taf->height];

	if (!taf->compressionFlag)
		_stream.read(taf->data, chunk->size);
	else
		unpackRLE(taf->data, chunk->size, taf->width * taf->height);

	return taf;
}

uint32 SpriteResource::getSpriteData(uint num, uint8 **buf, bool initBuffer) {
	TAFChunk *sprite = getSprite(num);
	uint32 size = sprite->width * sprite->height;
	if (initBuffer)
		*buf = (byte *)malloc(size + 4);
	// Sprite width and height is piggy-banked inside the sprite data
	uint16 *memPtr = (uint16 *)*buf;
	memPtr[0] = sprite->width;
	memPtr[1] = sprite->height;
	memcpy(*buf + 4, sprite->data, size);
	delete sprite;

	return size + 4;
}

TBFChunk *BackgroundResource::getImage(uint num, bool fixPalette) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	TBFChunk *tbf = new TBFChunk();

	_stream.seek(chunk->pos, SEEK_SET);

	if (_stream.readUint32BE() != MKTAG('T', 'B', 'F', '\0'))
		error("Corrupt TBF resource");

	tbf->screenMode = _stream.readUint16LE();
	tbf->compressionFlag = _stream.readUint16LE();
	tbf->size = _stream.readUint32LE();
	tbf->width = _stream.readUint16LE();
	tbf->height = _stream.readUint16LE();
	for (int j = 0; j < 3 * 256; j++)
		tbf->palette[j] = fixPalette ? (_stream.readByte() << 2) & 0xff : _stream.readByte();

	tbf->data = new uint8[tbf->size];

	if (!tbf->compressionFlag)
		_stream.read(tbf->data, chunk->size);
	else
		unpackRLE(tbf->data, chunk->size, tbf->size);

	return tbf;
}

SoundChunk *SoundResource::getSound(uint num) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	SoundChunk *sound = new SoundChunk();

	_stream.seek(chunk->pos, SEEK_SET);

	uint8 blocksRemaining;
	uint32 totalLength = 0;
	uint32 blockSize;

	do {
		blocksRemaining = _stream.readByte();

		uint8 b1 = _stream.readByte();
		uint8 b2 = _stream.readByte();
		uint8 b3 = _stream.readByte();
		blockSize = b1 + (b2 << 8) + (b3 << 16);

		totalLength += blockSize;
		_stream.skip(blockSize);
	} while (blocksRemaining > 1);

	sound->size = totalLength;
	sound->data = new uint8[totalLength];
	uint8 *ptr = sound->data;

	_stream.seek(chunk->pos, SEEK_SET);

	do {
		blocksRemaining = _stream.readByte();

		uint8 b1 = _stream.readByte();
		uint8 b2 = _stream.readByte();
		uint8 b3 = _stream.readByte();
		blockSize = b1 + (b2 << 8) + (b3 << 16);

		_stream.read(ptr, blockSize);
		ptr += blockSize;
	} while (blocksRemaining > 1);

	return sound;
}

VideoChunk *VideoResource::getVideoHeader(uint num) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	VideoChunk *vid = new VideoChunk();

	_stream.seek(chunk->pos, SEEK_SET);

	if (_stream.readUint32BE() != MKTAG('C', 'F', 'O', '\0'))
		error("Corrupt video resource");

	vid->size = _stream.readUint32LE();
	vid->frameCount = _stream.readUint16LE();
	vid->width = _stream.readUint16LE();
	vid->height = _stream.readUint16LE();
	vid->frameDelay = _stream.readUint32LE();
	vid->firstFrameOffset = _stream.readUint32LE();

	return vid;
}

Common::SeekableReadStream *VideoResource::getVideoStream(uint num) {
	assert(num < _chunkList.size());

	Chunk *chunk = &_chunkList[num];
	return new Common::SeekableSubReadStream(&_stream, chunk->pos, chunk->pos + chunk->size);
}

DialogResource::DialogResource(Common::String filename) : Resource(filename) {
	_dialogBuffer = new byte[_stream.size()];
	_stream.seek(0, SEEK_SET);
	_dialogStream = new Common::MemorySeekableReadWriteStream(_dialogBuffer, _stream.size());
	_dialogStream->writeStream(&_stream);
}

DialogResource::~DialogResource() {
	delete _dialogStream;
	delete _dialogBuffer;
}

DialogChunk *DialogResource::getDialog(uint dialog, uint block) {
	Chunk *chunk = &_chunkList[dialog];
	DialogChunk *item = new DialogChunk();

	_dialogStream->seek(chunk->pos + 3 * 6 * block, SEEK_SET);

	_dialogStream->read(item->show, 6);
	_dialogStream->read(item->next, 6);
	_dialogStream->read(item->flags, 6);

	return item;
}

bool DialogResource::isItemShown(uint dialog, uint block, uint num) {
	DialogChunk *item = getDialog(dialog, block);
	bool isShown = item->show[num];
	delete item;

	return isShown;
}

void DialogResource::setItemShown(uint dialog, uint block, uint num, bool shown) {
	Chunk *chunk = &_chunkList[dialog];

	_dialogStream->seek(chunk->pos + 3 * 6 * block, SEEK_SET);

	_dialogStream->skip(num);
	_dialogStream->writeByte(shown ? 1 : 0);
}

bool DialogResource::hasExitBit(uint dialog, uint block, uint num) {
	DialogChunk *item = getDialog(dialog, block);
	const bool isExit = (item->flags[num] & ADS_EXIT_BIT) != 0;
	delete item;

	return isExit;
}

bool DialogResource::hasRestartBit(uint dialog, uint block, uint num) {
	DialogChunk *item = getDialog(dialog, block);
	const bool isRestart = (item->flags[num] & ADS_RESTART_BIT) != 0;
	delete item;

	return isRestart;
}

bool DialogResource::hasShowBit(uint dialog, uint block, uint num) {
	DialogChunk *item = getDialog(dialog, block);
	const bool isShown = (item->flags[num] & ADS_SHOW_BIT) != 0;
	delete item;

	return isShown;
}

uint8 DialogResource::getNextBlock(uint dialog, uint block, uint num) {
	DialogChunk *item = getDialog(dialog, block);
	const uint8 next = item->next[num];
	delete item;

	return next;
}

void DialogResource::loadStream(Common::SeekableReadStream *s) {
	_dialogStream->seek(0, SEEK_SET);
	_dialogStream->writeStream(s, _stream.size());
}

void DialogResource::saveStream(Common::WriteStream* s) {
	_dialogStream->seek(0, SEEK_SET);
	s->writeStream(_dialogStream, _stream.size());
}

}
