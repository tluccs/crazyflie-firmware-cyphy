/*
 * beaconComm.c
 *
 *  Created on: Jul 3, 2018
 *      Author: bitcraze
 *
 *      Intended to communicate between drone and beacon
 */
/* USES THE FOLLOWING FUNCTIONS FROM LOCODECK.C
 *
bool lpsSendLppShort(uint8_t destId, void* data, size_t length)
{
  bool result = false;

  if (isInit)
  {
    lppShortPacket.dest = destId;
    lppShortPacket.length = length;
    memcpy(lppShortPacket.data, data, length);
    result = xQueueSend(lppShortQueue, &lppShortPacket,0) == pdPASS;
  }

  return result;
}
(if 1 then got packet, else did not)
bool lpsGetLppShort(lpsLppShortPacket_t* shortPacket)
{
  return xQueueReceive(lppShortQueue, shortPacket, 0) == pdPASS;
}
(check isInit to see if everything good)
static bool dwm1000Test()
{
  if (!isInit) {
    DEBUG_PRINT("Error while initializing DWM1000\n");
  }

  return isInit;
}
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "crtp.h"
#include "crc.h"
#include "config.h"
#include "droneComm.h"

#define DEBUG_MODULE "DWM"

#include <stdint.h>
#include "stm32fxxx.h"

#include "queue.h"

#include "deck.h"
#include "system.h"
#include "debug.h"
#include "nvicconf.h"

#include "configblock.h"
#include "lpsTdma.h"

#include "lpsTdoa2Tag.h"
#include "lpsTdoa3Tag.h"
#include "lpsTwrTag.h"
#include "locodeck.h"

#ifdef STM32F40_41xxx
#include "stm32f4xx.h"
#else
#include "stm32f10x.h"
#ifndef SCB_ICSR_VECTACTIVE_Msk
#define SCB_ICSR_VECTACTIVE_Msk 0x1FFUL
#endif
#endif
#define LPS_MAX_DATA_SIZE 30

uint8_t destId;
char message[LPS_MAX_DATA_SIZE];
size_t messageLength;

//static void beaconCommTask(void * prm);

static xSemaphoreHandle beaconLock;

static bool isInit;

void testMsg(void){
	if (true){
		  droneCommPflush("TWR init success! beacons online");
	  }
	  else{
		  droneCommPflush("beacon bad init");
	  }
}

static bool beaconCommSendMessage(void)
{

  if (lpsSendLppShort(destId, (void*)(message), messageLength))
  {
    messageLength = 0;
  }
  else
  {
    return false;
  }

  return true;
}

void beaconCommInit()
{
  if (isInit)
    return;

  messageLength = 0;
  vSemaphoreCreateBinary(beaconLock);
  //xTaskCreate(beaconCommTask, BEACON_COMM_TASK_NAME,BEACON_COMM_TASK_STACKSIZE, NULL, BEACON_COMM_TASK_PRI, NULL);
  isInit = true;
  testMsg();
}

bool beaconCommTest(void)
{
  return isInit;
}

int beaconCommPutcharFromISR(int ch) {
  BaseType_t higherPriorityTaskWoken;

  if (xSemaphoreTakeFromISR(beaconLock, &higherPriorityTaskWoken) == pdTRUE) {
    if (messageLength < LPS_MAX_DATA_SIZE)
    {
      message[messageLength] = (unsigned char)ch;
      messageLength++;
    }
    xSemaphoreGiveFromISR(beaconLock, &higherPriorityTaskWoken);
  }

  return ch;
}

int beaconCommPutchar(int ch)
{
  bool isInInterrupt = (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;

  if (!isInit) {
    return 0;
  }

  if (isInInterrupt) {
    return beaconCommPutcharFromISR(ch);
  }

  if (xSemaphoreTake(beaconLock, portMAX_DELAY) == pdTRUE)
  {
    if (messageLength < LPS_MAX_DATA_SIZE)
    {
      message[messageLength] = (unsigned char)ch;
      messageLength++;
    }
    if (ch == '\n' || messageLength >= LPS_MAX_DATA_SIZE)
    {
      if (ch == '\n' && messageLength <  LPS_MAX_DATA_SIZE){
    	  message[messageLength] = '\0';
    	  messageLength++;
      }
      beaconCommSendMessage();
    }
    xSemaphoreGive(beaconLock);
  }

  return (unsigned char)ch;
}

int beaconCommPuts(char *str)
{
  int ret = 0;

  while(*str)
    ret |= beaconCommPutchar(*str++);

  return ret;
}

void beaconCommFlush(void)
{
  if (xSemaphoreTake(beaconLock, portMAX_DELAY) == pdTRUE)
  {
    beaconCommSendMessage();
    xSemaphoreGive(beaconLock);
  }
}

void beaconCommPflush(char * str)
{
	beaconCommPuts(str);
	beaconCommFlush();
}

//SEND CODE ABOVE. RECEIVES IN lpsTwrTag AND ANALYZE BELOW

void beaconAnalyzePayload(char * data)
{
	//for now, send this data through droneComm
	droneCommPflush(data);
}

/*void beaconCommTask(void * prm)
{

	while(1) {
		crtpReceivePacketBlock(CRTP_PORT_, &messageReceived);

		if (messageReceived.channel==0){
			// currently this is data from the PC
			//as a test, send this data back
		  beaconCommFlush();
		  beaconCommPuts("CYPHY");
		  //messsageReceived.data is uint8_t, want to typecast to char.
		  beaconCommPflush((char*)(messageReceived.data));
		}
	}
}*/

