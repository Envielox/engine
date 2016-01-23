#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <GLFW/glfw3.h>

#include "cl.h"
#include "error.h"
#include "geom.h"
#include "globals.h"
#include "render.h"
#include "types.h"

// TODO zrobić na to odpowiedni plik
#include "HsFFI.h"

#ifdef __GLASGOW_HASKELL__
#include "FileLoader_stub.h"
#endif

#pragma once

void load_file(HsPtr name);
void push_oct_tree_empty();
void push_oct_tree_solid(float r, float g, float b);
void push_oct_tree_partial(int c0, int c1, int c2, int c3, int c4, int c5, int c6, int c7);

static void key_callback(GLFWwindow* windows, int key, int scancode, int action, int mods);
static void initOctTree(void);

int main(int argc, char* argv[])
{
	glfwSetErrorCallback(error_callback);

	/* Initialize the library */
	if (!glfwInit()){
		fprintf(stderr, "Initialization failed.\n");
		return -1;
	}

	/* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(width, height, "Hello World", NULL, NULL);
	if (!window) {
		glfwTerminate();
		fprintf(stderr, "Error creating window.\n");
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);
	glfwSetKeyCallback(window, key_callback);

	//**************************** generowanie przykładowych piksli
	hs_init(&argc, &argv);
	initOctTree();
	hs_exit();
	camera_target = (Point3f) { cosf(horizontal_angle) * cosf(vertical_angle)
							  , sinf(horizontal_angle) * cosf(vertical_angle)
							  , sinf(vertical_angle)};
	float * piksele = malloc(height*width*3*sizeof(*piksele));

    printf("sizeof(OctTreeNode)=%zd\n", sizeof(OctTreeNode));

	//****************************

#if TRACER_CL
    // prepare your anus
    // i mean gpu
    cl_int status;
    cl_platform_id platform_id;
    cl_int num_platforms;
    char *kernel_src = malloc(10240);
    check_nn(kernel_src, "kernel_src");
    status = clGetPlatformIDs(1, &platform_id, &num_platforms);
    check_cl(status, "get platform ids");

    printf("#platforms: %d\n", num_platforms);
    cl_context_properties *props = getContextProperties(platform_id);

    char info[4][128];
    status = clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, 128, info[0], NULL);
    check_cl(status, "get platform profile");
    status = clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, 128, info[1], NULL);
    check_cl(status, "get platform version");
    status = clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, 128, info[2], NULL);
    check_cl(status, "get platform name");
    status = clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 128, info[3], NULL);
    check_cl(status, "get platform vendor");

    printf("profile: %s\n", info[0]);
    printf("version: %s\n", info[1]);
    printf("name: %s\n", info[2]);
    printf("vendor: %s\n", info[3]);

    cl_context context = clCreateContextFromType(props, CL_DEVICE_TYPE_GPU, NULL, NULL, &status);
    check_cl(status, "create context");

    // create a command queue
    cl_device_id device_id;
    status = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);
    check_cl(status, "get device ids");
#ifdef CL_VERSION_2_0
    queue = clCreateCommandQueueWithProperties(context, device_id, NULL, &status);
#else
    queue = clCreateCommandQueue(context, device_id, 0, &status);
#endif
    check_cl(status, "create command queue");

    // allocate memory objects
    mainOctCL = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 9*sizeof(OctTreeNode), mainOctTree, &status);
    check_cl(status, "create buffer");

    cl_image_format fmt = { CL_RGBA, CL_FLOAT };
    cl_image_desc desc;
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_row_pitch = 0;
    desc.image_slice_pitch = 0;
    desc.num_mip_levels = 0;
    desc.num_samples = 0;
#ifdef CL_VERSION_2_0
    desc.mem_object = NULL;
#else
    desc.buffer = NULL;
#endif

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glFinish();

    image = clCreateFromGLTexture(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture, &status);
    check_cl(status, "create image");

    // create the compute program
    FILE *kernel_handle;
    fopen_s(&kernel_handle, "ray.cl", "rb");
    check_nn(kernel_handle, "fopen ray.cl");

    size_t n_bytes = fread(kernel_src, 1, 10239, kernel_handle);
    kernel_src[n_bytes] = '\0';
    check_ferror(kernel_handle, "fread");

    cl_program program = clCreateProgramWithSource(context, 1, &kernel_src, NULL, &status);
    check_cl(status, "create program");

    // build the compute program executable
    status = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (status != CL_BUILD_PROGRAM_FAILURE && status != CL_SUCCESS) {
        check_cl(status, "build program");
    } else {
        size_t log_size;
        status = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        check_cl(status, "get program build log size");

        char *log = malloc(log_size);
        check_nn(log, "log");

        status = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        check_cl(status, "get program build log");

        fprintf(stderr, "build kernel log:\n%s\n", log);
    }

    // create the compute kernel
    kernel = clCreateKernel(program, "ray_cl", &status);
    check_cl(status, "create kernel");

#endif
    double last_xpos = 0, last_ypos = 0;
	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		for (int i = 0; i < height * width * 3; i++)
			piksele[i] = 0.0;

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		xpos -= last_xpos;
		ypos -= last_ypos;
		last_xpos += xpos;
		last_ypos += ypos;
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
			const double alpha = 0.001;
			horizontal_angle += (float)(xpos * alpha);
			vertical_angle += (float)(ypos * alpha);
			camera_target = (Point3f) { cosf(horizontal_angle) * cosf(vertical_angle)
									  , sinf(horizontal_angle) * cosf(vertical_angle)
									  , sinf(vertical_angle)};
		}
		clock_t start = clock();
		captureOctTree(camera_pos, camera_target, up, width, height, piksele);
		clock_t end = clock();

		// show render time in window title
		char title[16];
		snprintf(title, 16, "%d ms", (int)((end - start) / (CLOCKS_PER_SEC / 1000)));
		glfwSetWindowTitle(window, title);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

#if TRACER_CL
    clReleaseMemObject(mainOctCL);
    clReleaseMemObject(image);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
#endif
	glfwDestroyWindow(window);

	glfwTerminate();
	return 0;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		Point3f right = vectMul(camera_target, up);
		switch (key) {
			case GLFW_KEY_W:
				camera_pos = vectMulScalar(camera_pos, camera_target, 0.05f);
				break;
			case GLFW_KEY_S:
				camera_pos = vectMulScalar(camera_pos, camera_target, -0.05f);
				break;
			case GLFW_KEY_A:
				camera_pos = vectMulScalar(camera_pos, right, -0.05f);
				break;
			case GLFW_KEY_D:
				camera_pos = vectMulScalar(camera_pos, right, 0.05f);
				break;
			case GLFW_KEY_Z:
				camera_pos = vectMulScalar(camera_pos, up, -0.05f);
				break;
			case GLFW_KEY_Q:
				camera_pos = vectMulScalar(camera_pos, up, 0.05f);
				break;
			case GLFW_KEY_I:
				vertical_angle += 0.05f;
				break;
			case GLFW_KEY_K:
				vertical_angle -= 0.05f;
				break;
			case GLFW_KEY_L:
				horizontal_angle -= 0.05f;
				break;
			case GLFW_KEY_J:
				horizontal_angle += 0.05f;
				break;
			case GLFW_KEY_M:
				light = vectMulScalar(camera_pos, camera_target, 1.f);
				break;
			case GLFW_KEY_P:
				printf("Current rendering method is: ");
				if (render_method == Stacking) {
					render_method = Stackless;
					printf("Stackless");
#if TRACER_CL
                } else if (render_method == Stackless) {
                    render_method = TracerCL;
                    printf("TracerCL");
#endif
                } else {
					render_method = Stacking;
					printf("Stacking");
				}
				printf("\n");
				break;
			/*default:*/
		}
		camera_target = (Point3f) { cosf(horizontal_angle) * cosf(vertical_angle)
								  , sinf(horizontal_angle) * cosf(vertical_angle)
								  , sinf(vertical_angle)};
	}
	printf("Camera position is: (%f, %f %f)\n", camera_pos.x, camera_pos.y, camera_pos.z);
	printf("Horizontal angle: %f, Vertical angle: %f\n", horizontal_angle, vertical_angle);
	printf("Camera target is: (%f, %f %f)\n", camera_target.x, camera_target.y, camera_target.z);
}


void push_oct_tree_partial(int c0, int c1, int c2, int c3, int c4, int c5, int c6, int c7)
{
	printf("Pushing partial %d %d %d %d %d %d %d %d\n", c0, c1, c2, c3, c4, c5, c6, c7 );
	mainOctTree[octTreeLength].type = Partial;
	mainOctTree[octTreeLength].nodes[0][1][1] = c0;
	mainOctTree[c0].parent = octTreeLength;
	mainOctTree[c0].x = 0;
	mainOctTree[c0].y = 1;
	mainOctTree[c0].z = 1;
	mainOctTree[octTreeLength].nodes[1][1][1] = c1;
	mainOctTree[c1].parent = octTreeLength;
	mainOctTree[c1].x = 1;
	mainOctTree[c1].y = 1;
	mainOctTree[c1].z = 1;
	mainOctTree[octTreeLength].nodes[0][0][1] = c2;
	mainOctTree[c2].parent = octTreeLength;
	mainOctTree[c2].x = 0;
	mainOctTree[c2].y = 0;
	mainOctTree[c2].z = 1;
	mainOctTree[octTreeLength].nodes[1][0][1] = c3;
	mainOctTree[c3].parent = octTreeLength;
	mainOctTree[c3].x = 1;
	mainOctTree[c3].y = 0;
	mainOctTree[c3].z = 1;
	mainOctTree[octTreeLength].nodes[0][1][0] = c4;
	mainOctTree[c4].parent = octTreeLength;
	mainOctTree[c4].x = 0;
	mainOctTree[c4].y = 1;
	mainOctTree[c4].z = 0;
	mainOctTree[octTreeLength].nodes[1][1][0] = c5;
	mainOctTree[c5].parent = octTreeLength;
	mainOctTree[c5].x = 1;
	mainOctTree[c5].y = 1;
	mainOctTree[c5].z = 0;
	mainOctTree[octTreeLength].nodes[0][0][0] = c6;
	mainOctTree[c6].parent = octTreeLength;
	mainOctTree[c6].x = 0;
	mainOctTree[c6].y = 0;
	mainOctTree[c6].z = 0;
	mainOctTree[octTreeLength].nodes[1][0][0] = c7;
	mainOctTree[c7].parent = octTreeLength;
	mainOctTree[c7].x = 1;
	mainOctTree[c7].y = 0;
	mainOctTree[c7].z = 0;

	octTreeLength++;
}

void push_oct_tree_solid(float r, float g, float b)
{
	printf("Pushing solid %f %f %f\n", r, g, b);
	mainOctTree[octTreeLength].type = Solid;
	mainOctTree[octTreeLength].color = (Color4f) {r,g,b,0};
	octTreeLength++;
}

void push_oct_tree_empty()
{
	printf("Pushing empty\n");
	mainOctTree[octTreeLength].type = Empty;
	octTreeLength++;
}


static void initOctTree(void)
{
	mainOctTree = malloc(32 * sizeof(*mainOctTree));
	octTreeLength = 0;

	load_file("model.json");

	/*mainOctTree[0].parent = -1;*/
    mainOctTree->parent = -1;
	printf("Done loading.\n");

}
