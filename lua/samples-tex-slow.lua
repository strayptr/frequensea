-- Visualize IQ data as a texture from the HackRF
-- You want seizures? 'Cause this is how you get seizures.

VERTEX_SHADER = [[
#version 400
layout (location = 0) in vec3 vp;
layout (location = 1) in vec3 vn;
layout (location = 2) in vec2 vt;
out vec3 color;
out vec2 texCoord;
uniform mat4 uViewMatrix, uProjectionMatrix;
uniform float uTime;
void main() {
    color = vec3(1.0, 1.0, 1.0);
    texCoord = vt; // vec2(vp.x + 0.5, vp.z + 0.5);
    gl_Position = vec4(vp.x*2, vp.z*2, 0, 1.0);
}
]]

FRAGMENT_SHADER = [[
#version 400
in vec3 color;
in vec2 texCoord;
uniform sampler2D uTexture;
layout (location = 0) out vec4 fragColor;
void main() {
    float r = texture(uTexture, texCoord).r * 1;
    fragColor = vec4(r, r, r, 0.95);
}
]]

time_to_switch = 400

function setup()
    freq = 1.0
    freq_time = 0
    device = nrf_device_new(freq, "../rfdata/rf-200.500-big.raw")
    interpolator = nrf_interpolator_new(0.01)

    camera = ngl_camera_new_look_at(0, 0, 0) -- Camera is unnecessary but ngl_draw_model requires it
    shader = ngl_shader_new(GL_TRIANGLES, VERTEX_SHADER, FRAGMENT_SHADER)
    texture = ngl_texture_new(shader, "uTexture")
    model = ngl_model_new_grid_triangles(2, 2, 1, 1)
end

function draw()
    samples_buffer = nrf_device_get_samples_buffer(device)
    nrf_interpolator_process(interpolator, samples_buffer)
    interpolator_buffer = nrf_interpolator_get_buffer(interpolator)

    ngl_clear(0.2, 0.2, 0.2, 1.0)
    ngl_texture_update(texture, interpolator_buffer, 512, 256)
    ngl_draw_model(camera, model, shader)
    freq_time = freq_time + 1
    if freq_time >= time_to_switch then
        freq = freq + 0.1
        nrf_device_set_frequency(device, freq)
        print("Frequency: " .. freq)
        freq_time = 0
    end
end

function on_key(key, mods)
    keys_frequency_handler(key, mods)
end
