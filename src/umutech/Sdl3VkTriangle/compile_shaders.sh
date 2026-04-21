# sudo apt install glslc
pushd "$(dirname "$0")"
glslc shaders/triangle.vert -o ../assets/triangle_vert.spv
glslc shaders/triangle.frag -o ../assets/triangle_frag.spv
popd
