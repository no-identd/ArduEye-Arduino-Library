/*
ArduEye Interface Example using the ArduEye library.

The ArduEye library implements an interface to the ArduEye Sensor using 
a 5 wire interface (SPI + a DataReady wire)
The SPI pins are defined in the SPI library ( MOSI: 11, MISO: 12, SCK: 13)
The Chip Select and Dataeady are defined by the ArduEye layout. (CS: 10, DataRdy: 9).  These
pins can be changed but the ArduEye firmware must be updated as well.

The ArduEye library can be used either as a pure embedded sensor interface (using
the optical flow or image data to control other embedded behaviors) or can pass data via serial to 
the ArduEye UI.  The UI provides additional diagonostic  capbilites and can display a graphical 
representation of the raw image and image processing data.  The UI can be activated at any time while the sensor
is running to check and see what's happening, even while running in embedded mode.

This example shows how one might interface with the UI.  In the UI scenario, all commands are entered in the UI and passed by
the Arduino to the ArduEye.  

*/

#include "WProgram.h"
#include <SPI.h>
#include <ArduEye.h>

ArduEye arduEye;

/// select pins for SPI chip select and DataReady.  These pins are defined on both the ARM and Arduino Pro Mini.  
/// different pins can be used if desired but the ARM firmware must also be adjusted accordingly
int dataReadyPin = 9;
int chipSelectPin = 10;


void setup()
{
  // begin must be called to initilialize the ArduEye class
  arduEye.begin(dataReadyPin, chipSelectPin);
  // enable Serial Tx for transmission to a UI or the Serial Monitor
  arduEye.enableSerialTx(true);
  
  // display types for each dataset are set to default values but can
  // also be defined here
  arduEye.setDisplayType(ARDUEYE_ID_RAW, DISPLAY_GRAYSCALE_IMAGE);
  
  //arduEye.setSerialMonitorMode(true);

}

void loop()
{
  // check if data frame is ready.  The ArduEye raises the dataReadyPin at the end of each frame
  if(arduEye.dataRdy())
      // get active datasets from the ArduEye.  Datasets are set active via the UI
      arduEye.getData();
      
   // check if any commands have come in from the UI and process   
   arduEye.checkUIData();
 
}


