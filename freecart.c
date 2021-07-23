#include <stdio.h>
#include <string.h> // memcpy
#include <stdlib.h> //atol
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"
#include <PicoTM1637.h>
#include "hardware/vreg.h"
#include "hardware/flash.h"


// End of PROGRAM IN FLASH
//  objdump --all testawe.elf | grep flash_binary_end
//   1000b78c g       .ARM.attributes	00000000 __flash_binary_end


// UNIO Doc : https://ww1.microchip.com/downloads/en/DeviceDoc/22067J.pdf

// Via openocd : openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program blink/blink.elfverify reset exit"


// TM 7 SEG :
//  _ a
//f|_|b G
//e|_|c
//  d

// Bit : 0000 0000
// Seg : .GFE DCBA

// Ex A = 0x77 = 0111 0111
//    B = 0x7F
//    b = 0x7C
//    e = 0x7B
//    d = 0x5E
//    H = 0x76
//    a = 0x5f 767b5f5e
//    C = 0x39
//    o = 0x5C
//    l = 0x18  0x395c185c
//    M = 0x37
//    t = 0x78  0x00375f78
//    F = 0x71
//    i = 0x10 00181071
//    S = 0x6D 006D7F77
//    P = 0x73
//    L = 0x38 00773873
//    E = 0x79  00793871
//    G = 0x7D PETG = 7D787973
//    U = 0x3E SOLU = 3E387E6D
//    O = 0x3F //7E
//    n = 0x54
//    Y = 0x72
//    r = 0x50
//    v = 0x1C
//    - = 0x40

// UVCR = 50391C3E

//    rdy = 00725E72
//    nYLO = 7E38721C
//    GrEy = 7279507D
//    GrEE = 7979507D 
//    BLAC = 3977387C
//    WHIt = 7810763E
//    YELL = 38387972
//    bLUE = 793E387C
//    rEd  = 005E7950
//    nAt  = 0078771C
//    CLEA = 77793839
//    PUrP = 73503E73


#define LED             25
// TM
#define CLK_PIN         26
#define DIO_PIN         27

// UNIO
#define UNIO_PIN        2
#define PULSE_PIN       3
#define DBG_PIN         4
#define MIN_STDB_PUSLE  590 // Standby pulse time

#define UNIO_HEADER     0x55
#define CMD_RDSR        0x05
#define CMD_WRSR        0x6E
#define CMD_READ        0x03
#define CMD_CRRD        0x06
#define CMD_WRITE       0x6C
#define CMD_WREN        0x96
#define CMD_WRDI        0x91
#define CMD_ERAL        0x6D
#define CMD_SETAL       0x67

#define EST_T           18
#define DEVICE_ADDR     0xA0

// For UNIO Timing
int64_t T=0;

// BTN
#define BTN_SEL         14
#define BTN_UP          15
#define BTN_DOWN        17
#define BTN_RAZ         16

#define FALL            0x04
#define RISE            0x08


uint8_t MENU  =         0;

/* UNIO EEPROM REGISTERS */
uint8_t STATUS_REGISTER = 0;
uint16_t ADDR_START = 0;
uint8_t EEPROM[1024]= {0x5A,0x41,0x4B,0x00,0x00,0x35,0x32,0x54,0x80,0xA9,0x03,0x00,0x80,0xA9,0x03,0x00,
                       0xFA,0x00,0x5A,0x00,0x54,0x48,0x47,0x42,0x30,0x33,0x35,0x37,0x00,0x00,0x00,0x00,
                       0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAA,0x55,0xAA,0x55,
                       0x88,0x33,0x55,0xAA,0x51,0x44,0x02,0x00,0x00,0x00,0x00,0x10,0x04,0xED,0x00,0xE0,
                       0x5A,0x41,0x4B,0x00,0x00,0x35,0x32,0x54,0x80,0xA9,0x03,0x00,0x80,0xA9,0x03,0x00,
                       0xFA,0x00,0x5A,0x00,0x54,0x48,0x47,0x42,0x30,0x33,0x35,0x37,0x00,0x00,0x00,0x00,
                       0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAA,0x55,0xAA,0x55,
                       0x88,0x33,0x55,0xAA,0x80,0xA9,0x03,0x00,0xAA,0x55,0xAA,0x55,0x1F,0x0E,0x0B,0x00, // End of Cartdrige Data
                       0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Blank line
                       90  ,210 ,0x00,0x47,100 ,250 ,0x00,0x4B,70  ,230 ,0x00,0x57, 45 ,195 ,0x00,'Y' , // First profile line
                       45  ,195 ,0x00,0x42, 100,250 ,0x00,0x52, 45 ,205 ,0x00,0x5A, 50 ,210 ,0x00,0x41,
                       45  ,180 ,0x00,0x33, 80 ,240 ,0x00,0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
                      };

#define   PROFILE_START 0x90
#define   OFFSET_BED    0x00
#define   OFFSET_EXT    0x01
#define   OFFSET_COLO   0x03
#define   PROFILE_SIZE  4

#define   SN            0x18
#define   BEDTEMP       0x12
#define   EXTTEMP       0x10
#define   MATERIAL      0x01
#define   COLOR         0x02
#define   TOTLEN        0x08
#define   NEWLEN        0x0C
#define   LEN2          0x34
#define   OFFSETMEM     0x40

#define   MAX_EXTTEMP   260
#define   MIN_EXTTEMP   50
#define   MAX_BEDTEMP   110
#define   MIN_BEDTEMP   0

#define FLASH_STORAGE   0xf000
#define FLASH_SIZE      1024

volatile int blink=0;
static int64_t interval;
uint8_t nbwr = 0;
uint8_t LOCKED = 0;



const uint8_t MaterialList[] = {
  'A',    // 0
  'F',    // 1
  'G',    // 2
  'P',    // 3
  'Q',    // 4
  'S',    // 5
  'T',    // 6
  'U',    // 7
  'V',    // 8
  'Y'    // 9
  };


const uint8_t ColorList[] = {
  0x31,    // 0
  0x47,    // 1
  0x4B,    // 2
  0x57,    // 3
  0x59,    // 4
  0x42,    // 5
  0x52,    // 6
  0x5A,    // 7
  0x33,    // 8
  0x41     // 9
  };

static const char *gpio_irq_str[] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
};

static char event_str[128];


void doSAK3 ();
uint8_t getBit();
uint8_t getByte (uint8_t *MAKout);

void SaveVirtualEEPROM();
void ReadEEPROM ();
void initBtn();

void gpio_event_string(char *buf, uint32_t events);
void unioevent_callback(uint gpio, uint32_t events);



// Disable int
void DisableInt()
{
  gpio_set_irq_enabled(UNIO_PIN,GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,false);
  gpio_acknowledge_irq(UNIO_PIN,GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
}



void putBit (bool bit)
{
  gpio_put(UNIO_PIN, !bit);
  // Attend 1T
  busy_wait_us(T+2);
  gpio_put(UNIO_PIN, bit);
  // Attend 1T  
  busy_wait_us(T+2);
}


uint8_t getByte (uint8_t *MAKout)
{
  uint8_t tes = 0;
  for (int y =0;y<8;y++)
  {
      uint8_t bi = getBit();
      tes = (tes << 1) + bi; 
  }

  *MAKout = getBit();
  doSAK3();
  return tes;
}


// Traitement de la commande reçue
void TreatCMD2 (uint8_t cmd)
{
  uint8_t Data = 0x55;
  //UnioLastCmd = cmd;
  DisableInt();

  switch (cmd)
  {
  case CMD_RDSR :
  {
    
    Data = STATUS_REGISTER;

    gpio_set_dir(UNIO_PIN, GPIO_OUT);
    for (int i=0;i<8;i++)
      putBit((Data<<i) & 0x80);
    gpio_set_dir(UNIO_PIN, GPIO_IN);

    uint8_t MAK = getBit();
    doSAK3();

    //UnioStatus = IDLE;
    break;
  }

  case CMD_WRSR :
  {
    uint8_t MAKed = 0;

    STATUS_REGISTER = getByte (&MAKed);

    //UnioStatus = IDLE;
    break;
  }

  case CMD_READ :
  {
    //UnioStatus = ARG1;
    //AddrComplet = false;

    uint8_t MAKed = 0;
    ADDR_START = getByte (&MAKed);

    if (MAKed==1)//MAK
      {
        ADDR_START = (ADDR_START<<8) + getByte (&MAKed);
        if (MAKed == 1)
          {
            // Envois jusqu'au NoMAK

            ReadEEPROM();
            
            while (getBit() == true)
              {
                doSAK3();
                ADDR_START++;
                ReadEEPROM();
              }

            doSAK3();
          }
      }

    //UnioStatus = IDLE;
    break;
  }

  case CMD_WRITE:
  {
    uint8_t MAKed = 0;

    ADDR_START = getByte (&MAKed);
    uint16_t adstrbackup = ADDR_START;

    if (MAKed==1)//MAK
      {
        ADDR_START = (ADDR_START<<8) + getByte (&MAKed);
        adstrbackup = ADDR_START;
        nbwr=0;
        while (MAKed == 1) 
        {
          if ((ADDR_START>=0) && (ADDR_START<1024))
            {
              EEPROM[ADDR_START] = getByte (&MAKed);
              ADDR_START++;
              nbwr++;
            }
        }

        interval = time_us_64();
        while ((time_us_64()-interval)<=(T)) {};
        ADDR_START = 0;
        LOCKED=0; // Free up the lock
        //printf("%X",adstrbackup);
        if (adstrbackup == 0x70)
        {
          SaveVirtualEEPROM(); // Write only if we are updating the last part of the EEPROM,
                               // Otherwise the print will fail because this procedure take too much time.
                               // We also reduce the write action to the FLASH to preserve it's life a little.
          printf("W\r\n");
        }
      }
    break;
  }

  case CMD_WREN :
      // We don't care about this.
    break;

  default:
    // Ohoh
    printf("UNKNOWN COMMAND : %X\r\n",cmd);
    break;
  }

return;
}


// Slave ACK
void doSAK3 ()
{
  // Low the Pin for 1T
  gpio_put(DBG_PIN,true);
  gpio_put(PULSE_PIN,true);
  gpio_set_dir(UNIO_PIN,GPIO_OUT);
  gpio_put(UNIO_PIN,false);
  
  // Attend 1T
  interval = time_us_64();
  while ((time_us_64()-interval)<=T) {}

  // Acquitte
  
  gpio_put(UNIO_PIN,true);
  
  // Attends 1T
  interval = time_us_64();
  while ((time_us_64()-interval)<=T) {} // ok +Jit
  
  gpio_put(UNIO_PIN,false);
  gpio_set_dir(UNIO_PIN,GPIO_IN);

  gpio_put(PULSE_PIN,false);

  gpio_acknowledge_irq(UNIO_PIN,GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);

  gpio_put(DBG_PIN,false);
}


// AW 27052021 
// Routine De lecture d'un bit en restant synchro sur le changement de front
// OK
uint8_t getBit ()
{
//gpio_put(PULSE_PIN,true);
  bool a,b=0;
  uint64_t T2 = T/2;
  // Attend 1/2 T
  interval = time_us_64();
  busy_wait_us(T2);
  // Lit la valeur du bit
  a = gpio_get(UNIO_PIN);
  // Attend 1T
  while ((gpio_get(UNIO_PIN) == a)){};// && (time_us_64()-interval<=(T))) {}
  // Récup la valeur
  b = gpio_get(UNIO_PIN);
  // On est synchro sur MAK, attend 1/2 T
  busy_wait_us(T);
  return bool_to_bit(b && !a);
}


void ReadEEPROM ()
{
  //gpio_put(PULSE_PIN,true);
  gpio_set_dir(UNIO_PIN, GPIO_OUT);
  for (int i=0;i<8;i++)
    putBit((EEPROM[ADDR_START]<<i) & 0x80);
  gpio_set_dir(UNIO_PIN, GPIO_IN);
  //gpio_put(PULSE_PIN,false);
  //interval = time_us_64();
  busy_wait_us(2);
}




//alarm_id_t id, void *user_data) {
bool alarm_callback(struct repeating_timer *t) {
  //printf("Repeat at %lld\n", time_us_64());
  // Put your timeout handler code in here
  gpio_put(LED,blink);
  blink = ~blink;
  return true;
}



void usdelay (uint64_t us)
{
  interval = time_us_64();
  while ((time_us_64()-interval)<=us) {};
}



void initTMS()
{
  TM1637_init(CLK_PIN, DIO_PIN);  
  TM1637_clear(); 

  TM1637_set_brightness(7); // max value, default is 0
  TM1637_put_4_bytes(1, 0x00725E50);  // raw bytes for rdY 
  usdelay(1000000);
}


// Convert input Material code to Digit for TM display
void MatToDigit(uint8_t Mat)
{
  switch (Mat)
  {
  case 'A':
    /* ABS */
    TM1637_put_4_bytes(0, 0x006D7F77);
    break;
  case 'F':
    /* FLEX */
    TM1637_put_4_bytes(0, 0x00793871);
    break;    
  case 'G':
    /* PETG */
    TM1637_put_4_bytes(0, 0x7D787973);
    break;   
  case 'P':
    /* PLA 1*/
    TM1637_put_4_bytes(0, 0x00773873);
    break;
  case 'Q':
    /* PLA 2 Metal FREE*/
    TM1637_put_4_bytes(0, 0x71773873);
    break;    
  case 'S':
    /* ASA */
    TM1637_put_4_bytes(0, 0x00776D77);
    break;   
  case 'T':
    /* Tough PLA */
    TM1637_put_4_bytes(0, 0x76773873);
    break;
  case 'U':
    /* UVCR */
    TM1637_put_4_bytes(0, 0x50391C3E);
    break;
  case 'V':
    /* WATER SOLUBLE */
    TM1637_put_4_bytes(0, 0x3E383F6D);
    break;
  case 'Y':
    /* NYLON */
    TM1637_put_4_bytes(0, 0x3F387254);
    break;
  default:
    TM1637_put_4_bytes(0, 0x40404040);
    break;
  }
}


// Convert Color code to Digit for TM Display
void ColoToDigit(uint8_t Colo)
{
  switch (Colo)
  {
  case 0x31:
    /* Grey */
    TM1637_put_4_bytes(0, 0x7279507D);
    break;
  case 0x47:
    /* GREEN */
    TM1637_put_4_bytes(0, 0x7979507D);
    break;    
  case 0x4B:
    /* BLACK */
    TM1637_put_4_bytes(0, 0x3977387C);
    break;   
  case 0x57:
    /* WHITE */
    TM1637_put_4_bytes(0, 0x7810763E);
    break;
  case 0x59:
    /* YELLOW */
    TM1637_put_4_bytes(0, 0x38387972);
    break;    
  case 0x42:
    /* BLUE */
    TM1637_put_4_bytes(0, 0x793E387C);
    break;   
  case 0x52:
    /* RED */
    TM1637_put_4_bytes(0, 0x005E7950);
    break;
  case 0x5A:
    /* NATURE */
    TM1637_put_4_bytes(0, 0x00787754);
    break;
  case 0x33:
    /* CLEAR */
    TM1637_put_4_bytes(0, 0x77793839);
    break;
  case 0x41:
    /* PURPLE */
    TM1637_put_4_bytes(0, 0x73503E73);
    break;
  default:
    TM1637_put_4_bytes(0, 0x40404040);
    break;
  }
}

// Increment the eeprom serial number (comes from arduino code from volvito)
uint32_t IncSN()
{
  char Ser[5] = {0};
  sprintf(Ser,"%c%c%c%c", EEPROM[SN],EEPROM[SN+1],EEPROM[SN+2],EEPROM[SN+3]);
  uint32_t serial = atol(Ser);
  serial++;
  sprintf(Ser,"%04d",serial);
  memcpy(&EEPROM[SN],Ser, 4);
  printf("New SN:%s\r\n",Ser);
}


// Return the index corresponding to the current material, very usefull.
uint8_t GetCurrentMatIndex()
{
  for (int i=0;i<10;i++) 
    if (MaterialList[i] == EEPROM[MATERIAL])
      {
        return i;
      }
  return 0;
}


// The Main Code runnning from Core 1
void core1main ()
{
  multicore_fifo_clear_irq();
  initTMS();
  initBtn();

  TM1637_pioPause();
  while (gpio_get(BTN_RAZ) == false){}

  while (1)
    {
      if (gpio_get(BTN_SEL) == false)
      {
        TM1637_pioResume();
         //TM1637_clear(); 
         usdelay(500000);
        MENU++;
        if (MENU > 4) MENU = 0;
        switch (MENU)
        {
        case 0:
          /* BED */
          TM1637_put_4_bytes(0, 0x005E797C);
          usdelay(1000000);
          TM1637_clear();
          TM1637_display(EEPROM[BEDTEMP], false);
          usdelay(1000000);
          break;

        case 1:
          /* HEAD */
          TM1637_put_4_bytes(0, 0x5e5f7b76);
          usdelay(1000000);
          TM1637_clear();
          uint16_t exttemp = (EEPROM[EXTTEMP+1]<<8)+EEPROM[EXTTEMP];
          TM1637_display(exttemp, false);
          usdelay(1000000);
          break;
        
        case 2:
          /* COLO */
          TM1637_put_4_bytes(0, 0x5c185c39);
          usdelay(1000000);
          TM1637_clear();
          ColoToDigit(EEPROM[COLOR]);
          usdelay(1000000);
          break;

        case 3:
          /* MAT */
          TM1637_put_4_bytes(0, 0x00785f37);
          usdelay(1000000);
          TM1637_clear();
          MatToDigit(EEPROM[MATERIAL]);
          usdelay(1000000);
          break;

        case 4:
          /* FIL */
          TM1637_put_4_bytes(0, 0x00181071);
          usdelay(1000000);
          TM1637_clear();
          uint32_t totallen = (EEPROM[TOTLEN+3]<<24)+(EEPROM[TOTLEN+2]<<16)+(EEPROM[TOTLEN+1]<<8)+(EEPROM[TOTLEN]);
          uint32_t actlen = (EEPROM[LEN2+3]<<24)+(EEPROM[LEN2+2]<<16)+(EEPROM[LEN2+1]<<8)+(EEPROM[LEN2]);
          TM1637_display(totallen/1000, false);
          usdelay(500000);
          TM1637_display(actlen/1000, false);
          usdelay(500000);
          break;

        default:
            MENU = 0;
          break;
        }
        TM1637_pioPause();
        
        // Wait until the button be released
        while (gpio_get(BTN_SEL) == false) {};
      }

    // Checkup Button UP

    if (gpio_get(BTN_UP) == false)
      {
        TM1637_pioResume();

        switch (MENU)
        {
        case 0:
          /* BED */
          if (EEPROM[BEDTEMP] < MAX_BEDTEMP)
            EEPROM[BEDTEMP] = EEPROM[BEDTEMP] + 1;
          EEPROM[PROFILE_START+OFFSET_BED+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[BEDTEMP];
          TM1637_clear();
          TM1637_display(EEPROM[BEDTEMP], false);
          EEPROM[BEDTEMP+OFFSETMEM] = EEPROM[BEDTEMP];
          usdelay(500000);
          break;

        case 1:
          /* HEAD */
          { 
          uint16_t exttemp = (EEPROM[EXTTEMP+1]<<8)+EEPROM[EXTTEMP];
          if (exttemp < MAX_EXTTEMP)
            exttemp = exttemp + 1;
          EEPROM[EXTTEMP] = exttemp & 0x00FF;
          EEPROM[EXTTEMP+1] = (exttemp>>8) & 0x00FF;
          EEPROM[PROFILE_START+OFFSET_EXT+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[EXTTEMP];
          EEPROM[PROFILE_START+OFFSET_EXT+1+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[EXTTEMP+1];
          TM1637_clear();
          TM1637_display(exttemp, false);
          EEPROM[EXTTEMP+OFFSETMEM] = EEPROM[EXTTEMP];
          EEPROM[EXTTEMP+1+OFFSETMEM] = EEPROM[EXTTEMP+1];
          usdelay(500000);
          break;
          }

        case 2:
          /* COLO */
          for (int i=0;i<10;i++) 
            if (ColorList[i] == EEPROM[COLOR])
              {
                if (i == 9)
                  EEPROM[COLOR] = ColorList[0];
                else
                  EEPROM[COLOR] = ColorList[i+1];
                break;
              }
          TM1637_clear();
          ColoToDigit(EEPROM[COLOR]);
          EEPROM[COLOR+OFFSETMEM] = EEPROM[COLOR];
          EEPROM[PROFILE_START+OFFSET_COLO+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[COLOR];
          usdelay(500000);
          break;

        case 3:
          {
          /* MAT */
          int mat = 0;
          for (mat=0;mat<10;mat++) 
            if (MaterialList[mat] == EEPROM[MATERIAL])
              {
                if (mat == 9)
                  EEPROM[MATERIAL] = MaterialList[0];
                else
                  EEPROM[MATERIAL] = MaterialList[mat+1];
                break;
              }
          TM1637_clear();
          MatToDigit(EEPROM[MATERIAL]);
          EEPROM[MATERIAL+OFFSETMEM] = EEPROM[MATERIAL];
          usdelay(1000000);

          // Load the corresponding profile
          EEPROM[COLOR]     = EEPROM[PROFILE_START+OFFSET_COLO+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[EXTTEMP]   = EEPROM[PROFILE_START+OFFSET_EXT+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[EXTTEMP+1] = EEPROM[PROFILE_START+OFFSET_EXT+1+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[BEDTEMP]   = EEPROM[PROFILE_START+OFFSET_BED+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[COLOR+OFFSETMEM]     = EEPROM[COLOR];
          EEPROM[EXTTEMP+OFFSETMEM]   = EEPROM[EXTTEMP];
          EEPROM[EXTTEMP+1+OFFSETMEM] = EEPROM[EXTTEMP+1];
          EEPROM[BEDTEMP+OFFSETMEM]   = EEPROM[BEDTEMP];
          break;
          }

        default:
          break;
        }
        TM1637_pioPause();
      }


    // Checkup the button DOWN

    if (gpio_get(BTN_DOWN) == false)
      {
        TM1637_pioResume();
        switch (MENU)
        {
        case 0:
          /* BED */
          if (EEPROM[BEDTEMP] > MIN_BEDTEMP)
            EEPROM[BEDTEMP] = EEPROM[BEDTEMP] - 1;
          TM1637_clear();
          TM1637_display(EEPROM[BEDTEMP], false);
          EEPROM[BEDTEMP+OFFSETMEM] = EEPROM[BEDTEMP];
          EEPROM[PROFILE_START+OFFSET_BED+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[BEDTEMP];
          usdelay(500000);
          break;

        case 1:
          {
          /* HEAD */
          uint16_t exttemp = (EEPROM[EXTTEMP+1]<<8)+EEPROM[EXTTEMP];
          if (exttemp > MIN_EXTTEMP)
            exttemp = exttemp - 1;
          EEPROM[EXTTEMP] = exttemp & 0x00FF;
          EEPROM[EXTTEMP+1] = (exttemp>>8) & 0x00FF;
          TM1637_clear();
          TM1637_display(exttemp, false);
          EEPROM[EXTTEMP+OFFSETMEM] = EEPROM[EXTTEMP];
          EEPROM[EXTTEMP+1+OFFSETMEM] = EEPROM[EXTTEMP+1];
          EEPROM[PROFILE_START+OFFSET_EXT+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[EXTTEMP];
          EEPROM[PROFILE_START+OFFSET_EXT+1+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[EXTTEMP+1];
          usdelay(500000);
          break;
          }

        case 2: 
          /* COLO */
          for (int i=0;i<10;i++) 
            if (ColorList[i] == EEPROM[COLOR])
              {
                if (i == 0)
                  EEPROM[COLOR] = ColorList[9];
                else
                  EEPROM[COLOR] = ColorList[i-1];
                break;
              }
          TM1637_clear();
          ColoToDigit(EEPROM[COLOR]);
          EEPROM[COLOR+OFFSETMEM] = EEPROM[COLOR];
          EEPROM[PROFILE_START+OFFSET_COLO+(PROFILE_SIZE*GetCurrentMatIndex())] = EEPROM[COLOR];
          usdelay(500000);
          break;

        case 3:
          /* MAT */
          for (int i=0;i<10;i++) 
            if (MaterialList[i] == EEPROM[MATERIAL])
              {
                if (i == 0)
                  EEPROM[MATERIAL] = MaterialList[9];
                else
                  EEPROM[MATERIAL] = MaterialList[i-1];
                break;
              }
          TM1637_clear();
          MatToDigit(EEPROM[MATERIAL]);
          EEPROM[MATERIAL+OFFSETMEM] = EEPROM[MATERIAL];
          usdelay(1000000);
          // Load the corresponding profile
          int mat = GetCurrentMatIndex();
          EEPROM[COLOR]     = EEPROM[PROFILE_START+OFFSET_COLO+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[EXTTEMP]   = EEPROM[PROFILE_START+OFFSET_EXT+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[EXTTEMP+1] = EEPROM[PROFILE_START+OFFSET_EXT+1+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[BEDTEMP]   = EEPROM[PROFILE_START+OFFSET_BED+(PROFILE_SIZE*GetCurrentMatIndex())];
          EEPROM[COLOR+OFFSETMEM]     = EEPROM[COLOR];
          EEPROM[EXTTEMP+OFFSETMEM]   = EEPROM[EXTTEMP];
          EEPROM[EXTTEMP+1+OFFSETMEM] = EEPROM[EXTTEMP+1];
          EEPROM[BEDTEMP+OFFSETMEM]   = EEPROM[BEDTEMP];
          break;

        default:
          break;
        }
        TM1637_pioPause();
      }

    // Checkup the button RAZ (in french Remise A Zero = RESET)

    if (gpio_get(BTN_RAZ) == false)
      {
        uint16_t cnt=0;
        printf("RAZ ?\r\n");
        TM1637_pioResume();
        TM1637_clear();
        uint32_t actlenbef = (EEPROM[LEN2+3]<<24)+(EEPROM[LEN2+2]<<16)+(EEPROM[LEN2+1]<<8)+(EEPROM[LEN2]);
        TM1637_display(actlenbef, false);
        busy_wait_us(10000);
        

        while (gpio_get(BTN_RAZ)==false)
        {
          busy_wait_us(1000);
          cnt++;
        }

      // Was the button pressed more than or equal to 3 sec ?
      if (cnt >= 3000)
        {
          
          printf("RAZ !\r\n");

          // New SN
          IncSN(EEPROM[SN],4);
          EEPROM[SN+OFFSETMEM+3] = EEPROM[SN+3];
          EEPROM[SN+OFFSETMEM+2] = EEPROM[SN+2];
          EEPROM[SN+OFFSETMEM+1] = EEPROM[SN+1];
          EEPROM[SN+OFFSETMEM]   = EEPROM[SN];

          // RESET the Size
          EEPROM[LEN2+3] = EEPROM[TOTLEN+3];
          EEPROM[LEN2+2] = EEPROM[TOTLEN+2];
          EEPROM[LEN2+1] = EEPROM[TOTLEN+1];
          EEPROM[LEN2]   = EEPROM[TOTLEN];

          EEPROM[LEN2+OFFSETMEM+3] = EEPROM[TOTLEN+3];
          EEPROM[LEN2+OFFSETMEM+2] = EEPROM[TOTLEN+2];
          EEPROM[LEN2+OFFSETMEM+1] = EEPROM[TOTLEN+1];
          EEPROM[LEN2+OFFSETMEM]   = EEPROM[TOTLEN]; 

          uint32_t actlen = (EEPROM[LEN2+3]<<24)+(EEPROM[LEN2+2]<<16)+(EEPROM[LEN2+1]<<8)+(EEPROM[LEN2]);
          // Display the new len
          TM1637_clear();
          TM1637_display(actlen, false);
          usdelay(500000);
          TM1637_pioPause();

          // Backup the RAM to the Flash
          SaveVirtualEEPROM();
        }
        else 
          {
            // Btn RAZ pressed less than 3s, we simply backup the RAM to the FLASH
            SaveVirtualEEPROM();
            TM1637_pioResume();
            TM1637_clear();
            TM1637_put_4_bytes(0,0x008000); // Only light on the :
            usdelay(500000);
            TM1637_pioPause();
          }
      }

      // Totaly useless ... to remove
      tight_loop_contents();
    }
}


// We init the UNIO PINs
void initUnio()
{
  gpio_init(PULSE_PIN);
  gpio_set_dir(PULSE_PIN,GPIO_OUT);

  gpio_init(DBG_PIN);
  gpio_set_dir(DBG_PIN,GPIO_OUT);

  gpio_init(UNIO_PIN);
  gpio_set_dir(UNIO_PIN,GPIO_IN);
  gpio_set_pulls(UNIO_PIN,true,false);

  // Interrupt
  //gpio_set_irq_enabled_with_callback(UNIO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &unioevent_callback);
}


// Buttons Init
void initBtn()
{
  gpio_init(BTN_SEL);
  gpio_set_dir(BTN_SEL,GPIO_IN);
  gpio_set_pulls(BTN_SEL,true,false);

  gpio_init(BTN_UP);
  gpio_set_dir(BTN_UP,GPIO_IN);
  gpio_set_pulls(BTN_UP,true,false);  

  gpio_init(BTN_DOWN);
  gpio_set_dir(BTN_DOWN,GPIO_IN);
  gpio_set_pulls(BTN_DOWN,true,false); 

  gpio_init(BTN_RAZ);
  gpio_set_dir(BTN_RAZ,GPIO_IN);
  gpio_set_pulls(BTN_RAZ,true,false);     

  // Default Menu to -1 to be sure that at the fisrt push of Menu button, we will display the Bed temp.
  MENU=-1;
}


// This procedure will take between 30-40ms !!!
void SaveVirtualEEPROM()
{
  while (LOCKED) {} // Wait until unio finish it's run, we don't want backup the flash will unio is working
  
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase (FLASH_STORAGE, FLASH_SECTOR_SIZE); // We must erase firt to reset the fuse state
  flash_range_program (FLASH_STORAGE, EEPROM, FLASH_SIZE);
  restore_interrupts (ints);
}


// Load the Flash to the RAM
void LoadVirtualEEPROM ()
{
  while (LOCKED) {} // Wait until unio finish it's run
  char * eep = (char *) (XIP_BASE+FLASH_STORAGE);

  if (eep[0] != 'Z')
    {
      SaveVirtualEEPROM();
      printf("First Save\r\n");
    }
  else printf("Loaded\r\n");

  int i=0;
  for (i=0; i<FLASH_SIZE ; i++)
    {
      EEPROM[i] = eep[i];
    }
  // Load Profile
  EEPROM[COLOR]     = EEPROM[PROFILE_START+OFFSET_COLO+(PROFILE_SIZE*GetCurrentMatIndex())];
  EEPROM[EXTTEMP]   = EEPROM[PROFILE_START+OFFSET_EXT+(PROFILE_SIZE*GetCurrentMatIndex())];
  EEPROM[EXTTEMP+1] = EEPROM[PROFILE_START+OFFSET_EXT+1+(PROFILE_SIZE*GetCurrentMatIndex())];
  EEPROM[BEDTEMP]   = EEPROM[PROFILE_START+OFFSET_BED+(PROFILE_SIZE*GetCurrentMatIndex())];

  EEPROM[COLOR+OFFSETMEM]     = EEPROM[COLOR];
  EEPROM[EXTTEMP+OFFSETMEM]   = EEPROM[EXTTEMP];
  EEPROM[EXTTEMP+1+OFFSETMEM] = EEPROM[EXTTEMP+1];
  EEPROM[BEDTEMP+OFFSETMEM]   = EEPROM[BEDTEMP];
  return;  
}



int main()
{
    //set_sys_clock_khz(270000, true);
    
    // !!! OVERCLOCK the PICO to 291MHz !!!
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(1000);
    set_sys_clock_khz(291000, true);
    sleep_ms(200);

    stdio_init_all();

    // Start TM control over Core 1
    multicore_launch_core1(core1main);

    // LED
    gpio_init(LED);
    gpio_set_dir(LED,GPIO_OUT);
    gpio_put(LED,1);

    
    sleep_ms(2000);
    puts("System Ready.\r\n");
    LoadVirtualEEPROM ();
    initUnio();
    
  
    // Core 0 will handle UNIO Driver
    bool bit = gpio_get(UNIO_PIN);

    // Tbit = 18us on the Davinci with the pico as slave receiver.
    T = EST_T;

    while(1)
    {
      interval = time_us_64();
      // Look for the long high level (stdby pulse)
      LOCKED = 0;
      while (gpio_get(UNIO_PIN) == 1) {}
      //printf("t:%lld\r\n",time_us_64() - interval);
      if (((time_us_64() - interval) >=90) && ((time_us_64() - interval) <6500))
        {
          LOCKED = 1;
          interval = time_us_64();
          while (gpio_get(UNIO_PIN) == 0) {} // Repasse à 1
          
          //printf("t:%lld\r\n",time_us_64() - interval); 
          gpio_put(PULSE_PIN,true);

          // READ HEADER
          uint8_t tes = 0;
          for (int y =0;y<8;y++)
          {
              uint8_t bi = getBit();
              tes = (tes << 1) + bi; 
          }
          
          uint8_t MAK = getBit(); // ok
          //printf("Header : %X,%lld\r\n",tes,T);
          gpio_put(PULSE_PIN,false);

          if (tes != UNIO_HEADER) 
            {
              printf("f,%X,%lld\r\n",tes,time_us_64()-interval);
              continue;
            }
          else
            {
              // NoSAK, bien placée, OK
              busy_wait_us(T*2);
              
              gpio_put(PULSE_PIN,true);

              // READING ADDRESS
              uint8_t tes = 0;
              for (int y =0;y<8;y++)
                {
                    uint8_t bi = getBit();
                    tes = (tes << 1) + bi; 
                }
              
              gpio_put(PULSE_PIN,false);
              
              if (getBit()==1)//MAK
                {
                  if (tes != DEVICE_ADDR) 
                    {
                      printf("WRONG DEVICE ADDR %X\r\n", tes);
                      while (bit == 0) {bit = gpio_get(UNIO_PIN);} // AJOUT
                      continue;
                    }

                  // ACKNOWLEDGE
                  doSAK3();

                  //READING COMMAND
                  tes = 0;
                  for (int y =0;y<8;y++)
                    {
                        uint8_t bi = getBit();
                        tes = (tes << 1) + bi; 
                    }
                  if (getBit()==1)//MAK
                    {
                      // ACKNOWLEDGE
                      doSAK3();

                      // TREAT COMMAND
                      TreatCMD2(tes);

                      // WAIT UNTIL WE RECEIVE 1 AGAIN
                      while (bit == 0) {bit = gpio_get(UNIO_PIN);}
                      continue;
                    }
                  else
                    {
                      // ACKNOWLEDGE
                      doSAK3();
                      continue;
                    }
                }
              else
                {
                  // ACKNOWLEDGE
                  doSAK3();
                }
              
            }
        }

      // WAIT UNTIL WE RECEIVE 1 AGAIN
      while (bit == 0) {bit = gpio_get(UNIO_PIN);}
    };

    return 0; // You never go there
}