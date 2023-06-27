#ifndef LDC_1612_1614.H
#define LDC_1612_1614.H
#include <Arduino.h>
#include <Wire.h>

#define CONVERTION_RESULT_REG_START             0X00
#define SET_CONVERSION_TIME_REG_START           0X08
#define SET_CONVERSION_OFFSET_REG_START         0X0C
#define SET_LC_STABILIZE_REG_START              0X10
#define SET_FREQ_REG_START                      0X14

#define SENSOR_STATUS_REG                       0X18
#define ERROR_CONFIG_REG                        0X19
#define SENSOR_CONFIG_REG                       0X1A
#define MUL_CONFIG_REG                          0X1B
#define SENSOR_RESET_REG                        0X1C
#define SET_DRIVER_CURRENT_REG                  0X1E

#define READ_MANUFACTURER_ID                    0X7E
#define READ_DEVICE_ID                          0X7F

#define LDC1612    2
#define LDC1614    4

#define CHANNEL_0  0
#define CHANNEL_1  1
#define CHANNEL_2  2  // LDC1614 only
#define CHANNEL_3  3  // LDC1614 only

#define EXTERNAL_FREQUENCY 40000000
#define MAX_SENSING_FREQ   10000000
#define MIN_SENSING_FREQ   1000
#define RP_TABLE_ELEMENTS  31
#define _I2C_ADDR          0x2B

#define ERROR_FREQUENCY_TOO_LARGE \
"ERROR: Sensor frequency is too large! \n  \
Try to increase the size of the parallel capacitor."
#define ERROR_FREQUENCY_TOO_SMALL \
"ERROR: Sensor frequency is too small! \n \
Try to decrease the size of the parallel capacitor."
#define ERROR_RP_TOO_LARGE \
"ERROR: Rp is too large! \n \
Try to add a 100 ohm resistor in parallel with the inductor."
#define ERROR_SETTLE_TOO_LARGE \
"ERROR Q is too large compared to the size of the coil. \n \
Try to reduce the size of the capacitor, \n \
or check that your parameters are set correctly."
#define ERROR_CHANNEL_NOT_SUPPORTED \
"ERROR: This chip does not support the channel specified. \n \
Either you mistyped the channel number or the chip number."
// #define ERROR_UNDER_RANGE
// "ERROR: "
// #define ERROR_OVER_RANGE             -4
// #define ERROR_WATCHDOG_TIMEOUT       -5
// #define ERROR_CONVERSION_AMPLITUDE   -6
#define ERROR_COIL_NOT_DETECTED \ 
"ERROR: The coil is not detected. \n \
Try checking all connections."

#define WARNING_CHANNEL_ALREADY_CONFIGURED \
"WARNING: This channel was previously configured \n \
Did you mean to configure another channel? \n \
The channel has been overwritten with the new parameters."

struct RpTable {
  float kohms;
  uint16_t current;
};  

class LDC {
private:
// knowing which channels are in use affects the MUX config
// and allows for faster conversions
// as it doesn't need to cycle through all the channels
    uint8_t _num_channels;
    uint8_t _channels_in_use;
    float _Rp[2];
    float _inductance[2];
    float _capacitance[2];
    float _q_factor[2];
    uint32_t _f_sensor[2];
    uint32_t _ref_frequency[2];

    bool _set_channel_in_use(uint8_t);

    bool _set_reference_divider(uint8_t);
    bool _set_settle_count(uint8_t);
    void _set_conversion_time(uint8_t);
    bool _set_driver_current(uint8_t);
    void _MUX_and_deglitch_config(uint8_t);
    void _LDC_config(uint8_t);

    int8_t _check_read_errors(uint8_t);

    int32_t I2C_write_16bit(uint8_t, uint16_t);
    uint16_t I2C_read_16bit(uint8_t);
  
public:
    LDC(uint8_t);
    int8_t configure_channel(uint8_t, float, float, float);
    uint32_t get_channel_data(uint8_t);
    void print_num_channels();
};

#endif