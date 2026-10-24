// Eigen: reads a surface into a matrix, removes the plane that fits its heights
// best, and writes the matrix back. The argument is the SDF file to read.

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
using Header = std::unique_ptr<sdfio_header, decltype(&sdfio_header_free)>;

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

// A header that the program can change: the copy of the header of a file.
Header copy_header(const sdfio_header *source)
{
    sdfio_header *header = nullptr;
    check(sdfio_header_create(sdfio_default_allocator(), &header),
          "Creating the header failed");
    Header owned(header, sdfio_header_free);
    check(sdfio_header_copy(header, source), "Copying the header failed");
    return owned;
}

Reader create_reader()
{
    sdfio_reader *reader = nullptr;
    check(sdfio_reader_create(sdfio_default_allocator(), &reader),
          "Creating the reader failed");
    Reader owned(reader, sdfio_reader_free);
    check(sdfio_reader_add_binary_format(reader),
          "Adding the binary format failed");
    check(sdfio_reader_add_ascii_format(reader),
          "Adding the ASCII format failed");
    return owned;
}

Writer create_writer()
{
    sdfio_writer *writer = nullptr;
    check(sdfio_writer_create(sdfio_default_allocator(), &writer),
          "Creating the writer failed");
    Writer owned(writer, sdfio_writer_free);
    check(sdfio_writer_add_binary_format(writer),
          "Adding the binary format failed");
    return owned;
}

// Eigen stores a matrix column by column: the elements of a row are one column
// apart, and the rows are one element apart. The matrix is double, whatever the
// data type of the file, and the library converts the heights.
sdfio_layout layout_of(const Eigen::MatrixXd &matrix)
{
    sdfio_layout layout = sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.element_stride =
        static_cast<ptrdiff_t>(matrix.outerStride() * sizeof(double));
    layout.row_stride =
        static_cast<ptrdiff_t>(matrix.innerStride() * sizeof(double));
    return layout;
}

Eigen::MatrixXd read_matrix(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_get_header(reader);
    Eigen::MatrixXd heights(
        static_cast<Eigen::Index>(sdfio_header_rows(header)),
        static_cast<Eigen::Index>(sdfio_header_columns(header)));
    check(sdfio_read_heights(reader, layout_of(heights), heights.data()),
          sdfio_reader_error_message(reader));
    return heights;
}

// z = offset + slope_x * x + slope_y * y, with x and y counted in points
struct Plane
{
    double offset;
    double slope_x;
    double slope_y;
};

Plane fit_plane(const Eigen::MatrixXd &heights)
{
    // A point that is not measured is NaN and is left out.
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

void write_matrix(const char *path, const sdfio_header *source,
                  const Eigen::MatrixXd &heights)
{
    Header header = copy_header(source);
    check(sdfio_header_set_data_type(header.get(), SDFIO_TYPE_FLOAT),
          "Setting the data type failed");
    Writer writer = create_writer();
    check(sdfio_create_file(writer.get(), path, header.get()),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_write_heights(writer.get(), layout_of(heights), heights.data()),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_end_trailer(writer.get()),
          sdfio_writer_error_message(writer.get()));
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
        Eigen::MatrixXd heights = read_matrix(reader.get());
        remove_plane(heights, fit_plane(heights));
        write_matrix("levelled.sdf", sdfio_get_header(reader.get()), heights);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
