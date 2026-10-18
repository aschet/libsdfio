// PCL: a surface as an organized point cloud, and its form removed.
//
// The z of every point is read where it is in the array of points: the element
// stride of the format is the size of a point. The sampling intervals give x
// and y. A point that was not measured is NaN in all three coordinates, as PCL
// marks the invalid points of an organized cloud. A plane is fitted to the
// measured points and taken off the z of the cloud, which is written to a file
// from the same array.
//
// The tolerances of PCL are absolute and made for coordinates of about the size
// of one: in metres a surface is at 1e-6 and every sample of a plane is too
// similar. The cloud is in micrometres and goes back to metres for the file.

#include <sdfio/sdfio.h>

#include <pcl/pcl_config.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/sac_model_plane.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>

namespace
{

using Cloud = pcl::PointCloud<pcl::PointXYZ>;
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

constexpr float micrometres_per_metre = 1e6F;

sdfio_format cloud_format()
{
    sdfio_format format = sdfio_metres(SDFIO_FLOAT32);
    format.element_stride = sizeof(pcl::PointXYZ);
    return format;
}

ptrdiff_t row_stride(const Cloud &cloud)
{
    return static_cast<ptrdiff_t>(cloud.width * sizeof(pcl::PointXYZ));
}

Cloud::Ptr read_cloud(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    auto cloud =
        std::make_shared<Cloud>(static_cast<std::uint32_t>(header->columns),
                                static_cast<std::uint32_t>(header->rows));
    check(sdfio_read_heights(reader, cloud_format(), &cloud->points[0].z,
                             row_stride(*cloud)),
          sdfio_reader_error_message(reader));
    for (std::uint32_t y = 0; y < cloud->height; ++y)
    {
        for (std::uint32_t x = 0; x < cloud->width; ++x)
        {
            pcl::PointXYZ &point = (*cloud)(x, y);
            const bool measured = !std::isnan(point.z);
            point.x = measured ? static_cast<float>(x * header->x_scale)
                                     * micrometres_per_metre
                               : NAN;
            point.y = measured ? static_cast<float>(y * header->y_scale)
                                     * micrometres_per_metre
                               : NAN;
            point.z *= micrometres_per_metre;
        }
    }
    cloud->is_dense = false;
    return cloud;
}

Cloud::Ptr measured_points(const Cloud &cloud)
{
    auto points = std::make_shared<Cloud>();
    for (const pcl::PointXYZ &point : cloud.points)
    {
        if (!std::isnan(point.z))
        {
            points->push_back(point);
        }
    }
    return points;
}

// The plane of least squares through the measured points, the reference plane
// of a form removal: a x + b y + c z + d = 0.
std::array<float, 4> fit_plane(const Cloud::Ptr &points)
{
    pcl::SampleConsensusModelPlane<pcl::PointXYZ> model(points);
    pcl::Indices all(points->size());
    std::iota(all.begin(), all.end(), 0);
    const Eigen::VectorXf flat = Eigen::Vector4f(0.0F, 0.0F, 1.0F, 0.0F);
    Eigen::VectorXf coefficients;
    model.optimizeModelCoefficients(all, flat, coefficients);
    return {coefficients[0], coefficients[1], coefficients[2], coefficients[3]};
}

void remove_plane(Cloud &cloud, const std::array<float, 4> &plane)
{
    const auto &[a, b, c, d] = plane;
    for (pcl::PointXYZ &point : cloud.points)
    {
        if (!std::isnan(point.z))
        {
            point.z -= -(a * point.x + b * point.y + d) / c;
        }
    }
}

// The root mean square of z about its mean, in the unit of the cloud.
double rms_of_z(const Cloud &cloud)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (const pcl::PointXYZ &point : cloud.points)
    {
        if (!std::isnan(point.z))
        {
            sum += point.z;
            ++count;
        }
    }
    const double mean = sum / static_cast<double>(count);
    double squares = 0.0;
    for (const pcl::PointXYZ &point : cloud.points)
    {
        if (!std::isnan(point.z))
        {
            squares += (point.z - mean) * (point.z - mean);
        }
    }
    return std::sqrt(squares / static_cast<double>(count));
}

void write_cloud(const char *path, sdfio_header header, Cloud &cloud)
{
    for (pcl::PointXYZ &point : cloud.points)
    {
        point.z /= micrometres_per_metre;
    }
    header.data_type = SDFIO_DATA_BINARY32;
    header.z_scale = SDFIO_AUTO_Z_SCALE;
    Writer writer(sdfio_writer_new(), sdfio_writer_free);
    check(sdfio_create_file(writer.get(), path, &header),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights(writer.get(), cloud_format(), &cloud.points[0].z,
                              row_stride(cloud)),
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
        const Cloud::Ptr cloud = read_cloud(reader.get());
        const Cloud::Ptr points = measured_points(*cloud);
        const double before = rms_of_z(*cloud);
        const std::array<float, 4> plane = fit_plane(points);
        remove_plane(*cloud, plane);
        const double after = rms_of_z(*cloud);
        write_cloud("levelled.sdf", *sdfio_get_header(reader.get()), *cloud);
        std::cout << "PCL " << PCL_VERSION_PRETTY << ": " << points->size()
                  << " of " << cloud->size() << " points, tilt "
                  << -plane[0] / plane[2] * 1e6 << " and "
                  << -plane[1] / plane[2] * 1e6 << " urad, rms " << before
                  << " um before and " << after << " um after\n";
        return after <= before ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
