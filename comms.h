#include <stdint.h>
#include <libusb-1.0/libusb.h>

unsigned char RB(DWORD ProcID, unsigned int lpBaseAddr);
unsigned int WB(DWORD ProcID, unsigned int lpBaseAddr, unsigned char value);
int mappar(void);
void EnterDebugLoop(void);

PROCESS_INFORMATION  pi;
STARTUPINFO si;
DEBUG_EVENT de;

CONTEXT c;

typedef struct {
  libusb_context * ctx;
  libusb_device_handle * dev;

  int mode;
  int writes_pending;
} gscomms;

enum {
  GSCOMMS_MODE_CAREFUL,
  GSCOMMS_MODE_STANDARD,
  GSCOMMS_MODE_FAST,
  GSCOMMS_MODE_BULK,
};

// hardware I/O
uint8_t READ_PORT(gscomms * g);
void WRITE_PORT(gscomms * g, uint8_t data);

gscomms * Init_Comms(void);
void cleanup_gscomms(gscomms * g);


