# Integration examples

Programs that use the library together with libraries of the sector. Each one
is built if its library is found and skipped with a note if not, with
`-DSDFIO_BUILD_INTEGRATIONS=ON`; they are for a developer's machine.

| Example | Language | What it shows |
| --- | --- | --- |
| `image.cpp` | C++ | The class most programs write first: a C++ image over the C interface, with the handles owned by objects, failed calls as exceptions, and the heights read into the `std::vector` where it is |
| `opencv.cpp` | C++ | A depth image in a `cv::Mat`: the negative row stride that turns a surface into an image without a copy, and a region of interest |
| `vtk.cpp` | C++ | A surface as `vtkImageData` read in place into its array, the points that were not measured hidden through the ghost array so the cells around them are left out, a height field by warping, the file of VTK, and the heights written back |
| `pcl.cpp` | C++ | A surface as an organized point cloud: the heights are read into the `z` of an array of points with the element stride, the reference plane of a form removal is fitted, and the levelled `z` is written back from the same array. PCL works in units of about one, so the cloud is in micrometres |

