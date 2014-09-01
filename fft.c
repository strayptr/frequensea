// Perform FFT analysis on HackRF data.

#include <GLFW/glfw3.h>
#include <libhackrf/hackrf.h>
#include <fftw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#define WIDTH 512
#define HEIGHT 512
#define BUFFER_SIZE (512 * 512)

fftw_complex *fft_in;
fftw_complex *fft_out;
fftw_plan fft_plan;
uint8_t buffer[BUFFER_SIZE];
hackrf_device *device;
double freq_mhz = 100.9;
GLFWwindow* window;
GLuint texture_id;
GLuint program;
int paused = 0;

// HackRF ///////////////////////////////////////////////////////////////////

#define HACKRF_CHECK_STATUS(status, message) \
    if (status != 0) { \
        printf("FAIL: %s\n", message); \
        hackrf_close(device); \
        hackrf_exit(); \
        exit(-1); \
    } \

int receive_sample_block(hackrf_transfer *transfer) {
    if (paused) return 0;
    memcpy(fft_in, transfer->buffer, BUFFER_SIZE);
    fftw_execute(fft_plan);
    return 0;
}

static void setup_hackrf() {
    int status;

    status = hackrf_init();
    HACKRF_CHECK_STATUS(status, "hackrf_init");

    status = hackrf_open(&device);
    HACKRF_CHECK_STATUS(status, "hackrf_open");

    status = hackrf_set_freq(device, freq_mhz * 1e6);
    HACKRF_CHECK_STATUS(status, "hackrf_set_freq");

    status = hackrf_set_sample_rate(device, 2.5e6);
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
}

static void teardown_hackrf() {
    hackrf_stop_rx(device);
    hackrf_close(device);
    hackrf_exit();
}

static void set_frequency() {
    freq_mhz = round(freq_mhz * 10.0) / 10.0;
    printf("Seting freq to %f MHz.\n", freq_mhz);
    int status = hackrf_set_freq(device, freq_mhz * 1e6);
    HACKRF_CHECK_STATUS(status, "hackrf_set_freq");
}

// FFTW /////////////////////////////////////////////////////////////////////

static void setup_fftw() {
    fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BUFFER_SIZE);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * BUFFER_SIZE);
    fft_plan = fftw_plan_dft_2d(512, 512, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);    
}

static void teardown_fftw() {
    fftw_destroy_plan(fft_plan);
    fftw_free(fft_in);
    fftw_free(fft_out);
}

// OpenGL ///////////////////////////////////////////////////////////////////

static void check_shader_error(GLuint shader) {
    int length = 0;
    int charsWritten  = 0;
    char *infoLog;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length > 0) {
        char message[length];
        glGetShaderInfoLog(shader, length, NULL, message);
        printf("%s\n", message);
    }
}

static void setup_gl() {
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const char *vertex_shader_source = 
        "void main(void) {"
        "  gl_TexCoord[0] = gl_MultiTexCoord0;"
        "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
        "}";

    const char *fragment_shader_source = 
        "uniform sampler2D texture;"
        "void main(void) {"
        "  vec4 c = texture2D(texture, gl_TexCoord[0].st);"
        "  float v = c.r;"
        "  if (v <= 0.05) {"
        "    v *= 10.0; "
        "  } else if (v >= 0.95) {"
        "    v = 0.5 + (v - 0.95) * 10.0;"
        "  }"
        "  gl_FragColor = vec4(v, v, v, 1);"
        "}";

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    check_shader_error(vertex_shader);

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    check_shader_error(fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glActiveTexture(0); 
    GLuint u_texture = glGetUniformLocation(program, "texture");
    glUniform1i(u_texture, texture_id);
}

static void prepare() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, 0, HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void update() {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, fft_out);
}

static void draw() {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_COLOR);

    glUseProgram(program);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex2f(0, 0);
    glTexCoord2f(1.0, 0.0);
    glVertex2f(WIDTH, 0);
    glTexCoord2f(1.0, 1.0);
    glVertex2f(WIDTH, HEIGHT);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(0, HEIGHT);
    glEnd();
    glUseProgram(0);
}

static void teardown_gl() {
}

// GLFW /////////////////////////////////////////////////////////////////////

static void export() {
    FILE *fp = fopen("out.raw", "wb");
    if (fp) {
        fwrite(fft_out, BUFFER_SIZE, 1, fp);
        fclose(fp);
        printf("Written file.\n");
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        freq_mhz += 0.1;
        set_frequency();
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        freq_mhz -= 0.1;
        set_frequency();
    } else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        paused = !paused;
    } else if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        export();
    }
}

static void setup_glfw() {
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    window = glfwCreateWindow(WIDTH, HEIGHT, "HackRF", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);    
}

static void teardown_glfw() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

// Main /////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    setup_glfw();
    setup_fftw();
    setup_hackrf();
    setup_gl();

    while (!glfwWindowShouldClose(window)) {
        prepare();
        update();
        draw();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    teardown_gl();
    teardown_hackrf();
    teardown_fftw();
    teardown_glfw();

    return 0;
}
