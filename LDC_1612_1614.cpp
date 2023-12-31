#include "LDC_1612_1614.h"

float _Rp_lookup_table[] = {
    90.0, 77.6, 66.9, 57.6, 49.7, 
    42.8, 36.9, 31.8, 27.4, 23.6, 
    20.4, 17.6, 15.1, 13.0, 11.2, 
    9.69, 8.35, 7.20, 6.21, 5.35, 
    4.61, 3.97, 3.42, 2.95, 2.54, 
    2.19, 1.89, 1.63, 1.40, 1.21, 
    1.05, 0.90
};

LDC::LDC(uint8_t num_channels) {
    _num_channels = num_channels;
    _channels_in_use = 0;
    _external_frequency = EXTERNAL;
    _I2C_address = 0x2B;
}

// void LDC::print_num_channels() {
//     Serial.println(_num_channels);
//     Serial.println(_Rp[1]);
// }

int8_t LDC::configure_channel(uint8_t channel, float inductance, float capacitance, float Rp) {
    //Serial.println(channel);
    _inductance[channel] = inductance; // in uH 
    _capacitance[channel] = capacitance; // in pF
    _Rp[channel] = Rp; // in kohms
    _q_factor[channel] = _Rp[channel] * (_capacitance[channel] / _inductance[channel]);
    if (_set_channel_in_use(channel)) {
        Serial.println(ERROR_CHANNEL_NOT_SUPPORTED);
        return -1;
    }
    if (_set_reference_divider(channel)) {
        return -1;
    }
    if (_set_settle_count(channel)) {
        Serial.println(ERROR_SETTLE_TOO_LARGE);
        return -1;
    }
    _set_conversion_time(channel);
    if (_set_driver_current(channel)) {
        Serial.println(ERROR_RP_TOO_LARGE);
        return -1;
    }
    _MUX_and_deglitch_config(channel);
    _LDC_config(channel);
    return 0;
}

bool LDC::_set_channel_in_use(uint8_t channel) {
    if (channel > _num_channels - 1) {
        return 1;
    }
    if (_channels_in_use & (1 << channel) == 1) {
        Serial.println(WARNING_CHANNEL_ALREADY_CONFIGURED);
    }
    _channels_in_use |= (1 << channel);
    return 0;
}

uint32_t LDC::get_channel_data(uint8_t channel) {
    uint32_t MSB = LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + channel * 2);
    uint32_t LSB = LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + channel * 2 + 1);
    // int8_t error = LDC::check_read_errors(MSB >> 11);
    // if (error) {
    //     return error;
    // }
    uint32_t data = ((MSB & 0x0FFF) << 16) | LSB;
    if (0xFFFFFFF == data) {
        Serial.println(ERROR_COIL_NOT_DETECTED);
    }
    return data; 
}

void LDC::delay_exact_time(uint8_t channel) {
    delay(_settle_time[channel]);
}

// int8_t LDC::_check_read_errors(uint8_t error_byte) {
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

// uint32_t LDC::get_channel_result(uint8_t channel) {
//     uint32_t raw_value = 0;
//     uint16_t value = 0;
//     value = LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + (channel * 2));
//     raw_value |= (uint32_t)value << 16;
//     value = LDC::I2C_read_16bit(CONVERTION_RESULT_REG_START + (channel * 2 + 1));
//     raw_value |= (uint32_t)value;
//     return raw_value;
//     // parse_result_data(channel, raw_value, result);
//     // return 0;
// }

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
    if (_f_sensor[channel] > MAX_SENSING_FREQ) {
        Serial.println(ERROR_FREQUENCY_TOO_LARGE);
        return -1;
    }
    if (_f_sensor[channel] < MIN_SENSING_FREQ) {
        Serial.println(ERROR_FREQUENCY_TOO_SMALL);
        return -1;
    }
    if (_f_sensor[channel] > 8750000) {
        value = 0x2000;
    }
    else {
        value = 0x1000;
    }
    divider = _external_frequency / (_f_sensor[channel] * 4);
    value |= divider;
    _ref_frequency[channel] = _external_frequency / divider; // the Grove board has an external occilator at 40MHz
    LDC::I2C_write_16bit(SET_FREQ_REG_START + channel, value);
    return 0;
}

// stabilize time is Q * reference frequency / (16 * sensor frequency)
// Settle Time (tS1)= (SETTLECOUNT1ˣ16) / ƒREF1
// the datasheet recommends a minimum of 10
// and adding a slight buffer to the calculation, here +4 was chosen
bool LDC::_set_settle_count(uint8_t channel) {
    uint16_t value = 10;
    uint32_t settle_count = ((_q_factor[channel] * _ref_frequency[channel]) / (16 * _f_sensor[channel])) + 4;
    if (settle_count > 0xFFFF) {
        return 1;
    }
    if (settle_count > value) {
        value = settle_count;
    }
    _settle_time[channel] = ((settle_count * 16) / _ref_frequency[channel]) * 1000;
    LDC::I2C_write_16bit(SET_LC_STABILIZE_REG_START + channel, value);
    return 0;
}

// conversion time is set to the highest value, 0xFFFF
// doing this give the best accuracy
// as it collects more samples per conversion
// TODO: ADD A FAST MODE THAT CONVERTS AS FAST AS ALLOWED
void LDC::_set_conversion_time(uint8_t channel) {
    LDC::I2C_write_16bit(SET_CONVERSION_TIME_REG_START + channel, 0xFFFF);
}

// The original library does not take into account the fact that the Rp
// links to the current. It asks for the Rp but does not look
// up the correct current value using that Rp value
// The table values are directly from the datasheet
// the lowest possible should be taken
// the value must be set in bits 15-11
// the rest are reserved to be 0
bool LDC::_set_driver_current(uint8_t channel) {
    uint16_t value;
    if (_Rp[channel] > _Rp_lookup_table[0]) {
        return -1;
    }
    for (int i = 0; i < RP_TABLE_ELEMENTS; i++) {
        if (_Rp[channel] <= _Rp_lookup_table[i]) {
            value = i;
        }
    }
    value = value << 11;
    LDC::I2C_write_16bit(SET_DRIVER_CURRENT_REG + channel, value);
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
    uint16_t value = 0x0208;
    // handling channel scanning
    if (_channels_in_use & (_channels_in_use - 1) != 0) {
        // if more than one bit is set in the byte 
        value |= 0x8000;
    }
    else if (_channels_in_use & 0b1000) {
        value |= 0x2000; // channel 2 in use
    }
    else if (_channels_in_use & 0b0100) {
        value |= 0x4000; // channel 3 in use
    }
    // handling input deglitch
    if (_f_sensor[channel] < 1000000) {
        value |= 0b001;
    }
    else if (_f_sensor[channel] < 3300000) {
        value |= 0b100;
    }
    else if (_f_sensor[channel] < 10000000) {
        value |= 0b101;
    }
    else {
        value |= 0b111;
    }
    LDC::I2C_write_16bit(MUL_CONFIG_REG, value);
}

// all of this is the default but some critical changes need to made
// this is the part of the library that needs the most tweaking
// and testing for better accuracy
// Need to look into:
// Automatic Sensor Amplitude Correction Disable (bit 10)
// Select Reference Frequency Source (bit 9) **TESTING DONE: USE EXTERNAL AT 40MHZ**
// INTB Disable (bit 7)
// High Current Sensor Drive (bit 6)
// Note: only one config, not per channel
void LDC::_LDC_config(uint8_t channel) {
    uint16_t value;
    switch (channel) {
    case CHANNEL_0: 
        value = 0x0000;
    case CHANNEL_1: 
        value = 0x4000;
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
}

int32_t LDC::I2C_write_16bit(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_I2C_address);
    Wire.write(reg);

    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)value);
    return Wire.endTransmission();
}

uint16_t LDC::I2C_read_16bit(uint8_t start_reg) {
    uint16_t result = 0;
    uint8_t byte = 0;
    Wire.beginTransmission(_I2C_address);
    Wire.write(start_reg);
    Wire.endTransmission(false);

    Wire.requestFrom(_I2C_address, sizeof(uint16_t));
    while (sizeof(uint16_t) != Wire.available());
    byte = Wire.read();
    result = (uint16_t)byte << 8;
    byte = Wire.read();
    result |= byte;
    return result;
}

// 1 for 0x2B (high voltage on ADDR pin)
// 0 for 0x2A (low voltage on ADDR pin)
void LDC::change_I2C_address(bool mode) {
    if (mode) {
        _I2C_address = 0x2B;
    }
    else {
        _I2C_address = 0x2A;
    }
}

// use INTERNAL or EXTERNAL for standard freqencies
// or input the oscillator value used in your design
// must be <40MHz and >2MHz
void LDC::change_clock_freq(uint32_t freq) {
    if (freq > 40000000 || freq < 2000000) {
        Serial.println(ERROR_REF_FREQUENCY_OOB);
        return;
    }
    _external_frequency = freq;
}