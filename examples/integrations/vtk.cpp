// VTK: a surface as image data, a height field by warping, and the file of VTK.
//
// The rows of vtkImageData start at y = 0 and the system is right-handed, as in
// the standard: nothing is flipped, and the heights are read into the array of
// the image where it is. A point that was not measured is NaN in the scalars,
// and VTK is told which points they are by its ghost array, so that the cells
// that touch one are left out.

#include <sdfio/sdfio.h>

#include <vtkDataSetAttributes.h>
#include <vtkGeometryFilter.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVersion.h>
#include <vtkWarpScalar.h>
#include <vtkXMLImageDataReader.h>
#include <vtkXMLImageDataWriter.h>

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using Reader = std::unique_ptr<sdfio_reader, decltype(&sdfio_reader_free)>;
using Writer = std::unique_ptr<sdfio_writer, decltype(&sdfio_writer_free)>;

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

float *heights_of(vtkImageData *image)
{
    return static_cast<float *>(image->GetScalarPointer());
}

vtkSmartPointer<vtkImageData> read_image(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(static_cast<int>(header->columns),
                         static_cast<int>(header->rows), 1);
    image->SetSpacing(header->x_scale, header->y_scale, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);
    check(sdfio_read_heights(
              reader, sdfio_metres(SDFIO_FLOAT32), heights_of(image),
              static_cast<ptrdiff_t>(header->columns * sizeof(float))),
          sdfio_reader_error_message(reader));
    return image;
}

void hide_missing(vtkImageData *image)
{
    const vtkIdType count = image->GetNumberOfPoints();
    vtkNew<vtkUnsignedCharArray> ghosts;
    ghosts->SetName(vtkDataSetAttributes::GhostArrayName());
    ghosts->SetNumberOfValues(count);
    for (vtkIdType i = 0; i < count; ++i)
    {
        ghosts->SetValue(i, std::isnan(heights_of(image)[i])
                                ? vtkDataSetAttributes::HIDDENPOINT
                                : 0);
    }
    image->GetPointData()->AddArray(ghosts);
}

vtkIdType visible_cells(vtkImageData *image)
{
    vtkNew<vtkGeometryFilter> surface;
    surface->SetInputData(image);
    surface->Update();
    return surface->GetOutput()->GetNumberOfCells();
}

// The height field: the polygons of the image, warped by the scalars. The
// hidden points are set to 0 in a copy, as a NaN would reach the bounds.
std::array<double, 2> height_field_range(vtkImageData *image)
{
    vtkNew<vtkImageData> patched;
    patched->DeepCopy(image);
    float *heights = heights_of(patched);
    for (vtkIdType i = 0; i < patched->GetNumberOfPoints(); ++i)
    {
        heights[i] = std::isnan(heights[i]) ? 0.0F : heights[i];
    }
    vtkNew<vtkGeometryFilter> surface;
    surface->SetInputData(patched);
    vtkNew<vtkWarpScalar> warp;
    warp->SetInputConnection(surface->GetOutputPort());
    warp->SetScaleFactor(1.0);
    warp->UseNormalOff();
    warp->SetNormal(0.0, 0.0, 1.0);
    warp->Update();
    double bounds[6];
    warp->GetOutput()->GetBounds(bounds);
    return {bounds[4], bounds[5]};
}

vtkSmartPointer<vtkImageData> round_trip(vtkImageData *image, const char *path)
{
    vtkNew<vtkXMLImageDataWriter> writer;
    writer->SetFileName(path);
    writer->SetInputData(image);
    writer->Write();
    vtkNew<vtkXMLImageDataReader> reader;
    reader->SetFileName(path);
    reader->Update();
    return reader->GetOutput();
}

void write_heights(const char *path, sdfio_header header, vtkImageData *image)
{
    header.data_type = SDFIO_DATA_BINARY32;
    header.z_scale = SDFIO_AUTO_Z_SCALE;
    Writer writer(sdfio_writer_new(), sdfio_writer_free);
    check(sdfio_create_file(writer.get(), path, &header),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights(
              writer.get(), sdfio_metres(SDFIO_FLOAT32), heights_of(image),
              static_cast<ptrdiff_t>(header.columns * sizeof(float))),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_trailer(writer.get(), nullptr, 0),
          sdfio_writer_error_message(writer.get()));
}

} // namespace

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : SDFIO_EXAMPLE_FILE;
    try
    {
        Reader reader(sdfio_reader_new(), sdfio_reader_free);
        check(sdfio_open_file(reader.get(), path),
              sdfio_reader_error_message(reader.get()));
        vtkSmartPointer<vtkImageData> image = read_image(reader.get());
        const vtkIdType all_cells = visible_cells(image);
        hide_missing(image);
        const vtkIdType cells = visible_cells(image);
        const std::array<double, 2> z_range = height_field_range(image);
        const vtkSmartPointer<vtkImageData> back =
            round_trip(image, "surface.vti");
        write_heights("again.sdf", *sdfio_get_header(reader.get()), back);
        std::cout << "VTK " << vtkVersion::GetVTKVersion() << ": " << cells
                  << " of " << all_cells << " cells, z from "
                  << z_range[0] * 1e6 << " to " << z_range[1] * 1e6 << " um\n";
        return cells < all_cells ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
