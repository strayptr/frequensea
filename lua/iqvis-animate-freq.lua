-- Visualize IQ data from the HackRF, animate through the spectrum
nwm_init()
freq = 200
device = nrf_start(freq, "../rfdata/rf-202.500-2.raw")
window = nwm_create_window(800, 600)

camera = ngl_camera_init_look_at(0, 0, 0) -- Shader ignores camera position, but camera object is required for ngl_draw_model
shader = ngl_load_shader(GL_LINE_STRIP, "../shader/lines.vert", "../shader/lines.frag")

while not nwm_window_should_close(window) do
    nwm_frame_begin(window)
    ngl_clear(0.2, 0.2, 0.2, 1.0)
    model = ngl_model_init_positions(3, NRF_SAMPLES_SIZE, device.samples)
    ngl_draw_model(camera, model, shader)
    nwm_frame_end(window)
    nrf_freq_set(device, freq)
    freq = freq + 0.01
end

nrf_stop(device)
nwm_destroy_window(window)
nwm_terminate()