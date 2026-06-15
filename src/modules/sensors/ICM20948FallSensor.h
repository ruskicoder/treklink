/*
 * ICM20948FallSensor — FallSensorInterface adapter for SparkFun ICM-20948
 *
 * Used by TrekLink v2.0 (custom PCB with ICM-20948 IMU on I2C).
 */

#pragma once

#include "configuration.h"

#if !defined(ARCH_STM32WL) && !MESHTASTIC_EXCLUDE_I2C && __has_include(<ICM_20948.h>)

#include "../FallSensorInterface.h"
#include <ICM_20948.h>
#include <Wire.h>

class ICM20948FallSensor : public FallSensorInterface
{
  public:
    ICM20948FallSensor() = default;

    bool init() override;
    bool readAccel(SensorVec3 &out) override;
    bool readGyro(SensorVec3 &out) override;
    const char *sensorName() const override { return "ICM20948"; }

  private:
    ICM_20948_I2C icm;
    uint32_t lastReadMs = 0;
    void updateSensorData();
};

#endif // ICM_20948 available
