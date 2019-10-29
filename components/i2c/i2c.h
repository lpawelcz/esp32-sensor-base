#include "driver/gpio.h"
#include "driver/i2c.h"

#include "common.h"

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

#define TAG_I2C		"I2C"

void i2c_master_init();
s8 i2c_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
s8 i2c_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
int scan_i2c(u8 reg_addr, u8 *reg_data, u8 cnt);
