/*
 * File:        i2c_demo.c
 * Purpose:     I2C example of talking to MMA8451Q on the Tower board.
 *
 *              This demo uses a single byte read, but there is also a
 *                u8ReadThreeRegisters() function available to show
 *                an example of doing multiple reads back to back.
 *
 */

//#include "common.h"
//#include "lptmr.h"
#include "sleep_timer.h"
#include "i2c.h"

extern signed char result[20];

void iar_i2c_main (void)
{
  printf("Kinetis I2C Demo\n");

  //Initialize I2C
  init_I2C();

  //Configure accelemoter sensor
  I2CWriteRegister(0x2A,0x01);

  printf("  X     Y     Z\n");

  while(1)
  {
    //Look at status of accelerometer
    I2CReadMultiRegisters(0x01,6);
    
    printf("%3d    %3d     %3d\n",result[0],result[2],result[4]);

    //Delay for 250ms
    sleep(250);
  }

}
