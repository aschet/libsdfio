// A C++ class for a surface over the C interface. It owns the handles of the
// library, reports a failed call as an exception, and gives the heights as
// image[y][x].

#include <sdfio/file.h>
#include <sdfio/sdfio.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using Reader = std::unique_ptr<sdfio_reader, decltype(&sdfio_reader_free)>;
using Writer = std::unique_ptr<sdfio_writer, decltype(&sdfio_writer_free)>;

sdfio_config make_config()
{
    sdfio_config config = {};
    config.reader_formats[0] = &sdfio_binary_reader_format;
    config.reader_formats[1] = &sdfio_ascii_reader_format;
    config.writer_formats[0] = &sdfio_binary_writer_format;
    config.writer_formats[1] = &sdfio_ascii_writer_format;
    return config;
}

const sdfio_config config = make_config();

void check(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        throw std::runtime_error(std::string(sdfio_status_string(status)) + ": "
                                 + message);
    }
}

class Image
{
  public:
    Image(std::size_t columns, std::size_t rows, double x_scale, double y_scale)
        : columns_(columns), rows_(rows), x_scale_(x_scale), y_scale_(y_scale),
          heights_(std::make_unique<float[]>(columns * rows))
    {
        // A point that is not measured is NaN.
        std::fill_n(heights_.get(), columns * rows, NAN);
    }

    static Image read(const std::filesystem::path &path)
    {
        Reader reader(sdfio_reader_new(&config), sdfio_reader_free);
        check(sdfio_open_file(reader.get(), path.string().c_str()),
              sdfio_reader_error_message(reader.get()));
        const sdfio_header *header = sdfio_get_header(reader.get());
        Image image(header->columns, header->rows, header->x_scale,
                    header->y_scale);
        check(sdfio_read_heights(reader.get(), SDFIO_TYPE_FLOAT,
                                 image.heights_.get()),
              sdfio_reader_error_message(reader.get()));
        return image;
    }

    void write(const std::filesystem::path &path) const
    {
        sdfio_header header;
        sdfio_header_init(&header);
        header.data_type = SDFIO_TYPE_INT16;
        header.columns = columns_;
        header.rows = rows_;
        header.x_scale = x_scale_;
        header.y_scale = y_scale_;
        Writer writer(sdfio_writer_new(&config), sdfio_writer_free);
        check(sdfio_create_file(writer.get(), path.string().c_str(), &header),
              sdfio_writer_error_message(writer.get()));
        check(
            sdfio_write_heights(writer.get(), SDFIO_TYPE_FLOAT, heights_.get()),
            sdfio_writer_error_message(writer.get()));
        check(sdfio_end_trailer(writer.get()),
              sdfio_writer_error_message(writer.get()));
    }

    std::size_t columns() const
    {
        return columns_;
    }
    std::size_t rows() const
    {
        return rows_;
    }

    // The library reads and writes the rows one after the other in one block
    // of memory, so that the row y starts at y * columns().
    float *operator[](std::size_t y)
    {
        return heights_.get() + y * columns_;
    }
    const float *operator[](std::size_t y) const
    {
        return heights_.get() + y * columns_;
    }

  private:
    std::size_t columns_;
    std::size_t rows_;
    double x_scale_;
    double y_scale_;
    std::unique_ptr<float[]> heights_;
};

Image ripples()
{
    Image image(160, 120, 1e-6, 1e-6);
    for (std::size_t y = 0; y < image.rows(); ++y)
    {
        for (std::size_t x = 0; x < image.columns(); ++x)
        {
            const bool hole = x > 60 && x < 70 && y > 40 && y < 50;
            if (!hole)
            {
                image[y][x] = static_cast<float>(
                    2e-6 * std::sin(0.1 * static_cast<double>(x))
                    * std::cos(0.07 * static_cast<double>(y)));
            }
        }
    }
    return image;
}

} // namespace

int main()
{
    try
    {
        ripples().write("image.sdf");
        const Image image = Image::read("image.sdf");
        std::cout << image.columns() << " x " << image.rows() << "\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
