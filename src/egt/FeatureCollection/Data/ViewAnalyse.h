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

/// @file ViewAnalyse.h
/// @author Alexandre Bardakoff - Timothy Blattner
/// @date  4/6/18
/// @brief Main interface to construct a tile loader


#ifndef EGT_REGIONLABELING_VIEWANALYSE_H
#define EGT_REGIONLABELING_VIEWANALYSE_H

#include <unordered_map>
#include <htgs/api/IData.hpp>
#include <egt/FeatureCollection/Data/Blob.h>

namespace egt {
/// \namespace fc FeatureCollection namespace


/**
  * @class ViewAnalyse ViewAnalyse.h <FastImage/FeatureCollection/Data//ViewAnalyse.h>
  *
  * @brief Describe a collection of blob in a view. Derive from IData.
  *
  * @details View analyse, designed and use to keep track of the different
  * blobs in a specific view. The blobs in the border could be merged to another
  * blob in an adjacent view. They are placed in the _toMerge map, the key is
  * the blob address and the values are all the possible coordinates the blob can
  * merge with.
  **/
    class ViewAnalyse : public htgs::IData {
    public:

        ViewAnalyse() = default;

        ViewAnalyse(uint32_t row, uint32_t col, uint32_t level) : row(row), col(col), level(level) {}

        uint32_t getLevel() const {
            return level;
        }

        uint32_t getRow() const {
            return row;
        }

        uint32_t getCol() const {
            return col;
        }

        void setLevel(uint32_t l) {
            level = l;
        }


        /// \brief Getter to the merge map
        /// \return Merge map
        std::map<Coordinate, std::unordered_map<Blob *, std::list<Coordinate>>> &getToMerge() {
            return _toMerge;
        }

        std::map<Coordinate, std::unordered_map<Blob *, std::list<Coordinate>>> &getHolesToMerge() {
            return _holesToMerge;
        }

        std::unordered_map<Blob *, std::list<Blob *>> &getBlobsParentSons() {
            return _blobsParentSons;
        }

        std::unordered_map<Blob *, std::list<Blob *>> &getFinalBlobsParentSons() {
            return _finalBlobsParentSons;
        }

        std::unordered_map<Blob *, std::list<Blob *>> &getHolesParentSons() {
            return _holesParentSons;
        }

        std::unordered_map<Blob *, std::list<Blob *>> &getFinalHolesParentSons() {
            return _finalHolesParentSons;
        }


        Blob *getSurroundingBlob(std::vector<Blob *> &blobs, uint32_t row, uint32_t col) {
            for (auto b : blobs){
                if(b->isPixelinFeature(row - 1, col)){
                    return b;
                }
                if(b->isPixelinFeature(row, col - 1)){
                    return b;
                }
            }
            return nullptr;
        }

        void filterHoles(uint32_t cutoff){

            //list of all blobs
            auto blobs = std::vector<Blob*>();

            for(auto parentBlob : this->getBlobsParentSons()) {
                for(auto blob : parentBlob.second) {
                    blobs.push_back(blob);
                }
            }
            for(auto finalParentBlob : this->getFinalBlobsParentSons()) {
                for(auto blob : finalParentBlob.second) {
                    blobs.push_back(blob);
                }
            }

            //for each final hole
            for(auto parentHole : this->getFinalHolesParentSons()) {
                auto holeGroup = parentHole.second;

                uint64_t area = 0;
                for (auto blob : holeGroup) {
                    area += blob->getCount();
                }

                VLOG(4) << "Final hole group of size : " << holeGroup.size() << " and of area : " << area;

                if(area < cutoff) {
                    VLOG(4) << "Hole too small : turn to foreground.";
                    auto blob = getSurroundingBlob(blobs, parentHole.first->getStartRow(),
                                                   parentHole.first->getStartCol());

                    if(blob == nullptr) {
                        VLOG(1) << "SPECIAL CASE" << " : hole at the border and not merging. We need a better way to find surrounding blobs";
                        this->getFinalBlobsParentSons().insert(parentHole);
                    }
                    else {
                        fc::UnionFind<Blob> uf{};
                        auto parent = uf.find(blob);
                        auto it = this->getFinalBlobsParentSons().find(parent);
                        if (it != this->getFinalBlobsParentSons().end()) {
                            std::list<Blob *> blobs = it->second;
                            blobs.splice(blobs.begin(), holeGroup);
                        } else {
                            auto it = this->getBlobsParentSons().find(parent);
                            if (it != this->getBlobsParentSons().end()) {
                                std::list<Blob *> blobs = it->second;
                                blobs.splice(blobs.begin(), holeGroup);
                            }
                        }
                    }
                }
                else {
                    for(auto hole : holeGroup) {
                        delete hole;
                    }
                }
            }

            this->getFinalHolesParentSons().clear();
        }


        /// \brief Add an entry to the to merge structure
        /// \param b Blob to add
        /// \param c Coordinate links to this  blob
        void addToMerge(Coordinate tileCoordinate, Blob *b, Coordinate c) {
            _toMerge[tileCoordinate][b].push_back(c);
        }

        void addHolesToMerge(Coordinate tileCoordinate, Blob *b, Coordinate c) {
            _holesToMerge[tileCoordinate][b].push_back(c);
        }

        /// \brief Insert a blob to the list of blobs
        /// \param b blob to add
        void insertBlob(Blob *b) {
            if(b->isToMerge()) {
                _blobsParentSons[b].push_back(b);
            }
            else {
                _finalBlobsParentSons[b].push_back(b);
            }
        }

        void insertHole(Blob *b) {
            _holesParentSons[b].push_back(b);
        }

        virtual ~ViewAnalyse() {
            _toMerge.clear();
            _holesToMerge.clear();
            _finalBlobsParentSons.clear();
            _blobsParentSons.clear();
            _holesParentSons.clear();
            _finalHolesParentSons.clear();
        }

    private:
        std::map<Coordinate, std::unordered_map<Blob *, std::list<Coordinate>>>
                _toMerge{};   ///< Map of blob which will need to be merged to a list of
        ///< coordinates

        std::map<Coordinate, std::unordered_map<Blob *, std::list<Coordinate>>>
                _holesToMerge{};   ///< Map of holes which will need to be merged to a list of
        ///< coordinates

        std::unordered_map<Blob *, std::list<Blob *>> _finalBlobsParentSons{};

        std::unordered_map<Blob *, std::list<Blob *>> _blobsParentSons{};

        std::unordered_map<Blob *, std::list<Blob *>> _holesParentSons{};

        std::unordered_map<Blob *, std::list<Blob *>> _finalHolesParentSons{};

        uint32_t level = 0;

        uint32_t row = 0, col = 0;

    };
}
#endif //EGT_REGIONLABELING_VIEWANALYSE_H
