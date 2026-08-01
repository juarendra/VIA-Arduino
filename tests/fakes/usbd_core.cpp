#include "usbd_core.h"

bool usbd_transmit_ok = true;
USBD_HandleTypeDef* usbd_last_pdev = nullptr;
uint8_t usbd_last_ep = 0;
const uint8_t* usbd_last_buf = nullptr;
uint16_t usbd_last_len = 0;

USBD_StatusTypeDef USBD_Init(USBD_HandleTypeDef*, USBD_ClassTypeDef*, uint8_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_RegisterClass(USBD_HandleTypeDef*, USBD_ClassTypeDef*) { return USBD_OK; }
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef*) { return USBD_OK; }
USBD_StatusTypeDef USBD_Stop(USBD_HandleTypeDef*) { return USBD_OK; }

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* pdev, uint8_t ep, uint8_t* buf, uint16_t len) {
  usbd_last_pdev = pdev;
  usbd_last_ep = ep;
  usbd_last_buf = buf;
  usbd_last_len = len;
  return usbd_transmit_ok ? USBD_OK : USBD_BUSY;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint16_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef*, uint8_t*, uint16_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_CtlReceiveData(USBD_HandleTypeDef*, uint8_t*, uint16_t) { return USBD_OK; }
uint32_t USBD_GetRxCount(USBD_HandleTypeDef*, uint8_t) { return 0; }
