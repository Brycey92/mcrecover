/***************************************************************************
 * GameCube Memory Card Recovery Program.                                  *
 * MemCardItemDelegate.hpp: MemCard item delegate for QListView.           *
 *                                                                         *
 * Copyright (c) 2013 by David Korth.                                      *
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

#ifndef __MCRECOVER_MEMCARDITEMDELEGATE_HPP__
#define __MCRECOVER_MEMCARDITEMDELEGATE_HPP__

// Qt includes.
#include <QtGui/QStyledItemDelegate>

class MemCardItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

	public:
		MemCardItemDelegate(QObject* parent = 0);

		void paint(QPainter *painter, const QStyleOptionViewItem &option,
			   const QModelIndex &index) const;
#if 0
		QSize sizeHint(const QStyleOptionViewItem &option,
			       const QModelIndex &index) const;
#endif
};

#endif /* __MCRECOVER_MEMCARDITEMDELEGATE_HPP__ */
