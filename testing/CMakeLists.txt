#
# Copyright 2022 Intel
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.
#
# enabled with ENABLE_TESTING
# run in <hdospray build dir>/testing 
# ctest (-VV for verbose)
# resulting images will be put into generated_images folder
# diff tests compare the generated_images to those in the golden_images folder
#

enable_testing()

include(FetchContent)
FetchContent_Populate(
    shoe
    URL https://devimages-cdn.apple.com/ar/photogrammetry/PegasusTrail.usdz
    URL_HASH SHA256=8550c6ec55da0e2916a523f9a3bccf9302b79cf856fdb48c99b1821c98ffaa94
    SOURCE_DIR external/shoe
    )

FetchContent_Populate(
    gramophone
    URL https://developer.apple.com/augmented-reality/quick-look/models/gramophone/gramophone.usdz
    URL_HASH SHA256=a1af44c699d2b7549531f69be4593eb80b2fc4f187a53e643d583f07043b6251
    SOURCE_DIR external/gramophone
    )

FetchContent_Populate(
    toy_biplane
    URL https://developer.apple.com/augmented-reality/quick-look/models/biplane/toy_biplane.usdz
    URL_HASH SHA256=d9edf5f4101bb28a96abcbee0c99e56167225d0dc842333e88003ddd59e39cc8
    SOURCE_DIR external/toy_biplane
    )

FetchContent_Populate(
    kitchen
    URL https://graphics.pixar.com/usd/files/Kitchen_set.zip
    URL_HASH SHA256=413e3b99de635d674815692357382433b5757f52c4fa88576b003acdfaa6ac74
    SOURCE_DIR external/kitchen
    )


set(IMAGE_GEN_CMD usdrecord)
set(IMAGE_GEN_ARGS --renderer OSPRay --imageWidth 800)

make_directory(testing/generated_images)

function(add_ospray_test name input)
add_test(NAME ${name}
    COMMAND ${IMAGE_GEN_CMD} ${IMAGE_GEN_ARGS} external/${name}/${input} generated_images/${name}.png)
set_property(TEST ${name}
PROPERTY ENVIRONMENT HDOSPRAY_SAMPLES_TO_CONVERGENCE=8)

add_test(NAME ${name}_diff
    COMMAND compare -metric AE -fuzz 5% generated_images/${name}.png golden_images/${name}.png /tmp/compare.png)
set_property(TEST ${name}_diff
PROPERTY ENVIRONMENT HDOSPRAY_SAMPLES_TO_CONVERGENCE=8)
endfunction()

add_ospray_test(shoe model.usdc)
add_ospray_test(gramophone gramophone.usdc)
add_ospray_test(toy_biplane toy_biplane.usdc)
add_ospray_test(kitchen Kitchen_set.usd)