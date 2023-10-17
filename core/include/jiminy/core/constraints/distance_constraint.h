#ifndef JIMINY_DISTANCE_CONSTRAINT_H
#define JIMINY_DISTANCE_CONSTRAINT_H

#include <memory>

#include "jiminy/core/types.h"
#include "jiminy/core/constraints/abstract_constraint.h"


namespace jiminy
{
    class Model;

    class DistanceConstraint : public AbstractConstraintTpl<DistanceConstraint>
    {

    public:
        ///////////////////////////////////////////////////////////////////////////////////////////////
        /// \brief      Forbid the copy of the class
        ///////////////////////////////////////////////////////////////////////////////////////////////
        DistanceConstraint(DistanceConstraint const & abstractConstraint) = delete;
        DistanceConstraint & operator = (DistanceConstraint const & other) = delete;

        auto shared_from_this() { return shared_from(this); }

        DistanceConstraint(std::string const & firstFrameName,
                           std::string const & secondFrameName);
        virtual ~DistanceConstraint(void) = default;

        std::vector<std::string> const & getFramesNames(void) const;
        std::vector<frameIndex_t> const & getFramesIdx(void) const;

        hresult_t setReferenceDistance(float64_t const & distanceRef);
        float64_t const & getReferenceDistance(void) const;

        virtual hresult_t reset(vectorN_t const & q,
                                vectorN_t const & v) override final;

        virtual hresult_t computeJacobianAndDrift(vectorN_t const & q,
                                                  vectorN_t const & v) override final;

    private:
        std::vector<std::string> framesNames_;    ///< Names of the frames on which the constraint operates.
        std::vector<frameIndex_t> framesIdx_;     ///< Corresponding frames indices.
        float64_t distanceRef_;                   ///< Reference Distance between the frames
        matrix6N_t firstFrameJacobian_;            ///< Stores first frame jacobian in world.
        matrix6N_t secondFrameJacobian_;           ///< Stores second frame jacobian in world.
    };
}

#endif //end of JIMINY_TRANSMISSION_CONSTRAINT_H