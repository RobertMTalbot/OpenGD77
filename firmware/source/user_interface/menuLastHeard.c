/*
 * Copyright (C)2019 Roger Clark. VK3KYY / G4KYF
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <user_interface/menuSystem.h>
#include <user_interface/menuUtilityQSOData.h>
#include <user_interface/uiLocalisation.h>
const int LAST_HEARD_NUM_LINES_ON_DISPLAY = 3;

static void updateScreen(void);
static void handleEvent(ui_event_t *ev);

int menuLastHeard(ui_event_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		gMenusStartIndex = LinkHead->id;// reuse this global to store the ID of the first item in the list
		gMenusEndIndex=0;
		updateScreen();
	}
	else
	{
		// do live update by checking if the item at the top of the list has changed
		if (gMenusStartIndex != LinkHead->id || menuDisplayQSODataState==QSO_DISPLAY_CALLER_DATA)
		{
			gMenusStartIndex = LinkHead->id;
			gMenusCurrentItemIndex=0;
			gMenusEndIndex=0;
			updateScreen();
		}

		if (ev->hasEvent)
			handleEvent(ev);
	}
	return 0;
}

static void updateScreen(void)
{
	static const int bufferLen = 17;
	char buffer[bufferLen];
	dmrIdDataStruct_t foundRecord;
	int numDisplayed=0;
	LinkItem_t *item = LinkHead;

	UC1701_clearBuf();
	menuDisplayTitle(currentLanguage->last_heard);

	// skip over the first gMenusCurrentItemIndex in the listing
	for(int i=0;i<gMenusCurrentItemIndex;i++)
	{
		item=item->next;
	}
	while((item != NULL) && item->id != 0)
	{
		if (dmrIDLookup(item->id,&foundRecord))
		{
			UC1701_printCentered(16+(numDisplayed*16), foundRecord.text,UC1701_FONT_8x16);
		}
		else
		{
			if (item->talkerAlias[0] != 0x00)
			{
				memcpy(buffer, item->talkerAlias, bufferLen - 1);// limit to 1 line of the display which is 16 chars at the normal font size
			}
			else
			{
				snprintf(buffer, bufferLen, "ID:%d", item->id);
			}
			buffer[bufferLen - 1] = 0;
			UC1701_printCentered(16+(numDisplayed*16), buffer, UC1701_FONT_8x16);
		}

		numDisplayed++;

		item=item->next;
		if (numDisplayed > (LAST_HEARD_NUM_LINES_ON_DISPLAY -1))
		{
			if (item!=NULL && item->id != 0)
			{
				gMenusEndIndex=0x01;
			}
			else
			{
				gMenusEndIndex=0;
			}
			break;
		}
	}
	UC1701_render();
	displayLightTrigger();
	menuDisplayQSODataState = QSO_DISPLAY_IDLE;
}

static void handleEvent(ui_event_t *ev)
{

	if (KEYCHECK_PRESS(ev->keys,KEY_DOWN) && gMenusEndIndex!=0)
	{
		gMenusCurrentItemIndex++;
	}
	else if (KEYCHECK_PRESS(ev->keys,KEY_UP))
	{
		gMenusCurrentItemIndex--;
		if (gMenusCurrentItemIndex<0)
		{
			gMenusCurrentItemIndex=0;
		}
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_GREEN))
	{
		menuSystemPopAllAndDisplayRootMenu();
		return;
	}
	updateScreen();
}
