//
// Created by gerardin on 7/19/19.
//

#ifndef EGT_EGTVIEWANALYZER_H
#define EGT_EGTVIEWANALYZER_H

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

/// @file EGTViewAnalyzer.h
/// @author Alexandre Bardakoff - Timothy Blattner - Antoine Gerardin
/// @date   7/19/19
/// @brief Task which analyse a view to find the different blob into it. It can also create a mask.

#include <cstdint>
#include <unordered_set>
#include<egt/FeatureCollection/Data/Blob.h>
#include <egt/utils/Utils.h>
#include <egt/api/SegmentationOptions.h>
#include <egt/FeatureCollection/Data/ViewAnalyse.h>
#include <egt/FeatureCollection/Data/ViewOrViewAnalyse.h>
#include <egt/api/DataTypes.h>


namespace egt {

/**
  * @class ViewAnalyser ViewAnalyser.h <FastImage/FeatureCollection/Tasks/ViewAnalyser.h>
  *
  * @brief View Analyser, HTGS task, take a FastImage view and produce a
  * ViewAnalyse to the BlobMerger.
  *
  * @details HTGS tasks, which run a flood algorithm
  * (https://en.wikipedia.org/wiki/Flood_fill) in a view to find the different
  * connected pixels called blob. Two connected rules are proposed:
  * _4 (North, South, East, West)
  * _8 (North, North-East, North-West, South, South-East, South-West, East, West)
  *
  * @tparam UserType File pixel type
  **/
    template<class UserType>
    class EGTViewAnalyzer : public htgs::ITask<htgs::MemoryData<fi::View<UserType>>,
            ViewOrViewAnalyse<UserType>> {

    public:

        enum Color { FOREGROUND, BACKGROUND };

        EGTViewAnalyzer(size_t numThreads,
                const uint32_t imageHeight,
                const uint32_t imageWidth,
                const int32_t tileHeight,
                const int32_t tileWidth,
                const uint8_t rank,
                const UserType background,
                SegmentationOptions* options
                )
                : ITask<htgs::MemoryData<fi::View<UserType>>, ViewOrViewAnalyse<UserType>>(numThreads),
                  _imageHeight(imageHeight),
                  _imageWidth(imageWidth),
                  _tileHeight(tileHeight),
                  _tileWidth(tileWidth),
                  _rank(rank),
                  _background(background),
                  _options(options),
                  _vAnalyse(nullptr) {
            _visited = std::vector<bool>((unsigned long)(_tileWidth * _tileHeight), false);
        }


        /// \brief Execute the task, do a view analyse
        /// \param view View given by the FI
        void executeTask(std::shared_ptr<MemoryData<fi::View<UserType>>> view)
        override {
            _view = view->get();
            //FOR EACH NEW TILE WE NEED TO RESET THE TASK STATE SINCE MANY TILES CAN BE PROCESSED BY ONE TASK
            _vAnalyse = new ViewAnalyse(); //OUTPUT
            _toVisit.clear(); //clear queue that keeps track of neighbors to visit when flooding
            _visited.assign(_visited.size(), false); //clear container that keeps track of all visited pixels in a pass through the image.
            _currentBlob = nullptr;
            //TILE DIMENSION MIGHT CHANGE AND BE SMALLER, LET'S DO LESS WORK IF POSSIBLE
            _tileHeight = _view->getTileHeight();
            _tileWidth = _view->getTileWidth();

            run(BACKGROUND); //find holes
            run(FOREGROUND); //find objects

            //if MASK_ONLY, we return the view with all pixel set to 0 (background) or 255 (foreground).
            //Let's not forget to delete the viewAnalyse since we are done with it.
            if(_options->MASK_ONLY) {
                VLOG(3) << "segmenting tile (" << _view->getRow() << " , " << _view->getCol() << ") :";
                VLOG(3) << "holes turned to foreground : " << holeRemovedCount;
                VLOG(3) << "objects removed because too small: " << objectRemovedCount;
                delete _vAnalyse;
                this->addResult(new ViewOrViewAnalyse<UserType>(view));
            }
            //we return a ViewAnalyse to be merged. Let's not forget to release the view since we are done with it.
            else {
                VLOG(3) << "segmenting tile (" << _view->getRow() << " , " << _view->getCol() << ") :";
                VLOG(3) << "holes turned to foreground : " << holeRemovedCount;
                VLOG(3) << "objects removed because too small: " << objectRemovedCount;
                VLOG(3) << "objects found: " << _vAnalyse->getBlobs().size();
                VLOG(3) << "objects to merge: " << _vAnalyse->getToMerge().size();
                view->releaseMemory();
                this->addResult(new ViewOrViewAnalyse<UserType>(_vAnalyse));
            }

        }

        /// \brief View analyser copy function
        /// \return A new View analyser
        EGTViewAnalyzer<UserType> *copy() override {
            auto viewAnalyzer = new EGTViewAnalyzer<UserType>(this->getNumThreads(),
                                                           _imageHeight,
                                                           _imageWidth,
                                                           _tileHeight,
                                                           _tileWidth,
                                                           _rank,
                                                           _background,
                                                           _options);
            return viewAnalyzer;
        }

        /// \brief Get task name
        /// \return Task name
        std::string getName() override {
            return "EGT View analyzer";
        }


    private:

        void run(Color blobColor){
            for (int32_t row = 0; row < _tileHeight; ++row) {
                for (int32_t col = 0; col < _tileWidth; ++col) {

                    //WE ENTER HERE ONCE WE HAVE CREATED A BLOB THAT HAS NEIGHBORS OF THE SAME COLOR
                    while (!_toVisit.empty()) {
                        expandBlob(blobColor);
                    }

                    //WE ENTER HERE WHEN WE ARE DONE EXPANDING THE CURRENT BLOB
                    if (_currentBlob != nullptr) {
                        blobCompleted(blobColor);
                    }

                    //UNLESS WE ARE DONE EXPLORING WE CREATE A ONE PIXEL BLOB AND EXPLORE ITS NEIGHBORS
                    if (!visited(row, col) && getColor(row, col) == blobColor) {
                        createBlob(row,col, blobColor);
                    }
                }
            }

            //If the last pixel was a blob by itself, it has not been saved yet.
            if (_currentBlob != nullptr) {
                blobCompleted(blobColor);
            }
       }

       /**
        * We have a queue of pixels to visit. Take the first one and explore its neighbors as well.
        * @param blobColor
        */
        void expandBlob(Color blobColor){
            auto neighbourCoord = *_toVisit.begin();
            _toVisit.erase(_toVisit.begin());
            //mark pixel as visited so we don't look at it again
            markAsVisited(neighbourCoord.first,
                          neighbourCoord.second);

            if(_options->MASK_ONLY){
                auto maskValue = (getColor(neighbourCoord.first, neighbourCoord.second) == FOREGROUND) ? 255 : 0;
                _view->setPixel(neighbourCoord.first,neighbourCoord.second, maskValue);
            }

            _currentBlob->addPixel(
                    _view->getGlobalYOffset() + neighbourCoord.first,
                    _view->getGlobalXOffset() + neighbourCoord.second);


            if (_rank == 4) {
                analyseNeighbour4(neighbourCoord.first, neighbourCoord.second, blobColor, true);
            }
        }

        /**
         * Decide what to do once we have completed a blob.
         * @param blobColor
         */
        void blobCompleted(Color blobColor) {
                VLOG(5) << "blob size: " << _currentBlob->getCount();

                //background and foreground blobs are not handled the same way
                if(blobColor == BACKGROUND) {

                    //IF WE HAVE A SMALL BLOB THAT IS NOT TO BE MERGED, WE CONSIDER IT IS A HOLE THAT NEEDS TO BE FILLED
                    //we reset all pixels as UNVISITED foreground.
                    if (_currentBlob->getCount() < _options->MIN_HOLE_SIZE && !_currentBlob->isToMerge()) {

                        for (auto it = _currentBlob->getRowCols().begin();
                             it != _currentBlob->getRowCols().end(); ++it) {
                            auto pRow = it->first;
                            for (auto pCol : it->second) {
                                auto xOffset = _view->getGlobalXOffset();
                                auto yOffset = _view->getGlobalYOffset();

                                markAsUnvisited(pRow - yOffset, pCol - xOffset);

                                if(_options->MASK_ONLY){
                                    _view->setPixel(pRow - yOffset, pCol - xOffset, 255);
                                }
                                else {
                                    _view->setPixel(pRow - yOffset, pCol - xOffset, _background + 1);
                                }
                            }
                        }
                        delete _currentBlob;
                        holeRemovedCount++;
                    }
                    //OPTIMIZATION, we do not keep around large holes because we consider them background.
                    //this can lead to significant savings in memory footprint
                    //however this force us to treat lonely holes differently in the merger.
                    else if(_currentBlob->getCount() > _options->MAX_HOLE_SIZE) {
                        if(_currentBlob->isToMerge()){
                            _vAnalyse->getHolesToMerge().erase(_currentBlob);
                        }
                        delete _currentBlob;
                        backgroundCount++;
                    }
                        //we do not know enough, so we need to merge the background blob
                    else {
                        //WE ADD IT
                        if(!_options->MASK_ONLY) {
                            _currentBlob->compactBlobDataIntoFeature();
                            _vAnalyse->insertHole(_currentBlob);
                        }
                    }

                }
                else {
                    //WE HAVE A SMALL OBJECT THAT DOES NOT NEED MERGE, WE REMOVE IT
                    if (_currentBlob->getCount() < _options->MIN_OBJECT_SIZE && !_currentBlob->isToMerge()) {

                        if(_options->MASK_ONLY){
                            for (auto it = _currentBlob->getRowCols().begin();
                                 it != _currentBlob->getRowCols().end(); ++it) {
                                auto pRow = it->first;
                                for (auto pCol : it->second) {
                                    auto xOffset = _view->getGlobalXOffset();
                                    auto yOffset = _view->getGlobalYOffset();
                                    _view->setPixel(pRow - yOffset, pCol - xOffset,0);
                                }
                            }
                        }

                        delete _currentBlob;
                        objectRemovedCount++;
                    }
                    else{
                        //WE ADD IT
                        if(!_options->MASK_ONLY) {
                            _currentBlob->compactBlobDataIntoFeature();
                            _vAnalyse->insertBlob(_currentBlob);
                        }
                    }
                }

            _currentBlob = nullptr;
        }


        /**
         * Create a new blob at this pixel and check its neighbors to kickstart its expansion.
         * @param row
         * @param col
         * @param blobColor
         */
        void createBlob(int32_t row, int32_t col, Color blobColor){
            markAsVisited(row, col);
            //add pixel to a new blob
            _currentBlob = new Blob(_view->getGlobalYOffset() + row, _view->getGlobalXOffset() + col);
            _currentBlob->addPixel(_view->getGlobalYOffset() + row, _view->getGlobalXOffset() + col);
            //make sure we don't look at it again

            //look at its neighbors
            if (_rank == 4) {
                analyseNeighbour4(row, col, blobColor, true);
            }
        }

        /// \brief Analyse the neighbour of a pixel for a 4-connectivity
        /// \param row Pixel's row
        /// \param col Pixel's col
        void analyseNeighbour4(int32_t row, int32_t col, Color color,bool erode) {

            auto globalRow = row + _view->getGlobalYOffset();
            auto globalCol = col + _view->getGlobalXOffset();

            //Explore neighbors in all directions to see if they need to be added to the blob.
            //Top Pixel
            if (row >= 1) {
                if (!visited(row - 1, col) && getColor(row - 1, col) == color) {
                    _toVisit.emplace(row - 1, col);
                }
            }
            // Bottom pixel
            if (row + 1 < _tileHeight) {
                if (!visited(row + 1, col) && getColor(row + 1, col) == color) {
                    _toVisit.emplace(row + 1, col);
                }
            }
            // Left pixel
            if (col >= 1) {
                if (!visited(row, col - 1) && getColor(row, col - 1) == color) {
                    _toVisit.emplace(row, col - 1);
                }
            }
            // Right pixel
            if ( (col + 1) < _tileWidth) {
                if (!visited(row, col + 1) && getColor(row, col + 1) == color) {
                    _toVisit.emplace(row, col + 1);
                }
            }

            //WE DON'T NEED MERGING IF WE GENERATE ONLY THE MASK
            if(_options->MASK_ONLY){
                return;
            }

            //Check if the blob in this tile belongs to a bigger blob extending several tiles.
            //We only to look at EAST and SOUTH so we can later merge only once, TOP to BOTTOM and LEFT to RIGHT.

            // Add blob to merge list if tile bottom pixel has the same value than the view pixel below (continuity)
            // test that we are also not at the edge of the full image.
            if (row + 1 == _tileHeight && globalRow + 1 != _imageHeight) {
                if (getColor(row + 1, col) == color) {
                    auto coords = std::pair<int32_t, int32_t>(globalRow + 1, globalCol);

                    if(color == BACKGROUND){
                        _vAnalyse->addHolesToMerge(_currentBlob, coords);
                    }
                    else {
                        _vAnalyse->addToMerge(_currentBlob, coords);
                    }
                    _currentBlob->setToMerge(true);
                }
            }
            // Add blob to merge list if tile right pixel has the same value than the view pixel on its right (continuity)
            // test that we are also not at the edge of the full image.
            if (col + 1 == _tileWidth && col + _view->getGlobalXOffset() + 1 != _imageWidth) {
                if (getColor(row, col + 1) == color) {

                    auto coords =std::pair<int32_t, int32_t>(globalRow,globalCol + 1);

                    if(color == BACKGROUND){
                        _vAnalyse->addHolesToMerge(_currentBlob, coords);
                    }
                    else {
                        _vAnalyse->addToMerge(_currentBlob, coords);
                    }
                    _currentBlob->setToMerge(true);
                }
            }

            //look at the pixel on the left. We need to set a flag so we know how to filter this blob.
            if (col == 0 && col + _view->getGlobalXOffset() != 0) {
                if (getColor(row, col - 1) == color) {
                    _currentBlob->setToMerge(true);
                }
            }

            //look at the pixel above. We need to set a flag so we know how to filter this blob.
            if (row == 0 && row + _view->getGlobalYOffset() != 0) {
                if (getColor(row - 1, col) == color) {
                    _currentBlob->setToMerge(true);
                }
            }
        }


        void markAsVisited(int32_t row, int32_t col) {
            _visited[row * _tileWidth + col] = true;
        }

        void markAsUnvisited(int32_t row, int32_t col) {
            _visited[row * _tileWidth + col] = false;
        }

        inline bool visited(int32_t row, int32_t col){
            return _visited[row * _tileWidth + col];
        }

        Color getColor(int32_t row, int32_t col) {
            if(_view->getPixel(row,col) > _background){
                return Color::FOREGROUND;
            }
            return Color::BACKGROUND;
        }


        fi::View<UserType>* _view;

        const uint8_t _rank{};                      ///< Rank to the connectivity:
        ///< 4=> 4-connectivity, 8=> 8-connectivity


        std::vector<bool> _visited{}; ///keep track of every pixel we looked at.
        std::set<Coordinate>_toVisit{}; /// for a current position, keep track of the neighbors that needs visit.


        Blob
                *_currentBlob = nullptr;      ///< Current blob

        int32_t
                _tileHeight{},                ///< Tile actual height
                _tileWidth{};                 ///< Tile actual width


        UserType _background{}; /// Threshold value chosen to represent the value above which we have a foreground pixel.

        ///We need those if we are trying to merge blobs globally.
        const uint32_t
                _imageHeight{},               ///< Mask height
                _imageWidth{};                ///< Mask width

        SegmentationOptions* _options{};


        ViewAnalyse *_vAnalyse = nullptr;         ///< Current view analyse

        uint32_t objectRemovedCount{},
                 holeRemovedCount{},
                 backgroundCount{};


    };


}

#endif //EGT_EGTVIEWANALYZER_H
