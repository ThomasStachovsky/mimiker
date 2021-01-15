/* PCI PIIX4 IDE */
#define KL_LOG KL_DEV
#include <sys/klog.h>
#include <sys/mimiker.h>
#include <sys/pci.h>
#include <sys/interrupt.h>
#include <sys/klog.h>
#include <sys/timer.h>
#include <sys/spinlock.h>
#include <sys/devclass.h>
#include <dev/atareg.h>
#include <dev/pciidereg.h>
#include <dev/idereg.h>

typedef struct IDEChannelRegisters {
  unsigned short base;  // I/O Base.
  unsigned short ctrl;  // Control Base
  unsigned short bmide; // Bus Master IDE
  unsigned char nIEN;   // nIEN (No Interrupt);
} IDEChannelRegisters_t;

typedef struct ide_device {
  unsigned char Reserved;      // 0 (Empty) or 1 (This Drive really exists).
  unsigned char Channel;       // 0 (Primary Channel) or 1 (Secondary Channel).
  unsigned char Drive;         // 0 (Master Drive) or 1 (Slave Drive).
  unsigned short Type;         // 0: ATA, 1:ATAPI.
  unsigned short Signature;    // Drive Signature
  unsigned short Capabilities; // Features.
  unsigned int CommandSets;    // Command Sets Supported.
  unsigned int Size;           // Size in Sectors.
  unsigned char Model[41];     // Model in string.
} ide_device_t;

typedef struct ide_state {
  resource_t *io_primary;
  resource_t *io_primary_control;
  resource_t *io_secondary;
  resource_t *io_secondary_control;
  resource_t *regs;
  resource_t *irq_res;
  ide_device_t ide_devices[4];
  IDEChannelRegisters_t channels[2];
} ide_state_t;

#define inb(addr) bus_read_1(ide->regs, (addr))
#define outb(addr, val) bus_write_1(ide->regs, (addr), (val))
#define out4b(addr, val) bus_write_4(ide->regs, (addr), (val))

/*
BAR0: Base address of primary channel (I/O space), if it is 0x0 or 0x1, the port
is 0x1F0. BAR1: Base address of primary channel control port (I/O space), if it
is 0x0 or 0x1, the port is 0x3F6. BAR2: Base address of secondary channel (I/O
space), if it is 0x0 or 0x1, the port is 0x170. BAR3: Base address of secondary
channel control port, if it is 0x0 or 0x1, the port is 0x376. BAR4: Bus Master
IDE; refers to the base of I/O range consisting of 16 ports. Each 8 ports
controls DMA on the primary and secondary channel respectively.
*/

#define STATUS_BSY 0x80
#define STATUS_RDY 0x40
#define STATUS_DRQ 0x08
#define STATUS_DF 0x20
#define STATUS_ERR 0x01

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
  if (reg < 0x08)
    outb(channels[channel].base + reg - 0x00, data);
  else if (reg < 0x0C)
    outb(channels[channel].base + reg - 0x06, data);
  else if (reg < 0x0E)
    outb(channels[channel].ctrl + reg - 0x0A, data);
  else if (reg < 0x16)
    outb(channels[channel].bmide + reg - 0x0E, data);
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

unsigned char ide_read(unsigned char channel, unsigned char reg) {
  unsigned char result;
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
  if (reg < 0x08)
    result = inb(channels[channel].base + reg - 0x00);
  else if (reg < 0x0C)
    result = inb(channels[channel].base + reg - 0x06);
  else if (reg < 0x0E)
    result = inb(channels[channel].ctrl + reg - 0x0A);
  else if (reg < 0x16)
    result = inb(channels[channel].bmide + reg - 0x0E);
  if (reg > 0x07 && reg < 0x0C)
    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
  return result;
}

static void ATA_wait_BSY(ide_state_t *ide) // Wait for bsy to be 0
{                                          /*
                                           for (int i = 0; i < 1000000; i++)
                                             ;
                                           return;*/

  while (inb(IDEDMA_CTL) & 0x01)
    ;
}
static void ATA_wait_DRQ(ide_state_t *ide) // Wait fot drq to be 1
{
  return;
  while (!(inb(7) & STATUS_RDY))
    ;
}
void read_sectors(ide_state_t *ide) {

  // reg cheat sheet https://www.bswd.com/idems100.pdf

  unsigned short a[256];
  for (int i = 0; i < 256; i++)
    a[i] = 2;

  struct idedma_table prd[1];

  prd[0].base_addr = (uint32_t)a;

  prd[0].byte_count = 1;

  outb(IDEDMA_CMD, 0);

  out4b(IDEDMA_TBL, (uint32_t)prd & IDEDMA_TBL_MASK);

  outb(IDEDMA_CTL, IDEDMA_CTL_INTR);

  outb(IDEDMA_CTL, IDEDMA_CTL_ERR);

  for (int i = 0; i < 8; i++)
    klog("%d  %d\n", i, inb(i));

  // outb(IDEDMA_CTL, IDEDMA_CTL_DRV_DMA(0));

  outb(IDEDMA_CTL, IDEDMA_CTL_DRV_DMA(0));

  for (int i = 0; i < 8; i++)
    klog("%d  %d\n", i, inb(i));

  outb(IDEDMA_CMD, IDEDMA_CMD_START);

  klog("---------------------------");

  for (int i = 0; i < 8; i++)
    klog("%d  %d\n", i, inb(i));

  /*
  volatile int i_ = 0;
  for (; i_ < 250000000; i_++)
    ;
    */

  klog("---------------------------");

  for (int i = 0; i < 8; i++)
    klog("%d  %d\n", i, inb(i));

  klog("%d\n", a[0]);

  // panic();

  // outb(0xc, (uint32_t)prd);

  // outb(0x8, 0);
  // outb(0xa, 6);

  // outb(0x8, 1);

  ATA_wait_BSY(ide);
  /*
  for (int i = 0; i < 16; i++)
    klog("%d  %d\n", i, inb(i));
  outb(5, 1);
  outb(2, 1);
  outb(3, 0);
  outb(4, 0);
  outb(5, 0);
  outb(7, 0x20); // Send the read command
  */

  uint16_t *target = (uint16_t *)a;

  ATA_wait_DRQ(ide);
  // for (int i = 0; i < 256; i++)
  //  target[i] = inb(0);

  klog("%d\n", target[0]);

  while (true)
    ;
}

static int ide_attach(device_t *dev) {
  ide_state_t *ide = dev->state;

  ide->io_primary = device_take_ioports(dev, 0, RF_ACTIVE);
  assert(ide->io_primary != NULL);

  ide->io_primary_control = device_take_ioports(dev, 1, RF_ACTIVE);
  assert(ide->io_primary_control != NULL);

  ide->io_secondary = device_take_ioports(dev, 2, RF_ACTIVE);
  assert(ide->io_secondary != NULL);

  ide->io_secondary_control = device_take_ioports(dev, 3, RF_ACTIVE);
  assert(ide->io_secondary_control != NULL);

  ide->regs = device_take_ioports(dev, 4, RF_ACTIVE);
  assert(ide->regs != NULL);

  // ide->irq_res = device_take_irq(dev, 0, RF_ACTIVE);

  // read_sectors(ide);

  return 0;
}

static int ide_probe(device_t *dev) {
  pci_device_t *pcid = pci_device_of(dev);

  if (!pci_device_match(pcid, 0x8086, 0x7111))
    return 0;

  return 1;
}

/* clang-format off */
static driver_t ide_driver = {
  .desc = "IDE driver",
  .size = sizeof(ide_state_t),
  .attach = ide_attach,
  .probe = ide_probe
};
/* clang-format on */

DEVCLASS_ENTRY(pci, ide_driver);
