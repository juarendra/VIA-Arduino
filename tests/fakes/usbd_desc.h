#pragma once

#include <stdint.h>

#define USBD_VID 0xFECA
#define USBD_PID 0x4003

typedef enum { USBD_IDX_MFC_STR = 0x01, USBD_IDX_PRODUCT_STR, USBD_IDX_SERIAL_STR,
               USBD_IDX_CONFIG_STR, USBD_IDX_INTERFACE_STR } USBD_IdxStr;
