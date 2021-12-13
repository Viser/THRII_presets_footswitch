#include <Arduino.h>
#include <Usb.h>
#include <usbh_midi.h>  
#include <Bounce2.h>
#include <TM1637Display.h>

const uint8_t button_l_pin = 2;
const uint8_t button_r_pin = 3;
const uint8_t display_clk_pin = 5;
const uint8_t display_dio_pin = 6;

const uint8_t seg_thr[] = {
    SEG_D | SEG_E | SEG_F | SEG_G, // t
    SEG_C | SEG_E | SEG_F | SEG_G, // h
    SEG_E | SEG_G,                 // r
    SEG_C | SEG_B | SEG_F | SEG_E  // ii
};

const uint8_t seg_hi[] = {
    SEG_C | SEG_E | SEG_F | SEG_G | SEG_B, // h
    SEG_C | SEG_B,  // i
    SEG_D,
    SEG_D
};

const uint8_t seg_halt[] = {
    SEG_C | SEG_E | SEG_F | SEG_G, // h
    SEG_A | SEG_F | SEG_B | SEG_E | SEG_C, // a
    SEG_F | SEG_E | SEG_D,  // l
    SEG_D | SEG_E | SEG_F | SEG_G, // t
};

USB Usb;
USBH_MIDI  Midi(&Usb);
TM1637Display display(display_clk_pin, display_dio_pin);
Bounce debouncer_r = Bounce();
Bounce debouncer_l = Bounce();

volatile static byte thr_connected = false;
static byte is_first_switch = true;

uint8_t identity_request[] =        { 0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7 };
uint8_t request_version[] =         { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t request_version1[] =        { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x00, 0x01, 0x00, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t request_version2[] =        { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x10, 0x5c, 0x61, 0x06, 0x79, 0x00, 0x00, 0x00, 0xf7 };

uint8_t preset_change_request_1[] = { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x01, 0x07, 0x00, 0x00, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t preset_change_request_2[] = { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x01, 0x0a, 0x00, 0x00, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t preset_change_request_3[] = { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x01, 0x0e, 0x00, 0x00, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t preset_change_request_4[] = { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x01, 0x10, 0x00, 0x00, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };
uint8_t preset_change_request_5[] = { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x00, 0x4d, 0x01, 0x12, 0x00, 0x00, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 };

void send_sysex_command(uint8_t *dataptr, uint8_t datasize, String data_type) {
  if(0 == Midi.SendSysEx(dataptr, datasize))
    Serial.println(data_type + " has been sent");
}

uint8_t read_sysex_responce() {
  uint8_t outBuf[4];
  uint8_t sysexBuf[3];
  char buf[3] = {0};
  uint8_t read_data_size = 0;
  uint8_t size = 0;
  Serial.println();
  do {
    if ((size = Midi.RecvRawData(outBuf)) > 0 ) {  
      uint8_t rc = Midi.extractSysExData(outBuf, sysexBuf);
      if ( rc != 0 ) { 
        //SysEx
        read_data_size += rc;
        for (int i = 0; i < rc; i++) {
          sprintf(buf, " %02X", sysexBuf[i]);
          Serial.print(buf);
        }  
      } else
        Serial.println("Unsupported");
    }
  } while (size > 0); 
  Serial.println(read_data_size);
  return read_data_size;
}

void setup() {
  Serial.begin(115200);
  if (Usb.Init() == -1) {
    Serial.println("Halted");
    display.setSegments(seg_halt);
    while (1);
  }

  // Buttons setup
  // Right button increases patch id.
  pinMode(button_r_pin, INPUT_PULLUP);
  debouncer_r.attach(button_r_pin);
  debouncer_r.interval(5);

  // Left button decreases patch id.
  pinMode(button_l_pin, INPUT_PULLUP);
  debouncer_l.attach(button_l_pin);
  debouncer_l.interval(5);

  // Display setup
  display.setBrightness(1);//0xf0
  display.clear();
  display.setSegments(seg_hi);
  delay( 200 );
}

/*void print_buffer(byte* _data, uint8_t _size)  {
  char data_buf[256];
  Serial.println("");
  Serial.print(_size);
  Serial.print(":");
  for (int i = 0; i < _size; i++) {
    sprintf(data_buf, " %02X", _data[i]);
    Serial.print(data_buf);
  }
  Serial.println();
}*/

void print_version(const byte* data) {
  char buf[2] = {0};
  sprintf(buf, "%d%d%c", data[15], data[14], data[12]);
  Serial.print(String(buf));
  Serial.println();
}

int read_buttons(void) {
  debouncer_r.update();
  debouncer_l.update();
  return (debouncer_r.changed() ? 1: 0) + (debouncer_l.changed() ? -1: 0);
}


void loop() {
  static uint8_t preset_id = 0;
  Usb.Task();
  if(Midi) {
    if(!thr_connected) {
      send_sysex_command(identity_request, sizeof(identity_request), "Init");
      read_sysex_responce();
      //print_version();
  
      send_sysex_command(request_version, sizeof(request_version), "request_version");
      read_sysex_responce();
      
      send_sysex_command(request_version1, sizeof(request_version1), "request_version1");
      read_sysex_responce();
  
      send_sysex_command(request_version2, sizeof(request_version2), "request_version2");
      uint8_t resp_size = read_sysex_responce();
      if (resp_size > 0) {
        Serial.println(F("THR Connected"));
        display.setSegments(seg_thr);
        thr_connected = true;
      }
    }
    int8_t button_state = read_buttons();
    if (button_state != 0) {
      preset_id = constrain(preset_id + button_state, 1, 5);
      bool switched = false;
      uint8_t attempts = 0;
      while (!switched && 3 > attempts) {
        switch(preset_id) {
        case 1:
            send_sysex_command(preset_change_request_1, sizeof(preset_change_request_1), "preset_change_request_1");
            break;
        case 2:
            send_sysex_command(preset_change_request_2, sizeof(preset_change_request_2), "preset_change_request_2");
            break;
        case 3:
            send_sysex_command(preset_change_request_3, sizeof(preset_change_request_3), "preset_change_request_3");
            break;
        case 4:
            send_sysex_command(preset_change_request_4, sizeof(preset_change_request_4), "preset_change_request_4");
            break;
        case 5:
            send_sysex_command(preset_change_request_5, sizeof(preset_change_request_5), "preset_change_request_5");
            break;
        }
        switched = 0 < read_sysex_responce() || is_first_switch ? true : false; //for some reason thr don't send responce for fist swith TODO: check
        attempts += 1;
        if (is_first_switch) is_first_switch = false;
      }
      if (switched) {
        Serial.println(preset_id);
        display.showNumberDec(preset_id, false);
      } else {
          Serial.println("Failed to swith");
      }
    }
  } else {
    thr_connected = false;
    display.setSegments(seg_hi);
    preset_id = 0;
    is_first_switch = true;
  }
}
