# sudo apt install glslc
pushd "$(dirname "$0")"
glslc shaders/spinning_triangle.vert -o ../assets/spinning_triangle_vert.spv
glslc shaders/spinning_triangle.frag -o ../assets/spinning_triangle_frag.spv
popd
