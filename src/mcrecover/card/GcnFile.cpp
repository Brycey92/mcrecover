/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * GcnFile.cpp: GameCube file entry class.                                 *
 *                                                                         *
 * Copyright (c) 2012-2015 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "GcnFile.hpp"

#include "card.h"
#include "util/byteswap.h"

// GcnCard class.
#include "GcnCard.hpp"

// GcImage class.
#include "GcImage.hpp"
#include "GcToolsQt.hpp"

// C includes. (C++ namespace)
#include <cerrno>

// C++ includes.
#include <string>
#include <vector>
using std::string;
using std::vector;

// Qt includes.
#include <QtCore/QByteArray>
#include <QtCore/QTextCodec>
#include <QtCore/QFile>
#include <QtCore/QIODevice>

#define NUM_ELEMENTS(x) ((int)(sizeof(x) / sizeof(x[0])))

/** GcnFilePrivate **/

#include "File_p.hpp"
class GcnFilePrivate : public FilePrivate
{
	public:
		/**
		 * Initialize the GcnFile private class.
		 * This constructor is for valid files.
		 * @param q GcnFile.
		 * @param card GcnCard.
		 * @param direntry Directory Entry pointer.
		 * @param mc_bat Block table.
		 */
		GcnFilePrivate(GcnFile *q, GcnCard *card,
			const card_direntry *dirEntry,
			const card_bat *mc_bat);

		/**
		 * Initialize the GcnFile private class.
		 * This constructor is for lost files.
		 * @param q GcnFile.
		 * @param card GcnCard.
		 * @param direntry Directory Entry pointer.
		 * @param fatEntries FAT entries.
		 */
		GcnFilePrivate(GcnFile *q, GcnCard *card,
			const card_direntry *dirEntry,
			const QVector<uint16_t> &fatEntries);

		virtual ~GcnFilePrivate();

	protected:
		Q_DECLARE_PUBLIC(GcnFile)
	private:
		Q_DISABLE_COPY(GcnFilePrivate)

		/**
		 * Load the file information.
		 */
		void loadFileInfo(void);

	public:
		const card_bat *mc_bat;	// Block table. (TODO: Do we need to store this?)

		/**
		 * Directory entry.
		 * This points to an entry within card's Directory Table.
		 * NOTE: If this is a lost file, this was allocated by us,
		 * and needs to be freed in the destructor.
		 */
		const card_direntry *dirEntry;

		// GcImages.
		GcImage *gcBanner;
		QVector<GcImage*> gcIcons;

		// CI8 SHARED image struct.
		struct CI8_SHARED_data {
			int iconIdx;
			uint32_t iconAddr;
		};

		/**
		 * Load the banner image.
		 * @return GcImage containing the banner image, or nullptr on error.
		 */
		GcImage *loadBannerImage(void);

		/**
		 * Load the icon images.
		 * @return QVector<GcImage*> containing the icon images, or empty QVector on error.
		 */
		QVector<GcImage*> loadIconImages(void);

		/**
		 * Load the banner and icon images.
		 */
		void loadImages(void);

		// Checksum data.
		QVector<Checksum::ChecksumDef> checksumDefs;
		QVector<Checksum::ChecksumValue> checksumValues;

		/**
		 * Calculate the file checksum.
		 */
		void calculateChecksum(void);
};

/**
 * Initialize the GcnFile private class.
 * This constructor is for valid files.
 * @param q GcnFile.
 * @param card GcnCard.
 * @param direntry Directory Entry pointer.
 * @param mc_bat Block table.
 */
GcnFilePrivate::GcnFilePrivate(GcnFile *q, GcnCard *card,
		const card_direntry *dirEntry,
		const card_bat *mc_bat)
	: FilePrivate(q, card)
	, mc_bat(mc_bat)
	, dirEntry(dirEntry)
{
	if (!dirEntry || !mc_bat) {
		// Invalid data.
		this->dirEntry = nullptr;
		this->mc_bat = nullptr;

		// This file is basically useless now...
		return;
	}

	// Clamp file length to the size of the memory card.
	// This shouldn't happen, but it's possible if either
	// the filesystem is heavily corrupted, or the file
	// isn't actually a GCN Memory Card image.
	int length = dirEntry->length;
	if (length > card->totalUserBlocks())
		length = card->totalUserBlocks();

	// Load the FAT entries.
	fatEntries.clear();
	fatEntries.reserve(length);
	uint16_t next_block = dirEntry->block;
	if (next_block >= 5 && next_block != 0xFFFF &&
	    next_block < (uint16_t)NUM_ELEMENTS(mc_bat->fat)) {
		fatEntries.append(next_block);

		// Go through the rest of the blocks.
		for (int i = length; i > 1; i--) {
			next_block = mc_bat->fat[next_block - 5];
			if (next_block == 0xFFFF || next_block < 5 ||
			    next_block >= (uint16_t)NUM_ELEMENTS(mc_bat->fat))
			{
				// Next block is invalid.
				break;
			}
			fatEntries.append(next_block);
		}
	}

	// Load the file information.
	loadFileInfo();
}

/**
 * Initialize the GcnFile private class.
 * This constructor is for lost files.
 * @param q GcnFile.
 * @param card GcnCard.
 * @param direntry Directory Entry pointer.
 * @param fatEntries FAT entries.
 */
GcnFilePrivate::GcnFilePrivate(GcnFile *q, GcnCard *card,
		const card_direntry *dirEntry,
		const QVector<uint16_t> &fatEntries)
	: FilePrivate(q, card)
	, mc_bat(nullptr)
	, dirEntry(dirEntry)
{
	if (!dirEntry || fatEntries.isEmpty()) {
		// Invalid data.
		this->dirEntry = nullptr;
		this->fatEntries.clear();

		// This file is basically useless now...
		return;
	}

	// gcc-4.9.2 doesn't like assigning variables owned by the
	// base class in the subclass constructor.
	// TODO: Maybe add lostFile and fatEntries to the FilePrivate constructor?
	this->lostFile = true;
	this->fatEntries = fatEntries;

	// This is a lost file.
	// We need to make a copy of dirEntry.
	card_direntry *dentry = (card_direntry*)malloc(sizeof(*dirEntry));
	memcpy(dentry, dirEntry, sizeof(*dentry));
	this->dirEntry = dentry;

	// Load the file information.
	loadFileInfo();
}

GcnFilePrivate::~GcnFilePrivate()
{
	if (lostFile) {
		// dirEntry was allocated by us.
		// Free it.
		free((void*)dirEntry);
	}

	// Delete GcImages.
	delete gcBanner;
	qDeleteAll(gcIcons);
	gcIcons.clear();
}

/**
 * Load the file information.
 */
void GcnFilePrivate::loadFileInfo(void)
{
	// Game ID is always Latin-1.
	// NOTE: gamecode and company are right next to each other,
	// so we can "overrun" the buffer here.
	gameID = QString::fromLatin1(dirEntry->gamecode,
			sizeof(dirEntry->gamecode) + sizeof(dirEntry->company));

	// Get the appropriate QTextCodec for this file.
	const char region = (gameID.size() >= 4
				? gameID.at(3).toLatin1()
				: 0);
	// FIXME: Unchecked cast, though card should always be GcnCard...
	QTextCodec *textCodec = ((GcnCard*)card)->textCodec(region);

	// Remove trailing NULL characters before converting to UTF-8.
	QByteArray filenameData(dirEntry->filename, sizeof(dirEntry->filename));
	int nullChr = filenameData.indexOf('\0');
	if (nullChr >= 0)
		filenameData.resize(nullChr);

	// Convert the filename to UTF-8.
	if (!textCodec) {
		// No text codec was found.
		// Default to Latin-1.
		filename = QString::fromLatin1(filenameData.constData(), filenameData.size());
	} else {
		// Use the text codec.
		filename = textCodec->toUnicode(filenameData.constData(), filenameData.size());
	}

	// Timestamp.
	mtime.setGcnTimestamp(dirEntry->lastmodified);

	// Mode.
	// TODO: Convert to system-independent value?
	this->mode = dirEntry->permission;

	// Get the block size.
	const int blockSize = card->blockSize();

	// Load the block containing the comments.
	const int commentBlock = (dirEntry->commentaddr / blockSize);
	const int commentOffset = (dirEntry->commentaddr % blockSize);

	char *commentData = (char*)malloc(blockSize);
	int ret = card->readBlock(commentData, blockSize, fileBlockAddrToPhysBlockAddr(commentBlock));
	if (ret != blockSize) {
		// Read error.
		// File is probably invalid.
		free(commentData);
		return;
	}

	// Load the file comments. (64 bytes)
	// NOTE: These comments are supposed to be NULL-terminated.
	// 0x00: Game description.
	// 0x20: File description.
	QByteArray gameDescData(&commentData[commentOffset], 32);
	QByteArray fileDescData(&commentData[commentOffset+32], 32);
	free(commentData);

	// Remove trailing NULL characters before converting to UTF-8.
	nullChr = gameDescData.indexOf('\0');
	if (nullChr >= 0)
		gameDescData.resize(nullChr);
	nullChr = fileDescData.indexOf('\0');
	if (nullChr >= 0)
		fileDescData.resize(nullChr);

	// Convert the descriptions to UTF-8.
	// Trim the descriptions while we're at it.
	if (!textCodec) {
		// No text codec was found.
		// Default to Latin-1.
		gameDesc = QString::fromLatin1(gameDescData.constData(), gameDescData.size()).trimmed();
		fileDesc = QString::fromLatin1(fileDescData.constData(), fileDescData.size()).trimmed();
	} else {
		// Use the text codec.
		gameDesc = textCodec->toUnicode(gameDescData.constData(), gameDescData.size()).trimmed();
		fileDesc = textCodec->toUnicode(fileDescData.constData(), fileDescData.size()).trimmed();
	}

	// Load the banner and icon images.
	loadImages();
}

/**
 * Load the banner image.
 * @return GcImage* containing the banner image, or nullptr on error.
 */
GcImage *GcnFilePrivate::loadBannerImage(void)
{
	// Determine the banner length.
	uint32_t imgSize = 0;
	switch (dirEntry->bannerfmt & CARD_BANNER_MASK) {
		case CARD_BANNER_CI:
			imgSize = (CARD_BANNER_W * CARD_BANNER_H * 1);
			break;
		case CARD_BANNER_RGB:
			imgSize = (CARD_BANNER_W * CARD_BANNER_H * 2);
			break;
		default:
			// No banner.
			return nullptr;
	}

	// Load the banner.
	uint32_t imgAddr = dirEntry->iconaddr;
	const int blockSize = card->blockSize();
	const uint16_t blockStart = (imgAddr / blockSize);
	QByteArray imgData = readBlocks(blockStart, 1);
	if (imgData.size() != blockSize)
		return nullptr;
	imgAddr &= 0x1FFF;

	GcImage *gcBannerImg = nullptr;
	switch (dirEntry->bannerfmt & CARD_BANNER_MASK) {
		case CARD_BANNER_CI:
			// CI8 palette is right after the banner.
			// (256 entries in RGB5A3 format.)
			gcBannerImg = GcImage::fromCI8(CARD_BANNER_W, CARD_BANNER_H,
					(const uint8_t*)&imgData.constData()[imgAddr], imgSize,
					(const uint16_t*)&imgData.constData()[imgAddr + imgSize], 0x200);
			break;

		case CARD_BANNER_RGB:
			gcBannerImg = GcImage::fromRGB5A3(CARD_BANNER_W, CARD_BANNER_H,
					(const uint16_t*)&imgData.constData()[imgAddr], imgSize);
			break;

		default:
			break;
	}

	return gcBannerImg;
}

/**
 * Load the icon images.
 * @return QVector<GcImage*> containing the icon images, or empty QVector on error.
 */
QVector<GcImage*> GcnFilePrivate::loadIconImages(void)
{
	// TODO: Convert these to system-independent values.
	// Icon animation metadata.
	// TODO: Should be part of a struct that's returned...
	this->iconAnimMode = (dirEntry->bannerfmt & CARD_ANIM_MASK);

	// Calculate the first icon address.
	uint32_t imgAddr = dirEntry->iconaddr;
	switch (dirEntry->bannerfmt & CARD_BANNER_MASK) {
		case CARD_BANNER_CI:
			imgAddr += (CARD_BANNER_W * CARD_BANNER_H * 1);
			imgAddr += 0x200; // palette
			break;
		case CARD_BANNER_RGB:
			imgAddr += (CARD_BANNER_W * CARD_BANNER_H * 2);
			break;
		default:
			// No banner.
			break;
	}

	// Calculate the icon sizes.
	int iconSizes[8];
	int iconLenTotal = 0;
	bool isShared = false;
	uint16_t iconfmt = dirEntry->iconfmt;
	uint16_t iconspeed = dirEntry->iconspeed;
	for (int i = 0; i < CARD_MAXICONS; i++, iconfmt >>= 2, iconspeed >>= 2) {
		if ((iconspeed & CARD_SPEED_MASK) == CARD_SPEED_END)
			break;

		switch (iconfmt & CARD_ICON_MASK) {
			case CARD_ICON_CI_SHARED:
				iconSizes[i] = (CARD_ICON_W * CARD_ICON_H * 1);
				iconLenTotal += iconSizes[i];
				isShared = true;
				break;
			case CARD_ICON_CI_UNIQUE:
				iconSizes[i] = (CARD_ICON_W * CARD_ICON_H * 1);
				iconLenTotal += iconSizes[i] + 0x200;
				break;
			case CARD_BANNER_RGB:
				iconSizes[i] = (CARD_ICON_W * CARD_ICON_H * 2);
				iconLenTotal += iconSizes[i];
				break;
		}
	}

	if (isShared) {
		// CARD_ICON_CI_SHARED has a palette stored
		// after all of the icons.
		iconLenTotal += 0x200;
	}

	// Load the icon data.
	const int blockSize = card->blockSize();
	const uint16_t blockStart = (imgAddr / blockSize);
	const uint16_t blockEnd = ((imgAddr + iconLenTotal) / blockSize);
	const int blockLen = ((blockEnd - blockStart) + 1);
	QByteArray imgData = readBlocks(blockStart, blockLen);
	if (imgData.size() != (blockLen * blockSize))
		return QVector<GcImage*>();
	imgAddr &= 0x1FFF;

	// Decode the icon(s).
	QVector<CI8_SHARED_data> lst_CI8_SHARED;
	QVector<GcImage*> gcImages;
	// TODO: Should be part of a struct that's returned...
	this->iconSpeed.clear();

	iconfmt = dirEntry->iconfmt;
	iconspeed = dirEntry->iconspeed;
	for (int i = 0; i < CARD_MAXICONS; i++, iconfmt >>= 2, iconspeed >>= 2) {
		if ((iconspeed & CARD_SPEED_MASK) == CARD_SPEED_END)
			break;
		// TODO: Should be part of a struct that's returned...
		this->iconSpeed.append(iconspeed & CARD_SPEED_MASK);

		switch (iconfmt & CARD_ICON_MASK) {
			case CARD_ICON_CI_SHARED: {
				// CI8 palette is after *all* the icons.
				// (256 entries in RGB5A3 format.)
				// This is handled after the rest of the icons.
				CI8_SHARED_data data;
				data.iconIdx = i;
				data.iconAddr = imgAddr;
				lst_CI8_SHARED.append(data);

				// Add a nullptr as a placeholder.
				gcImages.append(nullptr);

				// Next icon.
				imgAddr += (CARD_ICON_W * CARD_ICON_H * 1);
				continue;
			}

			case CARD_ICON_CI_UNIQUE: {
				// CI8 palette is right after the icon.
				// (256 entries in RGB5A3 format.)
				const int imageSize = (CARD_ICON_W * CARD_ICON_H * 1);
				GcImage *gcIcon = GcImage::fromCI8(CARD_ICON_W, CARD_ICON_H,
						(const uint8_t*)&imgData.constData()[imgAddr], imageSize,
						(const uint16_t*)&imgData.constData()[imgAddr + imageSize], 0x200);
				gcImages.append(gcIcon);
				imgAddr += imageSize + 0x200;
				break;
			}

			case CARD_BANNER_RGB: {
				const int imageSize = (CARD_ICON_W * CARD_ICON_H * 2);
				GcImage *gcIcon = GcImage::fromRGB5A3(CARD_ICON_W, CARD_ICON_H,
						(const uint16_t*)&imgData.constData()[imgAddr], imageSize);
				gcImages.append(gcIcon);
				imgAddr += imageSize;
				break;
			}

			default:
				// No icon.
				// Add a nullptr as a placeholder.
				gcImages.append(nullptr);
				break;
		}
	}

	if (!lst_CI8_SHARED.isEmpty()) {
		// Process CI8 SHARED icons.
		// TODO: Convert the palette once instead of every time?
		const int imageSize = (CARD_ICON_W * CARD_ICON_H * 1);
		foreach (const CI8_SHARED_data& data, lst_CI8_SHARED) {
			GcImage *gcIcon = GcImage::fromCI8(CARD_ICON_W, CARD_ICON_H,
						(const uint8_t*)&imgData.constData()[data.iconAddr], imageSize,
						(const uint16_t*)&imgData.constData()[imgAddr], 0x200);
			gcImages[data.iconIdx] = gcIcon;
		}
	}

	// Shrink the QVector.
	int iconCount = gcImages.size();
	for (; iconCount > 0; iconCount--) {
		if (gcImages.at(iconCount-1))
			break;
	}
	if (iconCount > 0)
		gcImages.resize(iconCount);
	else
		gcImages.clear();

	return gcImages;
}

/**
 * Load the banner and icon images.
 */
void GcnFilePrivate::loadImages(void)
{
	// Load the banner.
	this->gcBanner = loadBannerImage();
	if (gcBanner) {
		// Set the new banner image.
		QImage qBanner = gcImageToQImage(gcBanner);
		if (!qBanner.isNull())
			banner = QPixmap::fromImage(qBanner);
		else
			banner = QPixmap();
	} else {
		// No banner image.
		banner = QPixmap();
	}

	// Load the icons.
	this->gcIcons = loadIconImages();
	icons.clear();
	icons.reserve(gcIcons.size());
	foreach (GcImage *gcIcon, gcIcons) {
		if (gcIcon) {
			QImage qIcon = gcImageToQImage(gcIcon);
			if (!qIcon.isNull())
				icons.append(QPixmap::fromImage(qIcon));
			else
				icons.append(QPixmap());
		} else {
			// No icon image.
			icons.append(QPixmap());
		}
	}
}

/**
 * Calculate the file checksum.
 */
void GcnFilePrivate::calculateChecksum(void)
{
	checksumValues.clear();

	if (checksumDefs.empty()) {
		// No checksum definitions were set.
		return;
	}

	// Load the file data.
	QByteArray fileData = loadFileData();
	if (fileData.isEmpty()) {
		// File is empty.
		return;
	}

	// Pointer to fileData's internal data array.
	uint8_t *data = reinterpret_cast<uint8_t*>(fileData.data());

	// Process all of the checksum definitions.
	for (int i = 0; i < (int)checksumDefs.size(); i++) {
		const Checksum::ChecksumDef &checksumDef = checksumDefs.at(i);

		if (checksumDef.algorithm == Checksum::CHKALG_NONE ||
		    checksumDef.algorithm >= Checksum::CHKALG_MAX ||
		    checksumDef.length == 0)
		{
			// No algorithm or invalid algorithm set,
			// or the checksum data has no length.
			continue;
		}

		// Make sure the checksum definition is in range.
		if (fileData.size() < (int)checksumDef.address ||
		    fileData.size() < (int)(checksumDef.start + checksumDef.length))
		{
			// File is too small...
			// TODO: Also check the size of the checksum itself.
			continue;
		}

		// Get the expected checksum.
		// NOTE: Assuming big-endian for all values.
		uint32_t expected = 0;
		Checksum::ChaoGardenChecksumData chaoChk_orig;

		switch (checksumDef.algorithm) {
			case Checksum::CHKALG_CRC16:
				expected = (data[checksumDef.address+0] << 8) |
					   (data[checksumDef.address+1]);
				break;

			case Checksum::CHKALG_CRC32:
			case Checksum::CHKALG_ADDINVDUAL16:
			case Checksum::CHKALG_ADDBYTES32:
				expected = (data[checksumDef.address+0] << 24) |
					   (data[checksumDef.address+1] << 16) |
					   (data[checksumDef.address+2] << 8) |
					   (data[checksumDef.address+3]);
				break;

			case Checksum::CHKALG_SONICCHAOGARDEN: {
				memcpy(&chaoChk_orig, &data[checksumDef.address], sizeof(chaoChk_orig));

				// Temporary working copy.
				Checksum::ChaoGardenChecksumData chaoChk = chaoChk_orig;
				expected = (chaoChk.checksum_3 << 24) |
					   (chaoChk.checksum_2 << 16) |
					   (chaoChk.checksum_1 << 8) |
					   (chaoChk.checksum_0);

				// Clear some fields that must be 0 when calculating the checksum.
				chaoChk.checksum_3 = 0;
				chaoChk.checksum_2 = 0;
				chaoChk.checksum_1 = 0;
				chaoChk.checksum_0 = 0;
				chaoChk.random_3 = 0;
				memcpy(&data[checksumDef.address], &chaoChk, sizeof(chaoChk));
				break;
			}

			case Checksum::CHKALG_NONE:
			default:
				// Unsupported algorithm.
				expected = 0;
				break;
		}

		// Calculate the checksum.
		const char *const start = (fileData.constData() + checksumDef.start);
		uint32_t actual = Checksum::Exec(checksumDef.algorithm,
				start, checksumDef.length, checksumDef.param);

		if (checksumDef.algorithm == Checksum::CHKALG_SONICCHAOGARDEN) {
			// Restore the Chao Garden checksum data.
			memcpy(&data[checksumDef.address], &chaoChk_orig, sizeof(chaoChk_orig));
		}

		// Save the checksums.
		Checksum::ChecksumValue checksumValue;
		checksumValue.expected = expected;
		checksumValue.actual = actual;
		checksumValues.push_back(checksumValue);
	}
}

/** GcnFile **/

/**
 * Create a GcnFile for a GcnCard.
 * This constructor is for valid files.
 * @param q GcnFile.
 * @param card GcnCard.
 * @param direntry Directory Entry pointer.
 * @param mc_bat Block table.
 */
GcnFile::GcnFile(GcnCard *card,
		const card_direntry *dirEntry,
		const card_bat *mc_bat)
	: File(new GcnFilePrivate(this, card, dirEntry, mc_bat), card)
{ }

/**
 * Create a GcnFile for a GcnCard.
 * This constructor is for lost files.
 * @param card GcnCard.
 * @param direntry Directory Entry pointer.
 * @param fatEntries FAT entries.
 */
GcnFile::GcnFile(GcnCard *card,
		const card_direntry *dirEntry,
		const QVector<uint16_t> &fatEntries)
	: File(new GcnFilePrivate(this, card, dirEntry, fatEntries), card)
{ }

// TODO: Maybe not needed?
GcnFile::~GcnFile()
{ }

/**
 * Get the text encoding ID for this file.
 * @return Text encoding ID.
 */
int GcnFile::encoding(void) const
{
	Q_D(const GcnFile);
	const char region = (d->gameID.size() >= 4
				? d->gameID.at(3).toLatin1()
				: 0);
	// FIXME: Unchecked cast, though card should always be GcnCard...
	return ((GcnCard*)d->card)->encodingForRegion(region);
}

/**
 * Get the file's mode as a string.
 * This is system-specific.
 * @return File mode as a string.
 */
QString GcnFile::modeAsString(void) const
{
	Q_D(const GcnFile);
	char str[4];

	str[0] = ((d->mode & CARD_ATTRIB_GLOBAL) ? 'G' : '-');
	str[1] = ((d->mode & CARD_ATTRIB_NOMOVE) ? 'M' : '-');
	str[2] = ((d->mode & CARD_ATTRIB_NOCOPY) ? 'C' : '-');
	str[3] = ((d->mode & CARD_ATTRIB_PUBLIC) ? 'P' : '-');

	return QString::fromLatin1(str, sizeof(str));
}

/** TODO: Move checksum definitions to File. **/

/**
 * Get the checksum definitions.
 * @return Checksum definitions.
 */
QVector<Checksum::ChecksumDef> GcnFile::checksumDefs(void) const
{
	Q_D(const GcnFile);
	return d->checksumDefs;
}

/**
 * Set the checksum definitions.
 * @param checksumDefs Checksum definitions.
 */
void GcnFile::setChecksumDefs(const QVector<Checksum::ChecksumDef> &checksumDefs)
{
	Q_D(GcnFile);
	d->checksumDefs = checksumDefs;
	d->calculateChecksum();
}

/**
 * Get the checksum values.
 * @return Checksum values, or empty QVector if no checksum definitions were set.
 */
QVector<Checksum::ChecksumValue> GcnFile::checksumValues(void) const
{
	Q_D(const GcnFile);
	return d->checksumValues;
}

/**
 * Get the checksum algorithm.
 * NOTE: We're assuming each file only uses one algorithm...
 * @return Checksum algorithm.
 */
Checksum::ChkAlgorithm GcnFile::checksumAlgorithm(void) const
{
	Q_D(const GcnFile);
	if (d->checksumDefs.isEmpty())
		return Checksum::CHKALG_NONE;
	return d->checksumDefs.at(0).algorithm;
}

/**
 * Get the checksum status.
 * @return Checksum status.
 */
Checksum::ChkStatus GcnFile::checksumStatus(void) const
{
	Q_D(const GcnFile);
	return Checksum::ChecksumStatus(d->checksumValues.toStdVector());
}

/**
 * Format checksum values as HTML for display purposes.
 * @return QVector containing one or two HTML strings.
 * - String 0 contains the actual checksums.
 * - String 1, if present, contains the expected checksums.
 */
QVector<QString> GcnFile::checksumValuesFormatted(void) const
{
	Q_D(const GcnFile);
	vector<string> vs = Checksum::ChecksumValuesFormatted(d->checksumValues.toStdVector());
	QVector<QString> ret;
	ret.reserve((int)vs.size());
	for (int i = 0; i < (int)vs.size(); i++) {
		ret.append(QString::fromStdString(vs[i]));
	}
	return ret;
}

/**
 * Get the default export filename.
 * @return Default export filename.
 */
QString GcnFile::defaultExportFilename(void) const
{
	/**
	 * Filename format:
	 * GALE01_SuperSmashBros0110290334_066000.gci
	 * gamecode_filename_startaddress.gci
	 */
	Q_D(const GcnFile);

	QString filename;
	filename.reserve(d->gameID.size() + + 1 +
			 d->filename.size() + 1 + 6 + 4);
	filename += d->gameID + QChar(L'_');
	filename += d->filename + QChar(L'_');

	// Convert the start address to hexadecimal.
	char start_address[16];
	snprintf(start_address, sizeof(start_address), "%06X",
		 d->dirEntry->block * d->card->blockSize());
	filename += QLatin1String(start_address);

	// Finish the filename.
	filename += QLatin1String(".gci");

	// Make sure no invalid DOS characters are present in the filename.
	return d->StripInvalidDosChars(filename);
}

/**
 * Export the file.
 * @param filename Filename for the exported file.
 * @return 0 on success; non-zero on error.
 * TODO: Error code constants.
 */
int GcnFile::exportToFile(const QString &filename)
{
	// NOTE: This function doesn't actually do anything different
	// from the base class function, but gcc-4.9.2 attempts to use
	// the QIODevice version when using a GcnFile* instead of a
	// File*. Hence, we need this virtual function.
	return File::exportToFile(filename);
}

/**
 * Export the file.
 * @param qioDevice QIODevice to write the data to.
 * @return 0 on success; non-zero on error.
 * TODO: Error code constants.
 */
int GcnFile::exportToFile(QIODevice *qioDevice)
{
	Q_D(GcnFile);

	// GCI header is the 64-byte directory entry.
	// NOTE: This must be byteswapped!
	card_direntry dirEntry = *d->dirEntry;
	dirEntry.lastmodified	= cpu_to_be32(dirEntry.lastmodified);
	dirEntry.iconaddr	= cpu_to_be32(dirEntry.iconaddr);
	dirEntry.iconfmt	= cpu_to_be16(dirEntry.iconfmt);
	dirEntry.iconspeed	= cpu_to_be16(dirEntry.iconspeed);
	dirEntry.block		= cpu_to_be16(dirEntry.block);
	dirEntry.length		= cpu_to_be16(dirEntry.length);
	dirEntry.commentaddr	= cpu_to_be32(dirEntry.commentaddr);

	// Write the directory entry.
	qint64 ret = qioDevice->write((char*)&dirEntry, (qint64)sizeof(dirEntry));
	if (ret != (qint64)sizeof(dirEntry)) {
		// Error saving the directory entry.
		return -2;
	}

	// Write the file data.
	const QByteArray fileData = d->loadFileData();
	ret = qioDevice->write(fileData);
	if (ret != (qint64)fileData.size()) {
		// Error saving the file data.
		return -3;
	}

	// Finished saving the file.
	return 0;
}

/**
 * Get the directory entry.
 * @return Directory entry.
 */
const card_direntry *GcnFile::dirEntry(void) const
{
	Q_D(const GcnFile);
	return d->dirEntry;
}

/**
 * Save the banner image.
 * @param filenameNoExt Filename for the GCI file, sans extension.
 * @return 0 on success; non-zero on error.
 * TODO: Error code constants.
 */
int GcnFile::saveBanner(const QString &filenameNoExt) const
{
	Q_D(const GcnFile);
	if (!d->gcBanner)
		return -EINVAL;

	// Append the correct extension.
	QString filename = filenameNoExt;
	const char *ext = GcImageWriter::extForImageFormat(GcImageWriter::IMGF_PNG);
	if (ext)
		filename += QChar(L'.') + QLatin1String(ext);

	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		// Error opening the file.
		// TODO: Convert QFileError to a POSIX error code.
		return -EIO;
	}

	// Write the banner image.
	int ret = saveBanner(&file);
	file.close();

	if (ret != 0) {
		// Error saving the banner image.
		file.remove();
	}

	return ret;
}

/**
 * Save the banner image.
 * @param qioDevice QIODevice to write the banner image to.
 * @return 0 on success; non-zero on error.
 * TODO: Error code constants.
 */
int GcnFile::saveBanner(QIODevice *qioDevice) const
{
	Q_D(const GcnFile);
	if (!d->gcBanner)
		return -EINVAL;

	GcImageWriter gcImageWriter;
	int ret = gcImageWriter.write(d->gcBanner, GcImageWriter::IMGF_PNG);
	if (!ret) {
		const vector<uint8_t> *pngData = gcImageWriter.memBuffer();
		ret = qioDevice->write(reinterpret_cast<const char*>(pngData->data()), pngData->size());
		if (ret != (qint64)pngData->size())
			return -EIO;
		ret = 0;
	}

	// Saved the banner image.
	return ret;
}

/**
 * Save the icon.
 * @param filenameNoExt Filename for the icon, sans extension.
 * @param animImgf Animated image format to use for animated icons.
 * @return 0 on success; non-zero on error.
 * TODO: Error code constants.
 */
int GcnFile::saveIcon(const QString &filenameNoExt,
	GcImageWriter::AnimImageFormat animImgf) const
{
	Q_D(const GcnFile);
	if (d->gcIcons.isEmpty())
		return -EINVAL;

	// Append the correct extension.
	const char *ext;
	if (d->gcIcons.size() > 1) {
		// Animated icon.
		ext = GcImageWriter::extForAnimImageFormat(animImgf);
	} else {
		// Static icon.
		ext = GcImageWriter::extForImageFormat(GcImageWriter::IMGF_PNG);
	}

	// NOTE: Due to PNG_FPF saving multiple files, we can't simply
	// call a version of saveIcon() that takes a QIODevice.
	GcImageWriter gcImageWriter;
	int ret;
	if (d->gcIcons.size() > 1) {
		// Animated icon.
		vector<const GcImage*> gcImages;
		const int maxIcons = (d->gcIcons.size() * 2 - 2);
		gcImages.reserve(maxIcons);
		gcImages.resize(d->gcIcons.size());
		for (int i = 0; i < d->gcIcons.size(); i++)
			gcImages[i] = d->gcIcons[i];

		// Icon speed.
		vector<int> gcIconDelays;
		gcIconDelays.reserve(maxIcons);
		gcIconDelays.resize(d->gcIcons.size());
		for (int i = 0; i < d->gcIcons.size(); i++)
			gcIconDelays[i] = iconDelay(i);

		if (gcImages.size() > 1 && iconAnimMode() == CARD_ANIM_BOUNCE) {
			// BOUNCE animation.
			int src = (gcImages.size() - 2);
			int dest = gcImages.size();
			gcImages.resize(maxIcons);
			gcIconDelays.resize(maxIcons);
			for (; src >= 1; src--, dest++) {
				gcImages[dest] = gcImages[src];
				gcIconDelays[dest] = gcIconDelays[src];
			}
		}

		ret = gcImageWriter.write(&gcImages, &gcIconDelays, animImgf);
	} else {
		// Static icon.
		ret = gcImageWriter.write(d->gcIcons.at(0), GcImageWriter::IMGF_PNG);
	}

	if (ret != 0) {
		// Error writing the icon.
		return ret;
	}

	// Icon written successfully.
	// Save it to a file.
	for (int i = 0; i < gcImageWriter.numFiles(); i++) {
		QString filename = filenameNoExt;
		if (gcImageWriter.numFiles() > 1) {
			// Multiple files.
			// Append the file number.
			char tmp[8];
			snprintf(tmp, sizeof(tmp), "%02d", i+1);
			filename += QChar(L'.') + QLatin1String(tmp);
		}

		// Append the file extension.
		if (ext)
			filename += QChar(L'.') + QLatin1String(ext);

		QFile file(filename);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			// Error opening the file.
			// TODO: Convert QFileError to a POSIX error code.
			// TODO: Delete previous files?
			return -EIO;
		}

		const vector<uint8_t> *pngData = gcImageWriter.memBuffer(i);
		ret = file.write(reinterpret_cast<const char*>(pngData->data()), pngData->size());
		file.close();

		if (ret != (qint64)pngData->size()) {
			// Error saving the icon.
			file.remove();
			return -EIO;
		}

		ret = 0;
	}

	return ret;
}
