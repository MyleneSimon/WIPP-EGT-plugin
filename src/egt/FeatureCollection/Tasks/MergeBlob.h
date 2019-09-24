//
// Created by gerardin on 9/16/19.
//

#ifndef NEWEGT_MERGEBLOB_H
#define NEWEGT_MERGEBLOB_H

#include <tiffio.h>
#include <htgs/api/ITask.hpp>
#include <cstdint>
#include <utility>
#include <egt/FeatureCollection/Data/ListBlobs.h>
#include <egt/utils/FeatureExtraction.h>
#include <egt/api/DerivedSegmentationParams.h>
#include <egt/FeatureCollection/Data/ViewAnalyse.h>
#include "egt/FeatureCollection/Data/ViewAnalyzeBlock.h"
#include <egt/utils/UnionFind.h>

namespace egt {

class MergeBlob : public htgs::ITask<ViewAnalyseBlock, ViewAnalyse> {

public:

    explicit MergeBlob(size_t numThreads) : ITask(numThreads) {}

    void executeTask(std::shared_ptr<ViewAnalyseBlock> data) override {

        result = new ViewAnalyse(data->getRow(), data->getCol(), data->getLevel() + 1);


        auto views = data->getViewAnalyses();

        for(auto &entry : views) {
            auto view = entry.second;
            for(auto parentSon : view->getBlobsParentSons()) {
                result->getBlobsParentSons().insert(parentSon);
            }
        }

        //For each view, check BOTTOM, RIGHT, BOTTOM-RIGHT and TOP-RIGHT view FOR POTENTIAL MERGES
        //merges within this block are performed
        //others are scheduled for the next level
        for(auto &entry : views) {
            auto coordinates = entry.first;
            auto row = coordinates.first, col = coordinates.second;
            auto view = entry.second;

            //BOTTOM TILE
            if(views.find({row + 1, col}) != views.end()){
                merge(view, views[{row + 1, col}]);
            }
            else {
                addToNextLevelMerge(view , {row + 1, col}, {data->getRow() + 1, data->getCol()});
            }

            //RIGHT TILE
            if(views.find({row, col + 1}) != views.end()){
                merge(view, views[{row, col + 1}]);
            }
            else {
                addToNextLevelMerge(view , {row , col + 1}, {data->getRow(), data->getCol() + 1});
            }

            //BOTTOM RIGHT TILE
            if(views.find({row + 1, col + 1}) != views.end()){
                merge(view, views[{row + 1, col + 1}]);
            }
            else {
                addToNextLevelMerge(view , {row + 1 , col + 1}, {data->getRow() + 1, data->getCol() + 1});
            }

            //TOP RIGHT TILE
            if(views.find({row - 1, col + 1}) != views.end()){
                merge(view, views[{row - 1, col + 1}]);
            }
            else {
                addToNextLevelMerge(view , {row - 1 , col + 1}, {data->getRow() - 1, data->getCol() + 1});
            }

            //KEEP TRACK OF CORRESPONDING MERGES ON THE TOP, LEFT AND TOP-LEFT
            addToNextLevelMerge(view , {row - 1 , col}, {data->getRow() - 1, data->getCol()});
            addToNextLevelMerge(view , {row , col - 1}, {data->getRow(), data->getCol() - 1});
            addToNextLevelMerge(view , {row - 1 , col - 1}, {data->getRow() - 1, data->getCol() - 1});
            addToNextLevelMerge(view , {row + 1 , col - 1}, {data->getRow() + 1, data->getCol() - 1});
        }

        VLOG(3) << "Merge produce ViewAnalyse for level " << result->getLevel() << ": (" << result->getRow() << ", " << result->getCol() <<  "). ";
        this->addResult(result);
    }

    ITask<ViewAnalyseBlock, ViewAnalyse> *copy() override {
        return new MergeBlob(this->getNumThreads());
    }

    /**
     * Merge blobs at the border of two contiguous tiles.
     */
    void merge(std::shared_ptr<ViewAnalyse> v1, std::shared_ptr<ViewAnalyse> v2){
        VLOG(3) << result->getLevel() << ": (" << result->getRow() << ", " << result->getCol() <<  "). " << "merging tile at level " << result->getLevel() - 1 << " : " << "(" << v1->getRow() << "," << v1->getCol() << ") & (" << v2->getRow() << "," << v2->getCol() << ")"  << " ...";

        //find corresponding merge regions in each view
        auto blobsToMerge = v1->getToMerge()[{v2->getRow(),v2->getCol()}];
        auto otherBlobsToMerge = v2->getToMerge()[{v1->getRow(),v1->getCol()}];

        if(blobsToMerge.empty()) {
            VLOG(5) << "Nothing to do in merge region for tiles : "<< "(" << v1->getRow() << "," << v1->getCol() << ") & " << "(" << v2->getRow() << "," << v2->getCol();
            return;
        }

        //for each blob to merge, find the corresponding blobs to merge in the adjacent tile
        for(auto &blobPixelPair : blobsToMerge) {
                auto blob = blobPixelPair.first;
                for(auto coordinates : blobPixelPair.second) {
                        for(auto otherBlobPixelPair : otherBlobsToMerge) {
                            auto otherBlob = otherBlobPixelPair.first;
                            if (otherBlobPixelPair.first->isPixelinFeature(coordinates.first, coordinates.second)) {
                                //merge the blob groups
                                VLOG(5) << "Creating merged blob from blobsToMerge : blob_" << blob->getTag() << ", blob_" << otherBlob->getTag();
                                UnionFind<Blob> uf{};
                                blob->decreaseMergeCount();
                                otherBlob->decreaseMergeCount();
                                auto root1 = uf.find(blob);
                                auto root2 = uf.find(otherBlob);

                                auto blobGroup1 = v1->getBlobsParentSons()[root1];

                                //already connected through another path
                                if (root1 == root2) {
                                    VLOG(5) << "blob_" << blob->getTag() << "& blob_" << otherBlob->getTag()
                                            << " are already connected through another path. Common root is : blob_"
                                            << root1->getTag();

                                    auto blobGroup2 = v2->getBlobsParentSons()[root2];
                                    blobGroup1.insert(blobGroup2.begin(), blobGroup2.end()); //merge sons

                                    //are we done merging the whole blob?
                                    auto mergeLeftCount = std::accumulate(blobGroup1.begin(), blobGroup1.end(), 0,
                                                                          [](uint32_t i, const Blob *b) {
                                                                              return b->getMergeCount() + i;
                                                                          });
                                    if (mergeLeftCount == 0) {
                                        result->getFinalBlobsParentSons()[root1].insert(blobGroup1.begin(),
                                                                                       blobGroup1.end());
                                        result->getBlobsParentSons().erase(root1);
                                    }
                                    else {
                                        v1->getBlobsParentSons()[root1].insert(blobGroup1.begin(), blobGroup1.end());
                                        v2->getBlobsParentSons()[root1].insert(blobGroup1.begin(), blobGroup1.end());
                                    }

                                } else {
                                    auto root = uf.unionElements(blob, otherBlob); //pick one root as the new root
                                    auto blobGroup2 = v2->getBlobsParentSons()[root2];
                                    blobGroup1.insert(blobGroup2.begin(), blobGroup2.end()); //merge sons


                                    //are we done merging the whole blob?
                                    auto mergeLeftCount = std::accumulate(blobGroup1.begin(), blobGroup1.end(), 0,
                                                                          [](uint32_t i, const Blob *b) {
                                                                              return b->getMergeCount() + i;
                                                                          });
                                    if (mergeLeftCount == 0) {
                                        result->getFinalBlobsParentSons()[root].insert(blobGroup1.begin(),
                                                                                       blobGroup1.end());
                                        result->getBlobsParentSons().erase(root1);
                                        result->getBlobsParentSons().erase(root2);
                                    } else {
                                        result->getBlobsParentSons()[root].insert(blobGroup1.begin(), blobGroup1.end());
                                        v1->getBlobsParentSons()[root].insert(blobGroup1.begin(), blobGroup1.end());
                                        v2->getBlobsParentSons()[root].insert(blobGroup1.begin(), blobGroup1.end());
                                        if (root == root2) {
                                            result->getBlobsParentSons().erase(root1);
                                        } else {
                                            result->getBlobsParentSons().erase(root2);
                                        }
                                    }
                                }
                            }
                        }
                }
        }
    }

      /**
      * Record merges that needs to happen at the next level
      * @param currentView viewAnalyze we need to propagate merges from
      * @param tileCoordinates the coordinates of the merge region
      * @param contiguousBlockCoordinates the coordinates of the new merge region at the next level.
      */
    void addToNextLevelMerge(std::shared_ptr<ViewAnalyse> currentView, std::pair<uint32_t,uint32_t> tileCoordinates, std::pair<uint32_t,uint32_t> contiguousBlockCoordinates){
        auto blobsToMerge = currentView->getToMerge()[tileCoordinates];
        if(!(blobsToMerge).empty()){
            VLOG(5) << "level: " << result->getLevel() << " - block : (" << result->getRow() << ", " << result->getCol() <<  "). "  << " - add merge with block : " << "(" << contiguousBlockCoordinates.first << "," << contiguousBlockCoordinates.second << ")" << "- original tile : " << "(" << tileCoordinates.first << "," << tileCoordinates.second << ")" << "need to merge " << blobsToMerge.size() << " at the next level";
            for(auto blobToPixelEntry : currentView->getToMerge()[tileCoordinates]){
                result->getToMerge()[contiguousBlockCoordinates].insert(blobToPixelEntry);
            }
            //merge all entries of currentView->getToMerge()[tileCoordinates] into result->getToMerge()[contiguousBlockCoordinates]
            //we will merge them at the next level
            //empty if we are at the border
        }
    }

    //Managed by HTGS, no need to delete
    ViewAnalyse* result{};

};

}

#endif //NEWEGT_MERGEBLOB_H
