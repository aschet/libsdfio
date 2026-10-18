# Integration examples

Programs that use the library together with libraries of the sector. Each one
is built if its library is found and skipped with a note if not, with
`-DSDFIO_BUILD_INTEGRATIONS=ON`; they are for a developer's machine.

| Example | Language | What it shows |
| --- | --- | --- |
| `image.cpp` | C++ | The class most programs write first: a C++ image over the C interface, with the handles owned by objects, failed calls as exceptions, and the heights read into the `std::vector` where it is |
| `opencv.cpp` | C++ | A depth image in a `cv::Mat`: the negative row stride that turns a surface into an image without a copy, and a region of interest |
| `vtk.cpp` | C++ | A surface as `vtkImageData` read in place into its array, the points that were not measured hidden through the ghost array so the cells around them are left out, a height field by warping, the file of VTK, the heights written back, and a viewer of a few lines (`--show` opens a window with the mouse to turn and zoom; without it the picture goes offscreen into a PNG) |
| `eigen.cpp` | C++ | A surface as an `Eigen::MatrixXf`: the heights are read into the matrix where it is, whatever its storage order, through the element stride and the row stride, a plane is fitted by least squares for the form removal, and the matrix is written back in the same layout |

