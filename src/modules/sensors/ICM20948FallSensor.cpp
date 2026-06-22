/*
 * ICM20948FallSensor Implementation
 * Adapter for SparkFun ICM-20948 → FallSensorInterface
 */

#include "ICM20948FallSensor.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<ICM_20948.h>)

#include "configuration.h"
#include "main.h"

bool ICM20948FallSensor::init()
{
    Wire.begin(I2C_SDA, I2C_SCL);

    bool success = false;
    // Probe 0x69 first (default AD0=high)
    if (icm.begin(Wire, true) == ICM_20948_Stat_Ok) {
        success = true;
    }
    // Probe 0x68 (AD0=low)
    else if (icm.begin(Wire, false) == ICM_20948_Stat_Ok) {
        success = true;
    }

    if (!success) {
        LOG_WARN("ICM20948FallSensor: Sensor not found at 0x69 or 0x68");
        return false;
    }

    // SW reset to ensure clean startup
    if (icm.swReset() != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Reset failed");
        return false;
    }
    delay(100);

    // Wake up sensor
    if (icm.sleep(false) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Wakeup failed");
        return false;
    }

    // Set full power mode
    if (icm.lowPower(false) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Power mode config failed");
        return false;
    }

    // Set sample mode to continuous
    if (icm.setSampleMode((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), ICM_20948_Sample_Mode_Continuous) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Continuous mode set failed");
        return false;
    }

    // Set full scale ranges: Accel=±8g (gpm8), Gyro=±500dps (dps500)
    ICM_20948_fss_t fss;
    fss.a = gpm8;
    fss.g = dps500;
    if (icm.setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), fss) != ICM_20948_Stat_Ok) {
        LOG_WARN("ICM20948FallSensor: Full scale range setup failed");
        return false;
    }

    LOG_INFO("ICM20948FallSensor: Initialized (±8g, ±500dps)");
    return true;
}

void ICM20948FallSensor::updateSensorData()
{
    // Rate limit I2C transactions to once per 10ms (called by readAccel and readGyro sequentially)
    uint32_t now = millis();
    if (now - lastReadMs >= 10 || lastReadMs == 0) {
        icm.getAGMT();
        lastReadMs = now;
    }
}

bool ICM20948FallSensor::readAccel(SensorVec3 &out)
{
    updateSensorData();
    // Convert SparkFun library's milli-g (mg) to m/s²: (val / 1000) * 9.80665
    out.x = (icm.accX() / 1000.0f) * 9.80665f;
    out.y = (icm.accY() / 1000.0f) * 9.80665f;
    out.z = (icm.accZ() / 1000.0f) * 9.80665f;
    return true;
}

bool ICM20948FallSensor::readGyro(SensorVec3 &out)
{
    updateSensorData();
    // Convert SparkFun library's degrees per second (dps) to rad/s: val * (PI / 180)
    out.x = icm.gyrX() * (PI / 180.0f);
    out.y = icm.gyrY() * (PI / 180.0f);
    out.z = icm.gyrZ() * (PI / 180.0f);
    return true;
}

#endif // ICM_20948 available
