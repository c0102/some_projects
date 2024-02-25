/*
 * modbus_slave.c
 *
 *  Created on: Dec 17, 2019
 *      Author: iskra
 */


#include "main.h"

static const char *TAG = "modbus_slave";
//static const char * TAG = "modbus_slave";

#define MODBUS_REGISTER_WRITE_OK 0
#define MODBUS_FUNC_CODE_NOT_SUPPORTED 1
#define MODBUS_INVALID_ADDRESS 2
#define MODBUS_INVALID_VALUE   3

//int modbus_reset_flag = 0;
static int debug_modbus = 0;
static int production_mode = 0;

unsigned short modbus_short_buf[125]; //temporary buffer for register data
unsigned short getModbusRegFc3(int addr);
unsigned short getModbusRegFc4(int addr);
int writeModbusReg6(uint16_t addr, uint16_t val /*, s16 Plev*/);
int16_t writeModbusReg16(uint16_t in_addr, uint16_t data[], uint16_t len);

extern u8_t tcp_obuf[RS485_RX_BUF_SIZE];
extern const esp_app_desc_t *app_desc;

int modbus_slave(char *raw_msg, int raw_size, int tcp_modbus)
{
  int function_code;
  int modbus_address;
  int startReg;
  int nrReg;
  unsigned short value;
  unsigned short crc;
  //unsigned short calc_crc;
  int i;

  statistics.modbus_slave++;

  if(debug_modbus)
  {
    MY_LOGI(TAG,"Modbus size:%d tcp:%d",raw_size, tcp_modbus);
    for(i = 0; i < raw_size; i++)
      printf("%02x ", raw_msg[tcp_modbus*6+i]);
    printf("\n\r");
  }

  if(tcp_modbus) //copy tcp header
  {
	  for(i = 0; i < 6; i++)
		  tcp_obuf[i] = raw_msg[i];
  }

  modbus_address = raw_msg[tcp_modbus*6+0];
  tcp_obuf[tcp_modbus*6+0] = modbus_address;

  function_code = raw_msg[tcp_modbus*6+1];
  tcp_obuf[tcp_modbus*6+1] = function_code;

  if(raw_size < 8 || modbus_address != settings.sg_modbus_addr)
  {
    MY_LOGI(TAG, "ERROR %s modbus address:%d size:%d", __FUNCTION__, modbus_address, raw_size);
    statistics.errors++;
    return -1;
  }

  //if(debug_modbus)
    //MY_LOGI(TAG, "%s FC:%x start:%d nreg:%d crc:%x calc crc:%x",__FUNCTION__, function_code, startReg, nrReg, crc, calc_crc);

  switch (function_code)
  {
  case 3: //read registers at 40000
  case 4: //read registers at 30000
    if(raw_size != 8)
      return -2;  // neveljavna dolzina niza

    startReg = raw_msg[tcp_modbus*6+2]*256 + raw_msg[tcp_modbus*6+3];
    //if (startReg > 1600)//max modbus address
      //return -3;  //neveljaven register

    nrReg = raw_msg[tcp_modbus*6+4]*256 + raw_msg[tcp_modbus*6+5];
    if (nrReg > 121) 	// DSP v1.06+ (prej 120)
      return -4;  //neveljaven podatek

    tcp_obuf[tcp_modbus*6+2] = nrReg * 2; //size of return data
    raw_size = nrReg*2 + 3 + 2; //complete return size: 3 bytes of modbus header + 2 crc

    //get data
    for(i = 0; i < nrReg; i++)
    {
      if(function_code == 3)
        modbus_short_buf[i] = getModbusRegFc3(startReg + i); //40000 //todo:without temporary short size buffer
      else
        modbus_short_buf[i] = getModbusRegFc4(startReg + i); //30000 //get data to temporary short size buffer
    }

    memcpy(tcp_obuf + tcp_modbus*6 + 3, modbus_short_buf, nrReg*2);
    break;
  case 6: //single register write
    if (raw_size != 8)
      return 0;  // neveljavna dolzina niza
    startReg = raw_msg[tcp_modbus*6+2]*256 + raw_msg[tcp_modbus*6+3];
    value = raw_msg[tcp_modbus*6+4]*256 + raw_msg[tcp_modbus*6+5];
    writeModbusReg6(startReg, value);
    break;
  case 16: //multiple register write
    startReg = raw_msg[tcp_modbus*6+2]*256 + raw_msg[tcp_modbus*6+3];
    nrReg = raw_msg[tcp_modbus*6+6];
    for(i = 0; i < 6; i++)//copy write command to response
    	tcp_obuf[tcp_modbus*6 + i] = raw_msg[tcp_modbus*6 + i];
    raw_size = 8; //return size: 6 bytes of data + 2 crc
    //write data
    memcpy(modbus_short_buf, raw_msg + tcp_modbus*6 + 7, nrReg); //todo brez kopiranja
    writeModbusReg16(startReg, modbus_short_buf, nrReg/2);
    break;
  default:
    MY_LOGE(TAG, "ERROR %s Not supported function code: %d",__FUNCTION__, function_code);
    tcp_obuf[tcp_modbus*6+1] |= 0x80; //set error flag
    raw_size = 5;
    statistics.errors++;
    break;
  } //switch function code

  //finish modbus packet
  if(tcp_modbus)
  {
	  raw_size += 4; //6bytes of tcp header - 2 bytes of crc
	  tcp_obuf[5] = raw_size - 6; //size in tcp header
  }
  else //rtu: add crc
  {
    crc = CRC16( (unsigned char*) tcp_obuf + tcp_modbus*6, raw_size - 2);
    tcp_obuf[tcp_modbus*6 + raw_size - 2] = crc>>8;
    tcp_obuf[tcp_modbus*6 + raw_size - 1] = crc &0xff;
  }

  if(debug_modbus)
  {
    MY_LOGI(TAG,"Modbus OUT size:%d tcp:%d",raw_size, tcp_modbus);
    for(i = 0; i < raw_size; i++)
      printf("%02x ", tcp_obuf[i]);
    printf("\n\r");
  }

  return raw_size;
}

//read registers at 40000
unsigned short getModbusRegFc3(int addr)
{
  if(addr < 13)
	  return 0;

  if(addr < 101) // 40016 - 40100 empty
    return 0;
  if(addr < 121) // 40101 - 40120 description
    return ((unsigned short *)&settings.description)[addr-101];
  if(addr < 141) // 40121 - 40140 location
    return ((unsigned short *)&settings.location)[addr-121];
  if(addr == 141)
    return SwapW(settings.timezone);
  if(addr == 142)
	  return SwapW(settings.time_sync_src);
  if(addr == 145)//pulse counter enabled
	  return SwapW(settings.pulse_cnt_enabled);
  if(addr == 146)//PT1000 enabled
	  return SwapW(settings.adc_enabled);
  if(addr == 147)//TCP Modbus server enabled
	  return SwapW(settings.tcp_modbus_enabled);
  if(addr == 148)//UDP server enabled
	  return SwapW(settings.udp_enabled);
  if(addr == 149)//MDNS enabled
  	  return SwapW(settings.mdns_enabled);
  if(addr < 161) // 40150 - 40161 reserved
  	  return 0;
  if(addr < 165)//energy logger
	  return SwapW(settings.energy_log[addr - 161]);
  if(addr < 201) // 40165 - 40200 empty
	  return 0;
  if(addr == 201)
	  return SwapW(settings.tcp_port);
  if(addr == 202)
	  return SwapW(settings.sg_modbus_addr);
  if(addr == 203)
	  return SwapW(settings.connection_mode);
  if(addr == 204)
	  return SwapW(settings.http_port);
  if(addr == 205) //40205 web settings lock
  	  return SwapW(settings.locked);
  if(addr == 206) //40206 wifi_ap fallback
	  return SwapW(settings.wifi_ap_enabled);
  if(addr == 207) //40207 static ip
  	  return SwapW(settings.static_ip_enabled);
  if(addr == 228)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.ip);
	  return(info_t.ip.addr &0xFFFF);
  }
  if(addr == 229)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.ip);
	  return(info_t.ip.addr>>16);
  }
  if(addr == 230)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.netmask);
	  return(info_t.ip.addr &0xFFFF);
  }
  if(addr == 231)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.netmask);
	  return(info_t.ip.addr>>16);
  }
  if(addr == 232)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.gateway);
	  return(info_t.ip.addr &0xFFFF);
  }
  if(addr == 233)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.gateway);
	  return(info_t.ip.addr>>16);
  }
  if(addr == 234)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.dns1);
	  return(info_t.ip.addr &0xFFFF);
  }
  if(addr == 235)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.dns1);
	  return(info_t.ip.addr>>16);
  }
  if(addr == 236)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.dns2);
	  return(info_t.ip.addr &0xFFFF);
  }
  if(addr == 237)
  {
	  esp_netif_ip_info_t info_t;
	  info_t.ip.addr = esp_ip4addr_aton((const char *)settings.dns2);
	  return(info_t.ip.addr>>16);
  }
  if(addr < 301) // 40206 - 40300 empty
	  return 0;
  if(addr < 321) // 40301 - 40320
    return ((unsigned short *)&settings.ntp_server1)[addr-301];
  if(addr < 341) // 40321 - 40340
    return ((unsigned short *)&settings.ntp_server2)[addr-321];
  if(addr < 361) // 40341 - 40360
    return ((unsigned short *)&settings.ntp_server3)[addr-341];
  if(addr < 371) // 40361 - 40370
    return ((unsigned short *)&settings.wifi_ssid)[addr-361];
  //no more settings password via Modbus
  if(addr < 381) // 40371 - 40380
    return ((unsigned short *)&settings.wifi_password)[addr-371];
  if(addr == 381)
  	  return SwapW(settings.wifi_max_retry);
  if(addr < 399) // 40382 - 40399 empty
    return 0;
#ifdef ENABLE_RIGHT_IR_PORT
  if(addr == 399)
	  return SwapW(settings.right_ir_push.push_link);
  if(addr == 400)
	  return SwapW(settings.right_ir_push.push_interval);
  if(addr == 401)
	  return SwapW(settings.ir_ext_rel_mode);
#endif
#ifdef ENABLE_LEFT_IR_PORT
  if(addr == 402)
	  return SwapW(settings.left_ir_enabled);
  if(addr == 403)
	  return SwapW(settings.ir_counter_addr);
  if(addr == 415)
	  return SwapW(settings.left_ir_push.push_link);
  if(addr == 416)
	  return SwapW(settings.left_ir_push.push_interval);
#endif
  if(addr < 421) // 40404 - 40420 empty
    return 0;
//  if(addr == 421)
//    return SwapW(settings.rs485_is_debug_port);
#ifdef ENABLE_RS485_PORT
  if(addr == 422)
    return SwapW(settings.rs485_baud_rate);
  if(addr == 423)
    return SwapW(settings.rs485_stop_bits);
  if(addr == 424)
    return SwapW(settings.rs485_parity);
  if(addr == 425)
	  return SwapW(settings.rs485_data_bits);
#endif
  if(addr < 499) // 40426 - 40499 empty
	  return 0;

  if(addr > 499 && addr < 888) //mqtt settings
  {
	  int index = 0;//MQTT1
	  if(addr > 749)
		  index = 1;//MQTT2

	  if(addr < (520 + index*250)) // 40500 - 40519
		  return ((unsigned short *)&settings.publish_server[index].mqtt_server)[addr-500 - index*250];
	  if(addr == (520 + index*250))
		  return SwapW(settings.publish_server[index].mqtt_port);
	  if(addr == (521 + index*250))
		  return SwapW(settings.publish_server[index].push_protocol);
	  if(addr == (522 + index*250))
		  return SwapW(settings.publish_server[index].push_resp_time);
	  if(addr < (525 + index*250)) // 40523 - 40524 reserved
		  return 0;
	  if(addr == (525 + index*250))
		  return SwapW(settings.publish_server[index].mqtt_tls);
	  if(addr < (542 + index*250)) // 40526 - 40541
		  return ((unsigned short *)&settings.publish_server[index].mqtt_username)[addr-526 - index*250];
	  if(addr < (558 + index*250)) // 40542 - 40557
		  return ((unsigned short *)&settings.publish_server[index].mqtt_password)[addr-542 - index*250];
	  if(addr < (574 + index*250)) // 40558 - 40573
		  return ((unsigned short *)&settings.publish_server[index].mqtt_root_topic)[addr-558 - index*250];
	  if(addr < (590 + index*250)) // 40574 - 40589
		  return ((unsigned short *)&settings.publish_server[index].mqtt_sub_topic)[addr-574 - index*250];
	  if(addr < (606 + index*250)) // 40590 - 40605
		  return ((unsigned short *)&settings.publish_server[index].mqtt_pub_topic)[addr-590 - index*250];
	  if(addr < (622 + index*250)) // 40606 - 40621
		  return ((unsigned short *)&settings.publish_server[index].mqtt_cert_file)[addr-606 - index*250];
	  if(addr < (638 + index*250)) // 40622 - 40637
		  return ((unsigned short *)&settings.publish_server[index].mqtt_key_file)[addr-622 - index*250];
	  if(addr < (750 + index*250)) // 40638 - 40749 reserved
		  return 0;
  }//MQTT

  if(addr < 1001) // 40888 - 41000 empty
    return 0;
#ifdef ENABLE_RS485_PORT
  if(addr > 1000 && addr < 1161) // 41001 - 41160 //16 rs485 device configurations
  {
	  int index = (addr - 1001) / 10;

	  if((addr % 10) == 1)//41001 + index *10 = device type
		  return SwapW(settings.rs485[index].type );
	  else if((addr % 10) == 2)//41002 + index *10 = device modbus address
		  return SwapW(settings.rs485[index].address);
	  if((addr % 10) == 8)//41008 + index *10 = push link
		  return SwapW(settings.rs485_push[index].push_link );
	  else if((addr % 10) == 9)//41009 + index *10 = push interval
		  return SwapW(settings.rs485_push[index].push_interval);
	  else
		  return 0;
  }
#endif

  if(addr < 1501) // 41014 - 41500 empty
    return 0;
#ifdef ENABLE_RIGHT_IR_PORT
  if(addr < 1511) // 41501 - 41510 IR relay description
    return ((unsigned short *)&settings.ir_relay_description)[addr-1501];
#endif
#ifdef ENABLE_RS485_PORT
  if(addr > 1510 && addr < 1671) // 41511 - 41670 //16 rs485 relay device descriptions
  {
	  int index = (addr - 1511) / 10;
	  return ((unsigned short *)&settings.rs485[index].description)[addr-(1511 + index*10)];
  }
#endif
  //debug settings 45001 -
  if(addr == 5001) //45001
	  return SwapW(settings.log_disable1);
  if(addr == 5002) //45002
	  return SwapW(settings.log_disable2);
  if(addr == 5003) //45003 control task enable
  	  return SwapW(settings.control_task_enabled);

  return 0;
}

//read registers at 30000
unsigned short getModbusRegFc4(int addr)
{
	char tmp[40];

	//MiQen is also checking info at 39000
	if(addr > 9000 && addr < 9100)
		addr -= 9000;

	if(addr == 0)
		return SwapW(5); //Device group, read only
	if(addr < 9) // 30001 - 30008 model type
	{
		strcpy(tmp, factory_settings.model_type);
		return ((unsigned short *)&tmp)[addr-1];
	}
	if(addr < 13) // 30009 - 30012 serial number
		return ((unsigned short *)&factory_settings.serial_number)[addr-9];
	if(addr == 13) //sw version
		return SwapW((uint16_t)strtol(app_desc->version, NULL, 10));   //return SwapW(100 * strtod(app_desc->version, NULL));
	if(addr == 14) //hw version
		return SwapW('A' + factory_settings.hw_ver);
	if(addr == 15) //idf version
		return SwapW(ESP_IDF_VERSION_MAJOR*100 + ESP_IDF_VERSION_MINOR*10);
	if(addr == 16) //filesystem version
		return SwapW((uint16_t)strtol(status.fs_ver, NULL, 10));
	if(addr < 20) // 30017 - 30019 wifi MAC
		return ((unsigned short *)&status.mac_addr)[addr-17];
	if(addr < 101) //30020 - 30100 empty
		return 0;

	//if(addr > 100 )
		//MY_LOGI(TAG,"MiQen read addr:%d", addr);

	if(addr == 101) // 30101 - connection status
	    return SwapW(status.connection);
	if(addr < 111) // 30102 - 30110 IP address
		return ((unsigned short *)&status.ip_addr)[addr-102];
	if(addr == 111) //30111 wifi signal level
		return SwapW(status.wifi_rssi);
	if(addr == 112) //30112 external IR relay status
	{
		status.bicom_state = get_ir_bicom_state();
		return SwapW(status.bicom_state);
	}
	if(addr == 113) //30113 ir energy counter status
		return SwapW(status.ir_counter_status);
	//  if(addr == 114) //30114 digital_input_status
	//    return SwapW(ModbusStatus.digital_input_status);
	if(addr < 115)
		return 0;
	if(addr < 117) // 30115 - 30116 uptime
	{
		unsigned short tmp;

	  	  if(addr-115 == 0)
	  		  tmp = status.uptime >>16; //high word at 115
	  	  else
	  		  tmp = status.uptime; //low word at 116

	  	  return SwapW(tmp);
	    }
  if(addr < 201) //30117 - 30200 empty
      return 0;
  if(addr < 203) // 30201 - 30202 time
  {
	  //time_t now;
	  //time(&now);
	  unsigned short tmp;
	  //convert 32 bit time to big endian
	  if(addr-201 == 0)
		  tmp = (status.timestamp + settings.timezone*60) >>16; //high word at 201, timezone offset for local time
	  else
		  tmp = (status.timestamp + settings.timezone*60); //low word at 202
	  //return swap16(read2bytes((char*)&now, addr-201));
	  return SwapW(tmp);
  }
#ifdef ENABLE_PT1000
  if(addr == 203) //valid temperature mask in bit 0
  {
	  //    if(ResX > 1800 && ResX < 40000) //valid resistance for PT1000 is from 180ohms to 4kohms
	  if(settings.adc_enabled)
		  return SwapW(1);
	  else
		  return 0;
  }
  if(addr == 204)
	  return SwapW(status.pt1000_temp);

  if(addr < 207)
	  return 0;
#endif
#ifdef ENABLE_PULSE_COUNTER
  if(addr < 209) // 30207 - 30208 pulse counter
  {
	  unsigned short tmp;

	  if(addr-207 == 0)
		  tmp = status.dig_in_count >>16; //high word at 207
	  else
		  tmp = status.dig_in_count; //low word at 208

	  return SwapW(tmp);
  }
#endif

  //debug statuses 31001 -
  if(addr == 1001) //31001 internal errors 1
	  return SwapW(status.error_flags & 0xffff);
  if(addr == 1002) //31002 internal errors 2
	  return SwapW((status.error_flags >> 16) & 0xffff);
#ifdef EEPROM_STORAGE
  if(addr == 1003) //31003 Power up counter
	  return SwapW(status.power_up_counter & 0xffff);
  if(addr == 1004) //31004 Upgrade counter
	  return SwapW(status.upgrade.count & 0xffff);
#endif //#ifdef EEPROM_STORAGE
  if(addr == 1005) //31005 CRC NVS
	  return SwapW(status.nvs_settings_crc);
  if(addr == 1006) //31006 CRC settings
  	  return SwapW(status.settings_crc);
  if(addr == 1007) //31007 MQTT 1 state
	  return SwapW(status.mqtt[0].state);
  if(addr == 1008) //31008 MQTT 2 state
	  return SwapW(status.mqtt[1].state);

  return 0;
}

static const char model_type[4][16] = {
		{""},
		{MODEL_TYPE_W1},
		{MODEL_TYPE_W1A},
		{MODEL_TYPE_E1},
};

//write single register, function code 6
int writeModbusReg6(uint16_t addr, uint16_t val)
{
	//MODEL TYPE WRITE at addr 40000 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if(addr == 0)
	{
		if(production_mode)
		{
			if(val < 1 || val > 3)
				return MODBUS_INVALID_VALUE;
			memcpy(factory_settings.model_type, model_type[val], 16);
			MY_LOGI(TAG, "NEW Model type: %s", factory_settings.model_type);
			save_string_key_nvs("model_type", factory_settings.model_type);

			//set default connection mode
			if(val == 3)
				settings.connection_mode = ETHERNET_MODE;
			else
				settings.connection_mode = WIFI_MODE;

			save_i8_key_nvs("connection_mode", settings.connection_mode);

			CRC_update();//update CRC

			return MODBUS_REGISTER_WRITE_OK;
		}
		else
			return MODBUS_INVALID_VALUE;
	}
	if(addr == 1) //HW release at 40001
	{
		if(production_mode)
		{
			MY_LOGW(TAG, "HW Version: %d", val);
			factory_settings.hw_ver = val;
			save_i8_key_nvs("hw_ver", factory_settings.hw_ver);
			return MODBUS_REGISTER_WRITE_OK;
		}
		else
			return MODBUS_INVALID_VALUE;
	}
	//SERIAL NUMBER WRITE at addr 40002 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if(addr == 2) //prototype serial number format at 40002 SG00xxxx
	{
		if(production_mode)
		{
			sprintf(factory_settings.serial_number, "SG%06d", val);
			MY_LOGI(TAG, "NEW Serial number: %s", factory_settings.serial_number);
			save_string_key_nvs("serial_number", factory_settings.serial_number);

			return MODBUS_REGISTER_WRITE_OK;
		}
		else
			return MODBUS_INVALID_VALUE;
	}

  if(addr == 12)//40012 operator command
  {
    if(val == 1) //save settings
    {
      int ret = save_settings_nvs(RESTART);
      MY_LOGI(TAG, "Save Setings,ret:%d", ret);
      return MODBUS_REGISTER_WRITE_OK;
    }
    else if(val == 3) //reset
    {
      MY_LOGI(TAG, "Modbus reset command");
      reset_device();
      return MODBUS_REGISTER_WRITE_OK;
    }
    else if(val == 5)//load default settings
    {
    	if(production_mode)
    	{
    		MY_LOGI(TAG, "Loading Default Setings");
    		clear_nvs_settings();
    		reset_device();
    		return MODBUS_REGISTER_WRITE_OK;
    	}
    	return MODBUS_INVALID_VALUE;
    }
    else if(val == 6)//lock settings
    {
    	settings.locked = 1;
    }
    else if(val == 7)//toggle production mode
    {
    	production_mode = 1;
//    	if(production_mode)
//    		production_mode = 1;
//    	else
//    		production_mode = 0;

    	MY_LOGI(TAG, "Production mode: %d", production_mode);
    	return MODBUS_REGISTER_WRITE_OK;
    }
    else if(val == 8)//unlock settings
    {
    	settings.locked = 0;
    }
  }
  if(addr == 13) //40013 reset register
  {
	  if(val == 1) //bit 0
		  reset_pulse_counter();
	  if(val > 1 && val < 0x1F) //bit 1,2,3,4 : 0x2 - 0x1E
		  erase_energy_recorder(val >> 1);//recorder bits 0 - 3

	  return MODBUS_REGISTER_WRITE_OK;
  }

#ifdef ENABLE_RIGHT_IR_PORT
  if(addr == 15) //40015 relay command
  {
    if(val > 1 || settings.ir_ext_rel_mode != 1) //must be under iHUB control
      return MODBUS_INVALID_VALUE;
    //settings.external_relay_command = val;
    set_ir_bicom_state(val);
    return MODBUS_REGISTER_WRITE_OK;
  }
#endif
  if(addr == 141)
  {
    settings.timezone = val;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 142)
  {
    settings.time_sync_src = val;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 145)//pulse counter enable
  {
    settings.pulse_cnt_enabled = val;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 146)//PT1000 enable
  {
	  settings.adc_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 147)//TCP modbus server enable
  {
	  settings.tcp_modbus_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 148)//UDP server enable
  {
	  settings.udp_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 149)//MDNS enable
  {
	  settings.mdns_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr > 160 && addr < 165)//energy logger 40161 - 40164
  {
	  settings.energy_log[addr - 161] = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 201)
  {
	  if(val < 1)
		  return MODBUS_INVALID_VALUE;
	  settings.tcp_port = val;
	  //modbus_reset_flag = 1;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 202)
  {
    settings.sg_modbus_addr = val;
    //modbus_reset_flag = 1;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 203)
  {
	  settings.connection_mode = val;
	  //modbus_reset_flag = 1;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 204)
  {
	  settings.http_port = val;
	  //modbus_reset_flag = 1;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 205)
  {
	  settings.locked = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 206)
  {
	  settings.wifi_ap_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 207)
  {
	  settings.static_ip_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  //IP
  if(addr == 228)
  {
	  sprintf(settings.ip, "%d.%d.", val>>8, val &0xff);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 229)
  {
	  char temp[10];
	  if(strlen(settings.ip) > 8)
		  return MODBUS_INVALID_VALUE;
	  sprintf(temp, "%d.%d", val>>8, val &0xff);
	  strcat(settings.ip, temp);
//	  if(strcmp(settings.ip, "0.0.0.0"))
//		  settings.static_ip_enabled = 1;
//	  else
//		  settings.static_ip_enabled = 0;

	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 230)
  {
	  sprintf(settings.netmask, "%d.%d.", val>>8, val &0xff);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 231)
  {
	  char temp[10];
	  if(strlen(settings.netmask) > 8)
		  return MODBUS_INVALID_VALUE;
	  sprintf(temp, "%d.%d", val>>8, val &0xff);
	  strcat(settings.netmask, temp);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 232)
  {
	  sprintf(settings.gateway, "%d.%d.", val>>8, val &0xff);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 233)
  {
	  char temp[10];
	  if(strlen(settings.gateway) > 8)
		  return MODBUS_INVALID_VALUE;
	  sprintf(temp, "%d.%d", val>>8, val &0xff);
	  strcat(settings.gateway, temp);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 234)
  {
	  sprintf(settings.dns1, "%d.%d.", val>>8, val &0xff);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 235)
  {
	  char temp[10];
	  if(strlen(settings.dns1) > 8)
		  return MODBUS_INVALID_VALUE;
	  sprintf(temp, "%d.%d", val>>8, val &0xff);
	  strcat(settings.dns1, temp);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 236)
  {
	  sprintf(settings.dns2, "%d.%d.", val>>8, val &0xff);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 237)
  {
	  char temp[10];
	  if(strlen(settings.dns2) > 8)
		  return MODBUS_INVALID_VALUE;
	  sprintf(temp, "%d.%d", val>>8, val &0xff);
	  strcat(settings.dns2, temp);
	  return MODBUS_REGISTER_WRITE_OK;
  }
#ifdef ENABLE_RIGHT_IR_PORT
  if(addr == 399)
  {
	  settings.right_ir_push.push_link = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 400)
  {
	  settings.right_ir_push.push_interval = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 401)
  {
    settings.ir_ext_rel_mode = val;
    //detect_flag = 1;
    return MODBUS_REGISTER_WRITE_OK;
  }
#endif
#ifdef ENABLE_LEFT_IR_PORT
  if(addr == 402)
  {
	  settings.left_ir_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 403)
  {
	  settings.ir_counter_addr = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 415)
  {
	  settings.left_ir_push.push_link = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 416)
  {
	  settings.left_ir_push.push_interval = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
#endif

#ifdef ENABLE_RS485_PORT
  if(addr == 422)
  {
    settings.rs485_baud_rate = val;
    //modbus_reset_flag = 1;//todo test 485
    //RS485_ChangeSettings(settings.rs485_baud_rate, settings.rs485_parity, settings.rs485_stop_bits);
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 423)
  {
    settings.rs485_stop_bits = val;
    //modbus_reset_flag = 1;
    //RS485_ChangeSettings(settings.rs485_baud_rate, settings.rs485_parity, settings.rs485_stop_bits);
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 424)
  {
    settings.rs485_parity = val;
    //modbus_reset_flag = 1;
    //RS485_ChangeSettings(settings.rs485_baud_rate, settings.rs485_parity, settings.rs485_stop_bits);
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 425)
  {
    settings.rs485_data_bits = val;
    //modbus_reset_flag = 1;
    //RS485_ChangeSettings(settings.rs485_baud_rate, settings.rs485_parity, settings.rs485_stop_bits);
    return MODBUS_REGISTER_WRITE_OK;
  }
#endif
  if(addr == 472)
  {
    if(val > 2)
      return MODBUS_INVALID_VALUE;
    settings.publish_server[0].push_protocol = val;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 473)
  {
    if(val > 25)
      return MODBUS_INVALID_VALUE;
    settings.publish_server[0].push_resp_time = val;
    return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 520) //40520 mqtt 1 port
  {
	  settings.publish_server[0].mqtt_port = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 521)//40521 mqtt1 protocol
  {
	  settings.publish_server[0].push_protocol = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 522)//40522 mqtt1 response time
  {
	  settings.publish_server[0].push_resp_time = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 523)//40523 mqtt1 QoS
  {
	  settings.publish_server[0].mqtt_qos = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 525)//40525 mqtt1 tls
  {
	  settings.publish_server[0].mqtt_tls = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 770) //40770 mqtt 2 port
  {
	  settings.publish_server[1].mqtt_port = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 771)//40771 mqtt2 protocol
  {
	  settings.publish_server[1].push_protocol = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 772)//40772 mqtt2 response time
  {
	  settings.publish_server[1].push_resp_time = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 723)//40723 mqtt2 QoS
  {
	  settings.publish_server[1].mqtt_qos = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 775)//40775 mqtt2 tls
  {
	  settings.publish_server[1].mqtt_tls = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }

#ifdef ENABLE_RS485_PORT
  if(addr > 1000 && addr < 1161) // 41001 - 41160 //16 rs485 device configurations
  {
	  int index = (addr - 1001) / 10;

	  if((addr % 10) == 1)//41001 + index *10 = device type
	  {
		  settings.rs485[index].type = val;
		  return MODBUS_REGISTER_WRITE_OK;
	  }
	  else if((addr % 10) == 2)//41002 + index *10 = device modbus address
	  {
		  settings.rs485[index].address = val;
		  return MODBUS_REGISTER_WRITE_OK;
	  }
	  else if((addr % 10) == 8)//41008 + index *10 = device push link
	  {
		  settings.rs485_push[index].push_link = val;
		  return MODBUS_REGISTER_WRITE_OK;
	  }
	  else if((addr % 10) == 9)//41009 + index *10 = device push interval
	  {
		  settings.rs485_push[index].push_interval = val;
		  return MODBUS_REGISTER_WRITE_OK;
	  }
	  else
		  return MODBUS_INVALID_ADDRESS;
  }
#endif

  if(addr == 5001) //45001 log register 1
  {
	  settings.log_disable1 = val;
	  set_log_level1(settings.log_disable1, ESP_LOG_WARN);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 5002) //45002 log register 2
  {
	  settings.log_disable2 = val;
	  set_log_level2(settings.log_disable2, ESP_LOG_WARN);
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 5003) //45003 control task enable
  {
	  settings.control_task_enabled = val;
	  return MODBUS_REGISTER_WRITE_OK;
  }
  if(addr == 5004) //45004 debug console
  {
	  if(val == 0)
		  MY_LOGI(TAG, "internal debug port");
	  else
		  MY_LOGI(TAG, "rs485 debug port");

	  debug_console = val + DEBUG_CONSOLE_INTERNAL;
	  save_i8_key_nvs("debug_console", debug_console); //save debug_console setting
	  return MODBUS_REGISTER_WRITE_OK;
  }

  return MODBUS_INVALID_ADDRESS;
}

//write multiple registers, function code 16
int16_t writeModbusReg16(uint16_t in_addr, uint16_t data[], uint16_t len)
{
  int i;
  uint16_t addr;

  if(len > 40)
    return MODBUS_INVALID_ADDRESS;

  for(i = 0; i < len; i++)
  {
    addr = in_addr + i;
    if(debug_modbus)
    	MY_LOGI(TAG, "Writing to addr:%d, data:0x%x", addr, data[i]);

    //SERIAL NUMBER WRITE at addr 40000 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if(addr < 4)// 40000 - 40003 write serial number
    {
      if(production_mode != 1) //only in production mode
        return MODBUS_INVALID_ADDRESS;

      ((unsigned short *)&factory_settings.serial_number)[addr] = data[i];
      if(addr == 3) //end of serial number, write it to NVS
      {
    	  MY_LOGI(TAG, "NEW Serial number: %s", factory_settings.serial_number);
    	  save_string_key_nvs("serial_number", factory_settings.serial_number);

    	  //set default root topic to serial number
    	  save_string_key_nvs("mqtt1_roottopic", factory_settings.serial_number);//save to settings
    	  memcpy(settings.publish_server[0].mqtt_root_topic, factory_settings.serial_number, 8); //apply setting

    	  save_string_key_nvs("mqtt2_roottopic", factory_settings.serial_number);
    	  memcpy(settings.publish_server[1].mqtt_root_topic, factory_settings.serial_number, 8); //apply setting

    	  CRC_update();

        return MODBUS_REGISTER_WRITE_OK;
      }
      else
        continue;
    }

    if(addr < 101) // 40101 is first multibyte reg
      return MODBUS_INVALID_ADDRESS;
    if(addr < 121) // 40101 - 40120 description
    {
      ((unsigned short *)&settings.description)[addr-101] = data[i];
      settings.description[39] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 141) // 40121 - 40140 location
    {
      ((unsigned short *)&settings.location)[addr-121] = data[i];
      settings.location[39] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
#if 0
    if(addr < 208)
          return MODBUS_INVALID_ADDRESS;
    if(addr < 228) // 40208 - 40227 hostname
    {
    	((unsigned short *)&settings.hostname)[addr-208] = data[i];
    	settings.hostname[39] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
#endif
    if(addr < 301)
      return MODBUS_INVALID_ADDRESS;
    if(addr < 321) // 40301 - 40320
    {
      ((unsigned short *)&settings.ntp_server1)[addr-301] = data[i];
      settings.ntp_server1[39] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 341) // 40321 - 40340
    {
      ((unsigned short *)&settings.ntp_server2)[addr-321] = data[i];
      settings.ntp_server2[39] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 361) // 40341 - 40360
    {
      ((unsigned short *)&settings.ntp_server3)[addr-341] = data[i];
      settings.ntp_server3[39] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 371) // 40361 - 40370 wifi ssid
    {
      ((unsigned short *)&settings.wifi_ssid)[addr-361] = data[i];
      settings.wifi_ssid[19] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 381) // 40371 - 40380 wifi password
    {
    	if(data[i] == ' ') //miqen fills empty place with spaces, so we need to replace them with NULL
    		data[i] = 0;
      ((unsigned short *)&settings.wifi_password)[addr-371] = data[i];
      //settings.wifi_password[19] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
    if(addr < 500)
    	return MODBUS_INVALID_ADDRESS;
    if(addr < 520) // 40500 - 40519 //mqtt 1 server
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_server)[addr-500] = data[i];
    	settings.publish_server[0].mqtt_server[39] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 526)
    	return MODBUS_INVALID_ADDRESS;
    if(addr < 542) // 40526 - 40541 //mqtt 1 username
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_username)[addr-526] = data[i];
    	settings.publish_server[0].mqtt_username[32] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 558) // 40542 - 40557 //mqtt 1 password
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_password)[addr-542] = data[i];
    	settings.publish_server[0].mqtt_password[32] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 574) // 40558 - 40573 // mqtt 1 root topic
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_root_topic)[addr-558] = data[i];
    	settings.publish_server[0].mqtt_root_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 590) // 40574 - 40589 // mqtt 1 subscribe topic
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_sub_topic)[addr-574] = data[i];
    	settings.publish_server[0].mqtt_sub_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 606) // 40590 - 40605 // mqtt 1 publish topic
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_pub_topic)[addr-590] = data[i];
    	settings.publish_server[0].mqtt_pub_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 622) // 40606 - 40621 // mqtt 1 server certificate
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_cert_file)[addr-606] = data[i];
    	settings.publish_server[0].mqtt_cert_file[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 638) // 40622 - 40637 // mqtt 1 client certificate
    {
    	((unsigned short *)&settings.publish_server[0].mqtt_key_file)[addr-622] = data[i];
    	settings.publish_server[0].mqtt_key_file[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 750)
    	return MODBUS_INVALID_ADDRESS;
    if(addr < 770) // 40750 - 40769 //mqtt 2 server
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_server)[addr-750] = data[i];
    	settings.publish_server[1].mqtt_server[39] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 776)
    	return MODBUS_INVALID_ADDRESS;
    if(addr < 792) // 40776 - 40791 //mqtt 2 username
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_username)[addr-776] = data[i];
    	settings.publish_server[1].mqtt_username[32] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 808) // 40792 - 40807 //mqtt 2 password
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_password)[addr-792] = data[i];
    	settings.publish_server[1].mqtt_password[32] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 824) // 40808 - 40823 // mqtt 2 root topic
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_root_topic)[addr-808] = data[i];
    	settings.publish_server[1].mqtt_root_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 840) // 40824 - 40839 // mqtt 2 subscribe topic
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_sub_topic)[addr-824] = data[i];
    	settings.publish_server[1].mqtt_sub_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 856) // 40840 - 40855 // mqtt 2 publish topic
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_pub_topic)[addr-840] = data[i];
    	settings.publish_server[1].mqtt_pub_topic[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 872) // 40856 - 40871 // mqtt 2 server certificate
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_cert_file)[addr-856] = data[i];
    	settings.publish_server[1].mqtt_cert_file[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 888) // 40872 - 40887 // mqtt 2 client certificate
    {
    	((unsigned short *)&settings.publish_server[1].mqtt_key_file)[addr-872] = data[i];
    	settings.publish_server[1].mqtt_key_file[31] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
    if(addr < 1501) // 40888 - 41500 empty
    	return MODBUS_INVALID_ADDRESS;
#ifdef ENABLE_RIGHT_IR_PORT
    if(addr < 1511) // 41501 - 41510 //ir relay device description
    {
      ((unsigned short *)&settings.ir_relay_description)[addr-1501] = data[i];
      settings.ir_relay_description[19] = 0; //todo avtomatsko zakljucevanje stringov
      continue;
    }
#endif
#ifdef ENABLE_RS485_PORT
    if(addr > 1510 && addr < 1671) // 41511 - 41670 //16 rs485 relay device descriptions
    {
    	int index = (addr - 1511) / 10;

    	((unsigned short *)&settings.rs485[index].description)[addr-(1511 + index*10)] = data[i];
    	settings.rs485[index].description[19] = 0; //todo avtomatsko zakljucevanje stringov
    	continue;
    }
#endif

  }//for
  return 0;
}
