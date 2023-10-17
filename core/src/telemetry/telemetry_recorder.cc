///////////////////////////////////////////////////////////////////////////////
///
/// \brief TelemetryRecorder Implementation.
///
//////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <cmath>
#include <iomanip>
#include <fstream>


#include "jiminy/core/io/file_device.h"
#include "jiminy/core/telemetry/telemetry_data.h"
#include "jiminy/core/constants.h"

#include "jiminy/core/telemetry/telemetry_recorder.h"


namespace jiminy
{
    TelemetryRecorder::~TelemetryRecorder(void)
    {
        if (!flows_.empty())
        {
            flows_.back().close();
        }
    }

    hresult_t TelemetryRecorder::initialize(TelemetryData       * telemetryData,
                                            float64_t     const & timeUnit)
    {
        hresult_t returnCode = hresult_t::SUCCESS;

        if (isInitialized_)
        {
            PRINT_ERROR("TelemetryRecorder already initialized.");
            returnCode = hresult_t::ERROR_INIT_FAILED;
        }
        // Log the time unit as constant
        timeUnitInv_ = 1.0 / timeUnit;
        std::ostringstream timeUnitStr;
        int precision = - static_cast<int>(std::ceil(std::log10(STEPPER_MIN_TIMESTEP)));
        timeUnitStr << std::scientific << std::setprecision(precision) << timeUnit;
        telemetryData->registerConstant(TIME_UNIT, timeUnitStr.str());

        std::vector<char_t> header;
        if (returnCode == hresult_t::SUCCESS)
        {
            // Clear the MemoryDevice buffer
            flows_.clear();

            // Get telemetry data infos
            integersRegistry_ = telemetryData->getRegistry<int64_t>();
            integerSectionSize_ = sizeof(int64_t) * integersRegistry_->size();
            floatsRegistry_ = telemetryData->getRegistry<float64_t>();
            floatSectionSize_ = sizeof(float64_t) * floatsRegistry_->size();
            recordedBytesDataLine_ = integerSectionSize_ + floatSectionSize_
                                   + static_cast<int64_t>(START_LINE_TOKEN.size() + sizeof(int64_t));  // int64_t for Global.Time

            // Get the header
            telemetryData->formatHeader(header);
            headerSize_ = static_cast<int64_t>(header.size());

            // Create a new MemoryDevice and open it
            returnCode = createNewChunk();
        }

        // Write the Header
        if (returnCode == hresult_t::SUCCESS)
        {
            returnCode = flows_[0].write(header);
        }

        if (returnCode == hresult_t::SUCCESS)
        {
            recordedBytes_ = headerSize_;
            isInitialized_ = true;
        }

        return returnCode;
    }

    float64_t TelemetryRecorder::getMaximumLogTime(float64_t const & timeUnit)
    {
        return static_cast<float64_t>(std::numeric_limits<int64_t>::max()) * timeUnit;
    }

    float64_t TelemetryRecorder::getMaximumLogTime(void) const
    {
        return getMaximumLogTime(1.0 / timeUnitInv_);
    }

    bool_t const & TelemetryRecorder::getIsInitialized(void)
    {
        return isInitialized_;
    }

    void TelemetryRecorder::reset(void)
    {
        // Close the current MemoryDevice, if any and if it was opened
        if (!flows_.empty())
        {
            flows_.back().close();
        }

        isInitialized_ = false;
    }

    hresult_t TelemetryRecorder::createNewChunk()
    {
        hresult_t returnCode = hresult_t::SUCCESS;

        // Close the current MemoryDevice, if any and if it was opened
        if (!flows_.empty())
        {
            flows_.back().close();
        }

        /* Create a new chunk.
           The size of the first chunk is chosen to be large enough
           to contain the whole header (with constants). Doing this
           does not really affect the performances since it is written
           only once, at init of the simulation. The optimized buffer
           size is used for the log data. */
        int64_t isHeaderThere = flows_.empty();
        int64_t maxBufferSize = std::max(TELEMETRY_MIN_BUFFER_SIZE, isHeaderThere * headerSize_);
        int64_t maxRecordedDataLines = (maxBufferSize - isHeaderThere * headerSize_) / recordedBytesDataLine_;
        recordedBytesLimits_ = isHeaderThere * headerSize_ + maxRecordedDataLines * recordedBytesDataLine_;
        flows_.emplace_back(recordedBytesLimits_);
        returnCode = flows_.back().open(openMode_t::READ_WRITE);

        if (returnCode == hresult_t::SUCCESS)
        {
            recordedBytes_ = 0;
        }

        return returnCode;
    }

    hresult_t TelemetryRecorder::flushDataSnapshot(float64_t const & timestamp)
    {
        hresult_t returnCode = hresult_t::SUCCESS;

        if (recordedBytes_ == recordedBytesLimits_)
        {
            returnCode = createNewChunk();
        }

        if (returnCode == hresult_t::SUCCESS)
        {
            // Write new line token
            flows_.back().write(START_LINE_TOKEN);

            // Write time
            flows_.back().write(static_cast<int64_t>(std::round(timestamp * timeUnitInv_)));

            // Write data, integers first
            for (std::pair<std::string, int64_t> const & keyValue : *integersRegistry_)
            {
                flows_.back().write(keyValue.second);
            }

            // Write data, floats last
            for (std::pair<std::string, float64_t> const & keyValue : *floatsRegistry_)
            {
                flows_.back().write(keyValue.second);
            }

            // Update internal counter
            recordedBytes_ += recordedBytesDataLine_;
        }

        return returnCode;
    }

    hresult_t TelemetryRecorder::writeLog(std::string const & filename)
    {
        FileDevice myFile(filename);
        myFile.open(openMode_t::WRITE_ONLY | openMode_t::TRUNCATE);
        if (myFile.isOpen())
        {
            for (auto & flow : flows_)
            {
                int64_t const pos_old = flow.pos();
                flow.seek(0);

                std::vector<uint8_t> bufferChunk;
                bufferChunk.resize(static_cast<std::size_t>(pos_old));
                flow.read(bufferChunk);
                myFile.write(bufferChunk);

                flow.seek(pos_old);
            }

            myFile.close();
        }
        else
        {
            PRINT_ERROR("Impossible to create the log file. Check if root folder exists and if you have writing permissions.");
            return hresult_t::ERROR_BAD_INPUT;
        }
        return hresult_t::SUCCESS;
    }

    hresult_t parseLogDataRaw(std::vector<AbstractIODevice *> & flows,
                              int64_t const & integerSectionSize,
                              int64_t const & floatSectionSize,
                              int64_t const & headerSize,
                              logData_t & logData)
    {
        // Clear everything that may be stored
        logData = {};

        // Set data structure
        Eigen::Index const numInt =
            static_cast<Eigen::Index>(integerSectionSize / sizeof(int64_t));
        Eigen::Index const numFloat =
            static_cast<Eigen::Index>(floatSectionSize / sizeof(float64_t));
        logData.intData.resize(numInt, 0);
        logData.floatData.resize(numFloat, 0);

        // Process the provided data
        Eigen::Index timeIdx = 0;
        if (!flows.empty())
        {
            bool_t isReadingHeaderDone = false;
            for (auto & flow : flows)
            {
                // Save the cursor position and move it to the beginning
                int64_t const pos_old = flow->pos();
                flow->seek(0);

                // Dealing with version flag, constants, and variable names
                if (!isReadingHeaderDone)
                {
                    // Read version flag and check if valid
                    int32_t version;
                    flow->readData(&version, sizeof(int32_t));
                    if (version != TELEMETRY_VERSION)
                    {
                        PRINT_ERROR("Log telemetry version not supported. Impossible to read log.");
                        return hresult_t::ERROR_BAD_INPUT;
                    }
                    logData.version = version;

                    // Read the rest of the header
                    std::vector<char_t> headerCharBuffer;
                    headerCharBuffer.resize(static_cast<std::size_t>(headerSize - flow->pos()));
                    flow->read(headerCharBuffer);

                    // Parse constants
                    bool_t isLastConstant = false;
                    auto posHeaderIt = headerCharBuffer.begin();
                    posHeaderIt += START_CONSTANTS.size() + 1 + START_LINE_TOKEN.size();  // Skip tokens
                    while (true)
                    {
                        // Find position of the next constant
                        auto posHeaderNextIt = std::search(
                            posHeaderIt,
                            headerCharBuffer.end(),
                            START_LINE_TOKEN.begin(),
                            START_LINE_TOKEN.end());
                        isLastConstant = posHeaderNextIt == headerCharBuffer.end();
                        if (isLastConstant)
                        {
                            posHeaderNextIt = std::search(
                                posHeaderIt,
                                headerCharBuffer.end(),
                                START_COLUMNS.begin(),
                                START_COLUMNS.end());
                        }

                        // Split key and value
                        auto posDelimiterIt = std::search(
                            posHeaderIt,
                            posHeaderNextIt,
                            TELEMETRY_CONSTANT_DELIMITER.begin(),
                            TELEMETRY_CONSTANT_DELIMITER.end());
                        std::string const key(posHeaderIt, posDelimiterIt);
                        std::string const value(
                            posDelimiterIt + TELEMETRY_CONSTANT_DELIMITER.size(),
                            posHeaderNextIt - 1);  // Last char is '\0'
                        logData.constants.emplace_back(key, value);

                        // Stop if it was the last one
                        if (isLastConstant)
                        {
                            posHeaderIt = posHeaderNextIt + START_COLUMNS.size() + 1;  // Skip last '\0'
                            break;
                        }
                        posHeaderIt = posHeaderNextIt + START_LINE_TOKEN.size();
                    }

                    // Parse variable names
                    char_t const * pHeader = &(*posHeaderIt);
                    std::size_t posHeader = 0;
                    while (true)
                    {
                        // std::string constructor automatically reads till next '\0'
                        std::string const fieldname = std::string(pHeader + posHeader);
                        if (fieldname == START_DATA)
                        {
                            break;
                        }
                        posHeader += fieldname.size() + 1;  // Skip last '\0'
                        logData.fieldnames.push_back(fieldname);
                    }

                    isReadingHeaderDone = true;
                }

                // Look for timeUnit constant - if not found, use default time unit
                float64_t timeUnit = STEPPER_MIN_TIMESTEP;
                for (auto const & [key, value] : logData.constants)
                {
                    if (key == TIME_UNIT)
                    {
                        std::istringstream totalSString(value);
                        totalSString >> timeUnit;
                        break;
                    }
                }
                logData.timeUnit = timeUnit;

                // Allocate memory
                int64_t const startLineTokenSize = static_cast<int64_t>(START_LINE_TOKEN.size());
                int64_t const recordedBytesDataLine =
                    startLineTokenSize + sizeof(uint64_t) + integerSectionSize + floatSectionSize;
                Eigen::Index const numData = timeIdx + flow->bytesAvailable() / recordedBytesDataLine;
                logData.timestamps.conservativeResize(numData);
                logData.intData.conservativeResize(Eigen::NoChange, numData);
                logData.floatData.conservativeResize(Eigen::NoChange, numData);

                // Read all available data lines: [token, time, integers, floats]
                char_t startLineTokenBuffer;
                while (flow->bytesAvailable() > 0)
                {
                    // Check if actual data are still available.
                    // It is necessary because a pre-allocated memory may not be full.
                    flow->readData(&startLineTokenBuffer, 1);
                    if (startLineTokenBuffer != START_LINE_TOKEN[0])
                    {
                        break;
                    }
                    flow->seek(flow->pos() + startLineTokenSize - 1);

                    // Read data line
                    flow->readData(&logData.timestamps[timeIdx], sizeof(int64_t));
                    flow->readData(logData.intData.col(timeIdx).data(), integerSectionSize);
                    flow->readData(logData.floatData.col(timeIdx).data(), floatSectionSize);

                    // Increment timestamp counter
                    ++timeIdx;
                }

                // Restore the cursor position
                flow->seek(pos_old);
            }
        }

        /* Remove initialized data if any, which would occur if a
           pre-allocated memory buffer was not full. */
        logData.timestamps.conservativeResize(timeIdx);
        logData.intData.conservativeResize(Eigen::NoChange, timeIdx);
        logData.floatData.conservativeResize(Eigen::NoChange, timeIdx);

        return hresult_t::SUCCESS;
    }

    hresult_t TelemetryRecorder::getLog(logData_t & logData)
    {
        std::vector<AbstractIODevice *> abstractFlows_;
        for (MemoryDevice & device: flows_)
        {
            abstractFlows_.push_back(&device);
        }

        return parseLogDataRaw(
            abstractFlows_, integerSectionSize_, floatSectionSize_, headerSize_, logData);
    }

    hresult_t TelemetryRecorder::readLog(std::string const & filename,
                                         logData_t         & logData)
    {
        int64_t integerSectionSize;
        int64_t floatSectionSize;
        int64_t headerSize;

        std::ifstream file = std::ifstream(
            filename, std::ios::in | std::ifstream::binary);

        if (file.is_open())
        {
            // Skip the version flag
            int32_t header_version_length = sizeof(int32_t);
            file.seekg(header_version_length);

            std::vector<std::string> headerBuffer;
            std::string subHeaderBuffer;

            // Reach the beginning of the constants
            while (std::getline(file, subHeaderBuffer, '\0').good() &&
                   subHeaderBuffer != START_CONSTANTS)
            {
                // Empty on purpose
            }

            // Get all the logged constants
            while (std::getline(file, subHeaderBuffer, '\0').good() &&
                   subHeaderBuffer != START_COLUMNS)
            {
                headerBuffer.push_back(subHeaderBuffer);
            }

            // Get the names of the logged variables
            while (std::getline(file, subHeaderBuffer, '\0').good() &&
                   subHeaderBuffer != START_DATA)
            {
                // Do nothing
            }

            // Make sure the log file is not corrupted
            if (!file.good())
            {
                PRINT_ERROR("Corrupted log file.");
                return hresult_t::ERROR_BAD_INPUT;
            }

            // Extract the number of integers and floats from the list of logged constants
            std::string const & headerNumIntEntries = headerBuffer[headerBuffer.size() - 2];
            int64_t delimiter = headerNumIntEntries.find(TELEMETRY_CONSTANT_DELIMITER);
            int32_t const NumIntEntries = std::stoi(headerNumIntEntries.substr(delimiter + 1));
            std::string const & headerNumFloatEntries = headerBuffer[headerBuffer.size() - 1];
            delimiter = headerNumFloatEntries.find(TELEMETRY_CONSTANT_DELIMITER);
            int32_t const NumFloatEntries = std::stoi(headerNumFloatEntries.substr(delimiter + 1));

            // Deduce the parameters required to parse the whole binary log file
            integerSectionSize = (NumIntEntries - 1) * sizeof(int64_t);  // Remove Global.Time
            floatSectionSize = NumFloatEntries * sizeof(float64_t);
            headerSize = static_cast<int64_t>(file.tellg());  // Last '\0' is included

            // Close the file
            file.close();
        }
        else
        {
            PRINT_ERROR("Impossible to open the log file. Check that the file exists and "
                        "that you have reading permissions.");
            return hresult_t::ERROR_BAD_INPUT;
        }

        FileDevice device(filename);
        device.open(openMode_t::READ_ONLY);
        std::vector<AbstractIODevice *> flows;
        flows.push_back(&device);
        return parseLogDataRaw(
            flows, integerSectionSize, floatSectionSize, headerSize, logData);
    }
}