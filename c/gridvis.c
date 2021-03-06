// 3D visualization of spectrum

#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <libhackrf/hackrf.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <OpenGL/glu.h>

#define WIDTH 800
#define HEIGHT 600


GLFWwindow* window;
GLuint texture_id;
GLuint program;
uint8_t buffer[512 * 512];
hackrf_device *device;
double freq_mhz = 124.2;
int paused = 0;
float camera_x = 112;
float camera_y = 40;
float camera_z = -50;

#define HAVE_HACKRF


#define HACKRF_CHECK_STATUS(status, message) \
    if (status != 0) { \
        printf("FAIL: %s\n", message); \
        hackrf_close(device); \
        hackrf_exit(); \
        exit(EXIT_FAILURE); \
    } \

int receive_sample_block(hackrf_transfer *transfer) {
    if (paused) return 0;
    for (int idx = 0; idx < transfer->valid_length; idx += 2) {
        double i = transfer->buffer[idx] / 255.0;
        double q = transfer->buffer[idx + 1] / 255.0;
        double v = sqrt(i * i + q * q);



        // double v = transfer->buffer[i + 1] / 255.0;
        // if (v <= 0.05) {
        //     v *= 10.0;
        // } else if (v >= 0.95) {
        //     v = 0.5 + (v - 0.95) * 10.0;
        // }
        uint8_t vi = (uint8_t) round(v * 255);
        buffer[idx] = vi;
        buffer[idx + 1] = vi;
    }
    return 0;
}

static void setup_fake() {
	char *fname = "img.raw";
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        printf("ERROR: Could not write open file %s.\n", fname);
        exit(EXIT_FAILURE);
    }
    fread(buffer, 512 * 512, 1, fp);
    fclose(fp);
}

static void setup_hackrf() {
    int status;

    status = hackrf_init();
    HACKRF_CHECK_STATUS(status, "hackrf_init");

    status = hackrf_open(&device);
    HACKRF_CHECK_STATUS(status, "hackrf_open");

    status = hackrf_set_freq(device, freq_mhz * 1e6);
    HACKRF_CHECK_STATUS(status, "hackrf_set_freq");

    status = hackrf_set_sample_rate(device, 8e6);
    HACKRF_CHECK_STATUS(status, "hackrf_set_sample_rate");

    status = hackrf_set_amp_enable(device, 0);
    HACKRF_CHECK_STATUS(status, "hackrf_set_amp_enable");

    status = hackrf_set_lna_gain(device, 32);
    HACKRF_CHECK_STATUS(status, "hackrf_set_lna_gain");

    status = hackrf_set_vga_gain(device, 30);
    HACKRF_CHECK_STATUS(status, "hackrf_set_lna_gain");

    status = hackrf_start_rx(device, receive_sample_block, NULL);
    HACKRF_CHECK_STATUS(status, "hackrf_start_rx");

    memset(buffer, 0, 512 * 512);

    //status = hackrf_set_freq(device, freq_mhz * 1e6);
    //HACKRF_CHECK_STATUS(status, "hackrf_set_freq");
}

static void set_frequency() {
    #if defined(HAVE_HACKRF)
    freq_mhz = round(freq_mhz * 1000.0) / 1000.0;
    printf("Seting freq to %f MHz.\n", freq_mhz);
    int status = hackrf_set_freq(device, freq_mhz * 1e6);
    HACKRF_CHECK_STATUS(status, "hackrf_set_freq");
    #endif
}

static void teardown_hackrf() {
    hackrf_stop_rx(device);
    hackrf_close(device);
    hackrf_exit();
}

static void check_shader_error(char *prefix, GLuint shader) {
    int length = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length > 0) {
        char message[length];
        glGetShaderInfoLog(shader, length, NULL, message);
        printf("%s: %s\n", prefix, message);
        exit(EXIT_FAILURE);
    }
}

static void setup() {
    glEnable(GL_DEPTH_TEST);
    const char *vertex_shader_source =
        "#extension GL_EXT_gpu_shader4 : require\n"
        "\n"
        "vec4 light_pos = vec4(0, 4, 0, 1);\n"
        "vec3 light_color = vec3(1.5, 0.5, 5);\n"
        "vec3 diffuse_reflectivity = vec3(1.0, 1.0, 1.0);\n"
        "flat varying vec3 color;\n"
        "\n"
        "void main(void) {\n"
        "  vec3 normal_dir = normalize(gl_NormalMatrix * gl_Normal);\n"
        "  vec4 eye_coords = gl_ModelViewMatrix * gl_Vertex;\n"
        "  vec3 s = normalize(vec3(light_pos - eye_coords));\n"
        "  color = light_color * diffuse_reflectivity * max(0.0, dot(s, normal_dir));\n"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
        //"  normal = vec3(0, 0, 0);"
        "}\n";

    const char *fragment_shader_source =
        "#extension GL_EXT_gpu_shader4 : require\n"
        "\n"
        "flat varying vec3 color;\n"
        "\n"
        "void main(void) {\n"
        "  gl_FragColor = vec4(color, 0.8);\n"
        //"  gl_FragColor = vec4(0.5, 0.5, 1, 0.1);"
        "}\n";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    check_shader_error("V", vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    check_shader_error("F", fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    // glActiveTexture(0);
    // GLuint u_texture = glGetUniformLocation(program, "texture");
    // glUniform1i(u_texture, texture_id);
}

static void prepare() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(1, 1, 0.93, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    //glFrustum( -1.0, 1.0, -1.0, 1.0, 1.0, 102.0 );
    gluPerspective(80.0, width / height, 1, 5000);
    //glFrustum(-1.0, 1.0, -1.0, 1.0, 1.5, 20.0);

    //glOrtho(0, WIDTH, 0, HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(camera_x, camera_y, camera_z, 0, 0, 0, 0, 1, 0);
    //glTranslatef(camera_x, camera_y, camera_z);

    glShadeModel(GL_FLAT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);

    glEnable(GL_LIGHT0);
    GLfloat light0_position[] = {10.0, 10.0, 0.0, 1.0};

    GLfloat light0_ambient[] = {0.0, 0.0, 0.0, 1.0};
    GLfloat light0_diffuse[] = {1.0, 1.0, 1.0, 1.0};
    GLfloat light0_specular[] = {1.0, 1.0, 1.0, 1.0};
    glLightfv(GL_LIGHT1, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);

    glEnable(GL_LIGHT1);
    GLfloat light1_ambient[] = {0.8, 0.8, 0.8, 1.0};

    // glEnable(GL_FOG);
    // float fog_color[3] = {1.0, 1.0, 0.93};
    // glFogfv(GL_FOG_COLOR, fog_color);
    // glFogi(GL_FOG_MODE, GL_LINEAR);
    // glFogf(GL_FOG_START, 100.f);
    // glFogf(GL_FOG_END, 110.f);
}



static void update() {
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, buffer);
}

static void draw() {
    // Draw simple ground plane
    // int ground_size = 10;
    // glColor4f(1, 1, 1, 0.3);
    // glBegin(GL_QUADS);
    // glVertex3f(-ground_size, 0, -ground_size);
    // glVertex3f(-ground_size, 0, ground_size);
    // glVertex3f(ground_size, 0, ground_size);
    // glVertex3f(ground_size, 0, -ground_size);
    // glEnd();

    GLdouble vertices[256 * 256 * 3];
    GLdouble normals[256 * 256 * 3];
    GLushort indices[255 * 255 * 6];
    GLubyte colors[255 * 255 * 6];

    int vi = 0;
    int ni = 0;
    for (int y = 0; y < 256; y += 1) {
        for (int x = 0; x < 256; x += 1) {
            vertices[vi++] = (x - 128);
            //vertices[vi++] =  sin(x / 5.0) + cos(y / 7.0) * 10.2;
            vertices[vi++] =  buffer[(y * 256) + x] / 100.0;
            vertices[vi++] = (y - 128);
            //printf("%3.1f %3.1f %3.1f\n", points[i-3], points[i-2], points[i-1]);
            normals[ni++] = 0.0;
            normals[ni++] = 0;
            normals[ni++] = 0.0;
        }
    }

    int ii = 0;
    int ci = 0;
    for (int y = 0; y < 255; y += 1) {
        for (int x = 0; x < 255; x += 1) {
            indices[ii++] = (y * 256) + x;
            indices[ii++] = ((y + 1) * 256) + x;
            indices[ii++] = ((y + 1) * 256) + (x + 1);

            colors[ci++] = 0; //x % 255;
            colors[ci++] = 0; //y % 255;
            colors[ci++] = 0; //220;

            indices[ii++] = (y * 256) + x;
            indices[ii++] = ((y + 1) * 256) + (x + 1);
            indices[ii++] = (y * 256) + (x + 1);

            colors[ci++] = 0; //y % 255;
            colors[ci++] = 0; // x % 255;
            colors[ci++] = 0; //240;

        }
    }

    glColor4f(1, 0, 1, 1);
    glUseProgram(program);
    //glUseProgram(0);
    glPointSize(2);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_DOUBLE, 0, vertices);
    glNormalPointer(GL_DOUBLE, 0, normals);
    glColorPointer(3, GL_UNSIGNED_BYTE, 0, colors);
    //glDrawElements(GL_TRIANGLES, 255 * 255 * 6, GL_UNSIGNED_SHORT, indices);
    glDrawArrays(GL_POINTS, 0, 256 * 256);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glUseProgram(0);


    // glEnable(GL_TEXTURE_2D);
    // glBlendFunc(GL_ONE, GL_SRC_COLOR);

    // glBindTexture(GL_TEXTURE_2D, texture_id);
    // glBegin(GL_QUADS);
    // glTexCoord2f(0.0, 0.0);
    // glVertex2f(0, 0);
    // glTexCoord2f(1.0, 0.0);
    // glVertex2f(WIDTH, 0);
    // glTexCoord2f(1.0, 1.0);
    // glVertex2f(WIDTH, HEIGHT);
    // glTexCoord2f(0.0, 1.0);
    // glVertex2f(0, HEIGHT);
    // glEnd();
    // glUseProgram(0);
}

static void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

static void print_camera_pos() {
    printf("Camera: %.1f %.1f %.1f\n", camera_x, camera_y, camera_z);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    } else if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_z += 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_z -= 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_x -= 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_x += 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_Q && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_y += 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_E && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        camera_y -= 1;
        print_camera_pos();
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        freq_mhz += 0.01;
        set_frequency();
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        freq_mhz -= 0.01;
        set_frequency();
    } else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        paused = !paused;
    }
}

int main(void) {
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    window = glfwCreateWindow(WIDTH, HEIGHT, "Frequensea", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    #if defined(HAVE_HACKRF)
        setup_hackrf();
    #else
        setup_fake();
    #endif
    setup();
    while (!glfwWindowShouldClose(window)) {
        prepare();
        update();
        draw();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    #if defined(HAVE_HACKRF)
        teardown_hackrf();
    #endif
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
