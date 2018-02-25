#ifndef INCLUDED_DSOUND
#define INCLUDED_DSOUND

int dsound_init();

int dsound_destruct();

void dsound_renderer(void callback (float * buffer, uint64_t count, uint64_t channels, uint64_t freq), void cleanup ());

#endif
