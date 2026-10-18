// OpenCL: a surface on the GPU.
//
// The heights are read into the memory of a mapped buffer and written out of
// it again, so the surface is never copied on the host. A GPU wants the rows
// of a buffer aligned: the pitch is a multiple of 32 floats, and the row
// stride of the calls is the pitch in bytes. A point that was not measured is
// NaN, which the kernels leave out.

#define CL_TARGET_OPENCL_VERSION 120

#include <sdfio/sdfio.h>

#include <CL/cl.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum
{
    PITCH_ALIGNMENT = 32,
    SUMS_PER_ROW = 4,
    NO_DEVICE = 77
};

#define SOURCE(...) #__VA_ARGS__

static const char kernels[] = SOURCE(
    __kernel void smooth(__global const float *in, __global float *out,
                         uint width, uint height, uint pitch) {
        const int x = (int)get_global_id(0);
        const int y = (int)get_global_id(1);
        if (x >= (int)width || y >= (int)height)
        {
            return;
        }
        const float weights[3] = {6.0f, 4.0f, 1.0f};
        float sum = 0.0f;
        float norm = 0.0f;
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= (int)width || ny >= (int)height)
                {
                    continue;
                }
                const float value = in[ny * pitch + nx];
                if (!isnan(value))
                {
                    const float weight = weights[abs(dx)] * weights[abs(dy)];
                    sum += weight * value;
                    norm += weight;
                }
            }
        }
        const float centre = in[y * pitch + x];
        out[y * pitch + x] = isnan(centre) ? centre : sum / norm;
    }

    __kernel void row_sums(__global const float *data, __global float *sums,
                           uint width, uint pitch, float mean) {
        const int y = (int)get_global_id(0);
        float count = 0.0f;
        float total = 0.0f;
        float squares = 0.0f;
        float deviations = 0.0f;
        for (uint x = 0; x < width; ++x)
        {
            const float value = data[y * pitch + x];
            if (!isnan(value))
            {
                const float d = value - mean;
                count += 1.0f;
                total += value;
                squares += d * d;
                deviations += fabs(d);
            }
        }
        sums[4 * y] = count;
        sums[4 * y + 1] = total;
        sums[4 * y + 2] = squares;
        sums[4 * y + 3] = deviations;
    });

static void fail(const char *what, int code)
{
    fprintf(stderr, "%s: %d\n", what, code);
    exit(EXIT_FAILURE);
}

static void check(cl_int status, const char *what)
{
    if (status != CL_SUCCESS)
    {
        fail(what, status);
    }
}

static void check_sdfio(sdfio_status status, const char *message)
{
    if (status != SDFIO_OK)
    {
        fprintf(stderr, "%s: %s\n", sdfio_status_string(status), message);
        exit(EXIT_FAILURE);
    }
}

static void set_buffer(cl_kernel kernel, cl_uint index, cl_mem buffer)
{
    check(clSetKernelArg(kernel, index, sizeof(cl_mem), (const void *)&buffer),
          "clSetKernelArg");
}

static void set_uint(cl_kernel kernel, cl_uint index, cl_uint value)
{
    check(clSetKernelArg(kernel, index, sizeof value, &value),
          "clSetKernelArg");
}

static cl_device_id first_device(void)
{
    cl_platform_id platforms[8];
    cl_uint count = 0;
    if (clGetPlatformIDs(8, platforms, &count) != CL_SUCCESS || count == 0)
    {
        return NULL;
    }
    const cl_device_type kinds[] = {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_ALL};
    for (size_t kind = 0; kind < 2; ++kind)
    {
        for (cl_uint p = 0; p < count; ++p)
        {
            cl_device_id device = NULL;
            if (clGetDeviceIDs(platforms[p], kinds[kind], 1, &device, NULL)
                == CL_SUCCESS)
            {
                return device;
            }
        }
    }
    return NULL;
}

static cl_mem make_buffer(cl_context context, size_t floats)
{
    cl_int status = CL_SUCCESS;
    cl_mem buffer =
        clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                       floats * sizeof(float), NULL, &status);
    check(status, "clCreateBuffer");
    return buffer;
}

static float *map(cl_command_queue queue, cl_mem buffer, size_t floats,
                  cl_map_flags flags)
{
    cl_int status = CL_SUCCESS;
    float *memory =
        clEnqueueMapBuffer(queue, buffer, CL_TRUE, flags, 0,
                           floats * sizeof(float), 0, NULL, NULL, &status);
    check(status, "clEnqueueMapBuffer");
    return memory;
}

static void unmap(cl_command_queue queue, cl_mem buffer, float *memory)
{
    check(clEnqueueUnmapMemObject(queue, buffer, memory, 0, NULL, NULL),
          "clEnqueueUnmapMemObject");
    check(clFinish(queue), "clFinish");
}

typedef struct moments
{
    double mean;
    double sa;
    double sq;
} moments;

static moments areal_parameters(cl_command_queue queue, cl_kernel kernel,
                                cl_mem data, cl_mem sums, cl_uint width,
                                cl_uint height, cl_uint pitch)
{
    float *rows = NULL;
    double count = 0.0;
    double total = 0.0;
    moments result = {0.0, 0.0, 0.0};
    double squares = 0.0;
    double deviations = 0.0;
    const size_t global = height;
    float mean = 0.0F;
    set_buffer(kernel, 0, data);
    set_buffer(kernel, 1, sums);
    set_uint(kernel, 2, width);
    set_uint(kernel, 3, pitch);
    for (int pass = 0; pass < 2; ++pass)
    {
        check(clSetKernelArg(kernel, 4, sizeof mean, &mean), "arg");
        check(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0,
                                     NULL, NULL),
              "clEnqueueNDRangeKernel");
        rows = map(queue, sums, (size_t)height * SUMS_PER_ROW, CL_MAP_READ);
        count = total = squares = deviations = 0.0;
        for (size_t y = 0; y < height; ++y)
        {
            count += rows[SUMS_PER_ROW * y];
            total += rows[SUMS_PER_ROW * y + 1];
            squares += rows[SUMS_PER_ROW * y + 2];
            deviations += rows[SUMS_PER_ROW * y + 3];
        }
        unmap(queue, sums, rows);
        result.mean = total / count;
        mean = (float)result.mean;
    }
    result.sq = sqrt(squares / count);
    result.sa = deviations / count;
    return result;
}

static moments host_parameters(const float *data, size_t width, size_t height,
                               size_t pitch)
{
    double count = 0.0;
    double total = 0.0;
    for (size_t y = 0; y < height; ++y)
    {
        for (size_t x = 0; x < width; ++x)
        {
            const float value = data[y * pitch + x];
            if (!isnan(value))
            {
                count += 1.0;
                total += value;
            }
        }
    }
    moments result = {total / count, 0.0, 0.0};
    double squares = 0.0;
    double deviations = 0.0;
    for (size_t y = 0; y < height; ++y)
    {
        for (size_t x = 0; x < width; ++x)
        {
            const float value = data[y * pitch + x];
            if (!isnan(value))
            {
                squares += (value - result.mean) * (value - result.mean);
                deviations += fabs(value - result.mean);
            }
        }
    }
    result.sq = sqrt(squares / count);
    result.sa = deviations / count;
    return result;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : SDFIO_EXAMPLE_FILE;
    cl_device_id device = first_device();
    if (device == NULL)
    {
        fprintf(stderr, "no OpenCL device\n");
        return NO_DEVICE;
    }
    cl_int status = CL_SUCCESS;
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    check(status, "clCreateContext");
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &status);
    check(status, "clCreateCommandQueue");
    const char *text = kernels;
    cl_program program =
        clCreateProgramWithSource(context, 1, &text, NULL, &status);
    check(status, "clCreateProgramWithSource");
    check(clBuildProgram(program, 1, &device, NULL, NULL, NULL),
          "clBuildProgram");
    cl_kernel smooth = clCreateKernel(program, "smooth", &status);
    check(status, "smooth");
    cl_kernel sums_kernel = clCreateKernel(program, "row_sums", &status);
    check(status, "row_sums");

    sdfio_reader *reader = sdfio_reader_new();
    check_sdfio(sdfio_open_file(reader, path), sdfio_reader_message(reader));
    const sdfio_header *header = sdfio_get_header(reader);
    const cl_uint width = (cl_uint)header->columns;
    const cl_uint height = (cl_uint)header->rows;
    const cl_uint pitch =
        (width + PITCH_ALIGNMENT - 1) / PITCH_ALIGNMENT * PITCH_ALIGNMENT;
    const size_t floats = (size_t)pitch * height;

    cl_mem input = make_buffer(context, floats);
    cl_mem output = make_buffer(context, floats);
    cl_mem sums = make_buffer(context, (size_t)height * SUMS_PER_ROW);

    float *memory = map(queue, input, floats, CL_MAP_WRITE_INVALIDATE_REGION);
    check_sdfio(sdfio_read_heights(reader, sdfio_metres(SDFIO_FLOAT32), memory,
                                   (ptrdiff_t)((size_t)pitch * sizeof(float))),
                sdfio_reader_message(reader));
    unmap(queue, input, memory);

    const size_t global[2] = {width, height};
    set_buffer(smooth, 0, input);
    set_buffer(smooth, 1, output);
    set_uint(smooth, 2, width);
    set_uint(smooth, 3, height);
    set_uint(smooth, 4, pitch);
    check(clEnqueueNDRangeKernel(queue, smooth, 2, NULL, global, NULL, 0, NULL,
                                 NULL),
          "clEnqueueNDRangeKernel");
    const moments device_result = areal_parameters(queue, sums_kernel, output,
                                                   sums, width, height, pitch);

    memory = map(queue, output, floats, CL_MAP_READ);
    const moments host_result = host_parameters(memory, width, height, pitch);
    sdfio_header smoothed = *header;
    smoothed.data_type = SDFIO_DATA_BINARY32;
    smoothed.z_scale = SDFIO_AUTO_Z_SCALE;
    sdfio_writer *writer = sdfio_writer_new();
    check_sdfio(sdfio_create_file(writer, "smooth.sdf", &smoothed),
                sdfio_writer_message(writer));
    check_sdfio(sdfio_write_heights(writer, sdfio_metres(SDFIO_FLOAT32), memory,
                                    (ptrdiff_t)((size_t)pitch * sizeof(float))),
                sdfio_writer_message(writer));
    check_sdfio(sdfio_write_trailer(writer, NULL, 0),
                sdfio_writer_message(writer));
    unmap(queue, output, memory);

    const int agrees =
        fabs(device_result.sa - host_result.sa) <= 1e-4 * host_result.sa
        && fabs(device_result.sq - host_result.sq) <= 1e-4 * host_result.sq;
    char name[128] = "";
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof name, name, NULL);
    printf("OpenCL %s: %u x %u, Sa %g m, Sq %g m, host %s\n", name, width,
           height, device_result.sa, device_result.sq,
           agrees ? "agrees" : "DISAGREES");

    sdfio_writer_free(writer);
    sdfio_reader_free(reader);
    clReleaseMemObject(input);
    clReleaseMemObject(output);
    clReleaseMemObject(sums);
    clReleaseKernel(smooth);
    clReleaseKernel(sums_kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    return agrees ? EXIT_SUCCESS : EXIT_FAILURE;
}
