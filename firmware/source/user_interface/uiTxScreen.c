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


static void updateScreen(void);
static void handleEvent(ui_event_t *ev);

static const int PIT_COUNTS_PER_SECOND = 10000;
static int timeInSeconds;
static uint32_t nextSecondPIT;

static int micLevelUpdateCounter=100;

int menuTxScreen(ui_event_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		uiChannelModeScanActive=false;
		trxIsTransmittingTone = false;
		settingsPrivateCallMuteMode = false;
		micLevelUpdateCounter=100;
		if ((currentChannelData->flag4 & 0x04) == 0x00 && (  trxCheckFrequencyInAmateurBand(currentChannelData->txFreq) || nonVolatileSettings.txFreqLimited == false))
		{
			nextSecondPIT = PITCounter + PIT_COUNTS_PER_SECOND;
			timeInSeconds = currentChannelData->tot*15;

			GPIO_PinWrite(GPIO_LEDgreen, Pin_LEDgreen, 0);
			GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 1);

			txstopdelay=0;
			clearIsWakingState();
			trx_setTX();
			updateScreen();
		}
		else
		{
			// Note.
			// Currently these messages are not being displayed, because this screen gets immediately after the display is update
			// However the melody will play
			//
			// We need to work out how to display this message for 1 or 2 seconds, even if the PTT is released.
			// But this would require some sort of timer callback system, which we don't currently have.
			//
			UC1701_clearBuf();
			UC1701_printCentered(4, currentLanguage->error,UC1701_FONT_16x32);
			if ((currentChannelData->flag4 & 0x04) !=0x00)
			{
				UC1701_printCentered(40, currentLanguage->rx_only,UC1701_FONT_8x16);
			}
			else
			{
				UC1701_printCentered(40,currentLanguage->out_of_band,UC1701_FONT_8x16);
			}
			UC1701_render();
			displayLightOverrideTimeout(-1);
			set_melody(melody_ERROR_beep);
		}
	}
	else
	{
		if (trxIsTransmitting && (getIsWakingState() == WAKING_MODE_NONE ))
		{
			if (PITCounter >= nextSecondPIT )
			{
				if (currentChannelData->tot==0)
				{
					timeInSeconds++;
				}
				else
				{
					timeInSeconds--;
					if (timeInSeconds <= (nonVolatileSettings.txTimeoutBeepX5Secs * 5))
					{
						if (timeInSeconds%5==0)
						{
							set_melody(melody_key_beep);
						}
					}
				}
				if (currentChannelData->tot!=0 && timeInSeconds == 0)
				{
					set_melody(melody_tx_timeout_beep);
					UC1701_clearBuf();
					UC1701_printCentered(20, currentLanguage->timeout,UC1701_FONT_16x32);
					UC1701_render();
				}
				else
				{
					updateScreen();
				}

				nextSecondPIT = PITCounter + PIT_COUNTS_PER_SECOND;
			}
			else
			{
				if (trxGetMode() == RADIO_MODE_DIGITAL)
				{
					if (micLevelUpdateCounter-- == 0)
					{
						drawDMRMicLevelBarGraph();
						UC1701RenderRows(1,2);
						micLevelUpdateCounter=100;
					}
				}
			}

		}

		handleEvent(ev);
	}
	return 0;
}

static void updateScreen(void)
{
	menuDisplayQSODataState = QSO_DISPLAY_DEFAULT_SCREEN;
	if (menuControlData.stack[0]==MENU_VFO_MODE)
	{
		menuVFOModeUpdateScreen(timeInSeconds);
		displayLightOverrideTimeout(-1);
	}
	else
	{
		menuChannelModeUpdateScreen(timeInSeconds);
		displayLightOverrideTimeout(-1);
	}
}

static void handleEvent(ui_event_t *ev)
{
	int keyval;

	if ((ev->buttons & BUTTON_PTT) == 0
			|| (currentChannelData->tot != 0 && timeInSeconds == 0))
	{
		if (trxIsTransmitting)
		{
			trxIsTransmitting = false;
			trxIsTransmittingTone = false;
			if (trxGetMode() == RADIO_MODE_ANALOG)
			{
				// In analog mode. Stop transmitting immediately
				GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);

				// Need to wrap this in Task Critical to avoid bus contention on the I2C bus.
				taskENTER_CRITICAL();
				trxActivateRx();
				taskEXIT_CRITICAL();
				menuSystemPopPreviousMenu();
			}
			// When not in analogue mode, only the trxIsTransmitting flag is cleared
			// This screen keeps getting called via the handleEvent function and goes into the else clause - below.
		}
		else
		{
			// In DMR mode, wait for the DMR system to finish before exiting
			if (slot_state < DMR_STATE_TX_START_1)
			{
				GPIO_PinWrite(GPIO_LEDred, Pin_LEDred, 0);
				menuSystemPopPreviousMenu();
			}
		}
	}
	if (!trxIsTransmittingTone && (ev->buttons & BUTTON_PTT) != 0 && trxIsTransmitting && trxGetMode() == RADIO_MODE_ANALOG)
	{
		if ((ev->buttons & BUTTON_SK2) != 0)
		{
			trxIsTransmittingTone = true;
			trxSetTone1(1750);
			trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_TONE1);
			GPIO_PinWrite(GPIO_audio_amp_enable, Pin_audio_amp_enable, 1);
			GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);
		}
		else
		{
			keyval = 99;
			if (KEYCHECK_DOWN(ev->keys,KEY_0))
			{
				keyval = 0;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_1))
			{
				keyval = 1;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_2))
			{
				keyval = 2;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_3))
			{
				keyval = 3;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_4))
			{
				keyval = 4;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_5))
			{
				keyval = 5;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_6))
			{
				keyval = 6;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_7))
			{
				keyval = 7;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_8))
			{
				keyval = 8;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_9))
			{
				keyval = 9;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_LEFT))  // A
			{
				keyval = 10;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_RIGHT)) // B
			{
				keyval = 11;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_UP))    // C
			{
				keyval = 12;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_DOWN))  // D
			{
				keyval = 13;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_STAR))
			{
				keyval = 14;
			}
			if (KEYCHECK_DOWN(ev->keys,KEY_HASH))
			{
				keyval = 15;
			}
			if (keyval != 99)
			{
				trxSetDTMF(keyval);
				trxIsTransmittingTone = true;
				trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_DTMF);
				GPIO_PinWrite(GPIO_audio_amp_enable, Pin_audio_amp_enable, 1);
				GPIO_PinWrite(GPIO_RX_audio_mux, Pin_RX_audio_mux, 1);
			}
		}
	}
	if (trxIsTransmittingTone && (ev->buttons & BUTTON_SK2) == 0 && (ev->keys == 0))
	{
		trxIsTransmittingTone = false;
		trxSelectVoiceChannel(AT1846_VOICE_CHANNEL_MIC);
		GPIO_PinWrite(GPIO_audio_amp_enable, Pin_audio_amp_enable, 0);
	}

}
