#include <M5EPD.h>
#include <EEPROM.h>

M5EPD_Canvas canvas(&M5.EPD);
//https://docs.m5stack.com/en/api/m5paper/system_api
//https://docs.m5stack.com/en/api/m5paper/epd_canvas



char timeStrbuff[128];

int total_loop_count = 0;
int total_refresh_count = 0;
int next_address = 20;
int loop_count = 0;
int max_cycle_count = 10;

void reset_eeprom(){
    rtc_time_t _time;
    rtc_date_t _date;
    int eeAddress = 0;
    
    M5.RTC.getTime(&_time);
    M5.RTC.getDate(&_date);
    EEPROM.put(eeAddress, _time);
    eeAddress += sizeof(_time);
    EEPROM.put(eeAddress, _date);
    EEPROM.put(next_address, 0);
    int _next_address = next_address + sizeof(total_refresh_count);
    EEPROM.put(_next_address, 0);
    EEPROM.commit();
}

void load_initia_boot_time(rtc_time_t &_time, rtc_date_t &_date){
    int eeAddress = 0;
    EEPROM.get(eeAddress, _time);
    eeAddress += sizeof(_time);
    EEPROM.get(eeAddress, _date);
}

  
void setup()
{
    M5.begin();
    EEPROM.begin(512);
    M5.EPD.SetRotation(90);
    M5.EPD.Clear(true);
    canvas.createCanvas(540, 200);
    canvas.setTextSize(2);
    M5.RTC.begin();
    //save_initial_boot_time();
    
    rtc_time_t _time;
    rtc_date_t _date;
    load_initia_boot_time(_time, _date);
    sprintf(timeStrbuff," Initial boot: %02d/%02d %02d:%02d:%02d",
                        _date.mon,_date.day,
                        _time.hour,_time.min,_time.sec);
    canvas.drawString(timeStrbuff, 0, 20);

    M5.RTC.getTime(&_time);
    M5.RTC.getDate(&_date);
    sprintf(timeStrbuff," Current time: %02d/%02d %02d:%02d:%02d",
                        _date.mon,_date.day,
                        _time.hour,_time.min,_time.sec);
    canvas.drawString(timeStrbuff, 0, 40);

    sprintf(timeStrbuff," Battery: %2.2f volts",  M5.getBatteryVoltage()/1000.0);
    canvas.drawString(timeStrbuff, 0, 60);

    EEPROM.get(next_address, total_loop_count);
    sprintf(timeStrbuff," Loop Count: %d",  total_loop_count);
    canvas.drawString(timeStrbuff, 0, 80);

    int _next_address = next_address + sizeof(total_refresh_count);
    EEPROM.get(_next_address, total_refresh_count);
    sprintf(timeStrbuff," Refresh Count: %d",  total_refresh_count);
    canvas.drawString(timeStrbuff, 0, 100);

    total_refresh_count++;
    canvas.pushCanvas(0,0,UPDATE_MODE_DU4);
}

void loop()
{
    if( M5.BtnL.wasPressed()){
      canvas.deleteCanvas();
      canvas.createCanvas(540, 20);
      canvas.setTextSize(2);      
      sprintf(timeStrbuff,"BtnL was pressed...");
      canvas.drawString(timeStrbuff, 0, 0);
      canvas.pushCanvas(0,400,UPDATE_MODE_DU4);
    }
    if( M5.BtnP.wasPressed()){
      canvas.deleteCanvas();
      canvas.createCanvas(540, 20);
      canvas.setTextSize(2);      
      sprintf(timeStrbuff,"BtnP was pressed...");
      canvas.drawString(timeStrbuff, 0, 0);
      canvas.pushCanvas(0,400,UPDATE_MODE_DU4);
    }
    if( M5.BtnR.wasPressed()){
      canvas.deleteCanvas();
      canvas.createCanvas(540, 20);
      canvas.setTextSize(2);      
      sprintf(timeStrbuff," Reseting EEPROM...");
      canvas.drawString(timeStrbuff, 0, 0);
      canvas.pushCanvas(0,400,UPDATE_MODE_DU4);
      reset_eeprom();
      delay(500);
      M5.shutdown(10);
    }
    M5.update();
    
    delay(5000);
    total_loop_count++;
    if(++loop_count > max_cycle_count){
      EEPROM.put(next_address, total_loop_count);
      int _next_address = next_address + sizeof(total_refresh_count);
      EEPROM.put(_next_address, total_refresh_count);
      EEPROM.commit();
      M5.shutdown(60);
    }
}
