-- Visualize IQ data from the HackRF
nwm_init()

-- Start the device for a minute, wait a bit, and stop again
device = nrf_start(100.9)
os.execute("sleep " .. 1)
nrf_stop(device)

samples = 0
window = nwm_create_window(800, 600)

camera = ngl_camera_init_look_at(0, 0, 0) -- Shader ignores camera position, but camera object is required for ngl_draw_model
shader = ngl_load_shader(GL_LINE_STRIP, "../shader/lines.vert", "../shader/lines.frag")

while not nwm_window_should_close(window) do
    nwm_frame_begin(window)
    ngl_clear(0.2, 0.2, 0.2, 1.0)
    model = ngl_model_init_positions(2, math.min(samples, NRF_SAMPLES_SIZE / 2), device.samples)
    ngl_draw_model(camera, model, shader)
    nwm_frame_end(window)
    samples = samples + 2
end


nwm_destroy_window(window)
nwm_terminate()