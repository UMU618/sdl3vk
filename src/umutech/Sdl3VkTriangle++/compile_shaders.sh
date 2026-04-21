# sudo apt install glslc
pushd "$(dirname "$0")"
glslc shaders/triangle++.vert -o ../assets/triangle++_vert.spv
glslc shaders/triangle++.frag -o ../assets/triangle++_frag.spv
popd
