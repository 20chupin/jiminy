#ifndef JIMINY_BASIC_SENSORS_H
#define JIMINY_BASIC_SENSORS_H

#include "jiminy/core/robot/AbstractMotor.h"
#include "jiminy/core/robot/AbstractSensor.h"
#include "jiminy/core/Utilities.h"


namespace jiminy
{
    class Robot;

    class ImuSensor : public AbstractSensorTpl<ImuSensor>
    {
    public:
        ImuSensor(std::string const & name);
        ~ImuSensor(void) = default;

        auto shared_from_this() { return shared_from(this); }

        hresult_t initialize(std::string const & frameName);

        virtual hresult_t refreshProxies(void) final override;

        std::string const & getFrameName(void) const;

    private:
        virtual hresult_t set(float64_t                   const & t,
                              Eigen::Ref<vectorN_t const> const & q,
                              Eigen::Ref<vectorN_t const> const & v,
                              Eigen::Ref<vectorN_t const> const & a,
                              vectorN_t                   const & uMotor) final override;

    private:
        std::string frameName_;
        int32_t frameIdx_;
    };

    class ForceSensor : public AbstractSensorTpl<ForceSensor>
    {
    public:
        ForceSensor(std::string const & name);
        ~ForceSensor(void) = default;

        auto shared_from_this() { return shared_from(this); }

        hresult_t initialize(std::string const & frameName);

        virtual hresult_t refreshProxies(void) final override;

        std::string const & getFrameName(void) const;

    private:
        virtual hresult_t set(float64_t                   const & t,
                              Eigen::Ref<vectorN_t const> const & q,
                              Eigen::Ref<vectorN_t const> const & v,
                              Eigen::Ref<vectorN_t const> const & a,
                              vectorN_t                   const & uMotor) final override;

    private:
        std::string frameName_;
        int32_t frameIdx_;
    };

    class EncoderSensor : public AbstractSensorTpl<EncoderSensor>
    {
    public:
        EncoderSensor(std::string const & name);
        ~EncoderSensor(void) = default;

        auto shared_from_this() { return shared_from(this); }

        hresult_t initialize(std::string const & jointName);

        virtual hresult_t refreshProxies(void) final override;

        std::string const & getJointName(void) const;

    private:
        virtual hresult_t set(float64_t                   const & t,
                              Eigen::Ref<vectorN_t const> const & q,
                              Eigen::Ref<vectorN_t const> const & v,
                              Eigen::Ref<vectorN_t const> const & a,
                              vectorN_t                   const & uMotor) final override;

    private:
        std::string jointName_;
        int32_t jointPositionIdx_;
        int32_t jointVelocityIdx_;
    };

    class TorqueSensor : public AbstractSensorTpl<TorqueSensor>
    {
    public:
        TorqueSensor(std::string const & name);
        ~TorqueSensor(void) = default;

        auto shared_from_this() { return shared_from(this); }

        hresult_t initialize(std::string const & motorName);

        virtual hresult_t refreshProxies(void) final override;

        std::string const & getMotorName(void) const;

    private:
        virtual hresult_t set(float64_t                   const & t,
                              Eigen::Ref<vectorN_t const> const & q,
                              Eigen::Ref<vectorN_t const> const & v,
                              Eigen::Ref<vectorN_t const> const & a,
                              vectorN_t                   const & uMotor) final override;

    private:
        std::string motorName_;
        int32_t motorIdx_;
    };
}

#endif //end of JIMINY_BASIC_SENSORS_H