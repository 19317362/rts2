/* 
 * Labels support (for targets,..).
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "rts2db/labels.h"
#include "rts2db/sqlerror.h"

using namespace rts2db;
		
int Labels::getLabel (const char *label, int type)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_label_id;
	VARCHAR d_label[500];
	int d_type = type;
	EXEC SQL END DECLARE SECTION;
	int l = strlen (label);
	if (l > 500)
		throw SqlError ("label is too long");
	memcpy (d_label.arr, label, l);
	d_label.len = l;

	EXEC SQL SELECT
		label_id
	INTO
		:d_label_id
	FROM
		labels
	WHERE
		label_text LIKE :d_label AND label_type = :d_type;
	if (sqlca.sqlcode)
	{
		if (sqlca.sqlcode == ECPG_NOT_FOUND)
		{
			EXEC SQL ROLLBACK;
			return -1;
		}
		EXEC SQL ROLLBACK;
		throw SqlError ();
	}
	EXEC SQL COMMIT;
	return d_label_id;
}

int Labels::insertLabel (const char *label, int type)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_label_id;
	VARCHAR d_label[500];
	int d_type = type;
	EXEC SQL END DECLARE SECTION;

	int l = strlen (label);
	if (l > 500)
		throw SqlError ("too long label");
	memcpy (d_label.arr, label, l);
	d_label.len = l;

	EXEC SQL SELECT nextval('label_id') INTO :d_label_id;

	EXEC SQL INSERT INTO labels
		(label_id, label_type, label_text)
	VALUES
		(:d_label_id, :d_type, :d_label);
	
	if (sqlca.sqlcode)
	{
		EXEC SQL ROLLBACK;
		throw SqlError ();
	}
	EXEC SQL COMMIT;
	return d_label_id;
}

void Labels::addLabel (int tar_id, int label_id)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_tar_id = tar_id;
	int d_label_id = label_id;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL INSERT INTO target_labels
		(tar_id, label_id)
	VALUES
		(:d_tar_id, :d_label_id);
	if (sqlca.sqlcode)
	{
		EXEC SQL ROLLBACK;
		throw SqlError ();
	}
	EXEC SQL COMMIT;
}

void Labels::addLabel (int tar_id, const char *label, int type, bool create)
{
	int label_id = getLabel (label, type);
	if (label_id < 0)
	{
		if (!create)
			throw SqlError ("label not found");
		label_id = insertLabel (label, type);
	}
	addLabel (tar_id, label_id);
}

std::vector <std::pair <int, std::string> > Labels::getTargetLabels (int tar_id)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_tar_id = tar_id;
	int d_type;
	VARCHAR label[501];
	EXEC SQL END DECLARE SECTION;
	std::vector <std::pair <int, std::string> > ret;

	EXEC SQL DECLARE label_target_cur CURSOR FOR
	SELECT
		label_type, label_text
	FROM
		labels, target_labels
	WHERE
		target_labels.tar_id = :d_tar_id
		AND target_labels.label_id = labels.label_id;
	EXEC SQL OPEN label_target_cur;
	while (true)
	{
		EXEC SQL FETCH next FROM label_target_cur INTO :d_type, :label;
		if (sqlca.sqlcode)
			break; 
		label.arr[label.len] = '\0';
		ret.push_back (std::pair <int, std::string> (d_type, label.arr));
	}
	if (sqlca.sqlcode != ECPG_NOT_FOUND)
	{
		EXEC SQL ROLLBACK;
		throw SqlError ();
	}
	EXEC SQL CLOSE label_target_cur;
	EXEC SQL COMMIT;
	return ret;
}

std::vector <std::string> Labels::getTargetLabels (int tar_id, int type)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_tar_id = tar_id;
	int d_type = type;
	VARCHAR label[501];
	EXEC SQL END DECLARE SECTION;

	std::vector <std::string> ret;

	EXEC SQL DECLARE label_cur CURSOR FOR
	SELECT label_text
	FROM labels, target_labels
	WHERE
		labels.label_id = target_labels.label_id
		AND target_labels.tar_id = :d_tar_id
		AND labels.label_type = :d_type;
	EXEC SQL OPEN label_cur;
	while (true)
	{
		EXEC SQL FETCH next FROM label_cur INTO :label;
		if (sqlca.sqlcode)
			break;
		label.arr[label.len] = '\0';
		ret.push_back (std::string (label.arr));
	}
	if (sqlca.sqlcode != ECPG_NOT_FOUND)
	{
		EXEC SQL ROLLBACK;
		throw SqlError ();
	}
	EXEC SQL CLOSE label_cur;
	EXEC SQL COMMIT;
	return ret;
}

void Labels::deleteTargetLabels (int tar_id, int type)
{
	EXEC SQL BEGIN DECLARE SECTION;
	int d_tar_id = tar_id;
	int d_type = type;
	EXEC SQL END DECLARE SECTION;

	EXEC SQL DELETE FROM target_labels WHERE tar_id = :d_tar_id
		AND (SELECT label_type FROM labels WHERE labels.label_id = target_labels.label_id) = :d_type;
	EXEC SQL COMMIT;
	if (sqlca.sqlcode)
		throw SqlError ();
}

const char *getLabelName (int type)
{
	switch (type)
	{
		case LABEL_PI:
			return "PI";
		case LABEL_PROGRAM:
			return "PROGRAM";
	}
	return "unknown label type";
}
