#ifndef INCLUDED_WASAPI
#define INCLUDED_WASAPI

int wasapi_init();

int wasapi_destruct();

void wasapi_renderer(void callback (float * buffer, uint64_t count, uint64_t channels, uint64_t freq), void cleanup ());

#endif
