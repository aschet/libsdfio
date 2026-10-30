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

Header copy_header(const sdfio_header *source)
{
    sdfio_header *header = nullptr;
    sdfio_header_create(sdfio_default_allocator(), &header);
    sdfio_header_copy(header, source);
    return {header, sdfio_header_free};
}

Reader create_reader()
{
    sdfio_reader *reader = nullptr;
    sdfio_reader_create(sdfio_default_allocator(), &reader);
    sdfio_reader_add_binary(reader);
    sdfio_reader_add_ascii(reader);
    return {reader, sdfio_reader_free};
}

Writer create_writer()
{
    sdfio_writer *writer = nullptr;
    sdfio_writer_create(sdfio_default_allocator(), &writer);
    sdfio_writer_add_binary(writer);
    return {writer, sdfio_writer_free};
}

// Eigen stores a matrix column by column: the elements of a row are one column
// apart, and the rows are one element apart. The matrix is double, whatever the
// data type of the file, and the library converts the heights.
sdfio_layout layout_of(const Eigen::MatrixXd &matrix)
{
    sdfio_layout layout = *sdfio_layout_packed(SDFIO_TYPE_DOUBLE);
    layout.element_stride =
        static_cast<ptrdiff_t>(matrix.outerStride() * sizeof(double));
    layout.row_stride =
        static_cast<ptrdiff_t>(matrix.innerStride() * sizeof(double));
    return layout;
}

Eigen::MatrixXd read_matrix(sdfio_reader *reader)
{
    const sdfio_header *header = sdfio_reader_header(reader);
    Eigen::MatrixXd heights(
        static_cast<Eigen::Index>(sdfio_header_rows(header)),
        static_cast<Eigen::Index>(sdfio_header_columns(header)));
    const sdfio_layout layout = layout_of(heights);
    check(sdfio_read_heights(reader, &layout, heights.data()),
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
    const Eigen::Index columns = heights.cols();
    const Eigen::Index rows = heights.rows();
    for (Eigen::Index x = 0; x < columns; ++x)
    {
        const double point_x = static_cast<double>(x);
        for (Eigen::Index y = 0; y < rows; ++y)
        {
            if (!std::isnan(heights(y, x)))
            {
                design.row(row) << 1.0, point_x, static_cast<double>(y);
                z(row++) = heights(y, x);
            }
        }
    }
    const Eigen::Vector3d solution = design.colPivHouseholderQr().solve(z);
    return {solution(0), solution(1), solution(2)};
}

void remove_plane(Eigen::MatrixXd &heights, const Plane &plane)
{
    const Eigen::Index columns = heights.cols();
    const Eigen::Index rows = heights.rows();
    for (Eigen::Index x = 0; x < columns; ++x)
    {
        const double column_offset =
            plane.offset + plane.slope_x * static_cast<double>(x);
        for (Eigen::Index y = 0; y < rows; ++y)
        {
            heights(y, x) -=
                column_offset + plane.slope_y * static_cast<double>(y);
        }
    }
}

void write_matrix(const char *path, const sdfio_header *source,
                  const Eigen::MatrixXd &heights)
{
    Header header = copy_header(source);
    sdfio_header_set_data_type(header.get(), SDFIO_TYPE_FLOAT);
    sdfio_header_set_format(header.get(), SDFIO_BINARY);
    Writer writer = create_writer();
    check(sdfio_create_file(writer.get(), path, header.get()),
          sdfio_writer_error_message(writer.get()));
    const sdfio_layout layout = layout_of(heights);
    check(sdfio_write_heights(writer.get(), &layout, heights.data()),
          sdfio_writer_error_message(writer.get()));
    check(sdfio_end_file(writer.get()),
          sdfio_writer_error_message(writer.get()));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << " input.sdf output.sdf\n";
        return 1;
    }
    const char *input = argv[1];
    const char *output = argv[2];
    try
    {
        Reader reader = create_reader();
        check(sdfio_open_file(reader.get(), input),
              sdfio_reader_error_message(reader.get()));
        Eigen::MatrixXd heights = read_matrix(reader.get());
        remove_plane(heights, fit_plane(heights));
        write_matrix(output, sdfio_reader_header(reader.get()), heights);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
