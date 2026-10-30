// Display an SDF file with VTK.

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
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkWarpScalar.h>

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

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

Reader create_reader()
{
    sdfio_reader *reader = nullptr;
    sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_add_binary(reader);
    sdfio_reader_add_ascii(reader);
    return {reader, sdfio_reader_free};
}

float *heights_of(vtkImageData *image)
{
    return static_cast<float *>(image->GetScalarPointer());
}

// The rows of vtkImageData start at y = 0 and the coordinate system is
// right-handed, as in the standard, so nothing is flipped and the heights are
// read into the array of the image.
vtkSmartPointer<vtkImageData> read_image(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_reader_header(reader);
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(static_cast<int>(sdfio_header_columns(header)),
                         static_cast<int>(sdfio_header_rows(header)), 1);
    image->SetSpacing(sdfio_header_x_scale(header),
                      sdfio_header_y_scale(header), 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);
    check(sdfio_read_heights(reader, sdfio_layout_packed(SDFIO_TYPE_FLOAT),
                             heights_of(image)),
          sdfio_reader_error_message(reader));
    return image;
}

void hide_points_not_measured(vtkImageData *image)
{
    const vtkIdType count = image->GetNumberOfPoints();
    const float *heights = heights_of(image);
    vtkNew<vtkUnsignedCharArray> hidden;
    hidden->SetName(vtkDataSetAttributes::GhostArrayName());
    hidden->SetNumberOfValues(count);
    for (vtkIdType i = 0; i < count; ++i)
    {
        hidden->SetValue(
            i, std::isnan(heights[i]) ? vtkDataSetAttributes::HIDDENPOINT : 0);
    }
    image->GetPointData()->AddArray(hidden);
}

// The height field: the polygons of the image, warped by the scalars times a
// factor. The hidden points are set to 0 in a copy, as a NaN would reach the
// bounds.
vtkSmartPointer<vtkPolyData> height_field(vtkImageData *image, double factor)
{
    vtkNew<vtkImageData> patched;
    patched->DeepCopy(image);
    float *heights = heights_of(patched);
    const vtkIdType count = patched->GetNumberOfPoints();
    for (vtkIdType i = 0; i < count; ++i)
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

// The relief is raised to a quarter of the width of the surface, as a few
// micrometres of height would not be seen.
void view(vtkImageData *image)
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
    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    interactor->SetRenderWindow(window);
    interactor->SetInteractorStyle(style);
    window->Render();
    interactor->Start();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " file.sdf\n";
        return 1;
    }
    const char *path = argv[1];
    try
    {
        Reader reader = create_reader();
        check(sdfio_open_file(reader.get(), path),
              sdfio_reader_error_message(reader.get()));
        vtkSmartPointer<vtkImageData> image = read_image(reader.get());
        hide_points_not_measured(image);
        view(image);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
