#ifndef RK809_PMIC_H__
#define RK809_PMIC_H__

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "device_model.h"

/**
 * @file rk809_pmic.h
 * @brief RK809/RK817 PMIC support for Miyoo Flip
 * 
 * Integrated fuel gauge, battery monitoring, and power management
 * Based on Phase 5 documentation
 */

// I2C bus and address
#define RK809_I2C_BUS "/dev/i2c-0"
#define RK809_I2C_ADDR 0x20

// RK809 registers
#define RK809_REG_SOC 0xA4           // State of Charge (%)
#define RK809_REG_VOLTAGE_H 0xA6     // Voltage high byte
#define RK809_REG_VOLTAGE_L 0xA7     // Voltage low byte
#define RK809_REG_CURRENT_H 0xA8     // Current high byte
#define RK809_REG_CURRENT_L 0xA9     // Current low byte
#define RK809_REG_TEMP 0xAA          // Temperature
#define RK809_REG_CHRG_STS 0xAB      // Charge status

// Battery thresholds
#define BATTERY_VOLTAGE_MIN 3000     // mV (empty)
#define BATTERY_VOLTAGE_MAX 4200     // mV (full)
#define BATTERY_LOW_THRESHOLD 10     // %
#define BATTERY_CRITICAL_THRESHOLD 3 // %

// Battery state structure
typedef struct {
    uint8_t percent;              // 0-100%
    uint16_t voltage_mv;          // Millivolts
    int16_t current_ma;           // Milliamps (negative = discharge)
    int8_t temperature_c;         // Celsius
    bool charging;                // Charging state
    bool ac_connected;            // AC adapter connected
    uint32_t time_to_empty_min;   // Minutes remaining
    uint32_t time_to_full_min;    // Minutes to full charge
} battery_state_t;

// Global battery state
static battery_state_t battery_state = {
    .percent = 50,
    .voltage_mv = 3700,
    .current_ma = 0,
    .temperature_c = 25,
    .charging = false,
    .ac_connected = false,
    .time_to_empty_min = 0,
    .time_to_full_min = 0
};

// I2C file descriptor
static int rk809_i2c_fd = -1;

/**
 * @brief Initialize RK809 PMIC
 * @return true if successful
 */
static inline bool rk809_init(void)
{
    if (!is_miyoo_flip()) {
        return false;
    }
    
    rk809_i2c_fd = open(RK809_I2C_BUS, O_RDWR);
    if (rk809_i2c_fd < 0) {
        return false;
    }
    
    if (ioctl(rk809_i2c_fd, I2C_SLAVE, RK809_I2C_ADDR) < 0) {
        close(rk809_i2c_fd);
        rk809_i2c_fd = -1;
        return false;
    }
    
    return true;
}

/**
 * @brief Close RK809 PMIC
 */
static inline void rk809_close(void)
{
    if (rk809_i2c_fd >= 0) {
        close(rk809_i2c_fd);
        rk809_i2c_fd = -1;
    }
}

/**
 * @brief Read register from RK809
 * @param reg Register address
 * @param value Output value
 * @return true if successful
 */
static inline bool rk809_read_reg(uint8_t reg, uint8_t *value)
{
    if (rk809_i2c_fd < 0) {
        return false;
    }
    
    if (write(rk809_i2c_fd, &reg, 1) != 1) {
        return false;
    }
    
    if (read(rk809_i2c_fd, value, 1) != 1) {
        return false;
    }
    
    return true;
}

/**
 * @brief Read 16-bit register from RK809
 * @param reg_h High byte register
 * @param reg_l Low byte register
 * @param value Output value
 * @return true if successful
 */
static inline bool rk809_read_reg16(uint8_t reg_h, uint8_t reg_l, uint16_t *value)
{
    uint8_t high, low;
    
    if (!rk809_read_reg(reg_h, &high)) {
        return false;
    }
    
    if (!rk809_read_reg(reg_l, &low)) {
        return false;
    }
    
    *value = ((uint16_t)high << 8) | low;
    return true;
}

/**
 * @brief Update battery state from RK809
 * @return true if successful
 */
static inline bool rk809_update_battery_state(void)
{
    if (rk809_i2c_fd < 0) {
        return false;
    }
    
    uint8_t soc, temp, chrg_sts;
    uint16_t voltage, current;
    
    // Read State of Charge
    if (rk809_read_reg(RK809_REG_SOC, &soc)) {
        battery_state.percent = soc;
    }
    
    // Read Voltage (11-bit, 0.5mV resolution)
    if (rk809_read_reg16(RK809_REG_VOLTAGE_H, RK809_REG_VOLTAGE_L, &voltage)) {
        battery_state.voltage_mv = (voltage & 0x7FF) / 2;
    }
    
    // Read Current (signed 16-bit, 1mA resolution)
    if (rk809_read_reg16(RK809_REG_CURRENT_H, RK809_REG_CURRENT_L, &current)) {
        battery_state.current_ma = (int16_t)current;
    }
    
    // Read Temperature
    if (rk809_read_reg(RK809_REG_TEMP, &temp)) {
        battery_state.temperature_c = (int8_t)temp;
    }
    
    // Read Charge Status
    if (rk809_read_reg(RK809_REG_CHRG_STS, &chrg_sts)) {
        battery_state.charging = (chrg_sts & 0x01) != 0;
        battery_state.ac_connected = (chrg_sts & 0x02) != 0;
    }
    
    // Calculate time to empty (simple estimation)
    if (battery_state.current_ma < 0 && battery_state.percent > 0) {
        // Assume 3000mAh capacity
        uint32_t capacity_mah = 3000 * battery_state.percent / 100;
        uint32_t discharge_ma = -battery_state.current_ma;
        if (discharge_ma > 0) {
            battery_state.time_to_empty_min = (capacity_mah * 60) / discharge_ma;
        }
    } else {
        battery_state.time_to_empty_min = 0;
    }
    
    // Calculate time to full (simple estimation)
    if (battery_state.charging && battery_state.current_ma > 0) {
        uint32_t remaining_capacity = 3000 * (100 - battery_state.percent) / 100;
        uint32_t charge_ma = battery_state.current_ma;
        if (charge_ma > 0) {
            battery_state.time_to_full_min = (remaining_capacity * 60) / charge_ma;
        }
    } else {
        battery_state.time_to_full_min = 0;
    }
    
    return true;
}

/**
 * @brief Get battery percentage
 * @return Battery percentage (0-100)
 */
static inline uint8_t rk809_get_battery_percent(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
        return battery_state.percent;
    }
    return 50; // Default for non-Flip devices
}

/**
 * @brief Get battery state
 * @return Current battery state
 */
static inline battery_state_t rk809_get_battery_state(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
    }
    return battery_state;
}

/**
 * @brief Check if battery is charging
 * @return true if charging
 */
static inline bool rk809_is_charging(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
        return battery_state.charging;
    }
    return false;
}

/**
 * @brief Check if battery is low
 * @return true if below low threshold
 */
static inline bool rk809_is_battery_low(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
        return battery_state.percent <= BATTERY_LOW_THRESHOLD;
    }
    return false;
}

/**
 * @brief Check if battery is critical
 * @return true if below critical threshold
 */
static inline bool rk809_is_battery_critical(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
        return battery_state.percent <= BATTERY_CRITICAL_THRESHOLD;
    }
    return false;
}

/**
 * @brief Get battery time remaining
 * @return Minutes remaining (0 if charging or unknown)
 */
static inline uint32_t rk809_get_time_remaining(void)
{
    if (is_miyoo_flip()) {
        rk809_update_battery_state();
        return battery_state.time_to_empty_min;
    }
    return 0;
}

#endif // RK809_PMIC_H__
