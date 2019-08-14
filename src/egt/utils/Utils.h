//
// Created by Gerardin, Antoine D. (Assoc) on 12/19/18.
//

#ifndef PYRAMIDBUILDING_UTILS_H
#define PYRAMIDBUILDING_UTILS_H

#include <iostream>
#include <algorithm>
#include <glog/logging.h>
#include <iomanip>
#include <egt/api/DerivedSegmentationParams.h>

namespace egt {

    std::string getFileExtension(const std::string &path) {
        std::string extension = path.substr(path.find_last_of('.') + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;
    }

    template<class T>
    void printArray(std::string title, T *data, uint32_t w, uint32_t h) {
        std::ostringstream oss;

        oss << title << std::endl;
        for (size_t i = 0; i < h; ++i) {
            for (size_t j = 0; j < w; ++j) {
                oss << std::setw(6) << (data[i * w + j]) << " ";
            }
            oss << std::endl;
        }
        oss << std::endl;

        VLOG(3) << oss.str();
    }


    template<class T>
    void printArray(std::string title, T *data, uint32_t w, uint32_t h, int space) {
        std::ostringstream oss;

        oss << title << std::endl;
        for (size_t i = 0; i < h; ++i) {
            for (size_t j = 0; j < w; ++j) {
                oss << std::setw(space) << (data[i * w + j]) << " ";
            }
            oss << std::endl;
        }
        oss << std::endl;

        VLOG(3) << oss.str();
    }


    template<class T>
    void printBoolArray(std::string title, T *data, uint32_t w, uint32_t h) {
        std::ostringstream oss;

        oss << title << std::endl;
        for (size_t i = 0; i < h; ++i) {
            for (size_t j = 0; j < w; ++j) {
                oss << std::setw(1) << ((data[i * w + j] > 0) ? 1 : 0) << " ";
            }
            oss << std::endl;
        }
        oss << std::endl;

        VLOG(0) << oss.str();
    }

    template <class T>
    bool computeKeepHoleCriteria(uint64_t area, T meanIntensity, SegmentationOptions *segmentationOptions,
                                 DerivedSegmentationParams<T> &segmentationParams) {
        bool keepHole = (area > segmentationOptions->MIN_HOLE_SIZE && area < segmentationOptions->MAX_HOLE_SIZE);
        if (segmentationOptions->KEEP_HOLES_WITH_JOIN_OPERATOR == JoinOperator::AND) {
            keepHole = keepHole && (meanIntensity > segmentationParams.minPixelIntensityValue &&
                                meanIntensity < segmentationParams.maxPixelIntensityValue);
        }
        else if (segmentationOptions->KEEP_HOLES_WITH_JOIN_OPERATOR == JoinOperator::OR) {
            keepHole = keepHole || (meanIntensity > segmentationParams.minPixelIntensityValue &&
                                meanIntensity < segmentationParams.maxPixelIntensityValue);
        }

        return keepHole;
    }
}

#endif //PYRAMIDBUILDING_UTILS_H
