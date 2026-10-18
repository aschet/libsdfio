// VTK: a surface as image data, a height field by warping, and the file of VTK.
//
// The rows of vtkImageData start at y = 0 and the system is right-handed, as in
// the standard: nothing is flipped, and the heights are read into the array of
// the image where it is. A point that was not measured is NaN in the scalars,
// and VTK is told which points they are by its ghost array, so that the cells
// that touch one are left out.
//
// The viewer is a few lines at the end: the height field coloured by height,
// turned and zoomed with the mouse. It opens a window with --show, and without
// it draws offscreen into surface.png.

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetAttributes.h>
#include <vtkGeometryFilter.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVersion.h>
#include <vtkWarpScalar.h>
#include <vtkWindowToImageFilter.h>
#include <vtkXMLImageDataReader.h>
#include <vtkXMLImageDataWriter.h>

#include <algorithm>
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

// The height field: the polygons of the image, warped by the scalars times a
// factor. The hidden points are set to 0 in a copy, as a NaN would reach the
// bounds.
vtkSmartPointer<vtkPolyData> height_field(vtkImageData *image, double factor)
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
    warp->SetScaleFactor(factor);
    warp->UseNormalOff();
    warp->SetNormal(0.0, 0.0, 1.0);
    warp->Update();
    return vtkPolyData::SafeDownCast(warp->GetOutput());
}

std::array<double, 2> z_range(vtkImageData *image)
{
    double bounds[6];
    height_field(image, 1.0)->GetBounds(bounds);
    return {bounds[4], bounds[5]};
}

// The viewer. The relief is raised to a quarter of the width of the surface,
// as a few micrometres of height would not be seen. Returns false if the
// machine cannot draw.
bool view(vtkImageData *image, bool show, const char *png)
{
    const std::array<double, 2> z = z_range(image);
    double bounds[6];
    image->GetBounds(bounds);
    const double width = std::max(bounds[1] - bounds[0], bounds[3] - bounds[2]);
    const double raise = z[1] > z[0] ? 0.25 * width / (z[1] - z[0]) : 1.0;

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(height_field(image, raise));
    vtkNew<vtkLookupTable> colours;
    colours->SetHueRange(0.667, 0.0);
    colours->Build();
    mapper->SetLookupTable(colours);
    mapper->SetScalarRange(z[0], z[1]);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    vtkNew<vtkRenderer> renderer;
    renderer->AddActor(actor);
    renderer->SetBackground(0.1, 0.1, 0.15);
    renderer->GetActiveCamera()->Elevation(-40.0);
    renderer->GetActiveCamera()->OrthogonalizeViewUp();
    renderer->ResetCamera();
    vtkNew<vtkRenderWindow> window;
    window->AddRenderer(renderer);
    window->SetSize(800, 600);
    if (!window->SupportsOpenGL())
    {
        return false;
    }
    if (show)
    {
        vtkNew<vtkRenderWindowInteractor> interactor;
        vtkNew<vtkInteractorStyleTrackballCamera> style;
        interactor->SetRenderWindow(window);
        interactor->SetInteractorStyle(style);
        window->Render();
        interactor->Start();
        return true;
    }
    window->SetOffScreenRendering(1);
    window->Render();
    vtkNew<vtkWindowToImageFilter> picture;
    picture->SetInput(window);
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(png);
    writer->SetInputConnection(picture->GetOutputPort());
    writer->Write();
    return true;
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
    Writer writer(sdfio_writer_new(NULL), sdfio_writer_free);
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
    const char *path = SDFIO_EXAMPLE_FILE;
    bool show = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--show")
        {
            show = true;
        }
        else
        {
            path = argv[i];
        }
    }
    try
    {
        Reader reader(sdfio_reader_new(NULL), sdfio_reader_free);
        check(sdfio_open_file(reader.get(), path),
              sdfio_reader_error_message(reader.get()));
        vtkSmartPointer<vtkImageData> image = read_image(reader.get());
        const vtkIdType all_cells = visible_cells(image);
        hide_missing(image);
        const vtkIdType cells = visible_cells(image);
        const std::array<double, 2> z = z_range(image);
        const vtkSmartPointer<vtkImageData> back =
            round_trip(image, "surface.vti");
        write_heights("again.sdf", *sdfio_get_header(reader.get()), back);
        std::cout << "VTK " << vtkVersion::GetVTKVersion() << ": " << cells
                  << " of " << all_cells << " cells, z from " << z[0] * 1e6
                  << " to " << z[1] * 1e6 << " um\n";
        if (!view(image, show, "surface.png"))
        {
            std::cerr << "no OpenGL here: the viewer is skipped\n";
            return 77;
        }
        return cells < all_cells ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
