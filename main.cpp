#include <iostream>
#include <egt/api/EGT.h>

int main() {
 //   std::string path = "/Users/gerardin/Documents/images/egt_test/stitchedImage/inputs/stitched_c01t020p1_tiled1024_pyramid_lzw.ome.tif";

   // std::string path = "/Users/gerardin/Documents/images/egt_test/inputs/phase_image_002_tiled256_pyramid.tif";
    std::string path = "/home/gerardin/Documents/images/egt-test-images/egt_test/inputs/phase_image_002_tiled256_pyramid.tif";
//  std::string path = "/home/gerardin/Documents/images/egt-test-images/egt_test/inputs/phase_image_002_tiled256.tif";
//     std::string path = "/home/gerardin/Documents/images/egt-test-images/datasetSegmentationTest2/test2_160px_tiled64_8bit.tif";
//    std::string path = "/home/gerardin/Documents/images/egt-test-images/dataset01/images/test01-tiled.tif";
    auto egt = new egt::EGT<float>();


    //TODO should we choose the userType? We should just get it from the tif image and stick to it.
    egt->run(path);

    delete egt;

    return 0;
}
