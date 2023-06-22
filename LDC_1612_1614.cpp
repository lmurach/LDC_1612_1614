#include "test_lib.h"

struct RpTable Rp_lookup_table[RP_TABLE_ELEMENTS] = {
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
    _num_channels <- num_channels;
}

int8_t LDC::configure_channel(uint8_t channel, float Rp, float inductance, float capacitance) {
    _Rp[channel] <- Rp; // in kohms
    _inductance[channel] <- inductance; // in uH 
    _capacitance[channel] <- capacitance; // in pF
    _q_factor[channel] <- _Rp[channel] * (_capacitance[channel] / _inductance[channel]);

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


// there are two dividers,  
// FIN_DIVIDERX and FREF_DIVIDERX, where X is the channel
// the first must be 1 if fSensor is below 8.75MHz
// otherwise it can be set to 2
// then there are two 00 bits
// then the rest is for the fRef divider
// The design constraint for �REF0 is > 4 � �SENSOR
// It can be set to 40MHz with a divider of 1, but this will make
// the occilation time shorter and less accurate
bool LDC::_set_reference_divider(uint8_t channel) {
    uint8_t divider;
    uint16_t value;
    uint16_t FIN_DIV, FREF_DIV;

    // pull the ^-12 and ^6 for pF and uH out to the front
    // to change it to ^-6 and ^-3
    _f_sensor[channel] = 1 / (2 * 3.14 * sqrt(_inductance[channel] * _capacitance[channel]) * pow(10, -3) * pow(10, -6));
    if (_f_sensor[channel] > 40000000) {
        return ERROR_FREQUENCY_TOO_LARGE;
    }
    if (_f_sensor[channel] > 8750000) {
        value = 0x2000;
    }
    else {
        value = 0x1000;
    }
    divider = 40000000 / (_f_sensor[channel] * 4);
    value |= divider;
    _ref_frequency[channel] = 40000000 / divider;
    LDC::I2C_write_16bit(SET_FREQ_REG_START + channel, value);
    return 0;
}

// stabilize time is Q * reference frequency / (16 * sensor frequency)
// the datasheet recommends a minimum of 10
// and adding a slight buffer to the calculation, here +4 was chosen
void LDC::_set_stabilize_time(uint8_t channel) {
    u16 value = 10;
    u16 settleTime = ((q_factor[channel] * _ref_frequency[channel]) / (16 * _f_sensor[channel])) + 4;
    if (settleTime > value) {
        value = settleTime;
    }
    LDC::I2C_write_16bit(SET_LC_STABILIZE_REG_START + channel, value);
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
bool LDC::_set_driver_current(uint8_t channel) {
    uint8_t value;
    if (Rp[channel] > Rp_lookup_table[0].kohms) {
        return -1;
    }
    for (int i = 0; i < RP_TABLE_ELEMENTS; i++) {
        if (resistance[channel] <= Rp_lookup_table[i].kohms) {
            value = i;
        }
    }
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
    uint16_t value = 0x8208;  
    // TODO: ADD SUPPORT FOR LDC1614 WITH TURNING ON MORE CHANNELS
    if (F_Sensor < 1000000) {
        value |= 0b001;
    }
    else if (F_Sensor < 3300000) {
        value |= 0b100;
    }
    else if (F_Sensor < 10000000) {
        value |= 0b101;
    }
    else {
        value |= 0b111;
    }
    LDC::I2C_write_16bit(MUL_CONFIG_REG, value);
}

void LDC::_LDC_config(uint8_t channel) {
    uint16_t value;
    switch (channel) {
    case CHANNEL_0: 
        *value = 0x0000;
    case CHANNEL_1: 
        *value = 0x7000;
        break;
    case CHANNEL_2: 
        *value = 0x8000;
        break;
    case CHANNEL_3: 
        *value = 0xc000;
        break;
    }
    value |= 0x1401;
    LDC::I2C_write_16bit(SENSOR_CONFIG_REG, value);
}

int32_t LDC::I2C_write_16bit(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(_IIC_ADDR);
    Wire.write(reg);

    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)value);
    return Wire.endTransmission();
}

void LDC::I2C_read_16bit(uint8_t start_reg, uint16_t* value) {
    u8 val = 0;
    *value = 0;
    Wire.beginTransmission(_IIC_ADDR);
    Wire.write(start_reg);
    Wire.endTransmission(false);

    Wire.requestFrom(_IIC_ADDR, sizeof(uint16_t));
    while (sizeof(uint16_t) != Wire.available());
    val = Wire.read();
    *value |= (uint16_t)val << 8;
    val = Wire.read();
    *value |= val;
}