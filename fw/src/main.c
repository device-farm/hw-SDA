#include "mgos.h"
#include "mgos_gpio.h"
#include "mgos_mqtt.h"

int pinSDA = 12;
int pinSCL = 14;
int pinCS = 15;

void spiWrite(int data)
{
  for (int b = 8; b >= 0; b--)
  {
    mgos_gpio_write(pinSDA, (data >> b) & 1);
    mgos_gpio_write(pinSCL, 0);
    mgos_gpio_write(pinSCL, 1);
  }
}

void spiStart()
{
  mgos_gpio_write(pinCS, 0);
}

void spiEnd()
{
  mgos_gpio_write(pinCS, 1);
}

/*
msgType:
00 - CS->0; HEAD, DATA; CS->1
01 - CS->0; HEAD, DATA
02 - DATA
03 - DATA; CS->1
*/

void mqttHandler(struct mg_connection *nc, const char *topic,
                  int topic_len, const char *msg, int msg_len,
                  void *ud)
{
  (void)(nc);
  (void)(topic);
  (void)(topic_len);
  (void)(ud);

  //LOG(LL_INFO, ("LEN:%d", msg_len));

  int msgType = *msg++; msg_len--;

  //LOG(LL_INFO, ("TYPE:%d", msgType));
  
  if (msgType == 0 || msgType == 1) {
    //LOG(LL_INFO, ("SPI START"));
    spiStart();
    spiWrite(0x2A);
    spiWrite(0x100 | msg[0]);
    spiWrite(0x100 | msg[1]);
    spiWrite(0x100 | msg[2]);
    spiWrite(0x100 | msg[3]);
    spiWrite(0x2B);
    spiWrite(0x100 | msg[4]);
    spiWrite(0x100 | msg[5]);
    spiWrite(0x100 | msg[6]);
    spiWrite(0x100 | msg[7]);
    msg += 8;
    msg_len -= 8;
    spiWrite(0x2C);
  }  

  while (msg_len > 0)
  {
    int b = *msg++;
    msg_len--;

    if (b == 1) {
      int cnt = (msg[0] << 8) | msg[1]; msg += 2; msg_len -= 2;
      if (cnt > 0) {
        const char* pattern = msg; msg += 3; msg_len -= 3;
        for (int c = 0; c < cnt; c++) {
          for (int p = 0; p < 3; p++) {
            spiWrite(0x100 | pattern[p]);
          }
        }
      } else {
        spiWrite(0x100 | b);
        spiWrite(0x100 | *msg++); msg_len--;
        spiWrite(0x100 | *msg++); msg_len--;
      }

    } else {
      spiWrite(0x100 | b);
      spiWrite(0x100 | *msg++); msg_len--;
      spiWrite(0x100 | *msg++); msg_len--;
    }
    
  }
  
  if (msgType == 0 || msgType == 3) {
    spiEnd();
    //LOG(LL_INFO, ("SPI END"));
  }
}

void lcd_init()
{
  mgos_gpio_set_mode(pinSCL, MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_set_mode(pinSDA, MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_set_mode(pinCS, MGOS_GPIO_MODE_OUTPUT);

  spiStart();

  // SLPOUT
  spiWrite(0x11);

  // DISPON
  spiWrite(0x29);

  // COLMODE
  spiWrite(0x3A);
  spiWrite(0x106);

  spiWrite(0x2C);
  for (int c = 0; c < 128 * 128 * 3; c++) {
    spiWrite(0x100 | (c & 0xFF));
  }

  spiEnd();



  const char *clientId = mgos_sys_config_get_mqtt_client_id();
  if (!clientId)
  {
    clientId = mgos_sys_config_get_device_id();
  }

  char topic[100];
  c_snprintf(topic, sizeof(topic), "%s/screen/write", clientId);
  mgos_mqtt_sub(topic, &mqttHandler, 0);
}

enum mgos_app_init_result mgos_app_init(void)
{
  return MGOS_APP_INIT_SUCCESS;
}
