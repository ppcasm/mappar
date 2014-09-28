#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include "comms.h"

static const uint16_t VENDOR_ID = 0x9710;
static const uint16_t PRODUCT_ID = 0x7705;

static const uint8_t REQTYPE_READ =
 LIBUSB_ENDPOINT_IN |
 LIBUSB_TRANSFER_TYPE_CONTROL |
 LIBUSB_REQUEST_TYPE_VENDOR;

static const uint8_t REQTYPE_WRITE =
 LIBUSB_ENDPOINT_OUT |
 LIBUSB_TRANSFER_TYPE_CONTROL |
 LIBUSB_REQUEST_TYPE_VENDOR;

static const uint8_t ENDPOINT_MOS_BULK_READ = 0x81;
static const uint8_t ENDPOINT_MOS_BULK_WRITE = 0x02;

static const uint8_t REQ_MOS_WRITE = 0x0E;
static const uint8_t REQ_MOS_READ  = 0x0D;

// value field, only one port on the 7705
enum { MOS_PP_PORT = 0 };
static const uint16_t MOS_PORT_BASE = (MOS_PP_PORT+1)<<8;

// index field, simulated IBM PC interface
static const uint16_t MOS_PP_DATA_REG   = 0x0000;
static const uint16_t MOS_PP_STATUS_REG = 0x0001;
static const uint16_t MOS_PP_CONTROL_REG= 0x0002;
static const uint16_t MOS_PP_DEBUG_REG  = 0x0004;
static const uint16_t MOS_PP_EXTENDED_CONTROL_REG = 0x000A;

static const uint8_t MOS_SPP_MODE    = 0 << 5;
static const uint8_t MOS_NIBBLE_MODE = 1 << 5; // default on reset
static const uint8_t MOS_FIFO_MODE   = 2 << 5;

static const int TIMEOUT = 0; // ms

void set_mos_mode(gscomms * g, int mos_mode) {
  // a chance for any ongoing transfers to finish
  // TODO: this should really be using the 7705's status
  //nanosleep(&hundredms, NULL);
 // sleep(1);

  int rc = libusb_control_transfer(
      g->dev, 
      REQTYPE_WRITE,
      REQ_MOS_WRITE,
      MOS_PORT_BASE | mos_mode,
      MOS_PP_EXTENDED_CONTROL_REG,
      NULL,
      0,
      TIMEOUT
      );

  if (rc != 0) {
    fprintf(stderr, "mode set failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }

}

uint8_t READ_PORT(gscomms * g) {
 
  unsigned char data = 0; 
 
  int rc = libusb_control_transfer(
      g->dev,
      REQTYPE_READ,
      REQ_MOS_READ,
      MOS_PORT_BASE,
      MOS_PP_STATUS_REG,
      &data,
      1,
      TIMEOUT);

  if (rc != 1) {
    fprintf(stderr, "read failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }
  
  return data;
}

void WRITE_PORT(gscomms * g, uint8_t data){

  int rc = libusb_control_transfer(
      g->dev,
      REQTYPE_WRITE,
      REQ_MOS_WRITE,
      MOS_PORT_BASE|data,
      MOS_PP_DATA_REG,
      NULL,
      0,
      TIMEOUT);

  if (rc != 0) {
    fprintf(stderr, "write failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }
}

gscomms * Init_Comms() {
  int rc;

  gscomms * g = malloc(sizeof(gscomms));

  rc = libusb_init(&g->ctx);
  if (rc != 0) {
    fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }

  g->dev = libusb_open_device_with_vid_pid(g->ctx, VENDOR_ID, PRODUCT_ID);

  if (!g->dev) {
    fprintf(stderr, "failed to locate/open device\n");
    exit(-1);
  }

  rc = libusb_claim_interface(g->dev, 0);
  if (rc != 0) {
    fprintf(stderr, "claim failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }


  const char * speed_string;

  rc = libusb_get_device_speed(libusb_get_device(g->dev));
  switch (rc) {
    case LIBUSB_SPEED_LOW:
      speed_string = "Low (1.5MBit/s)";
      break;
    case LIBUSB_SPEED_FULL:
      speed_string = "Full (12MBit/s)";
      break;
    case LIBUSB_SPEED_HIGH:
      speed_string = "High (480MBit/s)";
      break;
    case LIBUSB_SPEED_SUPER:
      speed_string = "Super (5000MBit/s)";
      break;
    default:
      speed_string = "unknown";
      break;
  }

  printf("Device speed: %s\n", speed_string);

  rc = libusb_get_max_packet_size(libusb_get_device(g->dev), ENDPOINT_MOS_BULK_WRITE);
  printf("Max bulk write packet: %d bytes\n", rc);
 
  return g;
}

void cleanup_gscomms(gscomms * g) {
  int rc;

  rc = libusb_release_interface(g->dev, 0);
  if (rc != 0) {
    fprintf(stderr, "release failed: %s\n", libusb_error_name(rc));
    exit(-1);
  }

  libusb_exit(g->ctx);
}



