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
#include <hardware/fw_HR-C6000.h>
#include <user_interface/menuSystem.h>
#include <user_interface/menuUtilityQSOData.h>
#include <user_interface/uiLocalisation.h>
#include "fw_settings.h"
#include "fw_codeplug.h"
#include "fw_ticks.h"

static char digits[9];
static int pcIdx;
static struct_codeplugContact_t contact;

static void updateCursor(void);
static void updateScreen(void);
static void handleEvent(ui_event_t *ev);

static const uint32_t CURSOR_UPDATE_TIMEOUT = 1000;

static const char *menuName[4];
enum DISPLAY_MENU_LIST { ENTRY_TG = 0, ENTRY_PC, ENTRY_SELECT_CONTACT, ENTRY_USER_DMR_ID, NUM_ENTRY_ITEMS};
// public interface
int menuNumericalEntry(ui_event_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuName[0] = currentLanguage->tg_entry;
		menuName[1] = currentLanguage->pc_entry;
		menuName[2] = currentLanguage->contact;
		menuName[3] = currentLanguage->user_dmr_id;
		gMenusCurrentItemIndex=ENTRY_TG;
		digits[0]=0x00;
		pcIdx = 0;
		updateScreen();
	}
	else
	{
		if (ev->events == EVENT_BUTTON_NONE)
		{
			updateCursor();
		}
		else
		{
			handleEvent(ev);
		}
	}
	return 0;
}

static void updateCursor(void)
{
	size_t sLen;

	// Display blinking cursor only when digits could be entered.
	if ((gMenusCurrentItemIndex != ENTRY_SELECT_CONTACT) && ((sLen = strlen(digits)) < 7))
	{
		static uint32_t lastBlink = 0;
		static bool     blink = false;
		uint32_t        m = fw_millis();

		if ((m - lastBlink) > CURSOR_UPDATE_TIMEOUT)
		{
			sLen *= 8;

			UC1701_printCore((((128 - sLen) >> 1) + sLen), 32, "_", UC1701_FONT_8x16, 0, blink);

			blink = !blink;
			lastBlink = m;

			UC1701_render();
		}
	}
}

static void updateScreen(void)
{
	char buf[33];
	size_t sLen = strlen(menuName[gMenusCurrentItemIndex]) * 8;
	int16_t y = 8;

	UC1701_clearBuf();

	UC1701_drawRoundRectWithDropShadow(2, y - 1, (128 - 6), 21, 3, true);

	// Not really centered, off by 2 pixels
	UC1701_printAt(((128 - sLen) >> 1) - 2, y, (char *)menuName[gMenusCurrentItemIndex], UC1701_FONT_8x16);

	if (pcIdx == 0)
	{
		UC1701_printCentered(32, (char *)digits,UC1701_FONT_8x16);
	}
	else
	{
		codeplugUtilConvertBufToString(contact.name, buf, 16);
		UC1701_printCentered(32, buf, UC1701_FONT_8x16);
		UC1701_printCentered(52, (char *)digits,UC1701_FONT_6x8);
	}
	displayLightTrigger();

	UC1701_render();
}

static int getNextContact(int curidx, int dir, struct_codeplugContact_t *contact)
{
	int idx = curidx;

	do {
		idx += dir;
		if (idx >= 1024) {
			if (curidx == 0) {
				idx = 0;
				break;
			}
			idx = 1;
		} else if (idx ==0) {
			if (curidx == 0) {
				idx = 0;
				break;
			}
			idx = 1024;
		}
		codeplugContactGetDataForIndex(idx, contact);
	} while ((curidx != idx) && ((*contact).name[0] == 0xff));

	return idx;
}

static void handleEvent(ui_event_t *ev)
{
	size_t sLen;

	if (KEYCHECK_SHORTUP(ev->keys,KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_GREEN))
	{
		if (gMenusCurrentItemIndex != ENTRY_USER_DMR_ID)
		{
			uint32_t saveTrxTalkGroupOrPcId = trxTalkGroupOrPcId;
			trxTalkGroupOrPcId = atoi(digits);
			nonVolatileSettings.overrideTG = trxTalkGroupOrPcId;
			if (gMenusCurrentItemIndex == ENTRY_PC || (pcIdx != 0 && contact.callType == 0x01))
			{
				// Private Call

				if ((saveTrxTalkGroupOrPcId >> 24) != PC_CALL_FLAG)
				{
					// if the current Tx TG is a TalkGroup then save it so it can be stored after the end of the private call
					menuUtilityTgBeforePcMode = saveTrxTalkGroupOrPcId;
				}
				nonVolatileSettings.overrideTG |= (PC_CALL_FLAG << 24);
			}
		}
		else
		{
			trxDMRID = atoi(digits);
			if (ev->buttons & BUTTON_SK2)
			{
				// make the change to DMR ID permanent if Function + Green is pressed
				codeplugSetUserDMRID(trxDMRID);
			}
		}
		menuSystemPopAllAndDisplayRootMenu();
	}
	else if (KEYCHECK_SHORTUP(ev->keys,KEY_HASH))
	{
		pcIdx = 0;
		if ((ev->buttons & BUTTON_SK2)!= 0  && gMenusCurrentItemIndex == ENTRY_SELECT_CONTACT)
		{
			digits[0] = 0x00;
			gMenusCurrentItemIndex = ENTRY_USER_DMR_ID;
		}
		else
		{
			gMenusCurrentItemIndex++;
			if (gMenusCurrentItemIndex > ENTRY_SELECT_CONTACT)
			{
				gMenusCurrentItemIndex = ENTRY_TG;
			} else if (gMenusCurrentItemIndex == ENTRY_SELECT_CONTACT)
			{
				pcIdx = getNextContact(0, 1, &contact);
				if (pcIdx != 0 ) {
					itoa(contact.tgNumber, digits, 10);
				}
			}
		}

		updateScreen();
	}
	if (gMenusCurrentItemIndex == ENTRY_SELECT_CONTACT)
	{
		int idx = pcIdx;

		if (KEYCHECK_PRESS(ev->keys,KEY_DOWN)) {
			idx = getNextContact(pcIdx, 1, &contact);
		} else if (KEYCHECK_PRESS(ev->keys,KEY_UP)) {
			idx = getNextContact(pcIdx, -1, &contact);
		}
		if (pcIdx != idx ) {
			pcIdx = idx;
			itoa(contact.tgNumber, digits, 10);
			updateScreen();
		}
	}
	else if ((sLen = strlen(digits)) <= 7)
	{
		bool refreshScreen = false;

		// Inc / Dec entered value.
		if (KEYCHECK_PRESS(ev->keys,KEY_UP) || KEYCHECK_PRESS(ev->keys,KEY_DOWN))
		{
			if (strlen(digits))
			{
				unsigned long int ccs7 = strtoul(digits, NULL, 10);

				if (KEYCHECK_PRESS(ev->keys,KEY_UP))
				{
					if (ccs7 < 9999999)
						ccs7++;

					refreshScreen = true;
				}
				else
				{
					if (ccs7 > 1)
						ccs7--;

					refreshScreen = true;
				}

				if (refreshScreen)
				{
					sprintf(digits, "%lu", ccs7);
				}
			}
		} // Delete a digit
		else if (KEYCHECK_PRESS(ev->keys,KEY_LEFT))
		{
			if ((sLen = strlen(digits)) > 0)
			{
				digits[sLen - 1] = 0;
				refreshScreen = true;
			}
		}
		else
		{
			// Add a digit
			if (sLen < 7)
			{
				char c[2] = {0, 0};
				c[0] = keypressToNumberChar(ev->keys);
				
				if (c[0]!=0)
				{
					strcat(digits,c);
					refreshScreen = true;
				}
			}
		}

		if (refreshScreen)
		{
			updateScreen();
		}

		updateCursor();
	}
}
