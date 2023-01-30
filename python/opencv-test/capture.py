#!/usr/bin/env python3

import sys
import cv2

src = ''
dst = 'record'

def main():
    i = 0;
    if src == 'file':
        capture = cv2.VideoCapture("./camera.h265")
    else:
        capture = cv2.VideoCapture(0) # specify cam # whwn input is a camera
        ##
        ## set camera parameter using capture.set(...)
        ##
    print('AAAAAAAAAAAAA')
    if dst == 'record':
        #cc4 = cv2.VideoWriter_fourcc(*'m','p','4','v')
        #output = cv2.VideoWriter('output.mp4', cc4, 30.0, (640, 480))
#        gst_pipe = 'appsrc ! videoconvert ! filesink location=test'
        gst_pipe = 'appsrc ! videoconvert ! x264enc ! mp4mux ! filesink location=test'
        output = cv2.VideoWriter(gst_pipe, cv2.CAP_GSTREAMER, 30, (640, 480))
    print('BBBBBBBBBBBB')

    if capture.isOpened() is False:
        return 1

    while(capture.isOpened()):
        try:
            # capture a frame
            ret, frame = capture.read()  # frame is a ndarray of numpy
            if ret is False:
                break

            ##
            ## Here is the image processing place.
            ## eg. resize, color modify, ... etc.
            ##

            # show a frame
            if src == 'file':
                # wait for 1/FR when souece is a file
                k = cv2.waitKey(100)  # unit is ms
                if k == ord('q'):
                    break
            if dst == 'record':
                output.write(frame)
            else:
                cv2.imshoe(frame)
            i += 1
            if i > 100:
                break

        except KeyboardInterrupt:
            # Ctrl+C pressed
            break

    if dst == 'record':
        output.release()
    capture.release()
    cv2.destroyAllWindows()

    return 0

if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
