# Integration examples

Programs that use the library together with libraries of the sector. Each one
is built if its library is found and skipped with a note if not, with
`-DSDFIO_BUILD_INTEGRATIONS=ON`; they are for a developer's machine.

| Example | Language | What it shows |
| --- | --- | --- |
| `opencv.cpp` | C++ | A depth image in a `cv::Mat`: the negative row stride that turns a surface into an image without a copy, a region of interest, and 16-bit counts that come back unchanged |
| `vtk.cpp` | C++ | A surface as `vtkImageData` read in place into its array, the points that were not measured hidden through the ghost array so the cells around them are left out, a height field by warping, the file of VTK, and the heights written back |
| `pcl.cpp` | C++ | A surface as an organized point cloud: the heights are read into the `z` of an array of points with the element stride, the reference plane of a form removal is fitted, and the levelled `z` is written back from the same array. PCL works in units of about one, so the cloud is in micrometres |
| `opencl.c` | C | A surface on the GPU: the heights are read into a mapped buffer with the pitch a device wants, smoothed, the areal parameters Sa and Sq are computed there, and the result is written out of the buffer, without a copy on the host |

The OpenCL example returns 77, and the test is skipped, if the machine has no
OpenCL device.
