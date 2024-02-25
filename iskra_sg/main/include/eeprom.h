/*
 * eeprom.h
 *
 *  Created on: Mar 11, 2020
 *      Author: iskra
 */

#ifndef MAIN_INCLUDE_EEPROM_H_
#define MAIN_INCLUDE_EEPROM_H_
#pragma once

#include <stdio.h>
#include "driver/i2c.h"

// Definitions for i2c
#define I2C_MASTER_SCL_IO   5
#define I2C_MASTER_SDA_IO   0
#define I2C_MASTER_NUM  I2C_NUM_0
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_FREQ_HZ  100000

#define EEPROM_WRITE_ADDR   0x00
#define EEPROM_READ_ADDR    0x01

#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define ACK		(1)
#define NO_ACK	(0)

#define EEPROM_PAGE_SIZE	(16)  //24LC16 has 16 byte buffer

void init_i2c_master();
//esp_err_t eeprom_write_byte(uint8_t deviceaddress, uint16_t eeaddress, uint8_t byte);
esp_err_t eeprom_write(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size);

uint8_t eeprom_read_byte(uint8_t deviceaddress, uint16_t eeaddress);
esp_err_t eeprom_read(uint8_t deviceaddress, uint16_t eeaddress, uint8_t* data, size_t size);

#endif /* MAIN_INCLUDE_EEPROM_H_ */
