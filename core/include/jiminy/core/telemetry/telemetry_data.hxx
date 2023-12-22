#ifndef JIMINY_TELEMETRY_DATA_HXX
#define JIMINY_TELEMETRY_DATA_HXX

#include <string>

#include "jiminy/core/exceptions.h"


namespace jiminy
{
    template<typename T>
    hresult_t TelemetryData::registerVariable(const std::string & name, T *& positionInBuffer)
    {
        // Get the right registry
        std::deque<std::pair<std::string, T>> * registry = getRegistry<T>();

        // Check if already in memory
        auto variableIt = std::find_if(registry->begin(),
                                       registry->end(),
                                       [&name](const std::pair<std::string, T> & element) -> bool_t
                                       { return element.first == name; });
        if (variableIt != registry->end())
        {
            positionInBuffer = &(variableIt->second);
            return hresult_t::SUCCESS;
        }

        // Check if registration is possible
        if (!isRegisteringAvailable_)
        {
            PRINT_ERROR("Entry not found and registration is not available.");
            return hresult_t::ERROR_GENERIC;
        }

        // Create new variable in registry
        registry->emplace_back(name, T{});
        positionInBuffer = &(registry->back().second);

        return hresult_t::SUCCESS;
    }
}

#endif  // JIMINY_TELEMETRY_DATA_HXX
