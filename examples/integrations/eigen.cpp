// Eigen: a surface as a matrix, and its form removed.
//
// Eigen stores a matrix column by column. The heights are read into it where it
// is: the strides are those of the matrix, the element stride is the distance
// to the next column and the row stride the distance to the next row. A point
// that was not measured is NaN, which Eigen's array operations can mask. A
// plane is fitted to the measured points by least squares and taken off the
// matrix, which is written to a file in the same layout.
//
// The matrix is double, as the numerical code of Eigen usually is. What the
// file holds is a matter of its own: the file here is binary32, and the
// heights are converted on the way in and out.

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <Eigen/Dense>

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

sdfio_format layout_of(const Eigen::MatrixXd &matrix)
{
    sdfio_format format = sdfio_metres(SDFIO_FLOAT64);
    format.element_stride =
        static_cast<ptrdiff_t>(matrix.outerStride() * sizeof(double));
    return format;
}

ptrdiff_t row_stride_of(const Eigen::MatrixXd &matrix)
{
    return static_cast<ptrdiff_t>(matrix.innerStride() * sizeof(double));
}

Eigen::MatrixXd read_matrix(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    Eigen::MatrixXd heights(static_cast<Eigen::Index>(header->rows),
                            static_cast<Eigen::Index>(header->columns));
    check(sdfio_read_heights(reader, layout_of(heights), heights.data(),
                             row_stride_of(heights)),
          sdfio_reader_error_message(reader));
    return heights;
}

// z = offset + slope_x * x + slope_y * y, with x and y counted in samples
struct Plane
{
    double offset;
    double slope_x;
    double slope_y;
};

Plane fit_plane(const Eigen::MatrixXd &heights)
{
    const Eigen::Index count = (heights.array() == heights.array()).count();
    Eigen::MatrixXd design(count, 3);
    Eigen::VectorXd z(count);
    Eigen::Index row = 0;
    for (Eigen::Index x = 0; x < heights.cols(); ++x)
    {
        for (Eigen::Index y = 0; y < heights.rows(); ++y)
        {
            if (!std::isnan(heights(y, x)))
            {
                design.row(row) << 1.0, static_cast<double>(x),
                    static_cast<double>(y);
                z(row++) = heights(y, x);
            }
        }
    }
    const Eigen::Vector3d solution = design.colPivHouseholderQr().solve(z);
    return {solution(0), solution(1), solution(2)};
}

void remove_plane(Eigen::MatrixXd &heights, const Plane &plane)
{
    for (Eigen::Index x = 0; x < heights.cols(); ++x)
    {
        for (Eigen::Index y = 0; y < heights.rows(); ++y)
        {
            heights(y, x) -= plane.offset
                             + plane.slope_x * static_cast<double>(x)
                             + plane.slope_y * static_cast<double>(y);
        }
    }
}

// The root mean square about the mean, over the measured points.
double rms_of(const Eigen::MatrixXd &heights)
{
    const Eigen::ArrayXXd measured =
        (heights.array() == heights.array()).cast<double>();
    const Eigen::ArrayXXd z = (measured > 0.0).select(heights.array(), 0.0);
    const double count = measured.sum();
    const double mean = z.sum() / count;
    return std::sqrt((measured * (z - mean).square()).sum() / count);
}

void write_matrix(const char *path, sdfio_header header,
                  const Eigen::MatrixXd &heights)
{
    header.data_type = SDFIO_DATA_BINARY32;
    header.z_scale = SDFIO_AUTO_Z_SCALE;
    Writer writer(sdfio_writer_new(NULL), sdfio_writer_free);
    check(sdfio_create_file(writer.get(), path, &header),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights(writer.get(), layout_of(heights), heights.data(),
                              row_stride_of(heights)),
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
        Reader reader(sdfio_reader_new(NULL), sdfio_reader_free);
        check(sdfio_open_file(reader.get(), path),
              sdfio_reader_error_message(reader.get()));
        Eigen::MatrixXd heights = read_matrix(reader.get());
        const double before = rms_of(heights);
        const Plane plane = fit_plane(heights);
        remove_plane(heights, plane);
        const double after = rms_of(heights);
        write_matrix("levelled.sdf", *sdfio_get_header(reader.get()), heights);
        const sdfio_header *header = sdfio_get_header(reader.get());
        std::cout << "Eigen " << EIGEN_WORLD_VERSION << "."
                  << EIGEN_MAJOR_VERSION << "." << EIGEN_MINOR_VERSION << ": "
                  << heights.cols() << " x " << heights.rows() << ", tilt "
                  << plane.slope_x / header->x_scale * 1e6 << " and "
                  << plane.slope_y / header->y_scale * 1e6 << " urad, rms "
                  << before * 1e6 << " um before and " << after * 1e6
                  << " um after\n";
        return after <= before ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
