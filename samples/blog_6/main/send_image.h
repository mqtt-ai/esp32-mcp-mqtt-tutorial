#ifndef SEND_IMAGE_H
#define SEND_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Sends an image (embedded binary) and an optional question to the
// vision explain server at the given address. Returns a heap-allocated
// response string on success, or NULL on error. Caller must free.
char *http_send_image(char* vision_explain_address, char* question);

#ifdef __cplusplus
}
#endif

#endif // SEND_IMAGE_H


