#include "LDC_1612_1614.h"

struct RpTable _Rp_lookup_table[] = {
    {90.0, 16}, {77.6, 18}, {66.9, 20},
    {57.6, 23}, {49.7, 28}, {42.8, 32},
    {36.9, 40}, {31.8, 46}, {27.4, 52},
    {23.6, 59}, {20.4, 72}, {17.6, 82},
    {15.1, 95}, {13.0, 110}, {11.2, 127},
    {9.69, 146}, {8.35, 169}, {7.20, 195},
    {6.21, 212}, {5.35, 244}, {4.61, 297},
    {3.97, 342}, {3.42, 424}, {2.95, 489},
    {2.54, 551}, {2.19, 635}, {1.89, 763},
    {1.63, 880}, {1.40, 1017}, {1.21, 1173},
    {1.05, 1355}, {0.90, 1563}
};

LDC::LDC(uint8_t num_channels) {
    _num_channels = num_channels;
}

// void LDC::print_num_channels() {
//     Serial.println(_num_channels);
//     Serial.println(_Rp[1]);
// }

int8_t LDC::configure_channel(uint8_t channel, float Rp, float inductance, float capacitance) {
    //Serial.println(channel);
    _Rp[channel] = Rp; // in kohms
    _inductance[channel] = inductance; // in uH 
    _capacitance[channel] = capacitance; // in pF
    _q_factor[channel] = _Rp[channel] * (_capacitance[channel] / _inductance[channel]);
    if (_set_reference_divider(channel)) {
        return ERROR_FREQUENCY_TOO_LARGE;
    }
    _set_stabilize_time(channel);
    _set_conversion_time(channel);
    if (_set_driver_current(channel)) {
        return ERROR_RP_TOO_LARGE;
    }
    _MUX_and_deglitch_config(channel);
    _LDC_config(channel);
    return 0;
}

// TODO: THIS ENTIRE FUNCTION
// LIKELY NEED TO CHECK FOR ERRORS
// int32_t LDC::get_channel_data(uint8_t channel) {
//     uint16_t MSB;
//     uint16_t LSB;
//     LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + channel * 2, &MSB);
//     LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + channel * 2 + 1, &LSB);
//     int8_t error = LDC::check_read_errors(MSB >> 11);
//     if (error) {
//         return error;
//     }
//     int32_t data = ((MSB & 0x0FFF) << 8) + LSB;
//     if (data = 0xFFFFFFF) {
//         return ERROR_COIL_NOT_DETECTED;
//     }
//     return data; 
// }

// int8_t LDC::check_read_errors(uint8_t error_byte) {
//     if (error_byte & 0x8) {
//         return ERROR_UNDER_RANGE;
//     }
//     else if (error_byte & 0x4) {
//         return ERROR_OVER_RANGE;
//     }
//     else if (error_byte & 0x2) {
//         return ERROR_WATCHDOG_TIMEOUT;
//     }
//     else if (error_byte & 0x1) {
//         return ERROR_CONVERSION_AMPLITUDE;
//     }
//     return 0;
// }

int32_t LDC::parse_result_data(uint8_t channel, uint32_t raw_result, uint32_t* result) {
    uint8_t value = 0;
    *result = raw_result & 0x0fffffff;
    if (0xfffffff == *result) {
        Serial.println("can't detect coil Coil Inductance!!!");
        *result = 0;
        return -1;
    }
    value = raw_result >> 24;
    if (value & 0x80) {
        Serial.print("channel ");
        Serial.print(channel);
        Serial.println(": ERR_UR-Under range error!!!");
    }
    if (value & 0x40) {
        Serial.print("channel ");
        Serial.print(channel);
        Serial.println(": ERR_OR-Over range error!!!");
    }
    if (value & 0x20) {
        Serial.print("channel ");
        Serial.print(channel);
        Serial.println(": ERR_WD-Watch dog timeout error!!!");
    }
    if (value & 0x10) {
        Serial.print("channel ");
        Serial.print(channel);
        Serial.println(": ERR_AE-error!!!");
    }
    return 0;
}

uint32_t LDC::get_channel_result(uint8_t channel) {
    uint32_t raw_value = 0;
    uint16_t value = 0;
    LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + (channel * 2), &value);
    raw_value |= (uint32_t)value << 16;
    LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + (channel * 2 + 1), &value);
    raw_value |= (uint32_t)value;
    return raw_value;
    // parse_result_data(channel, raw_value, result);
    // return 0;
}

// there are two dividers,  
// FIN_DIVIDERX and FREF_DIVIDERX, where X is the channel
// the first must be 1 if fSensor is below 8.75MHz
// otherwise it can be set to 2
// then there are two 00 bits
// then the rest is for the fRef divider
// The design constraint for f_REF0 is > 4 * f_SENSOR
// It can be set to 40MHz with a divider of 1, but this will make
// the occilation time shorter and less accurate
// a lower frequency also neautralizes any chance
// for the inductor to self resonate and become a capacitor
// through its parasitic capacitance
bool LDC::_set_reference_divider(uint8_t channel) {
    uint8_t divider;
    uint16_t value;
    uint16_t FIN_DIV, FREF_DIV;
    // pull the ^-12 and ^6 for pF and uH out to the front
    // to change it to ^-6 and ^-3
    _f_sensor[channel] = 1 / (2 * 3.14 * sqrt(_inductance[channel] * _capacitance[channel]) * pow(10, -3) * pow(10, -6));
    if (_f_sensor[channel] > EXTERNAL_FREQUENCY) {
        return ERROR_FREQUENCY_TOO_LARGE;
    }
    if (_f_sensor[channel] > 8750000) {
        value = 0x2000;
    }
    else {
        value = 0x1000;
    }
    divider = EXTERNAL_FREQUENCY / (_f_sensor[channel] * 4);
    value |= divider;
    _ref_frequency[channel] = EXTERNAL_FREQUENCY / divider; // the Grove board has an external occilator at 40MHz
    LDC::I2C_write_16bit(SET_FREQ_REG_START + channel, value);
    Serial.println("Reference Divider");
    Serial.println(value);
    uint16_t test = 0;
    LDC::I2C_read_16bit(SET_FREQ_REG_START + channel, &test);
    Serial.println(test);
    return 0;
}

// stabilize time is Q * reference frequency / (16 * sensor frequency)
// the datasheet recommends a minimum of 10
// and adding a slight buffer to the calculation, here +4 was chosen
void LDC::_set_stabilize_time(uint8_t channel) {
    uint16_t value = 10;
    uint16_t settleTime = ((_q_factor[channel] * _ref_frequency[channel]) / (16 * _f_sensor[channel])) + 4;
    if (settleTime > value) {
        value = settleTime;
    }
    LDC::I2C_write_16bit(SET_LC_STABILIZE_REG_START + channel, value);
    Serial.println("Stabilize Time");
    Serial.println(value);
    uint16_t test = 0;
    LDC::I2C_read_16bit(SET_LC_STABILIZE_REG_START + channel, &test);
    Serial.println(test);
}

// conversion time is set to the highest value, 0xFFFF
// doing this give the best accuracy
// as it collects more samples per conversion
// TODO: ADD A FAST MODE THAT CONVERTS AS FAST AS ALLOWED
void LDC::_set_conversion_time(uint8_t channel) {
    LDC::I2C_write_16bit(SET_CONVERSION_TIME_REG_START + channel, 0xFFFF);
    Serial.println("Conversion Time");
    Serial.println(0xFFFF);
    uint16_t test = 0;
    LDC::I2C_read_16bit(SET_CONVERSION_TIME_REG_START + channel, &test);
    Serial.println(test);
}

// The original library does not take into account the fact that the Rp
// links to the current. It asks for the Rp but does not look
// up the correct current value using that Rp value
// The table values are directly from the datasheet
// the lowest possible should be taken
bool LDC::_set_driver_current(uint8_t channel) {
    uint16_t value;
    if (_Rp[channel] > _Rp_lookup_table[0].kohms) {
        return -1;
    }
    for (int i = 0; i < RP_TABLE_ELEMENTS; i++) {
        if (_Rp[channel] <= _Rp_lookup_table[i].kohms) {
            value = i;
        }
    }
    LDC::I2C_write_16bit(SET_DRIVER_CURRENT_REG + channel, value);
    Serial.println("Driver Current");
    Serial.println(value);
    uint16_t test = 0;
    LDC::I2C_read_16bit(SET_DRIVER_CURRENT_REG + channel, &test);
    Serial.println(test);
    return 0;
}

// Bit 15: Auto-Scan Enable (normally this should be 1, 
// but needs to be 0 so that high current mode could be enabled)
// WARNING: THIS DISABLES THE USE OF DUEL CHANNEL
// Bit 14-13 00 mode enables both channels
// Bit 12-3 RESERVED TO 00 0100 0001
// Bit 2-0  Deglitch frequency
// Deglitch reduces noise, 
// the lowest possible of the 4 provided options should
// be selected for cleaner results
void LDC::_MUX_and_deglitch_config(uint8_t channel) {
    uint16_t value = 0x8208;  
    // TODO: ADD SUPPORT FOR LDC1614 WITH TURNING ON MORE CHANNELS
    if (_f_sensor < 1000000) {
        value |= 0b001;
    }
    else if (_f_sensor < 3300000) {
        value |= 0b100;
    }
    else if (_f_sensor < 10000000) {
        value |= 0b101;
    }
    else {
        value |= 0b111;
    }
    LDC::I2C_write_16bit(MUL_CONFIG_REG, value);
    Serial.println("MUX");
    Serial.println(value);
    uint16_t test = 0;
    LDC::I2C_read_16bit(MUL_CONFIG_REG + channel, &test);
    Serial.println(test);
    return 0;
}

// all of this is the default but some critical changes need to made
// this is the part of the library that needs the most tweaking
// and testing for better accuracy
// Need to look into:
// Automatic Sensor Amplitude Correction Disable (bit 10)
// Select Reference Frequency Source (bit 9) **TESTING DONE: USE EXTERNAL AT 40MHZ**
// INTB Disable (bit 7)
// High Current Sensor Drive (bit 6)
void LDC::_LDC_config(uint8_t channel) {
    uint16_t value;
    switch (channel) {
    case CHANNEL_0: 
        value = 0x0000;
    case CHANNEL_1: 
        value = 0x7000;
        break;
    case CHANNEL_2: 
        value = 0x8000;
        break;
    case CHANNEL_3: 
        value = 0xc000;
        break;
    }
    value |= 0x1601;
    LDC::I2C_write_16bit(SENSOR_CONFIG_REG, value);
    Serial.println("LDC Config");
    Serial.println(value);
    uint16_t test = 0;
    LDC::I2C_read_16bit(SENSOR_CONFIG_REG + channel, &test);
    Serial.println(test);
}

int32_t LDC::I2C_write_16bit(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_I2C_ADDR);
    Wire.write(reg);

    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)value);
    return Wire.endTransmission();
}

void LDC::I2C_read_16bit(uint8_t start_reg, uint16_t* value) {
    uint8_t val = 0;
    *value = 0;
    Wire.beginTransmission(_I2C_ADDR);
    Wire.write(start_reg);
    Wire.endTransmission(false);

    Wire.requestFrom(_I2C_ADDR, sizeof(uint16_t));
    while (sizeof(uint16_t) != Wire.available());
    val = Wire.read();
    *value |= (uint16_t)val << 8;
    val = Wire.read();
    *value |= val;
}