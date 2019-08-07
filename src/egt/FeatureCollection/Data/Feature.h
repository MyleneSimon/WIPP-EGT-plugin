//
// Created by gerardin on 7/24/19.
//

#ifndef NEWEGT_FEATURE_H
#define NEWEGT_FEATURE_H

// NIST-developed software is provided by NIST as a public service.
// You may use, copy and distribute copies of the  software in any  medium,
// provided that you keep intact this entire notice. You may improve,
// modify and create derivative works of the software or any portion of the
// software, and you may copy and distribute such modifications or works.
// Modified works should carry a notice stating that you changed the software
// and should note the date and nature of any such change. Please explicitly
// acknowledge the National Institute of Standards and Technology as the
// source of the software.
// NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY
// OF ANY KIND, EXPRESS, IMPLIED, IN FACT  OR ARISING BY OPERATION OF LAW,
// INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT AND DATA ACCURACY. NIST
// NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION  OF THE SOFTWARE WILL
// BE UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST
// DOES NOT WARRANT  OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE
// SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE
// CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
// You are solely responsible for determining the appropriateness of using
// and distributing the software and you assume  all risks associated with
// its use, including but not limited to the risks and costs of program
// errors, compliance  with applicable laws, damage to or loss of data,
// programs or equipment, and the unavailability or interruption of operation.
// This software is not intended to be used in any situation where a failure
// could cause risk of injury or damage to property. The software developed
// by NIST employees is not subject to copyright protection within
// the United States.

/// @file Feature.h
/// @author Alexandre Bardakoff - Timothy Blattner
/// @date  8/9/17
/// @brief Feature representation
#include <iostream>
#include <sstream>
#include <bitset>
#include <iomanip>
#include <FastImage/exception/FastImageException.h>
#include "BoundingBox.h"
#include <FastImage/FeatureCollection/tools/Vector2.h>

namespace egt {
/// \namespace fc FeatureCollection namespace

/**
  * @class Feature Feature.h <FastImage/FeatureCollection/Feature.h>
  *
  * @brief Define a Feature in a FeatureCollection context.
  *
  * @details A Feature is a set of connected pixels (4 or 8), corresponding to
  * a part of mask delimiting a region.
  * The representation of a feature is center on this feature, not on the pixel.
  * Each Feature will be constituted by:
  * _An Id,
  * _A BoundingBox delimiting the feature,
  * _A bitmask, telling for each pixel in the BoundingBox if it belongs to the feature
  * _The number of pixel in the feature.
  **/
    class Feature {
    public:
        /// \brief Feature constructor from a bounding box and a bitmask
        /// \param id Feature Id
        /// \param boundingBox Feature bounding box
        /// \param bitMask Feature bit mask to copy
        Feature(uint32_t id, const BoundingBox &boundingBox, uint32_t *bitMask)
                : _id(id), _boundingBox(boundingBox) {
            _nbElementsBitMask = (uint32_t) (ceil(
                    boundingBox.getHeight() * boundingBox.getWidth() / 32.));
            _bitMask = new uint32_t[_nbElementsBitMask];
            memcpy(this->_bitMask, bitMask, _nbElementsBitMask * sizeof(uint32_t));
        }


        Feature(const Feature &f) : _id(f.getId()), _boundingBox(f.getBoundingBox()) {
            _nbElementsBitMask = (uint32_t) (ceil(
                    f.getBoundingBox().getHeight() * f.getBoundingBox().getWidth() / 32.));
            _bitMask = new uint32_t[_nbElementsBitMask];
            memcpy(this->_bitMask, f.getBitMask(), _nbElementsBitMask * sizeof(uint32_t));
        }


       // Feature(Feature && ) = default;

        Feature& operator=(const Feature& that)
        {
            if (this != &that)
            {
                delete[] _bitMask;
                _id = that.getId();
                _boundingBox = that.getBoundingBox();
                _nbElementsBitMask = (uint32_t) (ceil(
                        that.getBoundingBox().getHeight() * that.getBoundingBox().getWidth() / 32.));
                _bitMask = new uint32_t[_nbElementsBitMask];
                memcpy(this->_bitMask, that.getBitMask(), _nbElementsBitMask * sizeof(uint32_t));
            }
            return *this;
        }

    public:
        /// \param id Feature id
        /// \param boundingBox Feature bounding box
        /// \param nbElementsBitMask Bit Mask size
        /// \param bitMask Bit mask
        Feature(uint32_t id,
                const BoundingBox &boundingBox,
                uint32_t nbElementsBitMask,
                uint32_t *bitMask)
                : _id(id),
                  _boundingBox(boundingBox),
                  _bitMask(bitMask),
                  _nbElementsBitMask(nbElementsBitMask) {}

        /// \brief Id getter
        /// \return Id
        uint32_t getId() const { return _id; }

        /// \brief Bounding box getter
        /// \return Bounding box
        const BoundingBox &getBoundingBox() const { return _boundingBox; }

        /// \brief Bit Mask getter
        /// \return Bit Mask
        uint32_t *getBitMask() const { return _bitMask; }

        /// \brief Get maximum coordinate in a specific dimension, used by the AABB
        /// tree
        /// \param dim Dimension to get the maximum coordinate
        /// \return Maximum coordinate in a specific dimension
        double getMaxCoord(int dim) {
            switch (dim) {
                case 0:
                    return this->getBoundingBox().getUpperLeftCol()
                           + this->getBoundingBox().getWidth();
                case 1:
                    return this->getBoundingBox().getUpperLeftRow()
                           + this->getBoundingBox().getHeight();
                default:
                    std::stringstream message;
                    message << "Feature ERROR: Dimension > 2 not handled.";
                    std::string m = message.str();
                    throw (fi::FastImageException(m));
            }
        };

        /// \brief Get minimum coordinate in a specific dimension, used by the AABB
        /// tree
        /// \param dim Dimension to get the minimum coordinate
        /// \return Minimum coordinate in a specific dimension
        double getMinCoord(int dim) {
            switch (dim) {
                case 0:
                    return this->getBoundingBox().getUpperLeftCol();
                case 1:
                    return this->getBoundingBox().getUpperLeftRow();
                default:
                    std::stringstream message;
                    message << "Feature ERROR: Dimension > 2 not handled.";
                    std::string m = message.str();
                    throw (fi::FastImageException(m));
            }
        };

        /// \brief Get the distance between a feature and a point
        /// \param point Point to calculate the distance from
        /// \return The distance between the feature and a point
        double getDistanceSqrTo(fc::Vector2<double> const &point) {
            double
                    distanceRow = point.getI() - (this->getBoundingBox().getMiddleRow()),
                    distanceCol = point.getJ() - (this->getBoundingBox().getMiddleCol());

            return (distanceRow * distanceRow) + distanceCol * distanceCol;
        };

        /// \brief Using global coordinates, test if a pixel is in the bounding box
        /// \param row row point to test
        /// \param col column point to test
        /// \return True if the point is in the Feature, else False
        bool contains(uint32_t row, uint32_t col) const {
            return
                    row >= this->_boundingBox.getUpperLeftRow() &&
                    row < this->_boundingBox.getUpperLeftRow()
                          + this->_boundingBox.getHeight() &&
                    col >= this->_boundingBox.getUpperLeftCol() &&
                    col < this->_boundingBox.getUpperLeftCol()
                          + this->_boundingBox.getWidth();
        };

        /// \brief Test if a Vector2<double> point is in a bounding box
        /// \param point Vector2<double> to test
        /// \return True if the point is in the Feature, else False
        bool contains(fc::Vector2<double> const &point) {
            return
                    point.getY() >= this->_boundingBox.getUpperLeftRow() &&
                    point.getY() < this->_boundingBox.getUpperLeftRow()
                                   + this->_boundingBox.getHeight() &&
                    point.getX() >= this->_boundingBox.getUpperLeftCol() &&
                    point.getX() < this->_boundingBox.getUpperLeftCol()
                                   + this->_boundingBox.getWidth();
        };

        /// \brief Using global coordinates, test if a pixel is in the bitmask
        /// \param globalRow row point to test
        /// \param globalCol column point to test
        /// \return True if the point is in the Feature, else False
        bool isImagePixelInBitMask(uint32_t globalRow, uint32_t globalCol) const {
            bool answer = false;
            if (contains(globalRow, globalCol)) {
                auto
                // Find the position in the bounding box
                        localRow =
                        (uint32_t) (globalRow - this->getBoundingBox().getUpperLeftRow()),
                        localCol =
                        (uint32_t) (globalCol - this->getBoundingBox().getUpperLeftCol());
                        answer= isBitSet(localRow, localCol);
            }
            return answer;
        }


        bool isBitSet(uint32_t localRow, uint32_t localCol) const {
            bool answer = false;
                uint32_t
                // Find the position in the bounding box
                        absolutePosition = localRow * this->getBoundingBox().getWidth() + localCol,
                // Find the good "word" (uint32_t)
                        wordPosition = absolutePosition >> (uint32_t) 5,
                // Find the good bit in the word
                        bitPosition =
                        (uint32_t) abs((int32_t) 32 - ((int32_t) absolutePosition
                                                       - (int32_t) (wordPosition << (uint32_t) 5)));
                // Test if the bit is one
                answer = (((((uint32_t) 1 << (bitPosition - (uint32_t) 1))
                            & this->_bitMask[wordPosition])
                        >> (bitPosition - (uint32_t) 1)) & (uint32_t) 1) == (uint32_t) 1;
            return answer;
        }

        void printBitMask() const {
            std::ostringstream oss;
            oss << std::endl;
            for (size_t i = 0; i < _boundingBox.getHeight(); ++i) {
                for (size_t j = 0; j < _boundingBox.getWidth(); ++j) {
                    oss << std::setw(1) << this->isBitSet(i,j) << " ";
                }
                oss << std::endl;
            }
            oss << std::endl;

            VLOG(3) << oss.str();
        }


        /// \brief Serialize a Feature into an output stream
        /// \param outFile Output stream
        void serializeFeature(std::ofstream &outFile) {
            outFile << this->_id << " ";
            outFile << this->_nbElementsBitMask << " ";
            _boundingBox.serializeBoundingBox(outFile);
            for (uint32_t i = 0; i < this->_nbElementsBitMask; ++i) {
                outFile << this->_bitMask[i] << " ";
            }
        }

        /// \brief Deserialize a Feature from an input file stream
        /// \param inFile input file stream
        /// \return The feature deserialized
        static Feature deserializeFeature(std::ifstream &inFile) {
            uint32_t
                    id = 0,
                    nbElementsBitMask = 0;

            uint32_t *
                    bitMask = nullptr;

            inFile >> id;
            inFile >> nbElementsBitMask;

            BoundingBox bB = BoundingBox::deserializeBoundingBox(inFile);

            bitMask = new uint32_t[nbElementsBitMask];

            for (uint32_t i = 0; i < nbElementsBitMask; ++i) {
                inFile >> bitMask[i];
            }

            return Feature{id, bB, nbElementsBitMask, bitMask};
        }

        /// \brief Output stream operator
        /// \param os output stream to print the feature
        /// \param region Feature to print
        /// \return output stream with data printed
        friend std::ostream &operator<<(std::ostream &os, const Feature &region) {
            auto bB = region.getBoundingBox();

            os << "Feature #" << region._id << std::endl
               << "    BoundingBox: " << region._boundingBox << std::endl
               << "    BitMask: " << std::endl << "        ";
            for (uint32_t e = 0; e < region._nbElementsBitMask; ++e) {
                std::bitset<32> bits(region._bitMask[e]);
                os << std::setfill('0') << std::setw(32) << bits.to_string() << " ";
            }
            os << std::endl;
            for (uint32_t row = bB.getUpperLeftRow(); row < bB.getBottomRightRow();
                 ++row) {
//                std::cout << std::endl << "        ";
                std::cout << std::endl;
                for (uint32_t col = bB.getUpperLeftCol(); col < bB.getBottomRightCol();
                     ++col) {
                    if (region.isImagePixelInBitMask(row, col)) {
                        std::cout << std::setw(1) << 1 << " ";
                    } else {
                        std::cout << std::setw(1) << 0 << " ";
                    }
                }
            }

            os << std::endl;
            return os;
        }

        /// \brief Equality operator
        /// \param other Feature to compare with
        /// \return True is the feature are equal, else False
        bool operator==(const Feature &other) const {
            bool answer = true;
            if (this->_nbElementsBitMask == other._nbElementsBitMask) {
                for (uint32_t elem = 0; elem < this->_nbElementsBitMask; ++elem) {
                    answer = answer && (this->_bitMask[elem] == other._bitMask[elem]);
                }
                answer = answer && (this->getBoundingBox() == other.getBoundingBox());
            } else {
                answer = false;
            }
            return answer;
        }

        ~Feature() {
            if(_bitMask != nullptr) {
                delete[] _bitMask;
            }
        }

        /// \brief Inequality operator
        /// \param rhs Feature to test
        /// \return True if not equal, False Else
        bool operator!=(const Feature &rhs) const {
            return !(rhs == *this);
        }





        uint32_t
                _id;                     ///< Feature Id

        uint32_t
                *_bitMask,                ///< BitMask, each bit represent a pixel
                _nbElementsBitMask;      ///< BitMask size

        BoundingBox
                _boundingBox;            ///< Bounding box
    };
}


#endif //NEWEGT_FEATURE_H
