//
// Created by gerardin on 7/9/19.
//

#ifndef NEWEGT_EGT_H
#define NEWEGT_EGT_H


#include <string>
#include <egt/loaders/PyramidTiledTiffLoader.h>
#include <FastImage/api/FastImage.h>
#include <egt/FeatureCollection/Tasks/EGTViewAnalyzer.h>
#include <egt/FeatureCollection/Tasks/BlobMerger.h>
#include <egt/FeatureCollection/Tasks/FeatureCollection.h>
#include <egt/FeatureCollection/Tasks/ViewAnalyseFilter.h>
#include "DataTypes.h"
#include <egt/tasks/ThresholdFinder.h>
#include <egt/tasks/SobelFilterOpenCV.h>
#include <egt/tasks/FCSobelFilterOpenCV.h>
#include <htgs/log/TaskGraphSignalHandler.hpp>
#include <egt/tasks/CustomSobelFilter3by3.h>
#include <egt/tasks/FCCustomSobelFilter3by3.h>
#include <egt/memory/TileAllocator.h>
#include <egt/FeatureCollection/Tasks/ViewFilter.h>
#include <egt/tasks/TiffTileWriter.h>
#include <egt/api/EGTOptions.h>
#include <random>
#include <egt/tasks/EGTSobelFilter.h>
#include <egt/FeatureCollection/Tasks/EGTGradientViewAnalyzer.h>
#include "DerivedSegmentationParams.h"


namespace egt {

    template<class T>
    class EGT {

    public:

    private:


    public:

/// This algorithm is divided in several graph, each fed by a different fast image.
        /// The reason is that we request different radius each time and each fastImage instance is associated
        /// with a single radius. It would be possible to rewrite the algo by taking the widest radius and calculate
        /// smaller radius from there, but marginal gains would not worth the complexity introduced.
        void run(EGTOptions *options, SegmentationOptions *segmentationOptions,
                 std::map<std::string, uint32_t> &expertModeOptions) {

            options->nbLoaderThreads = (expertModeOptions.find("loader") != expertModeOptions.end())
                                       ? expertModeOptions.at("loader") : 1;
            options->concurrentTiles = (expertModeOptions.find("tile") != expertModeOptions.end())
                                       ? expertModeOptions.at("tile") : 1;
            options->nbSamples = (expertModeOptions.find("sample") != expertModeOptions.end()) ? expertModeOptions.at(
                    "sample") : 10;
            options->nbExperiments = (expertModeOptions.find("exp") != expertModeOptions.end()) ? expertModeOptions.at(
                    "exp") : 5;
            options->threshold = (expertModeOptions.find("threshold") != expertModeOptions.end())
                                 ? expertModeOptions.at("threshold") : -1;


            VLOG(1) << "Execution model : ";
            VLOG(1) << "loader threads : " << options->nbLoaderThreads;
            VLOG(1) << "concurrent tiles : " << options->concurrentTiles;
            VLOG(1) << "fixed threshold : " << std::boolalpha << (options->threshold != -1);
            if (options->threshold != -1) {
                VLOG(1) << "fixed threshold value : " << options->threshold << std::endl;
            }

            auto begin = std::chrono::high_resolution_clock::now();


            auto segmentationParams = DerivedSegmentationParams<T>();
            runPixelIntensityBounds(options, segmentationOptions, segmentationParams);

            //determining threshold
            auto beginThreshold = std::chrono::high_resolution_clock::now();
            T threshold{};

            if (options->threshold == -1) {
                threshold = runThresholdFinder(options);
            } else {
                threshold = options->threshold;
            }
            auto endThreshold = std::chrono::high_resolution_clock::now();

            segmentationParams.threshold = threshold;

            //segment
            std::shared_ptr<ListBlobs> blobs = nullptr;
            auto beginSegmentation = std::chrono::high_resolution_clock::now();
            if (segmentationOptions->MASK_ONLY) {
                //TODO remove threshold for params
                runLocalMaskGenerator(threshold, options, segmentationOptions, segmentationParams);
            } else {
                blobs = runSegmentation(threshold, options, segmentationOptions, segmentationParams);
            }
            auto endSegmentation = std::chrono::high_resolution_clock::now();

            //postprocessing of features
            auto beginFC = std::chrono::high_resolution_clock::now();


            if (!segmentationOptions->MASK_ONLY) {
                featureExtraction(blobs, options);
                runBinaryMaskGeneration(blobs, segmentationOptions);
            }
            auto endFC = std::chrono::high_resolution_clock::now();

            delete segmentationOptions;
            auto end = std::chrono::high_resolution_clock::now();

            VLOG(1) << "Execution time: ";
            VLOG(1) << "    Threshold Detection: "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(endThreshold - beginThreshold).count()
                    << " mS";
            VLOG(1) << "    Segmentation: " << std::chrono::duration_cast<std::chrono::milliseconds>(
                    endSegmentation - beginSegmentation).count() << " mS";
            VLOG(1) << "    Feature Collection: "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(endFC - beginFC).count() << " mS";
            VLOG(1) << "    Total: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
                    << " mS" << std::endl;
        }


        void runPixelIntensityBounds(EGTOptions *options, SegmentationOptions* segmentationOptions, DerivedSegmentationParams<T> &segmentationParams) {

            const uint32_t radiusForThreshold = 0;
            auto tileLoader = new PyramidTiledTiffLoader<T>(options->inputPath, options->nbLoaderThreads);
            auto *fi = new fi::FastImage<T>(tileLoader, radiusForThreshold);
            fi->getFastImageOptions()->setNumberOfViewParallel(options->concurrentTiles);
            fi->configureAndRun();

            //we try to figure at which resolution we could calculate the intensity (max at 2).
            uint32_t pyramidLevelToRequestforPixelIntensityBounds = options->pyramidLevel;
            auto levelUp = options->pixelIntensityBoundsLevelUp;
            while (pyramidLevelToRequestforPixelIntensityBounds + levelUp > fi->getNbPyramidLevels() - 1){
                    levelUp--;
            }
            pyramidLevelToRequestforPixelIntensityBounds += levelUp;

            uint32_t imageHeight = fi->getImageHeight(pyramidLevelToRequestforPixelIntensityBounds);
            uint32_t imageWidth = fi->getImageWidth(pyramidLevelToRequestforPixelIntensityBounds);
            uint32_t numTileCol = fi->getNumberTilesWidth(pyramidLevelToRequestforPixelIntensityBounds);
            uint32_t numTileRow = fi->getNumberTilesHeight(pyramidLevelToRequestforPixelIntensityBounds);
            uint32_t nbTiles = fi->getNumberTilesHeight(pyramidLevelToRequestforPixelIntensityBounds) *
                               fi->getNumberTilesWidth(pyramidLevelToRequestforPixelIntensityBounds);

            fi->requestAllTiles(true, pyramidLevelToRequestforPixelIntensityBounds);



            auto intensities = std::vector<T>();

            while(fi->isGraphProcessingTiles()) {
                auto pview = fi->getAvailableViewBlocking();
                if(pview != nullptr){
                    auto view = pview->get();
                    auto tileHeight = view->getTileHeight();
                    auto tileWidth = view->getTileWidth();
                    VLOG(3) << "intensity bounds : collecting pixel for tile (" << view->getRow() << "," << view->getCol() << ").";
                    //TODO check with it has to be zero
                    std::copy_if(view->getData(), view->getData() + tileHeight * tileWidth,  std::back_inserter(intensities) , [](T val){return (val!=0);});
                    pview->releaseMemory();
                }
            }

            auto size = imageHeight * imageWidth;
            auto nonZeroSize = intensities.size();

            std::sort(intensities.begin(), intensities.end());

            auto minIndex = segmentationOptions->MIN_PIXEL_INTENSITY_PERCENTILE * intensities.size() / 100  - 1;
            auto maxIndex = segmentationOptions->MAX_PIXEL_INTENSITY_PERCENTILE* intensities.size() / 100  - 1;

            auto minValue = intensities[minIndex];
            auto maxValue = intensities[maxIndex];

            segmentationParams.minPixelIntensityValue = intensities[minIndex];
            segmentationParams.maxPixelIntensityValue = intensities[maxIndex];

            fi->waitForGraphComplete();
            delete fi;
    }


        /// ----------------------------------
        /// The first graph finds the threshold value used to segment the image
        /// ----------------------------------
        T runThresholdFinder(EGTOptions *options) {

            const uint32_t pyramidLevelToRequestforThreshold = options->pyramidLevel;
            const uint32_t radiusForThreshold = 1;

            T threshold = 0;

            auto tileLoader = new PyramidTiledTiffLoader<T>(options->inputPath, options->nbLoaderThreads);
            auto *fi = new fi::FastImage<T>(tileLoader, radiusForThreshold);
            fi->getFastImageOptions()->setNumberOfViewParallel(options->concurrentTiles);
            auto fastImage = fi->configureAndMoveToTaskGraphTask("Fast Image");


            uint32_t imageHeight = fi->getImageHeight(pyramidLevelToRequestforThreshold);     //< Image Height
            uint32_t imageWidth = fi->getImageWidth(pyramidLevelToRequestforThreshold);      //< Image Width
            uint32_t tileWidth = fi->getTileWidth();
            uint32_t tileHeight = fi->getTileHeight();
            assert(tileWidth == tileHeight); //we work with square tiles
            uint32_t numTileCol = fi->getNumberTilesWidth(pyramidLevelToRequestforThreshold);
            uint32_t numTileRow = fi->getNumberTilesHeight(pyramidLevelToRequestforThreshold);
            uint32_t nbTiles = fi->getNumberTilesHeight(pyramidLevelToRequestforThreshold) *
                               fi->getNumberTilesWidth(pyramidLevelToRequestforThreshold);

            auto nbOfSamples = nbTiles;
            size_t nbOfSamplingExperiment = 1;

            auto graph = new htgs::TaskGraphConf<htgs::MemoryData<fi::View<T>>, Threshold<T>>();
            auto sobelFilter = new CustomSobelFilter3by3<T>(options->concurrentTiles, options->imageDepth, 1, 1);

            auto thresholdFinder = new ThresholdFinder<T>(nbOfSamplingExperiment, tileHeight * tileWidth, nbOfSamples,
                                                          options->imageDepth);
            graph->addEdge(fastImage, sobelFilter);
            graph->addEdge(sobelFilter, thresholdFinder);
            graph->addGraphProducerTask(thresholdFinder);

            //MEMORY MANAGEMENT
            graph->addMemoryManagerEdge("gradientTile", sobelFilter, new TileAllocator<T>(tileWidth, tileHeight),
                                        options->concurrentTiles, htgs::MMType::Dynamic);

            //FOR DEBUGGING
            htgs::TaskGraphSignalHandler::registerTaskGraph(graph);
            htgs::TaskGraphSignalHandler::registerSignal(SIGTERM);

            auto *runtime = new htgs::TaskGraphRuntime(graph);
            runtime->executeRuntime();

            auto seed1 = std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine generator(seed1);
            std::uniform_int_distribution<uint32_t> distributionRowRange(0, numTileRow - 1);
            std::uniform_int_distribution<uint32_t> distributionColRange(0, numTileCol - 1);

            for (auto i = 0; i < nbOfSamplingExperiment * nbOfSamples; i++) {
                uint32_t randomRow = distributionRowRange(generator);
                uint32_t randomCol = distributionColRange(generator);
                VLOG(3) << "Requesting tile : (" << randomRow << ", " << randomCol << ")";
                fi->requestTile(randomRow, randomCol, false, pyramidLevelToRequestforThreshold);
            }

            fi->finishedRequestingTiles();
            graph->finishedProducingData();

            std::vector<T> thresholdCandidates = {};

            while (!graph->isOutputTerminated()) {
                auto data = graph->consumeData();
                if (data != nullptr) {
                    thresholdCandidates.push_back(data->getValue());
                }
            }

            double sum = 0;
            for (T t : thresholdCandidates) {
                sum += t;
            }
            threshold = sum / thresholdCandidates.size();
            VLOG(3) << "Threshold value using average: " << threshold;

            std::sort(thresholdCandidates.begin(), thresholdCandidates.end());
            auto medianIndex = std::ceil(thresholdCandidates.size() / 2);
            threshold = thresholdCandidates[medianIndex];
            VLOG(3) << "Threshold value using median : " << threshold;

            VLOG(1) << "Threshold value : " << threshold;

            runtime->waitForRuntime();
//FOR DEBUGGING
            graph->writeDotToFile("thresholdGraph.xdot", DOTGEN_COLOR_COMP_TIME);

            delete fi;
            delete runtime;

            return threshold;
        }

        /**
         * Generate a binary mask encoded in uint8 from a threshold.
         * The thresholding is performed for each image tile without global processing.
         * This is very fast but cannot be 100% correct as features spanning several tiles will not be filtered.
         * @tparam T
         * @param threshold - Threshold value for the foreground.
         * @param options - Options for configuring EGT execution.
         * @param segmentationOptions - Parameters used for the segmentation.
         */
        void runLocalMaskGenerator(T threshold, EGTOptions *options, SegmentationOptions *segmentationOptions, DerivedSegmentationParams<T> segmentationParams) {
            htgs::TaskGraphConf<htgs::MemoryData<fi::View<T>>, VoidData> *localMaskGenerationGraph;
            htgs::TaskGraphRuntime *localMaskGenerationGraphRuntime;

            uint32_t pyramidLevelToRequestForSegmentation = options->pyramidLevel;
            //radius of 2 since we need first apply convo, obtain a gradient of size n+1,
            //and then check the ghost region for potential merges for each tile of size n.
            uint32_t segmentationRadius = 2;

            auto tileLoader2 = new PyramidTiledTiffLoader<T>(options->inputPath, options->nbLoaderThreads);
            auto *fi = new fi::FastImage<T>(tileLoader2, segmentationRadius);
            fi->getFastImageOptions()->setNumberOfViewParallel(options->concurrentTiles);
            auto fastImageTask = fi->configureAndMoveToTaskGraphTask("Fast Image");
            uint32_t imageHeightAtSegmentationLevel = fi->getImageHeight(pyramidLevelToRequestForSegmentation);
            uint32_t imageWidthAtSegmentationLevel = fi->getImageWidth(pyramidLevelToRequestForSegmentation);
            int32_t tileHeigthAtSegmentationLevel = fi->getTileHeight(pyramidLevelToRequestForSegmentation);
            int32_t tileWidthAtSegmentationLevel = fi->getTileWidth(pyramidLevelToRequestForSegmentation);
            uint32_t nbTiles = fi->getNumberTilesHeight(pyramidLevelToRequestForSegmentation) *
                               fi->getNumberTilesWidth(pyramidLevelToRequestForSegmentation);

            auto sobelFilter2 = new EGTSobelFilter<T>(options->concurrentTiles, options->imageDepth, 1, 1);
            auto viewSegmentation = new EGTGradientViewAnalyzer<T>(options->concurrentTiles,
                                                           imageHeightAtSegmentationLevel,
                                                           imageWidthAtSegmentationLevel,
                                                           tileHeigthAtSegmentationLevel,
                                                           tileWidthAtSegmentationLevel,
                                                           4,
                                                           threshold,
                                                           segmentationOptions,
                                                           segmentationParams);
            auto maskFilter = new ViewFilter<T>(options->concurrentTiles);
            auto merge = new BlobMerger<T>(imageHeightAtSegmentationLevel,
                                        imageWidthAtSegmentationLevel,
                                        nbTiles,
                                        options,
                                        segmentationOptions,
                                        segmentationParams);
            auto writeMask = new TiffTileWriter<T>(
                    1,
                    imageHeightAtSegmentationLevel,
                    imageWidthAtSegmentationLevel,
                    (uint32_t) tileHeigthAtSegmentationLevel,
                    ImageDepth::_8U,
                    outputPath + "mask.tif"
            );

            localMaskGenerationGraph = new htgs::TaskGraphConf<htgs::MemoryData<fi::View<T>>, VoidData>;
            localMaskGenerationGraph->addEdge(fastImageTask, sobelFilter2);
            localMaskGenerationGraph->addEdge(sobelFilter2, viewSegmentation);
            localMaskGenerationGraph->addEdge(viewSegmentation, maskFilter);
            localMaskGenerationGraph->addEdge(maskFilter, writeMask);
            localMaskGenerationGraphRuntime = new htgs::TaskGraphRuntime(localMaskGenerationGraph);
            localMaskGenerationGraphRuntime->executeRuntime();
            fi->requestAllTiles(true, pyramidLevelToRequestForSegmentation);
            localMaskGenerationGraph->finishedProducingData();
            localMaskGenerationGraphRuntime->waitForRuntime();
            //FOR DEBUGGING
            localMaskGenerationGraph->writeDotToFile("SegmentationGraph.xdot", DOTGEN_COLOR_COMP_TIME);
            delete fi;
            delete localMaskGenerationGraphRuntime;
            auto endSegmentation = std::chrono::high_resolution_clock::now();
        }

        /**
         * Generate a list of features.
         * @tparam T
         * @param threshold - Threshold value for the foreground.
         * @param options - Options for configuring EGT execution.
         * @param segmentationOptions - Parameters used for the segmentation.
         */
        std::shared_ptr<ListBlobs>
        runSegmentation(T threshold, EGTOptions *options, SegmentationOptions *segmentationOptions, DerivedSegmentationParams<T> segmentationParams) {
            htgs::TaskGraphConf<htgs::MemoryData<fi::View<T>>, ListBlobs> *segmentationGraph;
            htgs::TaskGraphRuntime *segmentationRuntime;

            uint32_t pyramidLevelToRequestForSegmentation = options->pyramidLevel;
            //radius of 2 since we need first apply convo, obtain a gradient of size n+1,
            //and then check the ghost region for potential merges for each tile of size n.
            uint32_t segmentationRadius = 2;

            auto tileLoader2 = new PyramidTiledTiffLoader<T>(options->inputPath, options->nbLoaderThreads);
            auto *fi = new fi::FastImage<T>(tileLoader2, segmentationRadius);
            fi->getFastImageOptions()->setNumberOfViewParallel(options->concurrentTiles);
            auto fastImage2 = fi->configureAndMoveToTaskGraphTask("Fast Image 2");
            imageHeightAtSegmentationLevel = fi->getImageHeight(pyramidLevelToRequestForSegmentation);
            imageWidthAtSegmentationLevel = fi->getImageWidth(pyramidLevelToRequestForSegmentation);
            tileHeightAtSegmentationLevel = fi->getTileHeight(pyramidLevelToRequestForSegmentation);
            tileWidthAtSegmentationLevel = fi->getTileWidth(pyramidLevelToRequestForSegmentation);
            uint32_t nbTiles = fi->getNumberTilesHeight(pyramidLevelToRequestForSegmentation) *
                               fi->getNumberTilesWidth(pyramidLevelToRequestForSegmentation);
            auto sobelFilter2 = new EGTSobelFilter<T>(options->concurrentTiles, options->imageDepth, 1, 1);

            auto viewSegmentation = new EGTGradientViewAnalyzer<T>(options->concurrentTiles,
                                                           imageHeightAtSegmentationLevel,
                                                           imageWidthAtSegmentationLevel,
                                                           tileHeightAtSegmentationLevel,
                                                           tileWidthAtSegmentationLevel,
                                                           4,
                                                           threshold,
                                                           segmentationOptions,
                                                           segmentationParams);
            auto labelingFilter = new ViewAnalyseFilter<T>(options->concurrentTiles);
            auto merge = new BlobMerger<T>(imageHeightAtSegmentationLevel,
                                        imageWidthAtSegmentationLevel,
                                        nbTiles,
                                        options,
                                        segmentationOptions,
                                        segmentationParams);
            segmentationGraph = new htgs::TaskGraphConf<htgs::MemoryData<fi::View<T>>, ListBlobs>;
            segmentationGraph->addEdge(fastImage2, sobelFilter2);
            segmentationGraph->addEdge(sobelFilter2, viewSegmentation);
            segmentationGraph->addEdge(viewSegmentation, labelingFilter);
            segmentationGraph->addEdge(labelingFilter, merge);
            segmentationGraph->addGraphProducerTask(merge);

            htgs::TaskGraphSignalHandler::registerTaskGraph(segmentationGraph);
            htgs::TaskGraphSignalHandler::registerSignal(SIGTERM);

            segmentationRuntime = new htgs::TaskGraphRuntime(segmentationGraph);
            segmentationRuntime->executeRuntime();
            fi->requestAllTiles(true, pyramidLevelToRequestForSegmentation);
            segmentationGraph->finishedProducingData();

            //we only generate one output, the list of all objects
            std::shared_ptr<ListBlobs> blobs = segmentationGraph->consumeData();
            segmentationRuntime->waitForRuntime();
            delete fi;
            delete segmentationRuntime;
            return blobs;
        }

        void runBinaryMaskGeneration(std::shared_ptr<ListBlobs> blob, SegmentationOptions *segmentationOptions) {
            VLOG(1) << "generating a segmentation mask";
            //    blob->erode(segmentationOptions);

            auto fc = new FeatureCollection();
            fc->createFCFromCompactListBlobs(blob.get(), imageHeightAtSegmentationLevel, imageWidthAtSegmentationLevel);
            //   fc->createBlackWhiteMaskStreaming("output-stream.tiff", (uint32_t)tileWidthAtSegmentationLevel);
            fc->createBlackWhiteMask("output-stream.tiff", (uint32_t) tileWidthAtSegmentationLevel);
            delete fc;
        }


        void featureExtraction(std::shared_ptr<ListBlobs> blobs, EGTOptions *options) {

            const uint32_t pyramidLevelToRequestforThreshold = options->pyramidLevel;
            const uint32_t radiusForFeatureExtraction = 0;

            auto tileLoader = new PyramidTiledTiffLoader<T>(options->inputPath, options->nbLoaderThreads);
            auto *fi = new fi::FastImage<T>(tileLoader, radiusForFeatureExtraction);
            fi->getFastImageOptions()->setNumberOfViewParallel(options->concurrentTiles);
            fi->configureAndRun();

            uint32_t
                    indexRowMin = 0,
                    indexRowMax = 0,
                    indexColMin = 0,
                    indexColMax = 0;

            uint32_t
                    minRow = 0,
                    maxRow = 0,
                    minCol = 0,
                    maxCol = 0;

            for (auto blob : blobs->_blobs) {
                T pixValue = 0,
                        featureMeanIntensity = 0;
                uint64_t count = 0;
                uint64_t sum = 0;
                uint32_t featureTotalTileCount = 0;

                auto feature = blob->getFeature();

                //We cannot use fc::feature because we reimplemented it in the egt namespace
                //let's request all tiles for each feature
                const auto &bb = feature->getBoundingBox();
                indexRowMin = bb.getUpperLeftRow() / fi->getTileHeight();
                indexColMin = bb.getUpperLeftCol() / fi->getImageWidth();
                // Handle border case
                if (bb.getBottomRightCol() == fi->getImageWidth())
                    indexColMax = fi->getNumberTilesWidth();
                else
                    indexColMax = (bb.getBottomRightCol() / fi->getTileWidth()) + 1;
                if (bb.getBottomRightRow() == fi->getImageHeight())
                    indexRowMax = fi->getNumberTilesHeight();
                else
                    indexRowMax = (bb.getBottomRightRow() / fi->getTileHeight()) + 1;
                for (auto indexRow = indexRowMin; indexRow < indexRowMax; ++indexRow) {
                    for (auto indexCol = indexColMin; indexCol < indexColMax; ++indexCol) {
                        fi->requestTile(indexRow,indexCol, false);
                        featureTotalTileCount++;
                    }
                }

                auto tileCount = 0;

                while (tileCount < featureTotalTileCount) {
                    auto view = fi->getAvailableViewBlocking();
                    if (view != nullptr) {
                        minRow = std::max(view->get()->getGlobalYOffset(),
                                          feature->getBoundingBox().getUpperLeftRow());
                        maxRow = std::min(
                                view->get()->getGlobalYOffset() + view->get()->getTileHeight(),
                                feature->getBoundingBox().getBottomRightRow());
                        minCol = std::max(view->get()->getGlobalXOffset(),
                                          feature->getBoundingBox().getUpperLeftCol());
                        maxCol = std::min(
                                view->get()->getGlobalXOffset() + view->get()->getTileWidth(),
                                feature->getBoundingBox().getBottomRightCol());

                        for (auto row = minRow; row < maxRow; ++row) {
                            for (auto col = minCol; col < maxCol; ++col) {
                                if (feature->isImagePixelInBitMask(row, col)) {
                                    pixValue = view->get()->getPixel(
                                            row - view->get()->getGlobalYOffset(),
                                            col - view->get()->getGlobalXOffset());
                                    sum += pixValue;
                                    count++;
                                }
                            }
                        }
                        view->releaseMemory();
                        tileCount ++;
                    }
                }

                assert(count != 0);
                assert(sum != 0);
                featureMeanIntensity = std::round(sum / count);
                meanIntensities[blob] = featureMeanIntensity;
            }

            fi->finishedRequestingTiles();
            fi->waitForGraphComplete();
            delete fi;
        }


    private:
        uint32_t imageHeightAtSegmentationLevel{},
                imageWidthAtSegmentationLevel{},
                tileWidthAtSegmentationLevel{},
                tileHeightAtSegmentationLevel{};


        std::map<Blob*, T> meanIntensities{};

    };


}


#endif //NEWEGT_EGT_H
