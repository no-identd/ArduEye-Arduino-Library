/*
  ArduEye.cpp - Library for interfacing with the ArduEye Sensor
  Centeye, Inc
  Created by Alison Leonard. August, 2011
  License Info here
*/

#include <ArduEye.h>
#include <SPI.h>

ArduEye::ArduEye()
{
	_SerialTx = true;
	_SerialMonitorMode = false;
    _NumActiveSets = 0;
}

// Initialize Ardue Eye Interface
void ArduEye::begin(int RdyPin, int CSPin)
{
	_dataReadyPin = RdyPin;
	_chipSelectPin = CSPin;
	
	Serial.begin(115200);
	
	// start the SPI library:
	  SPI.setClockDivider(SPI_CLOCK_DIV4);
	  SPI.setDataMode(SPI_MODE3);
	  SPI.setBitOrder(MSBFIRST);
	  SPI.begin();

  // initalize the  data ready and chip select pins:
	  pinMode(_dataReadyPin, INPUT);
	  pinMode(_chipSelectPin, OUTPUT);
	  digitalWrite(_chipSelectPin, HIGH);
	  
	  // initialize data set record
	  _DS[0].DSID = ARDUEYE_ID_RAW;
	  _DS[0].Array = RawImage;
	  _DS[0].MaxSize = ARDUEYE_RAW_SIZE;
      //_DS[0].DisplayType = DISPLAY_GRAYSCALE_IMAGE;
    
	  _DS[1].DSID = ARDUEYE_ID_OFX;
	  _DS[1].Array = OpticFlowX;
	  _DS[1].MaxSize = ARDUEYE_OF_SIZE;
      _DS[1].DisplayType = DISPLAY_CHARTX;
	  
	  _DS[2].DSID = ARDUEYE_ID_OFY;
	  _DS[2].Array = OpticFlowY;
	  _DS[2].MaxSize = ARDUEYE_OF_SIZE;
      _DS[2].DisplayType = DISPLAY_CHARTY;
	  
	  _DS[3].DSID = ARDUEYE_ID_FPS;
	  _DS[3].Array = _FPS;
	  _DS[3].MaxSize = ARDUEYE_FPS_SIZE;
      _DS[3].DisplayType = DISPLAY_TEXT;
	 	
}

// Start streaming a new dataset from the ArduEye
// multiple datasets can be active at the same time
void ArduEye::startDataStream(char DataSet)
{
  int i;  
  sendCommand((char)DISPLAY_CMD, &DataSet, 1);
    
  // update DSRecord
  for (i = 0; i < MAX_DATASETS; i++)
  {   
    if(_DS[i].DSID == DataSet)
    {
        if(_DS[i].Active == false)
        {
            _ActiveSets[_NumActiveSets] = i;
            _NumActiveSets++;
        }
        _DS[i].Active = true;
      break;
    }
  }
}

// Stop streaming a dataset from the ArduEye
// Any other active datasets will continue to stream
void ArduEye::stopDataStream(char DataSet)
{  
    int i, m;  
    sendCommand((char)STOP_CMD, &DataSet, 1);
    
      // update DSRecord
      for (i = 0; i < MAX_DATASETS; i++)
      {   
        if(_DS[i].DSID == DataSet)
        {
          _DS[i].Active = false;
          break;
        }
      }
  
    for(i = 0; i < _NumActiveSets; i++)
    {
        if(_DS[_ActiveSets[i]].DSID == DataSet)
        {
            for(m = i; m < _NumActiveSets-1; m++)
                _ActiveSets[m] = _ActiveSets[m+1];
            _NumActiveSets--;
            break;
        }
    }
    
}

// check that serial buffer is not full, so it is OK to send data
boolean ArduEye::CheckBufferFull()
{
    int BytesReceived = 0;
    int Count = 0;
    
    while(Count < 512)
    {
        Serial.print((char)GO_CHAR);
        while(BytesReceived == 0)
            BytesReceived = Serial.available();
        
        if(Serial.read() == ACK_CHAR)
            return true;
        
        Count++;
    }
    if(Count == 512)
    {
        _SerialTx = false;
        return false;
    }
        
}

// get data from active datasets
// this command can be run each loop to update
// dataRdy() must be checked before running getData()
void ArduEye::getData()
{
	int i, k, InSize, Rows, Cols, c, Idx, remaining;
	boolean EODS = false;
	unsigned char Header[FULL_HEAD_SIZE];
   //Header: 1 byte DataId, 2 byte rows, 2 byte cols
      
    // loop through active datasets
    for (k = 0; k < _NumActiveSets; k++)
    {
      	// read data packet header
        digitalWrite(_chipSelectPin, LOW);
        SPI.transfer(ESC_CHAR);
        SPI.transfer(WRITE_CHAR);
        
        SPI.transfer(ESC_CHAR);
        SPI.transfer(START_PCKT);
        SPI.transfer(SOH_CHAR); 
        SPI.transfer(_DS[_ActiveSets[k]].DSID);
        SPI.transfer(END_PCKT);
        
        SPI.transfer(ESC_CHAR);
        SPI.transfer(READ_CHAR);
        delayMicroseconds(1);
        for (i = 0; i < FULL_HEAD_SIZE; i++)
          Header[i] = SPI.transfer(0x00);
        digitalWrite(_chipSelectPin, HIGH);
        
        // assign row and colum data for serial display      
        Rows = (Header[1] << 8) + Header[2];
        Cols = (Header[3] << 8) + Header[4];
        InSize = Rows * Cols;
       

		// write header data to serial monitor / UI if active
		if(_SerialTx)
        {
           if(_SerialMonitorMode)
           {    
               if(CheckBufferFull())
               {
                   Serial.println(_DS[_ActiveSets[k]].DSID);
                   Serial.println(Header[0], DEC);
                   Serial.println(Rows, DEC);
                   Serial.println(Cols, DEC);
                   Serial.println(_DS[_ActiveSets[k]].DisplayType);
                   Serial.println(" ");
               }
            }
            else
            {
                if(CheckBufferFull())
                {
                    Serial.print((char)START_PCKT);  // send start of packet byte
                    Serial.print(Header[0] + 1); // send packet ID (HeaderID = DSID + 1)
                    for(i = 0; i < FULL_HEAD_SIZE; i++) // send header packet data
                        Serial.print(Header[i]);
                    Serial.print(_DS[_ActiveSets[k]].DisplayType); // append display type
                    Serial.print((char)END_PCKT);  //send end of packet byte   
                }
            }
         }
        
        // abort read if size data is incorrect
        if((InSize <= 0) || (InSize > _DS[_ActiveSets[k]].MaxSize))
            break;
          
          // send start of packet bytes
          if(_SerialTx)
          {
              if(_SerialMonitorMode) // print to serial monitor mode
              {
                  Serial.println(START_PCKT);
                  Serial.println(_DS[_ActiveSets[k]].DSID,DEC);
              }
              else  // print to UI mode
              {
                  Serial.print((char)START_PCKT); // send start of packet byte
                  Serial.print(_DS[_ActiveSets[k]].DSID); // send dataset ID
              }
          }
      
      
        // read data packet 
         digitalWrite(_chipSelectPin, LOW);
        SPI.transfer(ESC_CHAR);
        SPI.transfer(WRITE_CHAR);
        
        SPI.transfer(ESC_CHAR);
        SPI.transfer(START_PCKT);
        SPI.transfer(SOD_CHAR); 
        SPI.transfer(_DS[_ActiveSets[k]].DSID);
        SPI.transfer(END_PCKT);
        
        SPI.transfer(ESC_CHAR);
        SPI.transfer(READ_CHAR);
         delayMicroseconds(1);
          Idx = 0;
          while(Idx + MAX_PACKET_SIZE < InSize)
          {
             for(i = 0; i < MAX_PACKET_SIZE; i++)
               _DS[_ActiveSets[k]].Array[i] = SPI.transfer(0x00);
              
              if (_SerialTx)
              {   	
                 for (i = 0; i < MAX_PACKET_SIZE; i++)
                      Serial.print(_DS[_ActiveSets[k]].Array[i]);
                  if(_SerialMonitorMode)
                      Serial.println(" ");
                  
                  if(Idx % (MAX_PACKET_SIZE * 2) == 0)
                  {     
                      if(!CheckBufferFull())
                          break;
                  }
              }
              Idx+= MAX_PACKET_SIZE;
          }
          remaining = InSize - Idx;
        
          if(remaining)
          {
              for(i = 0; i < remaining; i++)
                  _DS[_ActiveSets[k]].Array[i] = SPI.transfer(0x00);
              
              if (_SerialTx)
              {   	
                  for (i = 0; i < remaining; i++)
                      Serial.print(_DS[_ActiveSets[k]].Array[i]);
                  if(_SerialMonitorMode)
                      Serial.println(" ");
                  
              } 
          }
          digitalWrite(_chipSelectPin, HIGH);
          
        // send end of Packet bye
          if(_SerialTx)
          {
              if(_SerialMonitorMode) //send to serial monitor
                  Serial.println((char)END_PCKT);
              else     //send to UI
                  Serial.print((char)END_PCKT);
          }        
  } 
    //send End of Data Indicator
    digitalWrite(_chipSelectPin, LOW);
    SPI.transfer(ESC_CHAR);
    SPI.transfer(WRITE_CHAR);
    
    SPI.transfer(ESC_CHAR);
    SPI.transfer(START_PCKT);
    SPI.transfer(EOD_CHAR);
    SPI.transfer(END_PCKT);   
    digitalWrite(_chipSelectPin, HIGH);
    
    // send End of Frame Indicator
    if((_NumActiveSets > 0) && _SerialTx)
    {
        Serial.print((char)START_PCKT);
        Serial.print((char)END_FRAME);
        Serial.print((char)END_PCKT);
        if(_SerialMonitorMode)
            Serial.println(" ");
    }
}

// check data ready pin to see if ArduEye Data is ready
// dataRdy() must be called before getData()
boolean ArduEye::dataRdy()
{
	return (digitalRead(_dataReadyPin) == HIGH);
}

// send calibrate command to ArduEye
// calibrate generates a new Fixed Pattern noise mask for the vison chip
void ArduEye::calibrate()
{
	sendCommand(CMD_CALIBRATE, 0, 0);
}
// change raw image resolution
//
void ArduEye::setResolution(int rows, int cols)
{
	char Cmd[2] = {rows,cols};
	sendCommand(CMD_RESOLUTION, Cmd, 2);
}
// change optic flow array resolution - raw image resolution will
// not be changed, this command sets the number of optic flow 
// computation regions in the image
void ArduEye::setOFResolution(int rows, int cols)
{
	char Cmd[2] = {rows,cols};
	sendCommand(CMD_OF_RESOLUTION, Cmd, 2);
}
// change the smoothing rate for the optic flow calculation
// depending on the sensor read speed, optic flow smoothing
// can provide a less noisy result
void ArduEye::setOFSmoothing(float level)
{
	char Cmd[1];
	Cmd[0] = 100/level;
	sendCommand(CMD_OF_SMOOTHING, Cmd, 1);
}
// send a user defined command to the ArduEye
//
void ArduEye::sendCommand(char Cmd, char * Value, int Size)
{
	int i;
    digitalWrite(_chipSelectPin, LOW);
    
    // set write mode
    SPI.transfer(ESC_CHAR);
    SPI.transfer(WRITE_CHAR);
    // send command
    SPI.transfer(ESC_CHAR);
    SPI.transfer(START_PCKT); // Size = Cmd byte + size of value array
    SPI.transfer(Cmd);

	for (i = 0; i < Size; i++)
	  	SPI.transfer(Value[i]);
    SPI.transfer(END_PCKT);
	digitalWrite(_chipSelectPin, HIGH);
    
    if(_SerialMonitorMode && _SerialTx)  
    { 
        Serial.println(Cmd);
        for(i = 0; i < Size; i++)
            Serial.println(Value[i]);
        Serial.println("cmd sent");
    }
	
}

// retrieve FPS value from dataset
//
short ArduEye::FPS()
{
	  short fps;
	  for (int i = 0; i < MAX_DATASETS; i++)
	  {   
	    if(_DS[i].DSID == ARDUEYE_ID_FPS)
	    {
			fps = (_DS[i].Array[0] << 8) + _DS[i].Array[1];
			return fps;
		}	
	  }
}

// check for communication from UI and pass commands on to ArduEye
void ArduEye::checkUIData()
{
	int i, StartIdx, EndIdx, CmdBytes;
	int BytesReceived = Serial.available();
	char InByte[MAX_IN_SERIAL];
  
	if(BytesReceived)
	{
		delay(50);
		BytesReceived = Serial.available();
		for (i = 0; i < BytesReceived; i++)
	    	InByte[i] = Serial.read();
        
	  StartIdx = EndIdx = 0;
	  while(StartIdx < BytesReceived)
	  {
	    // look for ESC char, signal for start of command  
	    while((InByte[StartIdx] != START_PCKT) && (StartIdx < BytesReceived))
	      StartIdx++;
          
        while((InByte[EndIdx] != END_PCKT) && (EndIdx < BytesReceived))
            EndIdx++;
	    
	    if((EndIdx >= BytesReceived) || (StartIdx >= BytesReceived))
            return;
	
        CmdBytes = EndIdx - StartIdx;
	       
	    switch(InByte[StartIdx+1])
	    {
	      case DISPLAY_CMD:
	        startDataStream(InByte[StartIdx+2]);
	        break;
	      case STOP_CMD:
	        stopDataStream(InByte[StartIdx+2]);
	        break;
	      case WRITE_CMD:
              sendCommand(InByte[StartIdx + 1], InByte + StartIdx + 2, CmdBytes);
	      
			  if(_SerialMonitorMode && _SerialTx)  
			  {          
	            for(i = 0; i < CmdBytes; i++)
	              Serial.print(InByte[StartIdx + i]);
	            Serial.println("  cmd sent");
			  }
	          break;
	       default:
	          break; 
	    }
	    StartIdx += CmdBytes;
	  }
	}	
}	
// Enable Serial Communication.  Enabled by default
// When enabled, all data sets will be sent out over serial to either the 
// ArduEye UI or the Serial Monitor
void ArduEye::enableSerialTx(boolean Enable)
{
	_SerialTx = Enable;
}
// SerialMonitorMode is disabled by default.  When enabled
// serial data will be formated for display on the serial monitor
// SerialTx must be enabled for SerialMonitorMode to display data 
void ArduEye::setSerialMonitorMode(boolean Enable)
{
	_SerialMonitorMode = Enable;
}

void ArduEye::SetDisplayType(int DSID, int DisplayType)
{
    for (int i = 0; i < MAX_DATASETS; i++)
    {   
        if(_DS[i].DSID == DSID)
        {
            _DS[i].DisplayType = DisplayType;
            break;
        }
    }
}
	

