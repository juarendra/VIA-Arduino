#pragma once

#include <stdint.h>
#include <string.h>

typedef enum { USBD_OK = 0, USBD_BUSY, USBD_FAIL } USBD_StatusTypeDef;

typedef struct {
  uint8_t id;
} USBD_HandleTypeDef;

typedef struct {
  uint8_t bmRequest;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} USBD_SetupReqTypedef;

typedef struct _USBD_ClassTypeDef {
  int8_t (*Init)(USBD_HandleTypeDef*, uint8_t);
  int8_t (*DeInit)(USBD_HandleTypeDef*, uint8_t);
  int8_t (*Setup)(USBD_HandleTypeDef*, USBD_SetupReqTypedef*);
  int8_t (*EP0_TxSent)(USBD_HandleTypeDef*);
  int8_t (*EP0_RxReady)(USBD_HandleTypeDef*);
  int8_t (*DataIn)(USBD_HandleTypeDef*, uint8_t);
  int8_t (*DataOut)(USBD_HandleTypeDef*, uint8_t);
  int8_t (*SOF)(USBD_HandleTypeDef*);
  int8_t (*IsoINIncomplete)(USBD_HandleTypeDef*, uint8_t);
  int8_t (*IsoOUTIncomplete)(USBD_HandleTypeDef*, uint8_t);
} USBD_ClassTypeDef;

extern bool usbd_transmit_ok;
extern USBD_HandleTypeDef* usbd_last_pdev;
extern uint8_t usbd_last_ep;
extern const uint8_t* usbd_last_buf;
extern uint16_t usbd_last_len;

USBD_StatusTypeDef USBD_Init(USBD_HandleTypeDef*, USBD_ClassTypeDef*, uint8_t);
USBD_StatusTypeDef USBD_RegisterClass(USBD_HandleTypeDef*, USBD_ClassTypeDef*);
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef*);
USBD_StatusTypeDef USBD_Stop(USBD_HandleTypeDef*);
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint16_t);
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint16_t);
USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef*, uint8_t*, uint16_t);
USBD_StatusTypeDef USBD_CtlReceiveData(USBD_HandleTypeDef*, uint8_t*, uint16_t);
uint32_t USBD_GetRxCount(USBD_HandleTypeDef*, uint8_t);
