#include "jiminy/core/stepper/runge_kutta_dopri_stepper.h"

namespace jiminy
{
    RungeKuttaDOPRIStepper::RungeKuttaDOPRIStepper(systemDynamics const & f,
                                                   std::vector<Robot const *> const & robots,
                                                   float64_t const & tolRel,
                                                   float64_t const & tolAbs):
    AbstractRungeKuttaStepper(f, robots, DOPRI::A, DOPRI::b, DOPRI::c, true),
    tolRel_(tolRel),
    tolAbs_(tolAbs),
    scale_(robots),
    otherSolution_(robots),
    error_(robots)
    {
        // Empty on purpose
    }

    bool_t RungeKuttaDOPRIStepper::adjustStep(state_t   const & initialState,
                                              state_t   const & solution,
                                              float64_t       & dt)
    {
        float64_t const error = computeError(initialState, solution, dt);
        return adjustStepImpl(error, dt);
    }

    float64_t RungeKuttaDOPRIStepper::computeError(state_t const & initialState,
                                                   state_t const & solution,
                                                   float64_t const & dt)
    {
        // Compute alternative solution
        stateIncrement_.setZero();
        for (std::size_t i = 0; i < ki_.size(); ++i)
        {
            stateIncrement_.sumInPlace(ki_[i], dt * DOPRI::e[i]);
        }
        initialState.sum(stateIncrement_, otherSolution_);

        // Evaluate error between both states to adjust step
        solution.difference(otherSolution_, error_);

        // Compute absolute and relative element-wise maximum error
        float64_t errorAbsNorm = INF;
        float64_t errorRelNorm = INF;
        if (tolAbs_ > EPS)
        {
            errorAbsNorm = error_.normInf() / tolAbs_;
        }
        if (tolRel_ > EPS)
        {
            otherSolution_.setZero();
            solution.difference(otherSolution_, scale_);
            error_ /= scale_;
            errorRelNorm = error_.normInf() / tolRel_;
        }

        // Return the smallest error between absolute and relative
        return std::min(errorAbsNorm, errorRelNorm);
    }

    bool_t RungeKuttaDOPRIStepper::adjustStepImpl(float64_t const & error,
                                                  float64_t       & dt)
    {
        // Make sure the error is defined, otherwise rely on a simple heuristic
        if (std::isnan(error))
        {
            dt *= 0.1;
            return false;
        }

        // Adjustment algorithm from boost implementation
        if (error < 1.0)
        {
            // Only increase if error is sufficiently small
            if (error < std::pow(DOPRI::SAFETY, DOPRI::STEPPER_ORDER))
            {
                // Prevent numeric rounding error when close to zero
                float64_t const newError = std::max(
                    error, std::pow(DOPRI::MAX_FACTOR / DOPRI::SAFETY, -DOPRI::STEPPER_ORDER));
                dt *= DOPRI::SAFETY * std::pow(newError, -1.0 / DOPRI::STEPPER_ORDER);
            }
            return true;
        }
        else
        {
            dt *= std::max(DOPRI::SAFETY * std::pow(error, -1.0 / (DOPRI::STEPPER_ORDER - 2)),
                           DOPRI::MIN_FACTOR);
            return false;
        }
    }
}