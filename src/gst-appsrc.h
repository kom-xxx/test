#ifndef GST_APPSRC_H
#define GST_APPSRC_H

#include <sys/cdefs.h>

__BEGIN_DECLS
/**
 ** CAM_TYPE_* are defined as -DCAM_TYPE_* compile option
 **/
#ifdef CAM_C525
#define CAM_FORMAT "YUY2"
#define CAM_WIDTH 640
#define CAM_HEIGHT 480
#define CAM_FR 30, 1
#endif

/* when NO CAMERA SPECIFIED */
#ifndef CAM_FORMAT
#define CAM_FORMAT "RGB16"
#define CAM_WIDTH 384
#define CAM_HEIGHT 288
#define CAM_FR 0, 1
#endif


void *init_gst(void);
int push_frame(void *, void *, size_t);
void deinit_gst(void *);

__END_DECLS

#endif	/* !GST_APPSRC_H */
