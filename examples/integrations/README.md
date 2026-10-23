# Integration examples

Programs that use the library together with libraries of the sector. Each one
is built if its library is found and skipped with a note if not, with
`-DSDFIO_BUILD_INTEGRATIONS=ON`; they are for a developer's machine.

| Example | Language | What it shows |
| --- | --- | --- |
| `image.cpp` | C++ | The class most programs write first: a C++ image over the C interface, with the handles owned by objects, failed calls as exceptions, and the heights read into a two-dimensional array, `image[y][x]`, where it is |
| `opencv.cpp` | C++ | A surface as an image in a `cv::Mat`: the negative row stride that turns a surface into an image without a copy, and a region of interest |
| `vtk.cpp` | C++ | A surface as `vtkImageData` read in place into its array, the points that were not measured hidden through the ghost array so the cells around them are left out, and a viewer of a few lines that shows the height field in a window with the mouse to turn and zoom. The argument is the SDF file to show |
| `eigen.cpp` | C++ | A surface as an `Eigen::MatrixXd`: the heights are read into the matrix where it is, whatever its storage order, and converted from the type of the file, through the element stride and the row stride, a plane is fitted by least squares for the form removal, and the matrix is written back in the same layout |

