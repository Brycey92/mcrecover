/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * SADataEvents.c: Sonic Adventure - Events data.                          *
 *                                                                         *
 * Copyright (c) 2015-2016 by David Korth.                                 *
 * Original data from SASave by MainMemory.                                *
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

#include "SAData.h"

// NOTE: Many of the event flags are unused, so instead of using
// an array, we're using a struct of event flags.
// FIXME: Alignment on 64-bit?
#include "bit_flag.h"

// Sonic Adventure event flags data.
// Borrowed from SASave.
// TODO: Auto-generate a .h file from SASave's Events.ini?
const bit_flag_t sa_event_flags_desc[SA_EVENT_FLAGS_COUNT+1] = {
	{65, "Sonic Unlocked in Adventure"},
	{66, "Tails Unlocked in Adventure"},
	{67, "Knuckles Unlocked in Adventure"},
	{68, "Amy Unlocked in Adventure"},
	{69, "Gamma Unlocked in Adventure"},
	{70, "Big Unlocked in Adventure"},
	{71, "Super Sonic Unlocked in Adventure"},
	{72, "Sonic's Story Complete"},
	{73, "Tails' Story Complete"},
	{74, "Knuckles' Story Complete"},
	{75, "Amy's Story Complete"},
	{76, "Gamma's Story Complete"},
	{77, "Big's Story Complete"},
	{78, "Super Sonic's Story Complete"},
	{79, "Gold Chao Egg Taken"},
	{80, "Silver Chao Egg Taken"},
	{81, "Black Chao Egg Taken"},
	{128, "Police Barricade Removed"},
	{129, "Station Square Hotel Open"},
	{130, "Station Square Station Open"},
	{131, "Car Covering Sewer Removed"},
	{132, "Casino Always Open"},
	{133, "Ice Stone Available"},
	{134, "Door From Hotel To Casino Open"},
	{135, "Casinopolis Open"},
	{136, "Door From Station To Casino Open"},
	{137, "Twinkle Park Open"},
	{138, "Twinkle Circuit Open"},
	{140, "Speed Highway Open"},
	{141, "Light Speed Shoes Obtained"},
	{142, "Crystal Ring Obtained"},
	{145, "Egg Carrier Transformed"},
	{146, "Egg Carrier Open"},
	{147, "Egg Carrier Sunk"},
	{148, "Windy Valley Open"},
	{150, "Angel Island Open"},
	{151, "Ice Cap Open"},
	{153, "Red Mountain Open"},
	{154, "Door To Cliff Open"},
	{155, "Mystic Ruins Jungle Open"},
	{156, "Lost World Open"},
	{157, "Final Egg Field Open"},
	{159, "Ancient Light Obtained"},
	{160, "Final Egg Open"},
	{161, "SS-MR Train Open"},
	{162, "Boat To Egg Carrier Open"},
	{163, "Raft To Egg Carrier Open"},
	{164, "Gamma Defeated"},
	{165, "Knuckles Defeated"},
	{166, "Emerald Coast Clear"},
	{167, "Windy Valley Clear"},
	{168, "Casinopolis Clear"},
	{169, "Twinkle Park Clear"},
	{170, "Speed Highway Clear"},
	{171, "Red Mountain Clear"},
	{172, "Ice Cap Clear"},
	{173, "Sky Deck Clear"},
	{174, "Lost World Clear"},
	{175, "Final Egg Clear"},
	{176, "Chaos 0 Clear"},
	{177, "Chaos 4 Clear"},
	{178, "Chaos 6 Clear"},
	{179, "Egg Hornet Clear"},
	{180, "Egg Viper Clear"},
	{181, "Sky Chase Act 1 Clear"},
	{182, "Sky Chase Act 2 Clear"},
	{193, "Station Square Hotel Open"},
	{197, "Casinopolis Open"},
	{202, "Station Square Station Open"},
	{205, "Jet Anklet Obtained"},
	{210, "Egg Carrier Sunk"},
	{211, "Windy Valley Open"},
	{214, "Ice Cap Open"},
	{219, "Rhythm Badge Obtained"},
	{220, "SS-MR Train Open"},
	{225, "Windy Valley Clear"},
	{226, "Casinopolis Clear"},
	{227, "Speed Highway Clear"},
	{228, "Ice Cap Clear"},
	{229, "Sky Deck Clear"},
	{230, "Sand Hill Clear"},
	{231, "Chaos 4 Clear"},
	{232, "Egg Walker Clear"},
	{233, "Egg Hornet Clear"},
	{234, "Sky Chase Act 1 Clear"},
	{235, "Sky Chase Act 2 Clear"},
	{236, "Emerald Coast Clear"},
	{237, "Red Mountain Clear"},
	{259, "Casinopolis Open"},
	{271, "Egg Carrier Sunk"},
	{281, "Shovel Claw Obtained"},
	{282, "Fighting Gloves Obtained"},
	{287, "Casinopolis Clear"},
	{288, "Speed Highway Clear"},
	{289, "Red Mountain Clear"},
	{290, "Lost World Clear"},
	{291, "Chaos 2 Clear"},
	{292, "Chaos 6 Clear"},
	{293, "Chaos 4 Clear"},
	{294, "Sky Deck Clear"},
	{333, "Egg Carrier Sunk"},
	{338, "Warrior Feather Obtained"},
	{342, "Twinkle Park Clear"},
	{343, "Hot Shelter Clear"},
	{344, "Final Egg Clear"},
	{345, "Zero Clear"},
	{350, "Long Hammer"},
	{393, "Jet Booster Obtained"},
	{394, "Laser Blaster Obtained"},
	{396, "Egg Carrier Sunk"},
	{400, "Windy Valley Open"},
	{411, "Emerald Coast Clear"},
	{412, "Windy Valley Clear"},
	{413, "Red Mountain Clear"},
	{414, "Hot Shelter Clear"},
	{415, "Final Egg Clear"},
	{416, "E-101 Clear"},
	{417, "E-101mkII Clear"},
	{459, "Egg Carrier Sunk"},
	{464, "Life Ring Obtained"},
	{465, "Power Rod Obtained"},
	{469, "Emerald Coast Clear"},
	{470, "Hot Shelter Clear"},
	{471, "Twinkle Park Clear"},
	{472, "Ice Cap Clear"},
	{473, "Chaos 6 Clear"},

	// End of list.
	{-1, nullptr}
};
