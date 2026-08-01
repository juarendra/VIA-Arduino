#pragma once

#include <stdint.h>

#define HID_EPIN_ADDR   0x81
#define HID_EPIN_SIZE   0x08
#define HID_EPOUT_ADDR  0x02
#define HID_EPOUT_SIZE  0x08

#define HID_REQ_SET_PROTOCOL    0x0B
#define HID_REQ_GET_PROTOCOL    0x03
#define HID_REQ_SET_IDLE        0x0A
#define HID_REQ_GET_IDLE        0x02
#define HID_REQ_SET_REPORT      0x09
#define HID_REQ_GET_REPORT      0x01

#define HID_DESCRIPTOR_TYPE     0x21
#define HID_REPORT_DESC         0x22

#define HID_HS_BINTERVAL        0x01
#define HID_FS_BINTERVAL        0x01
#define HID_POLLING_INTERVAL    2

typedef struct {
  uint8_t *pReport; // pointer to report descriptor on stack (not copied)
  uint16_t ReportSize;
} USBD_HID_DescTypeDef;
